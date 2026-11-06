#include "Visuals/BotanicusJuiceWorldSubsystem.h"
#include "Visuals/BotanicusPlacementJuiceSettings.h"
#include "Visuals/BotanicusJuiceRelay.h"
#include "Audio/BotanicusAudioWorldSubsystem.h"
#include "BotanicusPlayerController.h"
#include "BotanicusCharacter.h"
#include "Delivery/BotanicusPlaceableItemActor.h"
#include "Delivery/BotanicusLargeEquipmentActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "Scalability.h"

namespace
{
void GatherPlacementMeshes(AActor* Actor,TArray<UMeshComponent*>& Meshes,TSet<AActor*>& Seen)
{
    if(!IsValid(Actor) || Seen.Contains(Actor)) return;Seen.Add(Actor);
    TInlineComponentArray<UMeshComponent*> Parts(Actor);
    for(UMeshComponent* Part:Parts) if(Part && !Part->IsA<UWidgetComponent>()) Meshes.AddUnique(Part);
    TArray<AActor*> Children;Actor->GetAttachedActors(Children);
    for(AActor* Child:Children) GatherPlacementMeshes(Child,Meshes,Seen);
}
TArray<UMeshComponent*> PlacementMeshes(AActor* Actor)
{
    TArray<UMeshComponent*> Result;TSet<AActor*> Seen;GatherPlacementMeshes(Actor,Result,Seen);return Result;
}
FName PlacementKey(AActor* Actor)
{
    if(const auto* Item=Cast<ABotanicusPlaceableItemActor>(Actor)) return Item->GetItemKey();
    if(const auto* Equipment=Cast<ABotanicusLargeEquipmentActor>(Actor)) return Equipment->GetItemKey();
    return NAME_None;
}
void PlayContact(UWorld* World,FName Id,const FVector& Position)
{
    if(auto* Audio=World->GetSubsystem<UBotanicusAudioWorldSubsystem>()) Audio->PlayPlacementContact(Id,Position);
}
float PhaseCurve(UCurveFloat* Curve,float Time,float Fallback)
{
    const float Value=Curve ? Curve->GetFloatValue(Time) : Fallback;return FMath::IsFinite(Value) ? Value : Fallback;
}
void HideOnlyLocalRender(UMeshComponent* Mesh)
{
    // bVisible/HiddenInGame and actor visibility can replicate. These render flags do not.
    Mesh->SetRenderInMainPass(false);Mesh->SetRenderInDepthPass(false);
    Mesh->SetCastShadow(false);Mesh->SetRenderCustomDepth(false);Mesh->SetVisibleInRayTracing(false);
}
}

