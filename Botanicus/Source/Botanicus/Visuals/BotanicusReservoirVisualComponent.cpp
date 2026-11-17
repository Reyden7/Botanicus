#include "Visuals/BotanicusReservoirVisualComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

UBotanicusReservoirVisualComponent::UBotanicusReservoirVisualComponent()
{
    PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.bStartWithTickEnabled=false;
    PrimaryComponentTick.TickInterval=1.f/30;PrimaryComponentTick.TickGroup=TG_PostPhysics;
    const FString Root=TEXT("/Game/Botanicus/Materials/Reservoirs/");
    RobotWaterMaterial=TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(Root+TEXT("MI_WaterGauge_Robot.MI_WaterGauge_Robot")));
    ReserveWaterMaterial=TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(Root+TEXT("MI_WaterGauge_Reserve.MI_WaterGauge_Reserve")));
    RobotGlassMaterial=TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(Root+TEXT("MI_Glass_Robot.MI_Glass_Robot")));
    ReserveGlassMaterial=TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(Root+TEXT("MI_Glass_Reserve.MI_Glass_Reserve")));
    Liquid=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ReservoirLiquid"));Liquid->SetupAttachment(this);
    GaugeGlass=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ReservoirGaugeGlass"));GaugeGlass->SetupAttachment(this);
    for(auto* Part:{Liquid.Get(),GaugeGlass.Get()})
    {
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);Part->SetGenerateOverlapEvents(false);
        Part->SetCanEverAffectNavigation(false);Part->SetCastShadow(false);Part->SetVisibility(false);
        Part->SetComponentTickEnabled(false);
    }
}
void UBotanicusReservoirVisualComponent::OnRegister()
{
    Super::OnRegister();
    if(IsTemplate())return;
    for(auto* Part:{Liquid.Get(),GaugeGlass.Get()})if(Part)
    {
        // Blueprint subobject instancing can retain the archetype's attachment pointer.
        // Bind each instance to this scene component before its first registration.
        if(Part->GetAttachParent()!=this)
        {
            if(Part->IsRegistered())Part->AttachToComponent(this,FAttachmentTransformRules::KeepRelativeTransform);
            else Part->SetupAttachment(this);
        }
        if(!Part->IsRegistered())Part->RegisterComponent();
    }
}
void UBotanicusReservoirVisualComponent::Configure(UStaticMeshComponent* Container,bool bGauge)
{
    if(!Container || !Container->GetStaticMesh() || (GetWorld() && GetWorld()->GetNetMode()==NM_DedicatedServer))
    {SetReservoirVisible(false);return;}
    const FBox ContainerBounds=Container->GetStaticMesh()->GetBoundingBox();
    const FVector Center=bGauge?GaugeCenter:ContainerBounds.GetCenter();
    const FVector Half=bGauge?GaugeHalfSize:ContainerBounds.GetExtent()*FMath::Clamp(InteriorScale,.5f,.99f);
    const bool bRebuild=!bConfigured || bIsGauge!=bGauge || BuiltMesh!=Container->GetStaticMesh() || !BuiltCenter.Equals(Center) || !BuiltHalfSize.Equals(Half) || (bGauge && !FMath::IsNearlyEqual(BuiltGlassGap,GlassGap));
    bIsGauge=bGauge;
    if(ContainerMesh!=Container)
    {
        if(ContainerMesh)ContainerMesh->TransformUpdated.RemoveAll(this);
        ContainerMesh=Container;ContainerMesh->TransformUpdated.AddUObject(this,&ThisClass::ContainerTransformUpdated);
    }
    if(GetAttachParent()!=Container)AttachToComponent(Container,FAttachmentTransformRules::SnapToTargetIncludingScale);
    // Mesh-local coordinates; both the liquid and glass follow authored mesh transforms.
    SetRelativeTransform(FTransform::Identity);
    auto* Water=(bGauge?ReserveWaterMaterial:RobotWaterMaterial).LoadSynchronous();
    auto* Glass=(bGauge?ReserveGlassMaterial:RobotGlassMaterial).LoadSynchronous();
    if(!Water || !Glass){SetReservoirVisible(false);return;}
    if(!WaterMID || WaterMID->Parent!=Water)
    {WaterMID=UMaterialInstanceDynamic::Create(Water,this);Liquid->SetMaterial(0,WaterMID);bSloshWritten=false;}
    if(bGauge)GaugeGlass->SetMaterial(0,Glass);else if(Container->GetMaterial(0)!=Glass)Container->SetMaterial(0,Glass);
    if(bRebuild)
    {
        BuiltMesh=Container->GetStaticMesh();BuiltCenter=Center;BuiltHalfSize=Half;BuiltGlassGap=GlassGap;
        TArray<FVector> Vertices,Normals;TArray<int32> Triangles;TArray<FVector2D> UV;TArray<FLinearColor> Colors;
        auto Vertex=[&](FVector P,FVector N,FVector2D T,float Top,float Cap)
        {Vertices.Add(P);Normals.Add(N);UV.Add(T);Colors.Add(FLinearColor(Top,Cap,0,1));};
        auto Quad=[&](float Y)
        {
            Vertex(Center+FVector(-Half.X,Y-Center.Y,-Half.Z),FVector(0,1,0),FVector2D(0,0),0,0);
            Vertex(Center+FVector(-Half.X,Y-Center.Y, Half.Z),FVector(0,1,0),FVector2D(0,1),1,0);
            Vertex(Center+FVector( Half.X,Y-Center.Y, Half.Z),FVector(0,1,0),FVector2D(1,1),1,0);
            Vertex(Center+FVector( Half.X,Y-Center.Y,-Half.Z),FVector(0,1,0),FVector2D(1,0),0,0);
            Triangles={0,1,2,0,2,3};
        };
        if(bGauge)
        {
            Quad(Center.Y+GlassGap);
            GaugeGlass->CreateMeshSection_LinearColor(0,Vertices,Triangles,Normals,UV,Colors,{},false);
            Vertices.Reset();Normals.Reset();UV.Reset();Colors.Reset();Quad(Center.Y);
        }
        else
        {
            constexpr int32 Sides=32;
            for(int32 I=0;I<=Sides;++I)
            {
                const float A=I*2*PI/Sides;const FVector N(FMath::Cos(A),FMath::Sin(A),0);
                for(int32 J=0;J<2;++J)Vertex(Center+FVector(N.X*Half.X,N.Y*Half.Y,(J?1:-1)*Half.Z),N,FVector2D(float(I)/Sides,J),J,0);
                if(I<Sides){const int32 K=I*2;Triangles.Append({K,K+2,K+1,K+1,K+2,K+3});}
            }
            for(int32 Top=0;Top<2;++Top)
            {
                const int32 Base=Vertices.Num();const float Z=(Top?1:-1)*Half.Z;
                Vertex(Center+FVector(0,0,Z),FVector(0,0,Top?1:-1),FVector2D(.5,.5),Top,Top);
                for(int32 I=0;I<=Sides;++I)
                {
                    const float A=I*2*PI/Sides;const FVector P(FMath::Cos(A)*Half.X,FMath::Sin(A)*Half.Y,Z);
                    Vertex(Center+P,FVector(0,0,Top?1:-1),FVector2D(.5f+.5f*FMath::Cos(A),.5f+.5f*FMath::Sin(A)),Top,Top);
                    if(I<Sides)if(Top)Triangles.Append({Base,Base+I+1,Base+I+2});else Triangles.Append({Base,Base+I+2,Base+I+1});
                }
            }
            GaugeGlass->ClearAllMeshSections();
        }
        Liquid->CreateMeshSection_LinearColor(0,Vertices,Triangles,Normals,UV,Colors,{},false);
        bConfigured=true;PreviousVelocity=GetOwner()?GetOwner()->GetVelocity():FVector::ZeroVector;
    }
    WaterMID->SetVectorParameterValue(TEXT("BoundsCenter"),FLinearColor(Center.X,Center.Y,Center.Z,0));
    WaterMID->SetVectorParameterValue(TEXT("BoundsHalfSize"),FLinearColor(Half.X,Half.Y,Half.Z,0));
    WaterMID->SetScalarParameterValue(TEXT("GaugeMode"),bGauge?1:0);
    WaterMID->SetScalarParameterValue(TEXT("WaterLevel"),FillFraction);
    SetReservoirVisible(true);WriteSlosh();
}
void UBotanicusReservoirVisualComponent::SetReservoirVisible(bool bShow)
{
    bDisplay=bShow;Liquid->SetVisibility(bShow && bConfigured);GaugeGlass->SetVisibility(bShow && bConfigured && bIsGauge);
    if(!bShow)SetComponentTickEnabled(false);
}
bool UBotanicusReservoirVisualComponent::CanAnimate() const
{
    if(!GetWorld() || !ContainerMesh || !bDisplay || bIsGauge || !bSloshEnabled || FillFraction<=.0001f || FillFraction>=.9999f || !ContainerMesh->WasRecentlyRendered(.4f))return false;
    for(auto I=GetWorld()->GetPlayerControllerIterator();I;++I)if(auto* PC=I->Get())if(PC->IsLocalController())
    {FVector P;FRotator R;PC->GetPlayerViewPoint(P,R);if(FVector::DistSquared(P,GetComponentLocation())<FMath::Square(MaxVisualDistance))return true;}
    return false;
}
void UBotanicusReservoirVisualComponent::SetFillFraction(float Fraction)
{
    const float Next=FMath::Clamp(Fraction,0.f,1.f);
    if(Next!=FillFraction){FillFraction=Next;if(WaterMID)WaterMID->SetScalarParameterValue(TEXT("WaterLevel"),FillFraction);}
    if(!bConfigured || bIsGauge)return;
    const FVector Velocity=GetOwner()?GetOwner()->GetVelocity():FVector::ZeroVector;
    if(CanAnimate() && (!Velocity.IsNearlyZero(1) || !PreviousVelocity.IsNearlyZero(1) || Slosh.SizeSquared()>1.e-7f || SloshVelocity.SizeSquared()>1.e-7f))
        SetComponentTickEnabled(true);
    else if(!CanAnimate())
    {PreviousVelocity=Velocity;Slosh=SloshVelocity=FilteredAcceleration=FVector2D::ZeroVector;WriteSlosh();SetComponentTickEnabled(false);}
}
void UBotanicusReservoirVisualComponent::WriteSlosh()
{
    if(!WaterMID)return;
    FVector Scale=ContainerMesh?ContainerMesh->GetComponentScale().GetAbs():FVector::OneVector;
    const bool bUseSlosh=!bIsGauge && bSloshEnabled;
    const FVector Up=ContainerMesh?ContainerMesh->GetComponentTransform().InverseTransformVectorNoScale(FVector::UpVector):FVector::UpVector;
    const FVector2D GravitySlope=!bIsGauge && Up.Z>.25?FVector2D(-Up.X/Up.Z,-Up.Y/Up.Z):FVector2D::ZeroVector;
    const FVector2D MaterialSlope((Slosh.X+GravitySlope.X)*Scale.X/FMath::Max(.001,Scale.Z),(Slosh.Y+GravitySlope.Y)*Scale.Y/FMath::Max(.001,Scale.Z));
    if(!bSloshWritten || bLastSloshEnabled!=bUseSlosh)WaterMID->SetScalarParameterValue(TEXT("SloshEnabled"),bUseSlosh?1:0);
    if(!bSloshWritten || !LastWrittenSlosh.Equals(MaterialSlope,1.e-6))
    {WaterMID->SetScalarParameterValue(TEXT("SloshTiltX"),MaterialSlope.X);WaterMID->SetScalarParameterValue(TEXT("SloshTiltY"),MaterialSlope.Y);}
    bSloshWritten=true;bLastSloshEnabled=bUseSlosh;LastWrittenSlosh=MaterialSlope;
}
void UBotanicusReservoirVisualComponent::TickComponent(float Dt,ELevelTick Type,FActorComponentTickFunction* Tick)
{
    Super::TickComponent(Dt,Type,Tick);
    if(!CanAnimate() || Dt<=SMALL_NUMBER){SetFillFraction(FillFraction);return;}
    const FVector Velocity=GetOwner()->GetVelocity();
    FVector A=(Velocity-PreviousVelocity)/Dt;PreviousVelocity=Velocity;
    A=ContainerMesh->GetComponentTransform().InverseTransformVectorNoScale(A.GetClampedToMaxSize(1500));
    const FVector2D TargetAcceleration(-A.X/980.f*AccelerationStrength,-A.Y/980.f*AccelerationStrength);
    const float Limit=FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(MaxSloshAngle,0.f,15.f)));
    const int32 Steps=FMath::Clamp(FMath::CeilToInt(Dt*120),1,60);const float H=FMath::Min(Dt,.5f)/Steps;
    const float Frequency=FMath::Max(1.f,SpringFrequency);
    for(int32 I=0;I<Steps;++I)
    {
        FilteredAcceleration=FMath::Lerp(FilteredAcceleration,TargetAcceleration,1-FMath::Exp(-H/.12f));
        const FVector2D Target=FilteredAcceleration.GetClampedToMaxSize(Limit);
        SloshVelocity+=((Target-Slosh)*(Frequency*Frequency)-SloshVelocity*(2*FMath::Max(.1f,DampingRatio)*Frequency))*H;
        Slosh=(Slosh+SloshVelocity*H).GetClampedToMaxSize(Limit);
    }
    if(Velocity.IsNearlyZero(1) && Slosh.SizeSquared()<1.e-7f && SloshVelocity.SizeSquared()<1.e-7f)
    {Slosh=SloshVelocity=FilteredAcceleration=FVector2D::ZeroVector;SetComponentTickEnabled(false);}
    WriteSlosh();
}

void UBotanicusReservoirVisualComponent::ContainerTransformUpdated(USceneComponent* Component,EUpdateTransformFlags Flags,ETeleportType Teleport)
{
    if(bConfigured && !bIsGauge && bDisplay && FillFraction>0 && FillFraction<1 && ContainerMesh && ContainerMesh->WasRecentlyRendered(.4f))WriteSlosh();
}
