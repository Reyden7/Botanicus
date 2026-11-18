#include "Tools/BotanicusMultiToolComponent.h"
#include "BotanicusCharacter.h"
#include "BotanicusGameState.h"
#include "Gardening/BotanicusGardeningRobot.h"
#include "Gardening/BotanicusGardeningRobotBase.h"
#include "Catalog/BotanicusItemCatalogSubsystem.h"
#include "Engine/GameInstance.h"
#include "BotanicusPlayerController.h"
#include "QuickBar/BotanicusQuickBarComponent.h"
#include "Delivery/BotanicusDeliveryParcelActor.h"
#include "Growing/BotanicusPlantPotActor.h"
#include "IllegalTrade/BotanicusIllegalPlanterActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Engine/World.h"

UBotanicusMultiToolComponent::UBotanicusMultiToolComponent()
{
    PrimaryComponentTick.bCanEverTick=false;SetIsReplicatedByDefault(true);
    VisualClass=TSoftClassPtr<ABotanicusMultiToolActor>(FSoftObjectPath(TEXT("/Game/Botanicus/blueprints/BP_Item_MultiTool_L1.BP_Item_MultiTool_L1_C")));
}
void UBotanicusMultiToolComponent::BeginPlay()
{
    Super::BeginPlay();GetWorld()->GetTimerManager().SetTimer(UpdateTimer,this,&ThisClass::UpdateContext,.05f,true);OnRep_Presentation();
}
void UBotanicusMultiToolComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if(GetWorld())GetWorld()->GetTimerManager().ClearTimer(UpdateTimer);
    if(IsValid(RemoteVisual))RemoteVisual->Destroy();RemoteVisual=nullptr;
    Super::EndPlay(Reason);
}
void UBotanicusMultiToolComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(ThisClass,Presentation);}
bool UBotanicusMultiToolComponent::IsBlocked() const
{
    const auto* C=Cast<ABotanicusCharacter>(GetOwner());
    const auto* PC=C?Cast<ABotanicusPlayerController>(C->GetController()):nullptr;
    const auto* Bar=C?C->GetQuickBarComponent():nullptr;
    return !C || !PC || !Bar || C->IsCarryingEquipment() || C->GetHeldWateringCan() ||
        Bar->HasSelectedWateringCan() || PC->IsMultiToolContextBlocked();
}
int32 UBotanicusMultiToolComponent::FindOwnedSlot() const
{
    const auto* C=Cast<ABotanicusCharacter>(GetOwner());const auto* Bar=C?C->GetQuickBarComponent():nullptr;
    if(Bar && Bar->GetSelectedSlot().ItemKey==ItemKey() && !Bar->GetSelectedSlot().IsEmpty())return Bar->GetSelectedSlotIndex();
    if(Bar)for(int32 I=0;I<UBotanicusQuickBarComponent::SlotCount;++I){const auto S=Bar->GetSlot(I);if(!S.IsEmpty() && S.ItemKey==ItemKey())return I;}
    return INDEX_NONE;
}
bool UBotanicusMultiToolComponent::ResolveAim(AActor*& Target,int32& Slot,EBotanicusMultiToolMode& Mode) const
{
    Target=nullptr;Slot=INDEX_NONE;Mode=EBotanicusMultiToolMode::Neutral;
    const auto* C=Cast<ABotanicusCharacter>(GetOwner());auto* PC=C?Cast<ABotanicusPlayerController>(C->GetController()):nullptr;
    if(!C || !PC || !GetWorld())return false;
    FVector Start;FRotator View;PC->GetPlayerViewPoint(Start,View);
    FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(BotanicusMultiToolAim),false,C);
    const float Range=FMath::Min(InteractionDistance,PC->GetWorldInteractionDistance());
    if(!GetWorld()->LineTraceSingleByChannel(Hit,Start,Start+View.Vector()*(Range+200.f),ECC_Visibility,Q) ||
        FVector::DistSquared2D(C->GetActorLocation(),Hit.ImpactPoint)>FMath::Square(Range))return false;
    auto* A=Hit.GetActor();if(!IsValid(A) || A->ActorHasTag(TEXT("BotanicusPlacementPreview")) || A->ActorHasTag(TEXT("BotanicusEquippedPreview")))return false;
    if(auto* Parcel=Cast<ABotanicusDeliveryParcelActor>(A))
    {
        if(!Parcel->IsCuttingAreaHit(Hit))return false;
        Target=A;Slot=0;Mode=EBotanicusMultiToolMode::Cutter;return true;
    }
    if(auto* Pot=Cast<ABotanicusPlantPotActor>(A))
    {
        FName Required;
        if(!Pot->ResolveHarvestTool(C,Required,Slot) || Required!=TEXT("GardenTrowel"))return false;
        Target=A;Mode=EBotanicusMultiToolMode::Shovel;return true;
    }
    if(auto* Planter=Cast<ABotanicusIllegalPlanterActor>(A))
    {
        if (!Planter->ResolveHarvestTarget(C,Slot)) return false;
        Target=A;Mode=EBotanicusMultiToolMode::Shovel;return true;
    }
    if(auto* Robot=Cast<ABotanicusGardeningRobot>(A))
    {
        const auto* State=GetWorld()->GetGameState<ABotanicusGameState>();
        if(!State || !State->IsElectricWhipUnlocked() || !Robot->CanReceiveWhip() ||
            FVector::DistSquared(C->GetActorLocation(),Robot->GetActorLocation())>FMath::Square(FMath::Min(Range,WhipInteractionDistance)))return false;
        Target=A;Slot=0;Mode=EBotanicusMultiToolMode::ElectricWhip;return true;
    }
    return false;
}
bool UBotanicusMultiToolComponent::HasTool(const AActor* Interactor,FName RequiredTool,const AActor* Target,int32 Slot)
{
    const auto* C=Cast<ABotanicusCharacter>(Interactor);const auto* Bar=C?C->GetQuickBarComponent():nullptr;
    if(!Bar)return false;const auto Selected=Bar->GetSelectedSlot();
    // Preserve the original comparison, including species that allow an empty-hand harvest.
    if(Selected.ItemKey==RequiredTool)return true;
    if(RequiredTool.IsNone() || Selected.IsEmpty() || Selected.ItemKey!=ItemKey())return false;
    const auto* Tool=C->FindComponentByClass<ThisClass>();if(!Tool)return false;
    const auto RequiredMode=RequiredTool==TEXT("BoxCutter")?EBotanicusMultiToolMode::Cutter:
        RequiredTool==TEXT("GardenTrowel")?EBotanicusMultiToolMode::Shovel:EBotanicusMultiToolMode::Neutral;
    if(RequiredMode==EBotanicusMultiToolMode::Cutter)
        return Tool->Presentation.Mode==RequiredMode && Tool->Presentation.Target==Target && Tool->Presentation.Slot==Slot && !Tool->IsBlocked();
    return RequiredMode!=EBotanicusMultiToolMode::Neutral && Tool->GetPreparedTarget(RequiredMode)==Target && Tool->Presentation.Slot==Slot;
}
AActor* UBotanicusMultiToolComponent::GetPreparedTarget(EBotanicusMultiToolMode RequiredMode) const
{
    const auto* C=Cast<ABotanicusCharacter>(GetOwner());const auto* Bar=C?C->GetQuickBarComponent():nullptr;
    if(!Bar || Bar->GetSelectedSlot().ItemKey!=ItemKey() || Presentation.Mode!=RequiredMode || IsBlocked())return nullptr;
    AActor* A;int32 S;EBotanicusMultiToolMode M;
    return ResolveAim(A,S,M) && A==Presentation.Target && S==Presentation.Slot && M==RequiredMode?A:nullptr;
}
void UBotanicusMultiToolComponent::NotifyManualSelection()
{
    const bool bHadContext=Presentation.Mode!=EBotanicusMultiToolMode::Neutral || LastRequested.IsValid() || bAutoSelected;
    AActor* A;int32 S;EBotanicusMultiToolMode M;ResolveAim(A,S,M);
    bSuppressed=A!=nullptr;SuppressedTarget=A;SuppressedSlot=S;
    Candidate.Reset();LastRequested.Reset();CandidateSlot=LastRequestedSlot=INDEX_NONE;
    if(bHadContext)ServerCancelContext(true);
}
void UBotanicusMultiToolComponent::UpdateContext()
{
    auto* C=Cast<ABotanicusCharacter>(GetOwner());if(!C || !GetWorld())return;
    auto* Bar=C->GetQuickBarComponent();if(!Bar)return;
    const double Now=GetWorld()->GetTimeSeconds();
    if(C->IsLocallyControlled())
    {
        if(IsValid(RemoteVisual)){RemoteVisual->Destroy();RemoteVisual=nullptr;}
        AActor* A=nullptr;int32 S=INDEX_NONE;EBotanicusMultiToolMode M=EBotanicusMultiToolMode::Neutral;
        const bool bBlocked=IsBlocked();
        if(!bBlocked && Bar->GetTotalQuantity(ItemKey())>0)ResolveAim(A,S,M);
        if(auto* PC=Cast<ABotanicusPlayerController>(C->GetController());PC && PC->IsMultiToolCutInProgress() && Presentation.Mode==EBotanicusMultiToolMode::Cutter)
        {A=Presentation.Target;S=Presentation.Slot;M=Presentation.Mode;}
        if(bSuppressed)
        {
            if(A==SuppressedTarget.Get() && S==SuppressedSlot)A=nullptr;
            else {bSuppressed=false;SuppressedTarget.Reset();}
        }
        if(Candidate.Get()!=A || CandidateSlot!=S)
        {Candidate=A;CandidateSlot=S;CandidateSince=Now;}
        if(Now-CandidateSince>=FMath::Max(.05f,TargetStabilization))
        {
            if(A && (LastRequested.Get()!=A || LastRequestedSlot!=S ||
                (Presentation.Target!=A && Now-CandidateSince>=.5)))
            {LastRequested=A;LastRequestedSlot=S;CandidateSince=Now;ServerPrepareContext(A,S);}
            else if(!A && (LastRequested.IsValid() || Presentation.Mode!=EBotanicusMultiToolMode::Neutral))
            {LastRequested.Reset();LastRequestedSlot=INDEX_NONE;ServerCancelContext(bBlocked);}
        }
    }
    else if(!C->HasAuthority())
    {if(Presentation.bEquipped && !IsValid(RemoteVisual))OnRep_Presentation();return;}
    if(!C->HasAuthority())return;
    const auto Selected=Bar->GetSelectedSlot();
    if(Presentation.Mode!=EBotanicusMultiToolMode::Neutral)
    {
        AActor* A;int32 S;EBotanicusMultiToolMode M;
        const auto* Parcel=Cast<ABotanicusDeliveryParcelActor>(Presentation.Target);
        const bool bCutInProgress=Presentation.Mode==EBotanicusMultiToolMode::Cutter && Parcel && Parcel->IsBeingCutBy(C);
        const bool bValid=!IsBlocked() && !Selected.IsEmpty() && Selected.ItemKey==ItemKey() &&
            (bCutInProgress || (ResolveAim(A,S,M) && A==Presentation.Target && S==Presentation.Slot && M==Presentation.Mode));
        if(bValid)ServerInvalidSince=-1;
        else if(ServerInvalidSince<0)ServerInvalidSince=Now;
        else if(Now-ServerInvalidSince>=FMath::Max(.2f,TargetStabilization*2))ReleaseContext(true);
    }
    const bool bEquipped=Bar->GetSelectedSlot().ItemKey==ItemKey() && !IsBlocked();
    if(Presentation.bEquipped!=bEquipped){Presentation.bEquipped=bEquipped;Publish();}
}
void UBotanicusMultiToolComponent::ServerPrepareContext_Implementation(AActor* Target,int32 Slot)
{
    if(!IsValid(Target) || IsBlocked())return;
    auto* C=Cast<ABotanicusCharacter>(GetOwner());auto* Bar=C?C->GetQuickBarComponent():nullptr;
    const int32 ToolSlot=FindOwnedSlot();if(!C || !Bar || ToolSlot==INDEX_NONE)return;
    AActor* Actual;int32 ActualSlot;EBotanicusMultiToolMode Mode;
    if(!ResolveAim(Actual,ActualSlot,Mode) || Actual!=Target || ActualSlot!=Slot)return;
    const auto Selected=Bar->GetSelectedSlot();
    if(!bAutoSelected && Bar->GetSelectedSlotIndex()!=ToolSlot)
    {PreviousSlot=Bar->GetSelectedSlotIndex();PreviousInstance=Selected.InstanceId;bAutoSelected=true;AutoInstance=Bar->GetSlot(ToolSlot).InstanceId;}
    Bar->SelectSlotAuthoritative(ToolSlot);
    Presentation.Target=Actual;Presentation.Slot=ActualSlot;Presentation.Mode=Mode;Presentation.bEquipped=true;ServerInvalidSince=-1;Publish();
}
void UBotanicusMultiToolComponent::ServerCancelContext_Implementation(bool bManual){ReleaseContext(!bManual);}
void UBotanicusMultiToolComponent::ReleaseContext(bool bRestore)
{
    auto* C=Cast<ABotanicusCharacter>(GetOwner());auto* Bar=C?C->GetQuickBarComponent():nullptr;
    if(Bar && bRestore && bAutoSelected && Bar->GetSelectedSlot().InstanceId==AutoInstance && Bar->GetSelectedSlot().ItemKey==ItemKey())
    {
        int32 Restore=INDEX_NONE;
        if(PreviousInstance.IsValid())
        {for(int32 I=0;I<UBotanicusQuickBarComponent::SlotCount;++I)if(Bar->GetSlot(I).InstanceId==PreviousInstance && !Bar->GetSlot(I).IsEmpty()){Restore=I;break;}}
        else if(PreviousSlot!=INDEX_NONE && Bar->GetSlot(PreviousSlot).IsEmpty())Restore=PreviousSlot;
        if(Restore!=INDEX_NONE)Bar->SelectSlotAuthoritative(Restore);
    }
    bAutoSelected=false;AutoInstance.Invalidate();PreviousInstance.Invalidate();PreviousSlot=INDEX_NONE;ServerInvalidSince=-1;
    Presentation.Mode=EBotanicusMultiToolMode::Neutral;Presentation.Target=nullptr;Presentation.Slot=INDEX_NONE;
    Presentation.bEquipped=Bar && Bar->GetSelectedSlot().ItemKey==ItemKey() && !IsBlocked();Publish();
}
void UBotanicusMultiToolComponent::Publish(){GetOwner()->ForceNetUpdate();OnRep_Presentation();}
void UBotanicusMultiToolComponent::OnRep_Presentation()
{
    auto* C=Cast<ABotanicusCharacter>(GetOwner());if(!C || GetNetMode()==NM_DedicatedServer || C->IsLocallyControlled() || !HasBegunPlay())return;
    if(Presentation.bEquipped && !IsValid(RemoteVisual))
    {
        UClass* Class=VisualClass.LoadSynchronous();if(!Class)Class=ABotanicusMultiToolActor::StaticClass();
        FActorSpawnParameters P;P.Owner=C;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        RemoteVisual=GetWorld()->SpawnActor<ABotanicusMultiToolActor>(Class,C->GetActorTransform(),P);
        if(!RemoteVisual)return;
        RemoteVisual->SetActorHiddenInGame(true);RemoteVisual->Tags.Add(TEXT("BotanicusEquippedPreview"));
        RemoteVisual->InitializePlacedItem(ItemKey());RemoteVisual->ConfigureAsLocalPreview(true);RemoteVisual->ConfigureAsLocalInspection();RemoteVisual->SetActorTickEnabled(false);
        if(C->GetMesh()->DoesSocketExist(ThirdPersonSocket))RemoteVisual->AttachToComponent(C->GetMesh(),FAttachmentTransformRules::SnapToTargetNotIncludingScale,ThirdPersonSocket);
        else RemoteVisual->AttachToComponent(C->GetRootComponent(),FAttachmentTransformRules::SnapToTargetNotIncludingScale);
        RemoteVisual->SetActorRelativeTransform(ThirdPersonOffset);
    }
    if(RemoteVisual){RemoteVisual->SetMultiToolMode(Presentation.Mode);RemoteVisual->SetActorHiddenInGame(!Presentation.bEquipped);}
}

