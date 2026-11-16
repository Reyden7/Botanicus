#include "Gardening/BotanicusGardeningRobotBase.h"
#include "Gardening/BotanicusGardeningRobot.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "BotanicusGameMode.h"

ABotanicusGardeningRobotBase::ABotanicusGardeningRobotBase()
{
    bUseBlueprintAppearance=true;
    Mesh->SetRelativeScale3D(FVector(.7f,.6f,.12f));
    Mesh->SetCanEverAffectNavigation(false);
    DockPoint=CreateDefaultSubobject<USceneComponent>(TEXT("DockPoint"));
    DockPoint->SetupAttachment(SceneRoot); DockPoint->SetRelativeLocation(FVector(0,0,6));
    Indicator=CreateDefaultSubobject<UPointLightComponent>(TEXT("ChargeIndicator"));
    Indicator->SetupAttachment(SceneRoot);Indicator->SetRelativeLocation(FVector(0,0,10));
    Indicator->SetIntensity(40);Indicator->SetAttenuationRadius(60);Indicator->SetCastShadows(false);
    RobotClass=ABotanicusGardeningRobot::StaticClass();
}
void ABotanicusGardeningRobotBase::BeginPlay()
{
    Super::BeginPlay();
    if(HasAuthority() && !BaseId.IsValid())BaseId=FGuid::NewGuid();
    GetWorldTimerManager().SetTimer(StationTimer,this,&ThisClass::UpdateIndicator,.5f,true);
}
void ABotanicusGardeningRobotBase::EndPlay(const EEndPlayReason::Type R)
{
    bEnding=true;GetWorldTimerManager().ClearTimer(StationTimer);
    if(HasAuthority() && IsValid(Robot))Robot->Destroy();
    Super::EndPlay(R);
}
void ABotanicusGardeningRobotBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(ThisClass,Robot);DOREPLIFETIME(ThisClass,BaseId);
}
FVector ABotanicusGardeningRobotBase::DockLocation() const {return DockPoint->GetComponentLocation();}
bool ABotanicusGardeningRobotBase::IsStationAvailable() const
{return !bEnding && !GetCarrier() && !IsInPlacementMode();}
void ABotanicusGardeningRobotBase::ConfirmPlacement()
{
    const bool bAccepted=HasAuthority() && IsInPlacementMode() && IsPlacementValid();
    Super::ConfirmPlacement();
    if(bAccepted){bInstalled=true;NextSpawnAttempt=0;EnsureRobot();}
}
void ABotanicusGardeningRobotBase::EnsureRobot()
{
    if(!HasAuthority() || !bInstalled || bSpawnedOnce || IsValid(Robot) || !IsStationAvailable() || !GetIsReplicated() || bEnding)return;
    if(!RobotClass || GetWorld()->GetTimeSeconds()<NextSpawnAttempt)return;
    NextSpawnAttempt=GetWorld()->GetTimeSeconds()+5;
    FBotanicusGardeningSave Saved=PendingSave;Saved.bInstalled=true;Saved.BaseId=BaseId;
    if(!Saved.RobotId.IsValid())
    {
        Saved.RobotId=FGuid::NewGuid();Saved.Water=Config.WaterCapacity;
        Saved.RobotTransform=FTransform(GetActorRotation(),DockLocation()+FVector(0,0,40));
    }
    UClass* Class=Saved.RobotClass.IsValid()?Saved.RobotClass.TryLoadClass<ABotanicusGardeningRobot>():RobotClass.Get();
    if(!Class || !Class->IsChildOf(ABotanicusGardeningRobot::StaticClass()))return;
    Robot=GetWorld()->SpawnActorDeferred<ABotanicusGardeningRobot>(Class,Saved.RobotTransform,this,nullptr,ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding);
    if(Robot)
    {
        Robot->Config=Config;Robot->InitializeRobot(this,Saved);Robot->FinishSpawning(Saved.RobotTransform);
        // Deferred spawning can fail on collision at FinishSpawning. Retain the kit/saved resources and retry later.
        if(IsValid(Robot) && !Robot->IsActorBeingDestroyed()){bSpawnedOnce=true;PendingSave=Robot->Capture();}
        else Robot=nullptr;
        ForceNetUpdate();
    }
}
FBotanicusGardeningSave ABotanicusGardeningRobotBase::CaptureRobot() const
{
    auto S=IsValid(Robot)?Robot->Capture():PendingSave;S.bInstalled=bInstalled;S.BaseId=BaseId;return S;
}
void ABotanicusGardeningRobotBase::RestoreRobot(const FBotanicusGardeningSave& S)
{
    if(!HasAuthority())return;
    if(IsValid(Robot))Robot->Destroy();Robot=nullptr;
    PendingSave=S;bInstalled=S.bInstalled;bSpawnedOnce=false;if(S.BaseId.IsValid())BaseId=S.BaseId;
    // The next station timer runs after the complete world restore, including plants and nav obstacles.
}
void ABotanicusGardeningRobotBase::UpdateIndicator()
{
    if(HasAuthority()){if(IsValid(Robot))PendingSave=Robot->Capture();EnsureRobot();}
    const bool bCharging=Robot && Robot->IsDocked() && Robot->Battery<.99999f;
    Indicator->SetVisibility(Robot && Robot->IsDocked());Indicator->SetLightColor(bCharging?ChargingColor:FullColor);
    SetInteractionIndicatorVisibility(false);
}
FBotanicusInteractionPrompt ABotanicusGardeningRobotBase::GetInteractionPrompt_Implementation(AActor* Interactor) const
{
    if(!Robot)return Super::GetInteractionPrompt_Implementation(Interactor);
    FBotanicusInteractionPrompt P;P.ActionText=FText::FromString(Robot->bEnabled?TEXT("Desactiver le robot"):TEXT("Activer le robot"));
    const float Remaining=(1-Robot->Battery)*Config.BatteryRechargeHours*60;
    P.TargetName=FText::Format(NSLOCTEXT("Robot","BaseInfo","Station jardinier — batterie {0}% | eau {1}%\nRecharge restante : {2} min de jeu"),FMath::RoundToInt(Robot->Battery*100),FMath::RoundToInt(Robot->CurrentWater/FMath::Max(.01f,Config.WaterCapacity)*100),FMath::CeilToInt(Remaining));
    P.bCanInteract=CanInteract_Implementation(Interactor);return P;
}
bool ABotanicusGardeningRobotBase::CanInteract_Implementation(AActor* Interactor) const
{
    if(!Robot)return Super::CanInteract_Implementation(Interactor);
    return IsStationAvailable() && IsValid(Interactor) && FVector::DistSquared(GetActorLocation(),Interactor->GetActorLocation())<FMath::Square(250.f);
}
void ABotanicusGardeningRobotBase::Interact_Implementation(AActor* Interactor)
{
    if(!HasAuthority() || !CanInteract_Implementation(Interactor))return;
    if(Robot)Robot->SetEnabled(!Robot->bEnabled);else Super::Interact_Implementation(Interactor);
}
