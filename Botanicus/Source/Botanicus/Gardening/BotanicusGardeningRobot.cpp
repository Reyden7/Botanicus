#include "Gardening/BotanicusGardeningRobot.h"
#include "Visuals/BotanicusReservoirVisualComponent.h"
#include "Gardening/BotanicusRobotAudioComponent.h"
#include "Gardening/BotanicusRobotContactSurface.h"
#include "Gardening/BotanicusRobotFaceComponent.h"
#include "Gardening/BotanicusGardeningRobotBase.h"
#include "Gardening/BotanicusGardeningSubsystem.h"
#include "Growing/BotanicusPlantPotActor.h"
#include "Growing/BotanicusAutomaticWatering.h"
#include "Logging/LogMacros.h"
#include "BotanicusCharacter.h"
#include "BotanicusGameState.h"
#include "BotanicusGameMode.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Navigation/PathFollowingComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogBotanicusRobotRecovery, Log, All);

ABotanicusGardeningRobot::ABotanicusGardeningRobot()
{
    WhipImpactSound=TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Botanicus/Audio/Sounds/S_MultiToolElectricWhip.S_MultiToolElectricWhip")));
    WhipSparks=TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/Botanicus/Items/robot_aroseur/VFX/NS_Robot_WhipImpact.NS_Robot_WhipImpact")));
    WhipContactSurface=TSoftObjectPtr<UBotanicusRobotContactSurface>(FSoftObjectPath(TEXT("/Game/Botanicus/Items/robot_aroseur/VFX/DA_RobotWhipContactSurface.DA_RobotWhipContactSurface")));
    bReplicates=true;SetReplicateMovement(true);SetNetUpdateFrequency(10);
    PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.bStartWithTickEnabled=false;
    RobotSkeletalMesh=TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Botanicus/Items/robot_aroseur/rig.rig")));
    RobotAnimationBlueprint=TSoftClassPtr<UAnimInstance>(FSoftObjectPath(TEXT("/Game/Botanicus/Items/robot_aroseur/animations/ABP_BotanicusGardeningRobot.ABP_BotanicusGardeningRobot_C")));
    const FString AnimRoot=TEXT("/Game/Botanicus/Items/robot_aroseur/animations/");
    StartRunAnimation=TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(AnimRoot+TEXT("start_Run.start_Run")));
    StopRunAnimation=TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(AnimRoot+TEXT("stop_Run.stop_Run")));
    ArmAnimation=TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(AnimRoot+TEXT("retract___deploy.retract___deploy")));
    WaterAnimation=TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(AnimRoot+TEXT("Arosage.Arosage")));
    RunAnimation=TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(AnimRoot+TEXT("Run.Run")));
    bUseControllerRotationYaw=false;
    AutoPossessAI=EAutoPossessAI::PlacedInWorldOrSpawned;AIControllerClass=AAIController::StaticClass();
    GetCapsuleComponent()->InitCapsuleSize(22,40);GetCapsuleComponent()->bEditableWhenInherited=true;
    // Native defaults only; Blueprint component overrides remain authoritative.
    GetMesh()->SetRelativeTransform(FTransform(VisualRotation,VisualOffset,VisualScale));GetMesh()->bEditableWhenInherited=true;
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
    GetCapsuleComponent()->SetCanEverAffectNavigation(false);
    GetCharacterMovement()->MaxWalkSpeed=180;GetCharacterMovement()->bOrientRotationToMovement=true;
    GetCharacterMovement()->RotationRate=FRotator(0,200,0);GetCharacterMovement()->bUseRVOAvoidance=true;
    GetCharacterMovement()->AvoidanceConsiderationRadius=160;
    WaterTankMount=CreateDefaultSubobject<USceneComponent>(TEXT("WaterTankMount"));WaterTankMount->SetupAttachment(GetMesh(),ReservoirSocket);
    WaterTankPivot=CreateDefaultSubobject<USceneComponent>(TEXT("WaterTankPivot"));WaterTankPivot->SetupAttachment(WaterTankMount);
    WaterTankPivot->SetRelativeLocation(ReservoirMeshSpaceCenter);WaterTankPivot->SetRelativeScale3D(ReservoirScale);WaterTankPivot->bEditableWhenInherited=true;
    Body=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RobotBody"));Body->SetupAttachment(WaterTankPivot);
    RobotAudio=CreateDefaultSubobject<UBotanicusRobotAudioComponent>(TEXT("RobotAudio"));
    RobotFace=CreateDefaultSubobject<UBotanicusRobotFaceComponent>(TEXT("RobotFace"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Shape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if(Shape.Succeeded())Body->SetStaticMesh(Shape.Object);
    Body->SetRelativeTransform(FTransform::Identity);
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);Body->SetCanEverAffectNavigation(false);
    ReservoirVisual=CreateDefaultSubobject<UBotanicusReservoirVisualComponent>(TEXT("ReservoirVisual"));ReservoirVisual->SetupAttachment(Body);
    WaterOutlet=CreateDefaultSubobject<USceneComponent>(TEXT("WaterOutlet"));WaterOutlet->SetupAttachment(GetRootComponent());
    WaterOutlet->SetRelativeLocation(FVector(25,0,20));
    Stream=CreateDefaultSubobject<UNiagaraComponent>(TEXT("WaterStream"));Stream->SetupAttachment(WaterOutlet);
    Stream->SetAutoActivate(false);Stream->SetUsingAbsoluteScale(true);
    Splash=CreateDefaultSubobject<UNiagaraComponent>(TEXT("SoilSplash"));Splash->SetupAttachment(GetRootComponent());
    Splash->SetAutoActivate(false);Splash->SetUsingAbsoluteLocation(true);Splash->SetUsingAbsoluteScale(true);
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> Drops(TEXT("/Game/Botanicus/VFX/Juice/NS_BotanicusWaterDrops.NS_BotanicusWaterDrops"));
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> Burst(TEXT("/Game/Botanicus/VFX/Juice/NS_BotanicusJuiceBurst.NS_BotanicusJuiceBurst"));
    if(Drops.Succeeded())Stream->SetAsset(Drops.Object);if(Burst.Succeeded())Splash->SetAsset(Burst.Object);
}
void ABotanicusGardeningRobot::OnConstruction(const FTransform& Transform)
{Super::OnConstruction(Transform);SetupAnimatedVisual();ReservoirVisual->SetFillFraction(CurrentWater/FMath::Max(.001f,Config.WaterCapacity));}
void ABotanicusGardeningRobot::BeginPlay()
{
    Super::BeginPlay();
    if(HasAuthority() && Base && Presentation.Activity==EBotanicusRobotActivity::Docked)SetActorRotation(FRotator(0,Base->DockForward().Rotation().Yaw,0));
    SetupAnimatedVisual();OnRep_Water();GetCharacterMovement()->MaxWalkSpeed=Config.MovementSpeed;
    if(HasAuthority())WhipContactSurface.LoadSynchronous();
    if(GetNetMode()!=NM_DedicatedServer)if(auto* Impact=WhipSparks.LoadSynchronous())Impact->PrecacheAssetPSOs();
    if(HasAuthority() && !GetController())SpawnDefaultController();
    if(auto* Clock=GetWorld()->GetGameState<ABotanicusGameState>())LastClock=Clock->GetDayTimeMinutes();
    LastUpdate=GetWorld()->GetTimeSeconds();
    GetWorldTimerManager().SetTimer(UpdateTimer,this,&ThisClass::UpdateRobot,.1f,true);
}
void ABotanicusGardeningRobot::EndPlay(const EEndPlayReason::Type R)
{
    GetWorldTimerManager().ClearTimer(UpdateTimer);ReleaseTarget();Stream->DeactivateImmediate();Splash->DeactivateImmediate();
    if(HasAuthority())if(auto* AI=Cast<AAIController>(GetController())){AI->StopMovement();AI->UnPossess();AI->Destroy();}
    Super::EndPlay(R);
}
void ABotanicusGardeningRobot::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ThisClass,bTaskPowerLimited);DOREPLIFETIME(ThisClass,StopRunPlayRate);DOREPLIFETIME(ThisClass,Config);DOREPLIFETIME(ThisClass,RobotId);DOREPLIFETIME(ThisClass,Base);
    DOREPLIFETIME(ThisClass,Battery);DOREPLIFETIME(ThisClass,CurrentWater);DOREPLIFETIME(ThisClass,bEnabled);
    DOREPLIFETIME(ThisClass,State);DOREPLIFETIME(ThisClass,Target);DOREPLIFETIME(ThisClass,TargetSlot);
    DOREPLIFETIME(ThisClass,WaterTarget);DOREPLIFETIME(ThisClass,Carrier);DOREPLIFETIME(ThisClass,Presentation);
}
void ABotanicusGardeningRobot::InitializeRobot(ABotanicusGardeningRobotBase* B,const FBotanicusGardeningSave& S)
{
    if(!HasAuthority())return;
    Base=B;RobotId=S.RobotId.IsValid()?S.RobotId:FGuid::NewGuid();
    Battery=FMath::Clamp(S.Battery,0.f,1.f);CurrentWater=FMath::Clamp(S.Water,0.f,Config.WaterCapacity);
    bEnabled=S.bEnabled;
    // No saved target is trusted. Navigation and demand are re-evaluated after all actors restore.
    State=bEnabled?EBotanicusGardeningState::SearchingPlant:EBotanicusGardeningState::Inactive;
    if(Base && FVector::Dist2D(S.RobotTransform.GetLocation(),Base->DockLocation())<=Config.DockingDistance && FMath::Abs(S.RobotTransform.GetLocation().Z-GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-Base->DockLocation().Z)<25)
    {SetActorRotation(FRotator(0,Base->DockForward().Rotation().Yaw,0));SetActivity(EBotanicusRobotActivity::Docked);}
}
FBotanicusGardeningSave ABotanicusGardeningRobot::Capture() const
{
    FBotanicusGardeningSave S;S.bInstalled=true;S.BaseId=Base?Base->BaseId:FGuid();S.RobotId=RobotId;
    S.RobotClass=FSoftClassPath(GetClass());S.RobotTransform=GetActorTransform();
    // A carried robot restores beside its carrier's saved position, never attached to a stale Pawn.
    if(Carrier)S.RobotTransform.SetLocation(Carrier->GetActorLocation());
    S.Battery=Battery;S.Water=CurrentWater;S.bEnabled=bEnabled;S.State=State;return S;
}
bool ABotanicusGardeningRobot::IsDocked() const
{
    return Presentation.Activity==EBotanicusRobotActivity::Docked && IsValid(Base) && Base->IsStationAvailable() && !Carrier &&
        FVector::Dist2D(GetActorLocation(),Base->DockLocation())<=Config.DockingDistance &&
        FMath::Abs(GetActorLocation().Z-GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-Base->DockLocation().Z)<25;
}
void ABotanicusGardeningRobot::ReleaseTarget()
{
    if(HasAuthority())if(auto* R=GetWorld()->GetSubsystem<UBotanicusGardeningSubsystem>())R->Release(this);
    Target=nullptr;bFirstWaterPulse=true;
}
void ABotanicusGardeningRobot::SetEnabled(bool bValue)
{
    if(!HasAuthority())return;
    if(Presentation.Activity==EBotanicusRobotActivity::Watering || Presentation.Activity==EBotanicusRobotActivity::PreparingWater)FinishWatering();
    bEnabled=bValue;ReleaseTarget();if(auto* AI=Cast<AAIController>(GetController()))AI->StopMovement();
    State=bEnabled?EBotanicusGardeningState::SearchingPlant:EBotanicusGardeningState::Inactive;NextSearch=0;ForceNetUpdate();
    if(auto* GM=GetWorld()->GetAuthGameMode<ABotanicusGameMode>())GM->ScheduleInventoryAutosave();
}
void ABotanicusGardeningRobot::AccountResources(float Minutes,float Seconds)
{
    if(IsDocked())
    {
        const bool bHadDepartureCharge = HasDockDepartureCharge();
        Battery=FMath::Clamp(Battery+Minutes/(FMath::Max(.1f,Config.BatteryRechargeHours)*60),0.f,1.f);
        if (!bHadDepartureCharge && HasDockDepartureCharge()) NextSearch = 0;
        if(Base->bWaterSupplyEnabled)CurrentWater=FMath::Min(Config.WaterCapacity,CurrentWater+Config.RefillRate*Seconds);
    }
    else if(State==EBotanicusGardeningState::SearchingPlant || State==EBotanicusGardeningState::MovingToPlant ||
        State==EBotanicusGardeningState::Watering || State==EBotanicusGardeningState::ReturningToBase)
        Battery=FMath::Max(0.f,Battery-Minutes/(FMath::Max(.1f,Config.BatteryRuntimeHours)*60));
}
bool ABotanicusGardeningRobot::Path(const FVector& Start,const FVector& End,float& Length) const
{
    // Character actor origins are capsule centres; on the dock they can lie outside the nav query height.
    const FVector NavStart=Start.Equals(GetActorLocation(),1.f)?GetNavAgentLocation():Start;
    if(NavStart.Equals(End,5.f)){Length=0;return true;}
    UNavigationPath* P=UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(),NavStart,End,const_cast<ThisClass*>(this));
    if(!P || !P->IsValid() || P->IsPartial())return false;Length=P->GetPathLength();return true;
}
bool ABotanicusGardeningRobot::HasWaterLine(const FVector& Soil) const
{
    return HasWaterLineFrom(WaterOutlet->GetComponentLocation(), Soil, Target);
}