bool UBotanicusMultiToolComponent::IsRobotItem(const UObject* Context,FName Key)
{
    const auto* World=Context?Context->GetWorld():nullptr;
    const auto* Catalog=World && World->GetGameInstance()?World->GetGameInstance()->GetSubsystem<UBotanicusItemCatalogSubsystem>():nullptr;
    const auto* Def=Catalog?Catalog->FindItem(Key):nullptr;
    UClass* Class=Def?Def->WorldActorClass.LoadSynchronous():nullptr;
    return Class && (Class->IsChildOf(ABotanicusGardeningRobotBase::StaticClass()) || Class->IsChildOf(ABotanicusGardeningRobot::StaticClass()));
}
bool UBotanicusMultiToolComponent::TryWhip()
{
    auto* Robot=Cast<ABotanicusGardeningRobot>(GetPreparedTarget(EBotanicusMultiToolMode::ElectricWhip));
    if(!Robot)return false;ServerWhip(Robot);return true;
}
void UBotanicusMultiToolComponent::ServerWhip_Implementation(ABotanicusGardeningRobot* Robot)
{
    // Re-trace on the server on every intentional click, never trust the target sent by a client.
    const auto* State=GetWorld()->GetGameState<ABotanicusGameState>();
    if(!State || !State->IsElectricWhipUnlocked() || !Robot ||
        GetPreparedTarget(EBotanicusMultiToolMode::ElectricWhip)!=Robot)return;
    if(Robot->ApplyElectricWhip(GetOwner()))MulticastWhipSwing();
}
void UBotanicusMultiToolComponent::MulticastWhipSwing_Implementation()
{
    if(GetNetMode()==NM_DedicatedServer)return;
    auto* C=Cast<ABotanicusCharacter>(GetOwner());if(!C)return;
    if(C->IsLocallyControlled())
    {if(auto* PC=Cast<ABotanicusPlayerController>(C->GetController()))PC->PlayMultiToolWhipVisual();}
    else if(RemoteVisual)RemoteVisual->PlayWhipSwing();
}
