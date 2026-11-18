#include "Gardening/BotanicusRobotContactSurface.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#if WITH_EDITOR
#include "Engine/StaticMesh.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkinWeightVertexBuffer.h"
#include "StaticMeshResources.h"
#endif

bool UBotanicusRobotContactSurface::Trace(USkeletalMeshComponent* Mesh,UStaticMeshComponent* Tank,const FHitResult& Hit,FVector& Point,FVector& Normal) const
{
    if(!Mesh || SourceMesh.Get()!=Mesh->GetSkeletalMeshAsset() || Vertices.IsEmpty())return false;
    TArray<FTransform,TInlineAllocator<64>> Pose;Pose.Reserve(Bones.Num());
    for(FName Bone:Bones)Pose.Add(Mesh->GetSocketTransform(Bone,RTS_World));
    TArray<FVector> WorldVertices;WorldVertices.SetNumZeroed(Vertices.Num());
    for(int32 I=0;I<Vertices.Num();++I)for(const auto& Influence:Vertices[I].Influences)
        if(Pose.IsValidIndex(Influence.Bone))WorldVertices[I]+=Pose[Influence.Bone].TransformPosition(Influence.BonePosition)*Influence.Weight;
    float RayDistance=TNumericLimits<float>::Max(),NearestDistance=TNumericLimits<float>::Max();
    FVector Nearest=Hit.ImpactPoint,NearestNormal=Hit.ImpactNormal;bool bRay=false,bSurface=false;
    const FVector Direction=(Hit.TraceEnd-Hit.TraceStart).GetSafeNormal();
    auto Triangles=[&](const auto& Positions,const TArray<int32>& Triangles)
    {
        for(int32 I=0;I+2<Triangles.Num();I+=3)
        {
            const FVector A=Positions[Triangles[I]],B=Positions[Triangles[I+1]],C=Positions[Triangles[I+2]];
            FVector N=(B-A).Cross(C-A).GetSafeNormal();if(N.IsNearlyZero())continue;
            if(FVector::DotProduct(N,Direction)>0)N=-N;
            FVector At,TriangleNormal;
            if(FMath::SegmentTriangleIntersection(Hit.TraceStart,Hit.TraceEnd,A,B,C,At,TriangleNormal))
            {
                const float D=FVector::DistSquared(Hit.TraceStart,At);
                if(D<RayDistance){RayDistance=D;Point=At;Normal=N;bRay=true;}
            }
            // A capsule hit in a gap between visible parts still gets its closest actual surface.
            const FVector Closest=FMath::ClosestPointOnTriangleToPoint(Hit.ImpactPoint,A,B,C);
            const float D=FVector::DistSquared(Closest,Hit.ImpactPoint);
            if(D<NearestDistance){NearestDistance=D;Nearest=Closest;NearestNormal=N;bSurface=true;}
        }
    };
    Triangles(WorldVertices,Indices);
    if(Tank && SourceTank.Get()==Tank->GetStaticMesh() && !TankVertices.IsEmpty())
    {
        TArray<FVector> WorldTank;WorldTank.Reserve(TankVertices.Num());
        for(const FVector& V:TankVertices)WorldTank.Add(Tank->GetComponentTransform().TransformPosition(V));
        Triangles(WorldTank,TankIndices);
    }
    if(!bRay && bSurface){Point=Nearest;Normal=NearestNormal;}
    return bRay || bSurface;
}
#if WITH_EDITOR
bool UBotanicusRobotContactSurface::Bake(USkeletalMesh* Mesh,UStaticMesh* Tank)
{
    if(!Mesh || !Mesh->GetResourceForRendering() || Mesh->GetResourceForRendering()->LODRenderData.IsEmpty())return false;
    const auto& LOD=Mesh->GetResourceForRendering()->LODRenderData[0];const auto* Weights=LOD.GetSkinWeightVertexBuffer();
    if(!Weights)return false;
    SourceMesh=Mesh;Bones.Reset();Vertices.Reset();Indices.Reset();TankVertices.Reset();TankIndices.Reset();SourceTank=Tank;
    const auto& Ref=Mesh->GetRefSkeleton();TArray<FTransform> Bind;
    for(int32 I=0;I<Ref.GetNum();++I)
    {
        Bones.Add(Ref.GetBoneName(I));FTransform T=Ref.GetRefBonePose()[I];
        if(Ref.GetParentIndex(I)>=0)T*=Bind[Ref.GetParentIndex(I)];Bind.Add(T);
    }
    Vertices.SetNum(LOD.GetNumVertices());
    for(const auto& Section:LOD.RenderSections)for(uint32 I=Section.BaseVertexIndex;I<Section.BaseVertexIndex+Section.NumVertices;++I)
    {
        auto& V=Vertices[I];const FVector Position(LOD.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(I));float Total=0;
        for(uint32 W=0;W<Weights->GetMaxBoneInfluences();++W)
        {
            const uint32 Raw=Weights->GetBoneWeight(I,W);if(!Raw)continue;
            const int32 Bone=Section.BoneMap[Weights->GetBoneIndex(I,W)];auto& F=V.Influences.AddDefaulted_GetRef();
            F.Bone=Bone;F.Weight=Raw;F.BonePosition=Bind[Bone].InverseTransformPosition(Position);Total+=F.Weight;
        }
        if(Total<=0)return false;for(auto& F:V.Influences)F.Weight/=Total;
    }
    auto* Buffer=LOD.MultiSizeIndexContainer.GetIndexBuffer();for(int32 I=0;I<Buffer->Num();++I)Indices.Add(Buffer->Get(I));
    if(Tank && Tank->GetRenderData() && !Tank->GetRenderData()->LODResources.IsEmpty())
    {
        const auto& L=Tank->GetRenderData()->LODResources[0];
        for(uint32 I=0;I<L.VertexBuffers.PositionVertexBuffer.GetNumVertices();++I)TankVertices.Add(FVector(L.VertexBuffers.PositionVertexBuffer.VertexPosition(I)));
        for(int32 I=0;I<L.IndexBuffer.GetNumIndices();++I)TankIndices.Add(L.IndexBuffer.GetIndex(I));
    }
    return !Vertices.IsEmpty() && !Indices.IsEmpty();
}
#endif