bool ABotanicusGardeningRobot::HasWaterLineFrom(const FVector& Origin, const FVector& Soil, const AActor* Pot) const
{
    FCollisionQueryParams Query(SCENE_QUERY_STAT(RobotWater), false, this);
    if (Base) Query.AddIgnoredActor(Base);
    FHitResult Hit;
    return !GetWorld()->LineTraceSingleByChannel(Hit, Origin, Soil, ECC_Visibility, Query) || Hit.GetActor() == Pot;
}
float ABotanicusGardeningRobot::WateringApproachDistance() const
{
    // Retain the existing reach, adding only extra capsule radius above the original 22 cm.
    return Config.WateringDistance+FMath::Max(0.f,GetCapsuleComponent()->GetScaledCapsuleRadius()-22.f);
}
bool ABotanicusGardeningRobot::IsWithinWateringReach(const FVector& Soil) const
{
    return FVector::Dist2D(GetActorLocation(), Soil) <= WateringApproachDistance() + 45.f;
}

bool ABotanicusGardeningRobot::CanWaterFrom(const FVector& Feet, const FVector& Soil, const AActor* Pot) const
{
    const auto* Capsule = GetCapsuleComponent();
    const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
    const float Radius = Capsule->GetScaledCapsuleRadius();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(RobotWaterApproach), false, this);
    const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);
    const FCollisionResponseParams Responses(Capsule->GetCollisionResponseToChannels());
    // Recast's default agent is not necessarily the size of this Blueprint's capsule.
    if (GetWorld()->OverlapBlockingTestByChannel(Feet + FVector(0, 0, HalfHeight + 6.f),
        FQuat::Identity, Capsule->GetCollisionObjectType(), Shape, Query, Responses))
    {
        return false;
    }

    return HasWaterLineFrom(Feet + FVector(0, 0, HalfHeight + 20.f), Soil, Pot);
}

