#include "Visuals/BotanicusSoilPourComponent.h"
#include "Growing/BotanicusPlantPotActor.h"
#include "Growing/BotanicusMultiPlantPotActor.h"
#include "IllegalTrade/BotanicusIllegalPlanterActor.h"
#include "BotanicusCharacter.h"
#include "BotanicusPlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Engine/World.h"
UBotanicusSoilPourComponent::UBotanicusSoilPourComponent()
{
    PrimaryComponentTick.bCanEverTick=false;SetIsReplicatedByDefault(true);
    PourSystem=TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/Botanicus/VFX/Soil/NS_Botanicus_SoilPour.NS_Botanicus_SoilPour")));
    ContactSystem=TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/Botanicus/VFX/Soil/NS_Botanicus_SoilContact.NS_Botanicus_SoilContact")));
}
void UBotanicusSoilPourComponent::BeginPlay()
{
    Super::BeginPlay();if(GetNetMode()!=NM_DedicatedServer)
    {LoadedPour=PourSystem.LoadSynchronous();LoadedContact=ContactSystem.LoadSynchronous();if(LoadedPour)LoadedPour->PrecacheAssetPSOs();if(LoadedContact)LoadedContact->PrecacheAssetPSOs();}
    OnRep_Pour();
}
void UBotanicusSoilPourComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(ThisClass,Pour);}
void UBotanicusSoilPourComponent::Transferred(APawn* Source,int32 Slot)
{
    if(!GetOwner()->HasAuthority() || !IsValid(Source))return;
    if(Pour.Source==Source && Pour.Slot==Slot)return;
    Pour.Source=Source;Pour.Slot=Slot;GetOwner()->ForceNetUpdate();OnRep_Pour();
}
void UBotanicusSoilPourComponent::StopTransfer()
{
    if(!GetOwner()->HasAuthority() || !Pour.Source)return;
    Pour.Source=nullptr;GetOwner()->ForceNetUpdate();OnRep_Pour();
}
FVector UBotanicusSoilPourComponent::Surface() const
{
    if(auto* Planter=Cast<ABotanicusMultiPlantPotActor>(GetOwner()))return Planter->GetSoilPourTarget(Pour.Source);
    if(auto* Pot=Cast<ABotanicusPlantPotActor>(GetOwner()))return Pot->GetSoilSurfaceForSlot(Pour.Slot);
    if(auto* Pot=Cast<ABotanicusIllegalPlanterActor>(GetOwner()))return Pot->GetSoilPourTarget(Pour.Source);
    return GetOwner()->GetActorLocation();
}
void UBotanicusSoilPourComponent::OnRep_Pour()
{
    if(GetNetMode()==NM_DedicatedServer || !HasBegunPlay() || !GetWorld())return;
    auto& T=GetWorld()->GetTimerManager();T.ClearTimer(CleanupTimer);
    if(IsValid(Pour.Source))
    {
        StartedAt=GetWorld()->GetTimeSeconds();UpdateLocal();
        T.SetTimer(UpdateTimer,this,&ThisClass::UpdateLocal,1.f/30,true);
    }
    else
    {
        T.ClearTimer(UpdateTimer);
        for(auto* FX:{Stream.Get(),Contact.Get()})if(FX){FX->SetVariableFloat(TEXT("User.SpawnRate"),0);FX->SetVariableFloat(TEXT("User.DustRate"),0);FX->Deactivate();}
        T.SetTimer(CleanupTimer,this,&ThisClass::ClearVisuals,.8f,false);
    }
}
void UBotanicusSoilPourComponent::UpdateLocal()
{
    if(!IsValid(Pour.Source)){ClearVisuals();return;}
    const auto* C=Cast<ABotanicusCharacter>(Pour.Source);FVector Mouth=Pour.Source->GetPawnViewLocation()+Pour.Source->GetActorForwardVector()*40-FVector(0,0,20);
    if(C)
    {
        if(C->IsLocallyControlled())
        {if(auto* PC=Cast<ABotanicusPlayerController>(C->GetController()))Mouth=PC->GetSoilBagMouth(BagMouthNormalizedOffset);}
        else if(C->GetMesh()->DoesSocketExist(TEXT("hand_r")))Mouth=C->GetMesh()->GetSocketLocation(TEXT("hand_r"));
    }
    const FVector To=Surface();const float Flight=FMath::Clamp(FVector::Dist(Mouth,To)/500.f,.16f,.65f);
    auto Spawn=[&](TObjectPtr<UNiagaraComponent>& FX,UNiagaraSystem* System,FVector Position)
    {if(!FX && System)FX=UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(),System,Position,FRotator::ZeroRotator,FVector::OneVector,false,false,ENCPoolMethod::None);};
    Spawn(Stream,LoadedPour.Get(),Mouth);Spawn(Contact,LoadedContact.Get(),To);
    if(Stream)
    {
        Stream->SetWorldLocation(Mouth);Stream->SetVariableFloat(TEXT("User.SpawnRate"),FMath::Clamp(ParticlesPerSecond,0.f,220.f));
        Stream->SetVariableFloat(TEXT("User.Lifetime"),Flight);Stream->SetVariableFloat(TEXT("User.SizeMin"),FMath::Clamp(GrainSize,.1f,3.f)*.6f);Stream->SetVariableFloat(TEXT("User.SizeMax"),FMath::Clamp(GrainSize,.1f,3.f)*1.4f);
        Stream->SetVariableVec3(TEXT("User.Velocity"),(To-Mouth-FVector(0,0,-350)*Flight*Flight)/Flight);
        if(!Stream->IsActive())Stream->Activate(true);
    }
    if(Contact)
    {
        Contact->SetWorldLocation(To);const bool bArrived=GetWorld()->GetTimeSeconds()-StartedAt>=Flight;
        Contact->SetVariableFloat(TEXT("User.SpawnRate"),bArrived?18:0);Contact->SetVariableFloat(TEXT("User.DustRate"),bArrived?3:0);
        if(!Contact->IsActive())Contact->Activate(true);
    }
}
void UBotanicusSoilPourComponent::ClearVisuals()
{
    if(GetWorld()){GetWorld()->GetTimerManager().ClearTimer(UpdateTimer);GetWorld()->GetTimerManager().ClearTimer(CleanupTimer);}
    for(auto* FX:{Stream.Get(),Contact.Get()})if(FX){FX->SetAutoDestroy(false);FX->DeactivateImmediate();FX->DestroyComponent();}
    Stream=nullptr;Contact=nullptr;
}
void UBotanicusSoilPourComponent::EndPlay(const EEndPlayReason::Type Reason){ClearVisuals();Super::EndPlay(Reason);}
