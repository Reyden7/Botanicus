#include "Visuals/BotanicusJuiceWorldSubsystem.h"
#include "Visuals/BotanicusJuiceRelay.h"
#include "Visuals/BotanicusPlacementJuiceSettings.h"
#include "Sales/BotanicusSalesDisplayActor.h"
#include "Delivery/BotanicusPlaceableItemActor.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "Scalability.h"
namespace {
void PickupMeshes(AActor* A,TArray<UMeshComponent*>& Out,TSet<AActor*>& Seen) {
 if(!IsValid(A)||Seen.Contains(A))return;Seen.Add(A);TInlineComponentArray<UMeshComponent*> Parts(A);
 for(auto* M:Parts)if(M&&!M->IsA<UWidgetComponent>())Out.AddUnique(M);
 TArray<AActor*> Children;A->GetAttachedActors(Children);for(auto* Child:Children)PickupMeshes(Child,Out,Seen);
}
FBotanicusPickupMaterial PickupMaterial(UMaterialInterface* M) {
 FBotanicusPickupMaterial R;
 if(auto* D=Cast<UMaterialInstanceDynamic>(M)) {
  for(const auto& V:D->ScalarParameterValues)if(R.Scalars.Num()<8){R.ScalarNames.Add(V.ParameterInfo.Name);R.Scalars.Add(V.ParameterValue);}
  for(const auto& V:D->VectorParameterValues)if(R.Vectors.Num()<8){R.VectorNames.Add(V.ParameterInfo.Name);R.Vectors.Add(V.ParameterValue);}
  while(auto* Dynamic=Cast<UMaterialInstanceDynamic>(M))M=Dynamic->Parent;
 }
 if(M && !M->HasAnyFlags(RF_Transient))R.Asset=M;return R;
}
bool PickupEnabled() {const auto* S=GetDefault<UBotanicusPlacementJuiceSettings>();return S->bPickupPopEnabled && (!S->bDisableAnimationOnLowEffectsQuality || Scalability::GetQualityLevels().EffectsQuality>0);}
void MaskRender(FBotanicusPlacementCopy& R,UMeshComponent* M) {
 R.Source=M;R.bMain=M->bRenderInMainPass;R.bDepth=M->bRenderInDepthPass;R.bShadow=M->CastShadow;R.bCustom=M->bRenderCustomDepth;R.bRay=M->bVisibleInRayTracing;
 M->SetRenderInMainPass(false);M->SetRenderInDepthPass(false);M->SetCastShadow(false);M->SetRenderCustomDepth(false);M->SetVisibleInRayTracing(false);
}
void RestoreRender(const FBotanicusPlacementCopy& R) {if(auto* M=R.Source.Get()){M->SetRenderInMainPass(R.bMain);M->SetRenderInDepthPass(R.bDepth);M->SetCastShadow(R.bShadow);M->SetRenderCustomDepth(R.bCustom);M->SetVisibleInRayTracing(R.bRay);}}
}
FBotanicusPickupVisual UBotanicusJuiceWorldSubsystem::CapturePickupVisual(AActor* Item,bool bContentsOnly) {
 FBotanicusPickupVisual R;if(!IsValid(Item)||!GetDefault<UBotanicusPlacementJuiceSettings>()->bPickupPopEnabled)return R;
 if(const auto* GS=Item->GetWorld()->GetGameState())R.ServerTime=GS->GetServerWorldTimeSeconds();
 R.SourceName=Item->GetFName();R.SourceTransform=Item->GetActorTransform();R.bContentsOnly=bContentsOnly;
 TArray<UMeshComponent*> Meshes;TSet<AActor*> Seen;
 if(bContentsOnly){if(auto* D=Cast<ABotanicusSalesDisplayActor>(Item))D->GetSalePresentationMeshes(Meshes);}else PickupMeshes(Item,Meshes,Seen);
 FBox Bounds(ForceInit);int Vertices=0,Indices=0;
 for(auto* M:Meshes) {
  if(!M->IsVisible()||M->bHiddenInGame||(M->GetOwner()&&M->GetOwner()->IsHidden()))continue;
  Bounds+=M->Bounds.GetBox();FBotanicusPickupMesh P;P.WorldTransform=M->GetComponentTransform();
  if(auto* Static=Cast<UStaticMeshComponent>(M)) {
   if(!Static->GetStaticMesh())continue;P.Asset=Static->GetStaticMesh();P.bDisableNanite=Static->bDisallowNanite||Static->bForceDisableNanite;
   for(int I=0;I<FMath::Min(8,Static->GetNumMaterials());++I)P.Materials.Add(PickupMaterial(Static->GetMaterial(I)));
   if(auto* Instances=Cast<UInstancedStaticMeshComponent>(M)){if(Instances->GetInstanceCount()>256||Instances->NumCustomDataFloats>0)return {};for(int I=0;I<Instances->GetInstanceCount();++I){FTransform T;Instances->GetInstanceTransform(I,T,false);P.Instances.Add(T);}if(P.Instances.IsEmpty())continue;}
   R.Meshes.Add(MoveTemp(P));
  } else if(auto* Proc=Cast<UProceduralMeshComponent>(M)) {
   for(int I=0;I<Proc->GetNumSections();++I)if(auto* Section=Proc->GetProcMeshSection(I);Section && Section->bSectionVisible && !Section->ProcVertexBuffer.IsEmpty()){
    Vertices+=Section->ProcVertexBuffer.Num();Indices+=Section->ProcIndexBuffer.Num();if(Vertices>512||Indices>3072)return {};
    FBotanicusPickupMesh Part;Part.WorldTransform=P.WorldTransform;Part.Materials.Add(PickupMaterial(Proc->GetMaterial(I)));
    for(const auto& V:Section->ProcVertexBuffer){Part.Vertices.Add(FVector(V.Position));Part.Normals.Add(FVector(V.Normal));Part.UVs.Add(FVector2D(V.UV0));}
    for(auto Index:Section->ProcIndexBuffer)Part.Triangles.Add(static_cast<int32>(Index));R.Meshes.Add(MoveTemp(Part));
   }
  } else return {}; // Unsupported animated geometry keeps its established immediate presentation.
  if(R.Meshes.Num()>FMath::Min(48,GetDefault<UBotanicusPlacementJuiceSettings>()->MaximumMeshesPerAnimation))return {};
 }
 // Bound a one-shot snapshot as well as the number of local rendering components.
 int EstimatedBytes=0;
 for(const auto& Part:R.Meshes){EstimatedBytes+=160+Part.Instances.Num()*80+Part.Vertices.Num()*64+Part.Triangles.Num()*4;
  for(const auto& Mat:Part.Materials)EstimatedBytes+=64+Mat.Scalars.Num()*16+Mat.Vectors.Num()*32;
 }
 if(EstimatedBytes>32768)return {};
 if(Bounds.IsValid)R.Anchor=FVector(Bounds.GetCenter().X,Bounds.GetCenter().Y,Bounds.Min.Z);return R;
}
void UBotanicusJuiceWorldSubsystem::PickupSucceeded(UWorld* W,APawn* Pawn,FName Key,const FBotanicusPickupVisual& Visual,AActor* CarriedSource) {
 if(!W||W->GetNetMode()==NM_Client||!Pawn)return;
 if(auto* J=W->GetSubsystem<UBotanicusJuiceWorldSubsystem>())if(auto* Relay=J->GetRelay())Relay->Pickup(Pawn,Key,Visual,CarriedSource);
}
void UBotanicusJuiceWorldSubsystem::RequestPickupHand(APawn* Pawn,FName Key) {
 if(!Pawn||GetDefault<UBotanicusPlacementJuiceSettings>()->NoAnimationItems.Contains(Key)||!PickupEnabled())return;auto* Gate=PickupHandGates.FindByPredicate([=](const auto& G){return G.Pawn==Pawn&&G.ItemKey==Key;});if(!Gate)Gate=&PickupHandGates.AddDefaulted_GetRef();Gate->Pawn=Pawn;Gate->ItemKey=Key;Gate->Until=GetWorld()->GetTimeSeconds()+1.0;
}
void UBotanicusJuiceWorldSubsystem::CancelPickupHand(APawn* Pawn,FName Key) {
 PickupHandGates.RemoveAll([=](const auto& G){return G.Pawn==Pawn&&(Key.IsNone()||G.ItemKey==Key);});
 for(int I=PickupHandMasks.Num()-1;I>=0;--I)if(PickupHandMasks[I].Pawn==Pawn&&(Key.IsNone()||PickupHandMasks[I].ItemKey==Key)){for(const auto& M:PickupHandMasks[I].Meshes)RestoreRender(M);PickupHandMasks.RemoveAtSwap(I);}
}
void UBotanicusJuiceWorldSubsystem::MaskPickupHand(APawn* Pawn,FName Key,AActor* Actor,bool bContentsOnly) {
 if(!IsValid(Actor))return;const double Now=GetWorld()->GetTimeSeconds();const auto* Gate=PickupHandGates.FindByPredicate([=](const auto& G){return G.Pawn==Pawn&&G.ItemKey==Key&&G.Until>Now;});if(!Gate)return;
 for(int I=PlacementAnimations.Num()-1;I>=0;--I)if(PlacementAnimations[I].Source==Actor)FinishPlacementAnimation(I,false);
 auto* Mask=PickupHandMasks.FindByPredicate([=](const auto& M){return M.Actor==Actor;});
 if(!Mask){Mask=&PickupHandMasks.AddDefaulted_GetRef();Mask->Actor=Actor;Mask->Pawn=Pawn;Mask->ItemKey=Key;TArray<UMeshComponent*> Meshes;TSet<AActor*> Seen;if(bContentsOnly){if(auto* D=Cast<ABotanicusSalesDisplayActor>(Actor))D->GetSalePresentationMeshes(Meshes);}else PickupMeshes(Actor,Meshes,Seen);for(auto* M:Meshes){auto& Record=Mask->Meshes.AddDefaulted_GetRef();MaskRender(Record,M);}}
 Mask->Until=Gate->Until;
}
void UBotanicusJuiceWorldSubsystem::ReceivePickup(uint32 Sequence,APawn* Pawn,FName Key,const FBotanicusPickupVisual& Visual,AActor* CarriedSource) {
 if(!Sequence||static_cast<int32>(Sequence-LastPickupSequence)<=0)return;LastPickupSequence=Sequence;
 const auto* S=GetDefault<UBotanicusPlacementJuiceSettings>();
 if(Visual.Meshes.IsEmpty()){CancelPickupHand(Pawn,Key);return;}
 if(const auto* GS=GetWorld()->GetGameState();GS && GS->GetServerWorldTimeSeconds()-Visual.ServerTime>1.0)return;
 if(!Pawn||S->NoAnimationItems.Contains(Key)||!PickupEnabled()||Visual.Meshes.IsEmpty()||Visual.Meshes.Num()>48||PickupAnimations.Num()+PlacementAnimations.Num()>=S->MaximumSimultaneousAnimations||FVector::DistSquared(ListenerPosition(),Visual.Anchor)>FMath::Square(S->MaximumDisplayDistance)){CancelPickupHand(Pawn,Key);return;}
 FBotanicusPickupAnimation A;A.Pawn=Pawn;A.CarriedSource=CarriedSource;A.Anchor=Visual.Anchor;A.StartedAt=GetWorld()->GetTimeSeconds();A.Duration=FMath::Clamp(S->PickupPopDuration,.05f,.5f);A.StartScale=FMath::Clamp(S->PickupPopStartScale,.1f,1.2f);A.EndScale=FMath::Clamp(S->PickupPopEndScale,0.f,.2f);
 if(GetPlacementCopyCount()+GetPickupCopyCount()+Visual.Meshes.Num()>S->MaximumVisualComponents){CancelPickupHand(Pawn,Key);return;}
 for(const auto& Part:Visual.Meshes) {
  UMeshComponent* Copy=nullptr;
  if(Part.Asset){if(!Part.Instances.IsEmpty()){auto* M=NewObject<UInstancedStaticMeshComponent>(GetWorld(),NAME_None,RF_Transient);M->SetStaticMesh(Part.Asset);for(const auto& T:Part.Instances)M->AddInstance(T,false);Copy=M;}else{auto* M=NewObject<UStaticMeshComponent>(GetWorld(),NAME_None,RF_Transient);M->SetStaticMesh(Part.Asset);Copy=M;}Cast<UStaticMeshComponent>(Copy)->bForceDisableNanite=Part.bDisableNanite;}
  else if(!Part.Vertices.IsEmpty()){auto* M=NewObject<UProceduralMeshComponent>(GetWorld(),NAME_None,RF_Transient);M->CreateMeshSection_LinearColor(0,Part.Vertices,Part.Triangles,Part.Normals,Part.UVs,TArray<FLinearColor>(),TArray<FProcMeshTangent>(),false);Copy=M;}
  if(!Copy)continue;
  Copy->SetCanEverAffectNavigation(false);Copy->SetMobility(EComponentMobility::Movable);Copy->SetCollisionEnabled(ECollisionEnabled::NoCollision);Copy->SetGenerateOverlapEvents(false);Copy->SetIsReplicated(false);Copy->SetCastShadow(false);
  for(int I=0;I<Part.Materials.Num();++I){const auto& Mat=Part.Materials[I];if(!Mat.Asset)continue;UMaterialInterface* Use=Mat.Asset;if(!Mat.Scalars.IsEmpty()||!Mat.Vectors.IsEmpty()){auto* D=UMaterialInstanceDynamic::Create(Mat.Asset,Copy);for(int J=0;J<FMath::Min(Mat.ScalarNames.Num(),Mat.Scalars.Num());++J)D->SetScalarParameterValue(Mat.ScalarNames[J],Mat.Scalars[J]);for(int J=0;J<FMath::Min(Mat.VectorNames.Num(),Mat.Vectors.Num());++J)D->SetVectorParameterValue(Mat.VectorNames[J],Mat.Vectors[J]);Use=D;}Copy->SetMaterial(I,Use);}
  FTransform T=Part.WorldTransform;T.SetLocation(A.Anchor+(T.GetLocation()-A.Anchor)*A.StartScale);T.SetScale3D(T.GetScale3D()*A.StartScale);Copy->SetWorldTransform(T);Copy->RegisterComponentWithWorld(GetWorld());auto& Record=A.Meshes.AddDefaulted_GetRef();Record.Copy=Copy;Record.FinalWorldTransform=Part.WorldTransform;
 }
 if(A.Meshes.IsEmpty()){CancelPickupHand(Pawn,Key);return;}
 RequestPickupHand(Pawn,Key);auto* Gate=PickupHandGates.FindByPredicate([=](const auto& G){return G.Pawn==Pawn&&G.ItemKey==Key;});Gate->Until=A.StartedAt+A.Duration;
 if(!CarriedSource){
  AActor* Source=nullptr;AActor* UniquePose=nullptr;int Matches=0;
  for(TActorIterator<AActor> It(GetWorld());It;++It)if(It->GetIsReplicated()&&It->GetActorTransform().Equals(Visual.SourceTransform,.5f)){
   if(It->GetFName()==Visual.SourceName){Source=*It;break;}
   const auto* Item=Cast<ABotanicusPlaceableItemActor>(*It);
   if((Visual.bContentsOnly&&Cast<ABotanicusSalesDisplayActor>(*It))||(!Visual.bContentsOnly&&Item&&Item->GetItemKey()==Key)){UniquePose=*It;++Matches;}
  }
  if(!Source&&Matches==1)Source=UniquePose;
  if(Source)MaskPickupHand(Pawn,Key,Source,Visual.bContentsOnly);
 }
 if(CarriedSource)MaskPickupHand(Pawn,Key,CarriedSource);PickupAnimations.Add(MoveTemp(A));++PickupsStarted;
}
int32 UBotanicusJuiceWorldSubsystem::GetPickupCopyCount() const {int N=0;for(const auto& A:PickupAnimations)N+=A.Meshes.Num();return N;}
FVector UBotanicusJuiceWorldSubsystem::GetPickupPhaseSample() const {return PickupAnimations.IsEmpty()?FVector(-1,1,0):FVector(GetWorld()->GetTimeSeconds()-PickupAnimations[0].StartedAt,PickupAnimations[0].CurrentScale,PickupAnimations[0].Meshes.Num());}
void UBotanicusJuiceWorldSubsystem::UpdatePickupJuice() {
 const double Now=GetWorld()->GetTimeSeconds();
 for(int I=PickupAnimations.Num()-1;I>=0;--I){auto& A=PickupAnimations[I];float U=FMath::Clamp(float((Now-A.StartedAt)/A.Duration),0.f,1.f);
  if(U>=1||!A.Pawn.IsValid()||!PickupEnabled()){for(auto& M:A.Meshes)if(M.Copy)M.Copy->DestroyComponent();PickupAnimations.RemoveAtSwap(I);++PickupsFinished;continue;}
  float Ease=U*U*(3-2*U);if(PickupCurve){const float V=PickupCurve->GetFloatValue(U);if(FMath::IsFinite(V))Ease=FMath::Clamp(V,0.f,1.f);}A.CurrentScale=FMath::Lerp(A.StartScale,A.EndScale,Ease);
  for(auto& M:A.Meshes){auto T=M.FinalWorldTransform;T.SetLocation(A.Anchor+(T.GetLocation()-A.Anchor)*A.CurrentScale);T.SetScale3D(T.GetScale3D()*A.CurrentScale);M.Copy->SetWorldTransform(T);}
 }
 PickupHandGates.RemoveAll([=](const auto& G){return !G.Pawn.IsValid()||Now>=G.Until||!PickupEnabled();});
 for(int I=PickupHandMasks.Num()-1;I>=0;--I){auto& M=PickupHandMasks[I];if(!M.Actor.IsValid()||!M.Pawn.IsValid()||Now>=M.Until||!PickupEnabled()){for(const auto& Part:M.Meshes)RestoreRender(Part);PickupHandMasks.RemoveAtSwap(I);}}
}
void UBotanicusJuiceWorldSubsystem::AbortPickupSource(AActor* Source) {
 if(!IsValid(Source))return;
 if(GetWorld()->GetNetMode()!=NM_Client){if(auto* R=GetRelay())R->MulticastAbortPickup(Source);}else ReceiveAbortPickup(Source);
}
void UBotanicusJuiceWorldSubsystem::ReceiveAbortPickup(AActor* Source) {
 for(int I=PickupAnimations.Num()-1;I>=0;--I)if(PickupAnimations[I].CarriedSource==Source){for(auto& M:PickupAnimations[I].Meshes)if(M.Copy)M.Copy->DestroyComponent();PickupAnimations.RemoveAtSwap(I);++PickupsFinished;}
 for(int I=PickupHandMasks.Num()-1;I>=0;--I)if(PickupHandMasks[I].Actor==Source){auto P=PickupHandMasks[I].Pawn;const FName Key=PickupHandMasks[I].ItemKey;CancelPickupHand(P.Get(),Key);break;}
}
void UBotanicusJuiceWorldSubsystem::ClearPickupJuice(){for(auto& A:PickupAnimations)for(auto& M:A.Meshes)if(M.Copy)M.Copy->DestroyComponent();for(const auto& A:PickupHandMasks)for(const auto& M:A.Meshes)RestoreRender(M);PickupAnimations.Reset();PickupHandMasks.Reset();PickupHandGates.Reset();}