bool ABotanicusGardeningRobot::CanWaterFromHere(const FVector& Soil, const AActor* Pot) const
{
    const float Radius = GetCapsuleComponent()->GetScaledCapsuleRadius();
    return !IsDocked() && GetCharacterMovement()->IsMovingOnGround() && IsWithinWateringReach(Soil) &&
        FVector::DistSquared2D(GetActorLocation(), Soil) >= FMath::Square(Radius) &&
        HasWaterLineFrom(GetActorLocation() + FVector(0, 0, 20.f), Soil, Pot);
}

bool ABotanicusGardeningRobot::GetReturnPathLength(const FVector& From, float& Length) const
{
    FVector DockApproach;
    if (!GetDockApproach(DockApproach)) return false;
    if (Path(From, DockApproach, Length)) return true;

    // The capsule can settle a few centimeters outside the polygon after coming to rest.
    // Only recover to a nearby point on the same floor, never a remote or partial route.
    auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    FNavLocation Ground;
    const auto& Agent = GetNavAgentPropertiesRef();
    const FVector Feet = From.Equals(GetActorLocation(), 1.f) ? GetNavAgentLocation() : From;
    return Nav && Nav->ProjectPointToNavigation(Feet, Ground, FVector(30, 30, 33), &Agent) &&
        FVector::DistSquared2D(Feet, Ground.Location) <= FMath::Square(30.f) &&
        Path(Ground.Location, DockApproach, Length);
}