FVector ABotanicusPlayerController::GetPlacementCosmeticHandPosition(FName ExpectedItem) const
{
    if(IsValid(LocalInspectedQuickBarItem) && LocalInspectedQuickBarItem->GetItemKey()==ExpectedItem)
        return LocalInspectedQuickBarItem->GetActorLocation();
    if(IsValid(LocalEquippedQuickBarItem) && LocalEquippedQuickBarItem->GetItemKey()==ExpectedItem)
        return LocalEquippedQuickBarItem->GetActorLocation();
    FVector Eye;FRotator View;GetPlayerViewPoint(Eye,View);const FRotationMatrix Axes(View);
    return Eye+View.Vector()*55.f+Axes.GetUnitAxis(EAxis::Y)*24.f-Axes.GetUnitAxis(EAxis::Z)*24.f;
}
bool ABotanicusPlayerController::UsesPlacementCosmeticActorPivot(const AActor* Item) const
{
    const auto* Placeable=Cast<ABotanicusPlaceableItemActor>(Item);
    return Placeable && Placeable->UsesBlueprintAppearance() && IsFurnitureActor(Item);
}
FVector UBotanicusJuiceWorldSubsystem::PlacementHandPosition(APawn* Pawn,FName ExpectedItem)
{
    if(!IsValid(Pawn)) return FVector::ZeroVector;
    if(Pawn->IsLocallyControlled()) if(auto* PC=Cast<ABotanicusPlayerController>(Pawn->GetController()))
        return PC->GetPlacementCosmeticHandPosition(ExpectedItem);
    if(auto* Character=Cast<ABotanicusCharacter>(Pawn)) if(Character->GetMesh() && Character->GetMesh()->DoesSocketExist(TEXT("hand_r")))
        return Character->GetMesh()->GetSocketLocation(TEXT("hand_r"));
    return Pawn->GetPawnViewLocation()+Pawn->GetBaseAimRotation().Vector()*45.f-FVector(0,0,25);
}
void UBotanicusJuiceWorldSubsystem::PlacementSucceeded(AActor* Item,APawn* Placer)
{
    if(!IsValid(Item) || Item->GetWorld()->GetNetMode()==NM_Client || !Item->HasAuthority()) return;
    if(auto* Juice=Item->GetWorld()->GetSubsystem<UBotanicusJuiceWorldSubsystem>())
        if(auto* Events=Juice->GetRelay()) Events->Placement(Item,Placer,PlacementKey(Item),Item->GetActorTransform(),
            PlacementHandPosition(Placer,PlacementKey(Item)),UBotanicusAudioWorldSubsystem::PlacementEvent(Item));
}
bool UBotanicusJuiceWorldSubsystem::SetPlacementHologram(AActor* Preview,bool bValid)
{
    const auto* S=GetDefault<UBotanicusPlacementJuiceSettings>();
    if(!IsValid(Preview) || !S->bPlacementJuiceEnabled || !S->bPlacementHologramEnabled || GetWorld()->GetNetMode()==NM_DedicatedServer || !PlacementHologramMaterial) return false;
    auto* Hologram=PlacementHolograms.Find(Preview);
    if(!Hologram)
    {
        auto& New=PlacementHolograms.Add(Preview);New.Material=UMaterialInstanceDynamic::Create(PlacementHologramMaterial,this);
        New.Color=bValid ? S->ValidColor : S->InvalidColor;New.Rim=bValid ? S->ValidRim : S->InvalidRim;Hologram=&New;
    }
    Hologram->bValid=bValid;RefreshPlacementHologram(Preview,*Hologram,0);return true;
}
void UBotanicusJuiceWorldSubsystem::RefreshPlacementHologram(AActor* Preview,FBotanicusPlacementHologram& Hologram,float DeltaTime)
{
    const auto* S=GetDefault<UBotanicusPlacementJuiceSettings>();
    const float Alpha=S->ColorTransitionSeconds<=0 ? 1.f : 1.f-FMath::Exp(-3.f*DeltaTime/FMath::Max(.01f,S->ColorTransitionSeconds));
    Hologram.Color=FMath::Lerp(Hologram.Color,Hologram.bValid ? S->ValidColor : S->InvalidColor,Alpha);
    Hologram.Rim=FMath::Lerp(Hologram.Rim,Hologram.bValid ? S->ValidRim : S->InvalidRim,Alpha);
    auto* MID=Hologram.Material.Get();if(!MID) return;
    MID->SetVectorParameterValue(TEXT("BodyTint"),Hologram.Color);MID->SetVectorParameterValue(TEXT("RimTint"),Hologram.Rim);
    MID->SetScalarParameterValue(TEXT("Opacity"),FMath::Clamp(S->Opacity,.1f,.8f));
    MID->SetScalarParameterValue(TEXT("FresnelIntensity"),FMath::Clamp(S->FresnelIntensity,0.f,2.f));
    MID->SetScalarParameterValue(TEXT("Emission"),FMath::Clamp(S->EmissiveIntensity,0.f,3.f));
    for(UMeshComponent* Mesh:PlacementMeshes(Preview))
    {
        auto* Record=Hologram.Meshes.FindByPredicate([Mesh](const auto& R){return R.Mesh==Mesh;});
        if(!Record)
        {
            auto& New=Hologram.Meshes.AddDefaulted_GetRef();New.Mesh=Mesh;New.bCustomDepth=Mesh->bRenderCustomDepth;if(auto* Static=Cast<UStaticMeshComponent>(Mesh)) New.bForceDisableNanite=Static->bForceDisableNanite;Record=&New;
        }
        if(auto* Static=Cast<UStaticMeshComponent>(Mesh);Static && !Static->bForceDisableNanite) Static->SetForceDisableNanite(true);
        for(int32 I=Record->Materials.Num();I<Mesh->GetNumMaterials();++I) Record->Materials.Add(Mesh->GetMaterial(I));
        for(int32 I=0;I<Mesh->GetNumMaterials();++I) if(Mesh->GetMaterial(I)!=MID) {Record->Materials[I]=Mesh->GetMaterial(I);Mesh->SetMaterial(I,MID);}
        Mesh->SetRenderCustomDepth(false);Mesh->SetOverlayMaterial(nullptr);
    }
}
void UBotanicusJuiceWorldSubsystem::ClearPlacementHologram(AActor* Preview)
{
    if(auto* Hologram=PlacementHolograms.Find(Preview))
    {
        for(auto& Record:Hologram->Meshes) if(auto* Mesh=Record.Mesh.Get())
        {
            for(int32 I=0;I<Record.Materials.Num();++I) if(Mesh->GetMaterial(I)==Hologram->Material) Mesh->SetMaterial(I,Record.Materials[I]);
            Mesh->SetRenderCustomDepth(Record.bCustomDepth);
            if(auto* Static=Cast<UStaticMeshComponent>(Mesh);Static && Static->bForceDisableNanite!=Record.bForceDisableNanite) Static->SetForceDisableNanite(Record.bForceDisableNanite);
        }
        PlacementHolograms.Remove(Preview);
    }
}
void UBotanicusJuiceWorldSubsystem::ReceivePlacement(uint32 Sequence,AActor* Item,APawn* Placer,FName ItemKey,const FTransform& Final,const FVector& Start,FName AudioId)
{
    if(Sequence==0 || static_cast<int32>(Sequence-LastPlacementSequence)<=0) return;LastPlacementSequence=Sequence;
    const auto* S=GetDefault<UBotanicusPlacementJuiceSettings>();
    if(!S->bPlacementJuiceEnabled || (S->bDisableAnimationOnLowEffectsQuality && Scalability::GetQualityLevels().EffectsQuality==0) ||
        S->NoAnimationItems.Contains(ItemKey) || FVector::DistSquared(ListenerPosition(),Final.GetLocation())>FMath::Square(S->MaximumDisplayDistance) ||
        PlacementAnimations.Num()+PendingPlacements.Num()>=FMath::Clamp(S->MaximumSimultaneousAnimations,0,8))
    {PlayContact(GetWorld(),AudioId,Final.GetLocation());return;}
    if(!IsValid(Item)) {PlayContact(GetWorld(),AudioId,Final.GetLocation());return;}
    // A second legitimate move supersedes only this object's transient rendering.
    for(int32 I=PlacementAnimations.Num()-1;I>=0;--I) if(PlacementAnimations[I].Source==Item) FinishPlacementAnimation(I,false);
    PendingPlacements.RemoveAll([Item](const auto& P){return P.Source==Item;});

    FBotanicusPendingPlacement Pending;Pending.Source=Item;Pending.Pawn=Placer;Pending.ItemKey=ItemKey;Pending.FinalTransform=Final;
    Pending.Start=Placer && Placer->IsLocallyControlled() ? PlacementHandPosition(Placer,ItemKey) : Start;
    Pending.AudioId=AudioId;Pending.ExpiresAt=GetWorld()->GetTimeSeconds()+.20;
    PendingPlacements.Add(Pending);
}
int32 UBotanicusJuiceWorldSubsystem::GetPlacementCopyCount() const
{
    int32 Count=0;for(const auto& A:PlacementAnimations) Count+=A.Meshes.Num();return Count;
}
FVector UBotanicusJuiceWorldSubsystem::GetPlacementPhaseSample() const
{
    if(PlacementAnimations.IsEmpty()) return FVector(-1,1,0);
    const auto& A=PlacementAnimations[0];return FVector(GetWorld()->GetTimeSeconds()-A.StartedAt,A.CurrentScale,A.CurrentHeight);
}
bool UBotanicusJuiceWorldSubsystem::BeginPlacementAnimation(const FBotanicusPendingPlacement& Pending)
{
    AActor* Actor=Pending.Source.Get();if(!IsValid(Actor) || Actor->IsHidden()) return false;
    const auto* S=GetDefault<UBotanicusPlacementJuiceSettings>();
    if(PlacementAnimations.ContainsByPredicate([Actor](const auto& A){return A.Source==Actor;})) return false;
    TArray<UMeshComponent*> Sources;FBox Box(ForceInit);int32 Vertices=0;
    for(UMeshComponent* Mesh:PlacementMeshes(Actor))
    {
        if(!Mesh->IsVisible() || Mesh->bHiddenInGame || !Mesh->bRenderInMainPass || (Mesh->GetOwner() && Mesh->GetOwner()->IsHidden())) continue;
        if(auto* Static=Cast<UStaticMeshComponent>(Mesh))
        {
            if(!Static->GetStaticMesh()) continue;
            if(auto* Instances=Cast<UInstancedStaticMeshComponent>(Mesh)) {if(Instances->GetInstanceCount()==0) continue;if(Instances->GetInstanceCount()>1024 || Instances->NumCustomDataFloats>0) return false;}
        }
        else if(auto* Proc=Cast<UProceduralMeshComponent>(Mesh))
        {
            int32 Count=0;for(int32 I=0;I<Proc->GetNumSections();++I) if(auto* Section=Proc->GetProcMeshSection(I)) Count+=Section->ProcVertexBuffer.Num();
            if(Count==0) continue;Vertices+=Count;if(Vertices>12000) return false;
        }
        else return false; // Unsupported animated geometry: leave gameplay rendering untouched.
        Sources.Add(Mesh);Box+=Mesh->Bounds.GetBox();
    }
    if(Sources.IsEmpty() || Sources.Num()>FMath::Clamp(S->MaximumMeshesPerAnimation,1,96) ||
        GetPlacementCopyCount()+Sources.Num()>FMath::Clamp(S->MaximumVisualComponents,1,192)) return false;
    FBotanicusPlacementAnimation Animation;Animation.Source=Actor;Animation.FinalTransform=Actor->GetActorTransform();Animation.StartedAt=GetWorld()->GetTimeSeconds();
    Animation.Anchor=FVector(Box.GetCenter().X,Box.GetCenter().Y,Box.Min.Z);
    // Reuse the exact native furniture-pivot policy rather than trusting invisible render bounds.
    const auto* PC=Cast<ABotanicusPlayerController>(GetWorld()->GetFirstPlayerController());
    if(S->GroundPivotItems.Contains(Pending.ItemKey) || (PC && PC->UsesPlacementCosmeticActorPivot(Actor))) Animation.Anchor.Z=Actor->GetActorLocation().Z;
    Animation.Start=Pending.Start;Animation.AudioId=Pending.AudioId;
    Animation.Fly=S->bFlyFromHand && !S->NoFlyItems.Contains(Pending.ItemKey) ? FMath::Clamp(S->FlyDuration,.03f,.3f) : 0.f;
    Animation.Grow=FMath::Clamp(S->GrowDuration,.06f,.3f);Animation.Bounce=FMath::Clamp(S->BounceDuration,.04f,.3f);
    Animation.Initial=FMath::Clamp(S->InitialVisualScale,.1f,.6f);Animation.Overshoot=FMath::Clamp(S->OvershootScale,1.f,1.2f);
    Animation.Height=FMath::Clamp(S->BounceHeight,0.f,8.f);Animation.Arc=FMath::Clamp(S->FlightArcHeight,0.f,20.f);
    Animation.HandScale=FMath::Min(Animation.Initial*.65f,FMath::Clamp(S->MaximumHandVisualSpan,10.f,100.f)/FMath::Max(1.f,Box.GetSize().GetMax()));
    for(UMeshComponent* Source:Sources)
    {
        UMeshComponent* Copy=nullptr;
        if(auto* Instances=Cast<UInstancedStaticMeshComponent>(Source))
        {
            auto* Clone=NewObject<UInstancedStaticMeshComponent>(GetWorld(),NAME_None,RF_Transient);Clone->InstancingRandomSeed=Instances->InstancingRandomSeed;Clone->SetStaticMesh(Instances->GetStaticMesh());
            Clone->SetCanEverAffectNavigation(false);Clone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            for(int32 I=0;I<Instances->GetInstanceCount();++I) {FTransform T;Instances->GetInstanceTransform(I,T,false);Clone->AddInstance(T,false);}
            Copy=Clone;
        }
        else if(auto* Static=Cast<UStaticMeshComponent>(Source))
        {
            auto* Clone=NewObject<UStaticMeshComponent>(GetWorld(),NAME_None,RF_Transient);Clone->SetStaticMesh(Static->GetStaticMesh());Copy=Clone;
        }
        else if(auto* Proc=Cast<UProceduralMeshComponent>(Source))
        {
            auto* Clone=NewObject<UProceduralMeshComponent>(GetWorld(),NAME_None,RF_Transient);
            for(int32 I=0;I<Proc->GetNumSections();++I) if(auto* Section=Proc->GetProcMeshSection(I)) {auto Data=*Section;Data.bEnableCollision=false;Clone->SetProcMeshSection(I,Data);}
            Copy=Clone;
        }
        if(!Copy) continue;
        if(auto* Static=Cast<UStaticMeshComponent>(Source)) if(auto* Clone=Cast<UStaticMeshComponent>(Copy))
        {Clone->bDisallowNanite=Static->bDisallowNanite;Clone->bForceDisableNanite=Static->bForceDisableNanite;Clone->bEvaluateWorldPositionOffset=Static->bEvaluateWorldPositionOffset;}
        Copy->SetCanEverAffectNavigation(false);Copy->SetMobility(EComponentMobility::Movable);
        Copy->SetCollisionEnabled(ECollisionEnabled::NoCollision);Copy->SetGenerateOverlapEvents(false);Copy->SetIsReplicated(false);Copy->SetCastShadow(false);
        for(int32 I=0;I<Source->GetNumMaterials();++I) Copy->SetMaterial(I,Source->GetMaterial(I));
        const FVector InitialAnchor=Animation.Fly>0 ? Animation.Start : Animation.Anchor;
        const float InitialScale=Animation.Fly>0 ? Animation.HandScale : Animation.Initial;
        FTransform T=Source->GetComponentTransform();T.SetLocation(InitialAnchor+(T.GetLocation()-Animation.Anchor)*InitialScale);T.SetScale3D(T.GetScale3D()*InitialScale);
        Copy->SetWorldTransform(T);Copy->RegisterComponentWithWorld(GetWorld());
        auto& Record=Animation.Meshes.AddDefaulted_GetRef();Record.Source=Source;Record.Copy=Copy;Record.FinalWorldTransform=Source->GetComponentTransform();
        Record.bMain=Source->bRenderInMainPass;Record.bDepth=Source->bRenderInDepthPass;Record.bShadow=Source->CastShadow;Record.bCustom=Source->bRenderCustomDepth;Record.bRay=Source->bVisibleInRayTracing;
        ++PlacementComponentsCreated;
    }
    if(Animation.Meshes.IsEmpty()) return false;
    for(auto& Record:Animation.Meshes) HideOnlyLocalRender(Record.Source.Get());
    PlacementAnimations.Add(MoveTemp(Animation));++PlacementsStarted;return true;
}
void UBotanicusJuiceWorldSubsystem::FinishPlacementAnimation(int32 Index,bool bContact)
{
    auto& Animation=PlacementAnimations[Index];
    for(auto& Record:Animation.Meshes)
    {
        if(auto* Source=Record.Source.Get())
        {
            Source->SetRenderInMainPass(Record.bMain);Source->SetRenderInDepthPass(Record.bDepth);Source->SetCastShadow(Record.bShadow);
            Source->SetRenderCustomDepth(Record.bCustom);Source->SetVisibleInRayTracing(Record.bRay);
        }
        if(Record.Copy) {Record.Copy->DestroyComponent();++PlacementComponentsDestroyed;}
    }
    if(bContact && Animation.Source.IsValid()) PlayContact(GetWorld(),Animation.AudioId,Animation.FinalTransform.GetLocation());
    PlacementAnimations.RemoveAtSwap(Index);++PlacementsFinished;
}
void UBotanicusJuiceWorldSubsystem::UpdatePlacementJuice(float DeltaTime)
{
    const auto* S=GetDefault<UBotanicusPlacementJuiceSettings>();const double Now=GetWorld()->GetTimeSeconds();
    for(auto It=PlacementHolograms.CreateIterator();It;++It)
    {
        if(!It.Key().IsValid()) {It.RemoveCurrent();continue;}
        if(!S->bPlacementJuiceEnabled || !S->bPlacementHologramEnabled)
        {
            for(auto& Record:It.Value().Meshes) if(auto* Mesh=Record.Mesh.Get())
            {
                for(int32 I=0;I<Record.Materials.Num();++I) if(Mesh->GetMaterial(I)==It.Value().Material) Mesh->SetMaterial(I,Record.Materials[I]);
                Mesh->SetRenderCustomDepth(Record.bCustomDepth);
            if(auto* Static=Cast<UStaticMeshComponent>(Mesh);Static && Static->bForceDisableNanite!=Record.bForceDisableNanite) Static->SetForceDisableNanite(Record.bForceDisableNanite);
            }
            It.RemoveCurrent();continue;
        }
        RefreshPlacementHologram(It.Key().Get(),It.Value(),DeltaTime);
    }
    for(int32 I=PendingPlacements.Num()-1;I>=0;--I)
    {
        const auto Pending=PendingPlacements[I];auto* Item=Pending.Source.Get();
        const auto* Large=Cast<ABotanicusLargeEquipmentActor>(Item);
        const bool bReady=IsValid(Item) && PlacementKey(Item)==Pending.ItemKey && Item->GetActorTransform().Equals(Pending.FinalTransform,.5f) &&
            (!Large || (!Large->IsInPlacementMode() && !Large->GetCarrier()));
        if(bReady)
        {
            ClearPlacementHologram(Item);
            const bool bAllowed=S->bPlacementJuiceEnabled && (!S->bDisableAnimationOnLowEffectsQuality || Scalability::GetQualityLevels().EffectsQuality>0);
            if(!bAllowed || !BeginPlacementAnimation(Pending)) PlayContact(GetWorld(),Pending.AudioId,Pending.FinalTransform.GetLocation());
            PendingPlacements.RemoveAtSwap(I);
        }
        else if(!IsValid(Item) || Now>=Pending.ExpiresAt) {PlayContact(GetWorld(),Pending.AudioId,Pending.FinalTransform.GetLocation());PendingPlacements.RemoveAtSwap(I);}
    }
    for(int32 I=PlacementAnimations.Num()-1;I>=0;--I)
    {
        auto& A=PlacementAnimations[I];auto* Item=A.Source.Get();const float Elapsed=Now-A.StartedAt;
        const bool bCancel=!S->bPlacementJuiceEnabled || (S->bDisableAnimationOnLowEffectsQuality && Scalability::GetQualityLevels().EffectsQuality==0) ||
            !IsValid(Item) || Item->IsHidden() || !Item->GetActorTransform().Equals(A.FinalTransform,.5f) ||
            A.Meshes.ContainsByPredicate([](const auto& M){return !M.Source.IsValid();});
        if(bCancel || Elapsed>=A.Fly+A.Grow+A.Bounce) {FinishPlacementAnimation(I,!bCancel);continue;}
        FVector Anchor=A.Anchor;float Scale=A.Initial,Height=0;
        if(A.Fly>0 && Elapsed<A.Fly)
        {
            const float U=FMath::Clamp(Elapsed/A.Fly,0.f,1.f);const float T=FMath::Clamp(PhaseCurve(PlacementFlightCurve,U,U*U*(3-2*U)),0.f,1.f);
            Anchor=FMath::Lerp(A.Start,A.Anchor,T)+FVector(0,0,FMath::Sin(PI*U)*A.Arc);Scale=FMath::Lerp(A.HandScale,A.Initial,T);
        }
        else if(Elapsed<A.Fly+A.Grow)
        {
            const float U=FMath::Clamp((Elapsed-A.Fly)/A.Grow,0.f,1.f);const float T=PhaseCurve(PlacementGrowthCurve,U,1-FMath::Pow(1-U,3));
            Scale=FMath::Lerp(A.Initial,A.Overshoot,FMath::Clamp(T,0.f,1.2f));
        }
        else
        {
            const float U=FMath::Clamp((Elapsed-A.Fly-A.Grow)/A.Bounce,0.f,1.f);
            Scale=PlacementBounceCurve ? FMath::Lerp(A.Overshoot,1.f,PhaseCurve(PlacementBounceCurve,U,U)) :
                1.f+(A.Overshoot-1.f)*FMath::Square(1-U)*FMath::Cos(PI*1.15f*U);
            Height=A.Height*4.f*U*(1-U);Anchor.Z+=Height;
        }
        Scale=FMath::Clamp(Scale,.025f,1.25f);A.CurrentScale=Scale;A.CurrentHeight=Height;
        for(auto& Record:A.Meshes)
        {
            HideOnlyLocalRender(Record.Source.Get());FTransform T=Record.FinalWorldTransform;
            T.SetLocation(Anchor+(T.GetLocation()-A.Anchor)*Scale);T.SetScale3D(T.GetScale3D()*Scale);Record.Copy->SetWorldTransform(T);
        }
    }
}
void UBotanicusJuiceWorldSubsystem::ClearPlacementJuice()
{
    while(!PlacementAnimations.IsEmpty()) FinishPlacementAnimation(PlacementAnimations.Num()-1,false);
    TArray<TWeakObjectPtr<AActor>> Keys;PlacementHolograms.GetKeys(Keys);for(auto Key:Keys) ClearPlacementHologram(Key.Get());
    PlacementHolograms.Reset();PendingPlacements.Reset();
}