bool ABotanicusGardeningRobot::Approach(AActor* Pot, int32 Slot, FVector& Point, float& Length,
                                       TConstArrayView<FVector> Rejected) const
{
    auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    FBotanicusAutomaticWateringNeed Need;
    if (!Nav || !BotanicusAutomaticWatering::Read(Pot, Slot, 1.f, Need)) return false;

    const FNavAgentProperties& Agent = GetNavAgentPropertiesRef();
    const FVector Feet = GetNavAgentLocation();
    const float Reach = WateringApproachDistance();
    const float CapsuleRadius = GetCapsuleComponent()->GetScaledCapsuleRadius();
    const double RetrySeparation = FMath::Clamp<double>(WateringApproachTolerance, 25., 55.);
    const auto WasRejected = [Rejected, RetrySeparation](const FVector& Position)
    {
        return Rejected.ContainsByPredicate([&](const FVector& Failed)
        {
            return FVector::DistSquared2D(Failed, Position) < FMath::Square(RetrySeparation);
        });
    };

    // Adjacent crops can share a stance. Keep the actual feet position, not a snapped point on a new ring.
    if (!WasRejected(Feet) && CanWaterFromHere(Need.SoilPoint, Pot))
    {
        Point = Feet;
        Length = 0.f;
        return true;
    }

    bool bFound = false;
    Length = TNumericLimits<float>::Max();
    const float MinimumDistance = FMath::Max(55.f, CapsuleRadius + 33.f);
    // Test adjacent points, never the pot centre. Navigation must reach the complete path.
    for (int32 Index = 0; Index < 8; ++Index)
    {
        const float Angle = Index * PI / 4;
        const FVector Candidate = Need.SoilPoint + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0) * Reach;
        FNavLocation Projected;
        if (!Nav->ProjectPointToNavigation(Candidate, Projected, FVector(45, 45, 160), &Agent) ||
            WasRejected(Projected.Location)) continue;
        const double Distance = FVector::Dist2D(Projected.Location, Need.SoilPoint);
        if (Distance < MinimumDistance || Distance > Reach + 35.f || !CanWaterFrom(Projected.Location, Need.SoilPoint, Pot)) continue;

        float PathLength = 0.f;
        if (Path(GetActorLocation(), Projected.Location, PathLength) && PathLength < Length)
        {
            Point = Projected.Location;
            Length = PathLength;
            bFound = true;
        }
    }
    return bFound;
}
bool ABotanicusGardeningRobot::CanAfford(float Distance,float Water) const
{
    // World clock advances one game minute per world second. Time dilation affects navigation and clock equally.
    const float Minutes=Distance/FMath::Max(1.f,Config.MovementSpeed)+Water/FMath::Max(.001f,FMath::Min(.12f,Config.WateringRate))+2;
    return Battery>Config.BatteryReturnThreshold+Minutes/(FMath::Max(.1f,Config.BatteryRuntimeHours)*60);
}
bool ABotanicusGardeningRobot::HasDockDepartureCharge() const
{
    const float Minimum = FMath::Clamp(Config.MinimumBatteryToLeaveDock, 0.f, 100.f) / 100.f;
    return Battery >= Minimum - KINDA_SMALL_NUMBER;
}
EBotanicusGardeningState ABotanicusGardeningRobot::GetDockedState() const
{
    if (!HasDockDepartureCharge()) return EBotanicusGardeningState::Charging;
    if (CurrentWater < Config.WaterCapacity) return EBotanicusGardeningState::RefillingWater;
    return Battery < 1.f - KINDA_SMALL_NUMBER ? EBotanicusGardeningState::Charging : EBotanicusGardeningState::IdleAtBase;
}
bool ABotanicusGardeningRobot::Navigate(const FVector& Point,EBotanicusGardeningState NextState)
{
    auto* AI=Cast<AAIController>(GetController());if(!AI)return false;
    FVector MovePoint=Point;
    if(NextState==EBotanicusGardeningState::ReturningToBase && !GetDockApproach(MovePoint))return false;
    if(IsDocked() && NextState!=EBotanicusGardeningState::ReturningToBase)
    {
        FVector Approach;if(!GetDockApproach(Approach))return false;
        DockDestination=Approach+FVector(0,0,GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
        if(!DockLaneClear(DockDestination))return false;
        PendingDestination=Point;PendingState=NextState;State=NextState;
        AI->StopMovement();SetActivity(EBotanicusRobotActivity::LeavingDock,12);return true;
    }
    if (NextState == EBotanicusGardeningState::MovingToPlant && Point.Equals(GetNavAgentLocation(), 1.f))
    {
        FBotanicusAutomaticWateringNeed Need;
        if (!BotanicusAutomaticWatering::Read(Target, TargetSlot, 1.f, Need)) return false;
        Destination = Point;
        WaterTarget = Need.SoilPoint;
        State = NextState;
        LastWaterMoveRequest = int32(EPathFollowingRequestResult::AlreadyAtGoal);
        Presentation.WateringCompletedAt = -1;
        if (AI->GetMoveStatus() != EPathFollowingStatus::Idle) AI->StopMovement();
        GetCharacterMovement()->StopMovementImmediately();
        SetActivity(EBotanicusRobotActivity::AimingAtPlant, 3.f);
        return true;
    }
    const float Acceptance=NextState==EBotanicusGardeningState::ReturningToBase?5.f:8.f;
    if (NextState == EBotanicusGardeningState::MovingToPlant)
    {
        Destination = MovePoint;
    }
    auto Result=AI->MoveToLocation(MovePoint,Acceptance,false,true,false,false,nullptr,false);
    if (NextState == EBotanicusGardeningState::MovingToPlant)
    {
        LastWaterMoveRequest = static_cast<int32>(Result);
    }
    if(Result==EPathFollowingRequestResult::Failed)return false;
    SetActivity(NextState==EBotanicusGardeningState::ReturningToBase?EBotanicusRobotActivity::ApproachingDock:EBotanicusRobotActivity::None);
    Destination=MovePoint;State=NextState;MoveStarted=LastProgress=GetWorld()->GetTimeSeconds();LastProgressLocation=GetActorLocation();return true;
}
void ABotanicusGardeningRobot::Search()
{
    NextSearch=GetWorld()->GetTimeSeconds()+FMath::Max(.5f,Config.PlantSearchInterval);
    const double Now = GetWorld()->GetTimeSeconds();
    for (auto ActorIt = RetryAfter.CreateIterator(); ActorIt; ++ActorIt)
    {
        for (auto SlotIt = ActorIt.Value().CreateIterator(); SlotIt; ++SlotIt)
        {
            if (SlotIt.Value() <= Now) SlotIt.RemoveCurrent();
        }
        if (!ActorIt.Key().IsValid() || ActorIt.Value().IsEmpty()) ActorIt.RemoveCurrent();
    }
    if(!Base || !Base->IsStationAvailable()){State=EBotanicusGardeningState::Blocked;return;}
    if (IsDocked() && (!HasDockDepartureCharge() || CurrentWater < Config.WaterCapacity))
    {
        State = GetDockedState();
        return;
    }
    if (!CanAfford(0,0) || CurrentWater <= KINDA_SMALL_NUMBER){ReturnToBase();return;}
    bTaskPowerLimited=false;
    auto* Registry=GetWorld()->GetSubsystem<UBotanicusGardeningSubsystem>();if(!Registry)return;
    struct FCandidate {AActor* Pot;int32 Slot;float Deficit,Distance,Water;};
    TArray<FCandidate> Candidates;
    for(auto& Weak:Registry->GetPots())
    {
        auto* P=Weak.Get();if(!P || FVector::DistSquared(P->GetActorLocation(),Base->DockLocation())>FMath::Square(Config.PlantSearchRadius))continue;
        const auto* DeferredSlots = RetryAfter.Find(P);
        for(int32 I=0;I<BotanicusAutomaticWatering::SlotCount(P);++I)
        {
            FBotanicusAutomaticWateringNeed N;
            if(!BotanicusAutomaticWatering::Read(P,I,1.f /* species maximum */,N) || N.Current>=N.Minimum-KINDA_SMALL_NUMBER || !Registry->IsAvailable(P,I,this))continue;
            if (DeferredSlots && DeferredSlots->FindRef(I) > Now)
            {
                continue;
            }
            Candidates.Add({P,I,(N.Minimum-N.Current)/FMath::Max(.01f,N.Minimum),static_cast<float>(FVector::Dist2D(GetActorLocation(),N.SoilPoint)),N.UnitsNeeded()});
        }
    }
    // Keep the thirst priority, but don't cross the shop for a marginally drier plant.
    float HighestDeficit = 0.f;
    for (const FCandidate& Candidate : Candidates) HighestDeficit = FMath::Max(HighestDeficit, Candidate.Deficit);
    Candidates.Sort([HighestDeficit](const FCandidate& A, const FCandidate& B)
    {
        constexpr float ComparableDeficit = 0.10f;
        const int32 PriorityA = FMath::FloorToInt((HighestDeficit - A.Deficit) / ComparableDeficit);
        const int32 PriorityB = FMath::FloorToInt((HighestDeficit - B.Deficit) / ComparableDeficit);
        if (PriorityA != PriorityB) return PriorityA < PriorityB;
        if (A.Distance != B.Distance) return A.Distance < B.Distance;
        return A.Deficit > B.Deficit;
    });
    AActor* Best=nullptr;int32 Slot=0;FVector Point=FVector::ZeroVector;
    float BestReturnLength = 0.f;
    // Bound costly navigation work. Unreachable candidates enter cooldown, allowing later candidates next search.
    FVector DockApproach;
    const bool bHasDockApproach = !Candidates.IsEmpty() && GetDockApproach(DockApproach);
    const int32 Count = bHasDockApproach ? FMath::Min(8, Candidates.Num()) : 0;
    for(int32 I=0;I<Count;++I)
    {
        const auto& C=Candidates[I];FVector At;float Distance=0,Back=0;
        if(!Approach(C.Pot,C.Slot,At,Distance) || !Path(At,DockApproach,Back))
        {
            RetryAfter.FindOrAdd(C.Pot).Add(C.Slot, Now + Config.UnreachableRetrySeconds);
            continue;
        }
        if(!CanAfford(Distance+Back,FMath::Min(CurrentWater,C.Water))){bTaskPowerLimited=true;continue;}
        bTaskPowerLimited=false;
        Best=C.Pot;Slot=C.Slot;Point=At;BestReturnLength=Back;break;
    }
    if(Best && Registry->Reserve(Best,Slot,this))
    {
        Target=Best;TargetSlot=Slot;bFirstWaterPulse=true;
        ReturnPathLength = BestReturnLength;
        WateringRecoveryAttempts = 0;
        LastWaterMoveRequest = INDEX_NONE;
        RejectedWateringPositions.Reset();
        Destination = Point;
        if (!Navigate(Point, EBotanicusGardeningState::MovingToPlant)) RecoverWatering(TEXT("move request rejected"));
    }
    else if (IsDocked()) State = GetDockedState();
    else ReturnToBase();
}
void ABotanicusGardeningRobot::ReturnToBase()
{
    ReleaseTarget();
    if (IsDocked()){State=GetDockedState();return;}
    if(!Base || !Base->IsStationAvailable() || Battery<=0 || !Navigate(Base->DockLocation(),EBotanicusGardeningState::ReturningToBase))
    {State=EBotanicusGardeningState::Blocked;NextSearch=GetWorld()->GetTimeSeconds()+Config.UnreachableRetrySeconds;}
}
void ABotanicusGardeningRobot::FailMove()
{
    if(auto* AI=Cast<AAIController>(GetController()))AI->StopMovement();
    State=EBotanicusGardeningState::Blocked;NextSearch=GetWorld()->GetTimeSeconds()+Config.UnreachableRetrySeconds;
}
void ABotanicusGardeningRobot::AbandonWateringTarget()
{
    if (IsValid(Target))
    {
        RetryAfter.FindOrAdd(Target.Get()).Add(TargetSlot, GetWorld()->GetTimeSeconds() + Config.UnreachableRetrySeconds);
    }
    ReleaseTarget();
    if (!IsDocked()) SetActivity(EBotanicusRobotActivity::None);
    State = EBotanicusGardeningState::SearchingPlant;
    NextSearch = GetWorld()->GetTimeSeconds();
}

void ABotanicusGardeningRobot::RetryWateringApproach()
{
    FBotanicusAutomaticWateringNeed Need;
    if (!IsValid(Target) || !BotanicusAutomaticWatering::Read(Target, TargetSlot, 1.f, Need) ||
        Need.Current >= Need.Target - KINDA_SMALL_NUMBER)
    {
        AbandonWateringTarget();
        return;
    }
    if (CurrentWater <= KINDA_SMALL_NUMBER || !CanAfford(0,0))
    {
        ReturnToBase();
        return;
    }

    FVector Point;
    float Distance = 0.f, ReturnDistance = 0.f;
    if (IsValid(Base) && Base->IsStationAvailable() &&
        Approach(Target, TargetSlot, Point, Distance, RejectedWateringPositions) &&
        GetReturnPathLength(Point, ReturnDistance))
    {
        if (!CanAfford(Distance + ReturnDistance, FMath::Min(CurrentWater, Need.UnitsNeeded())))
        {
            ReturnToBase();
            return;
        }
        ReturnPathLength = ReturnDistance;
        if (Navigate(Point, EBotanicusGardeningState::MovingToPlant)) return;
    }
    UE_LOG(LogBotanicusRobotRecovery, Log, TEXT("%s: no alternative approach for %s slot %d; deferred."),
        *GetName(), *GetNameSafe(Target), TargetSlot);
    AbandonWateringTarget();
}

void ABotanicusGardeningRobot::RecoverWatering(const TCHAR* Reason)
{
    const auto* AI = Cast<AAIController>(GetController());
    const bool bRetry = IsValid(Target) && WateringRecoveryAttempts < FMath::Clamp(WateringRecoveryMaxAttempts, 0, 5);
    UE_LOG(LogBotanicusRobotRecovery, Log,
        TEXT("%s: %s slot %d, state=%d phase=%d, soil distance=%.0f cm, nav status=%d request=%d, %s (%s %d)."),
        *GetName(), *GetNameSafe(Target), TargetSlot, int32(State), int32(Presentation.Activity),
        FVector::Dist2D(GetActorLocation(), WaterTarget), AI ? int32(AI->GetMoveStatus()) : INDEX_NONE,
        LastWaterMoveRequest, Reason, bRetry ? TEXT("retry") : TEXT("defer after"), WateringRecoveryAttempts + (bRetry ? 1 : 0));

    if (auto* PathController = Cast<AAIController>(GetController())) PathController->StopMovement();
    GetCharacterMovement()->StopMovementImmediately();
    if (bRetry)
    {
        ++WateringRecoveryAttempts;
        RejectedWateringPositions.Add(Destination);
        // The usable stop can be offset from the requested goal by the arrival tolerance.
        const auto Phase = Presentation.Activity;
        if (Phase == EBotanicusRobotActivity::StoppingForWater || Phase == EBotanicusRobotActivity::AimingAtPlant ||
            Phase == EBotanicusRobotActivity::PreparingWater || Phase == EBotanicusRobotActivity::Watering)
        {
            RejectedWateringPositions.Add(GetNavAgentLocation());
        }
    }

    const auto Activity = Presentation.Activity;
    if (Activity == EBotanicusRobotActivity::PreparingWater || Activity == EBotanicusRobotActivity::Watering)
    {
        if (!bRetry && IsValid(Target))
        {
            RetryAfter.FindOrAdd(Target.Get()).Add(TargetSlot, GetWorld()->GetTimeSeconds() + Config.UnreachableRetrySeconds);
        }
        FinishWatering(false, bRetry);
    }
    else if (bRetry)
    {
        if (!IsDocked()) SetActivity(EBotanicusRobotActivity::None);
        RetryWateringApproach();
    }
    else
    {
        AbandonWateringTarget();
    }
}

void ABotanicusGardeningRobot::UpdateRobot()
{
    if(!HasAuthority()){UpdateCosmetics();return;}
    const double Now=GetWorld()->GetTimeSeconds();const float Seconds=FMath::Max(0.,Now-LastUpdate);LastUpdate=Now;
    float Minutes=0;
    if(auto* Clock=GetWorld()->GetGameState<ABotanicusGameState>())
    {
        const float Current = Clock->GetDayTimeMinutes();
        if (LastClock >= 0)
        {
            Minutes = Current - LastClock;
            if (Minutes < -720.f) Minutes += 1440.f;
            // A clock jump changes the hour, but the robot has only moved for the elapsed world time.
            if (FMath::Abs(Minutes - Seconds) > .1f) Minutes = Seconds;
            Minutes = FMath::Max(0.f, Minutes);
        }
        LastClock = Current;
    }
    if(Carrier)
    {
        if(Carrier->IsActorBeingDestroyed() || !Carrier->GetController())DropCarriedRobot();
        UpdateCosmetics();return;
    }
    // Attachment parent may have disappeared between updates; restore physical recovery at current location.
    if(GetCharacterMovement()->MovementMode==MOVE_None && !Carrier)OnRep_Carrier();
    AccountResources(Minutes,Seconds);
    // Clandestine access can expire during approach, arm preparation or watering.
    // Finish the current arm phase normally, then use the existing return navigation.
    if (Presentation.Activity == EBotanicusRobotActivity::FinishingWater && bRetryAfterFinish &&
        IsValid(Target) && !BotanicusAutomaticWatering::CanAccess(Target))
    {
        bRetryAfterFinish = false;
        bReturnAfterFinish = true;
        ReleaseTarget();
    }
    if(IsValid(Target) && !BotanicusAutomaticWatering::CanAccess(Target))
    {
        if(auto* AI=Cast<AAIController>(GetController()))AI->StopMovement();
        const auto Activity=Presentation.Activity;
        if(Activity==EBotanicusRobotActivity::Watering || Activity==EBotanicusRobotActivity::PreparingWater ||
            Activity==EBotanicusRobotActivity::StoppingForWater || Activity==EBotanicusRobotActivity::AimingAtPlant)
            FinishWatering(true);
        else ReturnToBase();
        UpdateCosmetics();return;
    }
    if(Presentation.Activity==EBotanicusRobotActivity::WhipBump){UpdateCosmetics();return;}
    if(UpdateActivity(Now)){UpdateCosmetics();return;}
    if(!IsValid(Base) || !bEnabled || (Battery<=0 && !IsDocked()))
    {
        if(Presentation.Activity==EBotanicusRobotActivity::Watering)FinishWatering();
        if(auto* AI=Cast<AAIController>(GetController()))AI->StopMovement();ReleaseTarget();
        State=Battery<=0?EBotanicusGardeningState::Blocked:EBotanicusGardeningState::Inactive;UpdateCosmetics();return;
    }
    if(!Base->IsStationAvailable()){if(Presentation.Activity==EBotanicusRobotActivity::Watering)FinishWatering();if(auto* AI=Cast<AAIController>(GetController()))AI->StopMovement();ReleaseTarget();State=EBotanicusGardeningState::Blocked;NextSearch=Now+1;UpdateCosmetics();return;}
    if(IsDocked() && State!=EBotanicusGardeningState::MovingToPlant && State!=EBotanicusGardeningState::Watering)
    {
        if(auto* AI=Cast<AAIController>(GetController()))AI->StopMovement();MoveFailures=0;
        if (HasDockDepartureCharge() && CurrentWater >= Config.WaterCapacity && Now >= NextSearch)
            Search();
        else
            State = GetDockedState();
    }
    else if(State==EBotanicusGardeningState::Watering)
    {
        // Use the route budget validated when this task was accepted. No replanning while stationary.
        FBotanicusAutomaticWateringNeed N;
        if(!IsValid(Target) || !BotanicusAutomaticWatering::Read(Target,TargetSlot,1.f /* species maximum */,N) || N.Current>=N.Target-KINDA_SMALL_NUMBER)
        {FinishWatering();}
        else if(!CanAfford(ReturnPathLength,0))FinishWatering(true);
        else if(CurrentWater<=KINDA_SMALL_NUMBER)FinishWatering(true);
        else if (!IsWithinWateringReach(N.SoilPoint) || !HasWaterLine(N.SoilPoint))
        {
            WaterTarget = N.SoilPoint;
            RecoverWatering(TEXT("water line lost or plant out of reach"));
        }
        else
        {
            WaterTarget = N.SoilPoint;
            const double AimYaw = (WaterTarget - GetActorLocation()).Rotation().Yaw;
            if (FMath::Abs(FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, AimYaw)) > 5.)
            {
                SetActorRotation(FRotator(0, AimYaw, 0));
            }
            const float Used=BotanicusAutomaticWatering::Apply(Target,TargetSlot,1.f /* species maximum */,FMath::Min(CurrentWater,FMath::Min(.12f,Config.WateringRate)*Seconds),bFirstWaterPulse);
            CurrentWater=FMath::Max(0.f,CurrentWater-Used);
            if (Used > 0)
            {
                bFirstWaterPulse = false;
                LastWaterTransfer = GetWorld()->GetRealTimeSeconds();
            }
            else if (GetWorld()->GetRealTimeSeconds() - LastWaterTransfer >= FMath::Max(1.f, WateringStartTimeout))
            {
                RecoverWatering(TEXT("water transfer made no progress"));
            }
            // Finish on the accepted transfer, before natural drying can lower the plant again next tick.
            const float RemainingMoisture=FMath::Max(0.f,N.Target-N.Current-Used/N.UnitsPerMoisture);
            if(Used>0 && (RemainingMoisture<=KINDA_SMALL_NUMBER || CurrentWater<=KINDA_SMALL_NUMBER))
            {
                // Cosmetic success belongs to this accepted transfer, never a cancellation or an empty tank.
                if(RemainingMoisture<=KINDA_SMALL_NUMBER && CurrentWater>KINDA_SMALL_NUMBER)
                    Presentation.WateringCompletedAt=PresentationNow();
                FinishWatering(CurrentWater<=KINDA_SMALL_NUMBER);
            }
        }
    }
    else if(State==EBotanicusGardeningState::MovingToPlant || State==EBotanicusGardeningState::ReturningToBase)
    {
        auto* AI=Cast<AAIController>(GetController());
        if(State==EBotanicusGardeningState::MovingToPlant)
        {
            FBotanicusAutomaticWateringNeed N;
            if(!IsValid(Target) || !BotanicusAutomaticWatering::Read(Target,TargetSlot,1.f /* species maximum */,N) || N.Current>=N.Target-KINDA_SMALL_NUMBER)
            {if(AI)AI->StopMovement();ReleaseTarget();State=EBotanicusGardeningState::SearchingPlant;NextSearch=Now;}
            else if (!CanAfford(ReturnPathLength + FVector::Dist2D(GetActorLocation(), Destination), 0))
                ReturnToBase();
            else if (CurrentWater <= KINDA_SMALL_NUMBER) ReturnToBase();
            else
            {
                WaterTarget = N.SoilPoint;
                const double ApproachDistance = FVector::Dist2D(GetActorLocation(), Destination);
                const bool bMoveEnded = !AI || AI->GetMoveStatus() == EPathFollowingStatus::Idle;
                // Let a healthy path reach its goal. Apply the wider tolerance after navigation stops.
                const bool bNearApproach = ApproachDistance <= (bMoveEnded ? FMath::Max(8.f, WateringApproachTolerance) : 8.f);
                if ((bNearApproach && IsWithinWateringReach(N.SoilPoint)) ||
                    (bMoveEnded && CanWaterFromHere(N.SoilPoint, Target)))
                {
                    BeginWaterPreparation();
                }
                else if (bMoveEnded)
                {
                    RecoverWatering(TEXT("navigation ended outside the watering approach"));
                }
                else
                {
                    if (FVector::DistSquared2D(GetActorLocation(), LastProgressLocation) > FMath::Square(8.f))
                    {
                        LastProgress = Now;
                        LastProgressLocation = GetActorLocation();
                    }
                    if (Now - LastProgress > FMath::Max(1.f, WateringStartTimeout) || Now - MoveStarted > 60)
                    {
                        RecoverWatering(TEXT("approach made no navigation progress"));
                    }
                }
            }
        }
        else if(AI && AI->GetMoveStatus()==EPathFollowingStatus::Idle && !IsDocked())
        {
            if(FVector::Dist2D(GetActorLocation(),Destination)<25){GetCharacterMovement()->StopMovementImmediately();SetActivity(EBotanicusRobotActivity::StoppingAtDock,StopRunDuration());}
            else FailMove();
        }
        if (State == EBotanicusGardeningState::ReturningToBase && Presentation.Activity == EBotanicusRobotActivity::ApproachingDock)
        {
            if(FVector::DistSquared(GetActorLocation(),LastProgressLocation)>FMath::Square(8.f)){LastProgress=Now;LastProgressLocation=GetActorLocation();}
            if((State==EBotanicusGardeningState::MovingToPlant || State==EBotanicusGardeningState::ReturningToBase) && (Now-LastProgress>5 || Now-MoveStarted>60))
            {
                if(++MoveFailures<=2 && AI && Navigate(Destination,State)){}else FailMove();
            }
        }
    }
    else if(State==EBotanicusGardeningState::Blocked && Now>=NextSearch)ReturnToBase();
    else if(Now>=NextSearch){State=EBotanicusGardeningState::SearchingPlant;Search();}
    UpdateCosmetics();
}
void ABotanicusGardeningRobot::OnRep_Water()
{if(ReservoirVisual)ReservoirVisual->SetFillFraction(CurrentWater/FMath::Max(.001f,Config.WaterCapacity));}
void ABotanicusGardeningRobot::UpdateCosmetics()
{
    if(GetNetMode()==NM_DedicatedServer)return;
    ReservoirVisual->SetFillFraction(CurrentWater/FMath::Max(.001f,Config.WaterCapacity));
    if(RobotAudio)RobotAudio->UpdatePresentation();
    const bool bWater=State==EBotanicusGardeningState::Watering && Presentation.Activity==EBotanicusRobotActivity::Watering && IsValid(Target) && !BotanicusAutomaticWatering::IsMature(Target,TargetSlot) && BotanicusAutomaticWatering::CanAccess(Target) && !Carrier && CurrentWater>0;
    if(RobotAudio)RobotAudio->SetWaterActive(bWater);
    if(RobotFace)RobotFace->UpdatePresentation(bWater);
    if(!bWater){if(Stream->IsActive())Stream->DeactivateImmediate();if(Splash->IsActive())Splash->DeactivateImmediate();return;}
    const FVector Nozzle=WaterOutlet->GetComponentLocation();
    const float Flight=FMath::Clamp(FVector::Distance(Nozzle,WaterTarget)/450.f,.1f,.6f);
    Stream->SetVariableFloat(TEXT("User.SpawnRate"),25);Stream->SetVariableFloat(TEXT("User.Lifetime"),Flight);
    Stream->SetVariableFloat(TEXT("User.Size"),1.1f);Stream->SetVariableVec3(TEXT("User.Gravity"),FVector(0,0,-500));
    Stream->SetVariableVec3(TEXT("User.Velocity"),(WaterTarget-Nozzle-FVector(0,0,-250)*Flight*Flight)/Flight);
    Stream->SetVariableLinearColor(TEXT("User.Tint"),FLinearColor(.2f,.65f,1));
    if(!Stream->IsActive())Stream->Activate(true);
    Splash->SetWorldLocation(WaterTarget);Splash->SetVariableInt(TEXT("User.BurstCount"),2);
    Splash->SetVariableFloat(TEXT("User.Size"),1);Splash->SetVariableFloat(TEXT("User.Lifetime"),.18f);
    Splash->SetVariableVec3(TEXT("User.Velocity"),FVector(0,0,45));Splash->SetVariableVec3(TEXT("User.Gravity"),FVector(0,0,-300));
    Splash->SetVariableLinearColor(TEXT("User.Tint"),FLinearColor(.2f,.65f,1));
    if(!Splash->IsActive())Splash->Activate(true);
}
FText ABotanicusGardeningRobot::Describe() const
{
    static const TCHAR* Names[]={TEXT("Desactive"),TEXT("En attente"),TEXT("Recherche d'une plante"),TEXT("En deplacement"),TEXT("Arrosage en cours"),TEXT("Retour a la base"),TEXT("Remplissage"),TEXT("Recharge"),TEXT("Bloque — desactivez pour porter")};
    return FText::Format(NSLOCTEXT("Robot","Status","Robot jardinier — Batterie {0}% | Eau {1}%\n{2}"),FMath::RoundToInt(Battery*100),FMath::RoundToInt(CurrentWater/FMath::Max(.01f,Config.WaterCapacity)*100),FText::FromString(Names[static_cast<uint8>(State)]));
}
FBotanicusInteractionPrompt ABotanicusGardeningRobot::GetInteractionPrompt_Implementation(AActor* I) const
{
    FBotanicusInteractionPrompt P;P.TargetName=Describe();
    const bool bNearDock=Carrier && Base && Base->IsStationAvailable() && FVector::Dist2D(Carrier->GetActorLocation(),Base->DockLocation())<150;
    P.ActionText=FText::FromString(Carrier?(bNearDock?TEXT("Reposer sur la station"):TEXT("Reposer le robot")):(!bEnabled || Battery<=0?TEXT("Porter le robot"):TEXT("Desactiver le robot")));
    P.bCanInteract=CanInteract_Implementation(I);return P;
}
bool ABotanicusGardeningRobot::CanInteract_Implementation(AActor* I) const
{
    const auto* Character=Cast<ABotanicusCharacter>(I);
    if(!IsValid(Character) || (Carrier && Carrier!=I) || FVector::DistSquared(I->GetActorLocation(),GetActorLocation())>=FMath::Square(250.f))return false;
    if(!Carrier && (!bEnabled || Battery<=0))
    {
        if(Character->IsCarryingEquipment() || Character->GetHeldWateringCan())return false;
        TArray<AActor*> Attached;Character->GetAttachedActors(Attached);
        for(auto* A:Attached)if(A!=this && A->IsA<ABotanicusGardeningRobot>())return false;
    }
    return true;
}
void ABotanicusGardeningRobot::Interact_Implementation(AActor* I)
{
    if(!HasAuthority() || !CanInteract_Implementation(I))return;
    if(Carrier){DropCarriedRobot();return;}
    if(bEnabled && Battery>0){SetEnabled(false);return;}
    SetActivity(EBotanicusRobotActivity::None);ReleaseTarget();if(auto* AI=Cast<AAIController>(GetController()))AI->StopMovement();
    bEnabled=false;State=EBotanicusGardeningState::Inactive;Carrier=Cast<ACharacter>(I);OnRep_Carrier();ForceNetUpdate();
}
void ABotanicusGardeningRobot::OnRep_Carrier()
{
    if(Carrier)
    {
        GetCharacterMovement()->StopMovementImmediately();GetCharacterMovement()->DisableMovement();
        // Keep a Visibility-only target so E can put down the carried robot; no physical obstruction.
        GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECR_Ignore);
        GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
        AttachToComponent(Carrier->GetRootComponent(),FAttachmentTransformRules::SnapToTargetNotIncludingScale);
        SetActorRelativeLocation(FVector(65,0,0));
    }
    else
    {
        DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
        GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));
        GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
        GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);GetCharacterMovement()->SetMovementMode(MOVE_Falling);
    }
}
bool ABotanicusGardeningRobot::DropCarriedRobot()
{
    if(!HasAuthority() || bDropping)return false;TGuardValue<bool> Guard(bDropping,true);
    auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());FNavLocation At;
    const bool bDockDrop=Carrier && Base && Base->IsStationAvailable() && FVector::Dist2D(Carrier->GetActorLocation(),Base->DockLocation())<150;
    FVector Desired=GetActorLocation();if(Carrier)Desired=Carrier->GetActorLocation()+Carrier->GetActorForwardVector()*95;
    if(bDockDrop)Desired=Base->DockLocation();
    if(!Nav || !Nav->ProjectPointToNavigation(Desired,At,FVector(100,100,180)))return false;
    const float HalfHeight=GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const float Radius=GetCapsuleComponent()->GetScaledCapsuleRadius();
    const FVector Position=(bDockDrop?Base->DockLocation():At.Location)+FVector(0,0,HalfHeight+2.5f);
    FCollisionQueryParams Q(SCENE_QUERY_STAT(RobotRecovery),false,this);if(Carrier)Q.AddIgnoredActor(Carrier);
    const auto Shape=FCollisionShape::MakeCapsule(Radius,FMath::Max(Radius,HalfHeight-1));
    if(GetWorld()->OverlapBlockingTestByChannel(Position,FQuat::Identity,ECC_Pawn,Shape,Q))return false;
    FHitResult Obstruction;
    if(GetWorld()->SweepSingleByChannel(Obstruction,GetActorLocation(),Position,FQuat::Identity,ECC_Pawn,Shape,Q))return false;
    // A deliberate player drop within reach of the dock, never an automatic return teleport.
    Carrier=nullptr;OnRep_Carrier();SetActorLocation(Position,false,nullptr,ETeleportType::TeleportPhysics);
    if(bDockDrop){SetActorRotation(FRotator(0,Base->DockForward().Rotation().Yaw,0));SetActivity(EBotanicusRobotActivity::Docked);}else SetActivity(EBotanicusRobotActivity::None);ForceNetUpdate();
    if(auto* GM=GetWorld()->GetAuthGameMode<ABotanicusGameMode>())GM->ScheduleInventoryAutosave();return true;
}

bool ABotanicusGardeningRobot::HasPowerForTask() const
{
    return IsValid(Base) && Base->IsStationAvailable() && !Carrier && bEnabled && Battery>0 &&
        !bTaskPowerLimited && (!IsDocked() || HasDockDepartureCharge()) && CanAfford(0,0);
}
bool ABotanicusGardeningRobot::CanLaunchRound() const
{
    return HasPowerForTask() && IsDocked() && CurrentWater>=Config.WaterCapacity &&
        State!=EBotanicusGardeningState::MovingToPlant && State!=EBotanicusGardeningState::Watering && !IsValid(Target);
}
bool ABotanicusGardeningRobot::LaunchRound()
{
    if(!HasAuthority() || !CanLaunchRound())return false;
    // Search owns reservations and checks the real outbound + return route and water cost.
    State=EBotanicusGardeningState::SearchingPlant;NextSearch=0;Search();ForceNetUpdate();return true;
}
