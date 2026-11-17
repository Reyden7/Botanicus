#include "Gardening/BotanicusGardeningRobot.h"
#include "Visuals/BotanicusReservoirVisualComponent.h"
#include "Gardening/BotanicusRobotAudioComponent.h"
#include "Gardening/BotanicusGardeningRobotBase.h"
#include "Gardening/BotanicusGardeningSubsystem.h"
#include "Growing/BotanicusPlantPotActor.h"
#include "Growing/BotanicusAutomaticWatering.h"
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

ABotanicusGardeningRobot::ABotanicusGardeningRobot()
{
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
    DOREPLIFETIME(ThisClass,bAwaitFullCharge);DOREPLIFETIME(ThisClass,bTaskPowerLimited);DOREPLIFETIME(ThisClass,StopRunPlayRate);DOREPLIFETIME(ThisClass,Config);DOREPLIFETIME(ThisClass,RobotId);DOREPLIFETIME(ThisClass,Base);
    DOREPLIFETIME(ThisClass,Battery);DOREPLIFETIME(ThisClass,CurrentWater);DOREPLIFETIME(ThisClass,bEnabled);
    DOREPLIFETIME(ThisClass,State);DOREPLIFETIME(ThisClass,Target);DOREPLIFETIME(ThisClass,TargetSlot);
    DOREPLIFETIME(ThisClass,WaterTarget);DOREPLIFETIME(ThisClass,Carrier);DOREPLIFETIME(ThisClass,Presentation);
}
void ABotanicusGardeningRobot::InitializeRobot(ABotanicusGardeningRobotBase* B,const FBotanicusGardeningSave& S)
{
    if(!HasAuthority())return;
    Base=B;RobotId=S.RobotId.IsValid()?S.RobotId:FGuid::NewGuid();
    Battery=FMath::Clamp(S.Battery,0.f,1.f);CurrentWater=FMath::Clamp(S.Water,0.f,Config.WaterCapacity);
    bEnabled=S.bEnabled;bAwaitFullCharge=S.bAwaitFullCharge;
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
    S.Battery=Battery;S.Water=CurrentWater;S.bEnabled=bEnabled;S.State=State;S.bAwaitFullCharge=bAwaitFullCharge;return S;
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
        Battery=FMath::Clamp(Battery+Minutes/(FMath::Max(.1f,Config.BatteryRechargeHours)*60),0.f,1.f);
        if(Base->bWaterSupplyEnabled)CurrentWater=FMath::Min(Config.WaterCapacity,CurrentWater+Config.RefillRate*Seconds);
        if(Battery>=1-KINDA_SMALL_NUMBER)bAwaitFullCharge=false;
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
    FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(RobotWater),false,this);if(Base)Q.AddIgnoredActor(Base);
    return !GetWorld()->LineTraceSingleByChannel(H,WaterOutlet->GetComponentLocation(),Soil,ECC_Visibility,Q) || H.GetActor()==Target;
}
float ABotanicusGardeningRobot::WateringApproachDistance() const
{
    // Retain the existing reach, adding only extra capsule radius above the original 22 cm.
    return Config.WateringDistance+FMath::Max(0.f,GetCapsuleComponent()->GetScaledCapsuleRadius()-22.f);
}
bool ABotanicusGardeningRobot::Approach(AActor* Pot,int32 Slot,FVector& Point,float& Length) const
{
    auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());if(!Nav)return false;
    FBotanicusAutomaticWateringNeed N;if(!BotanicusAutomaticWatering::Read(Pot,Slot,1.f /* species maximum */,N))return false;
    const float Reach=WateringApproachDistance();const float MinimumDistance=FMath::Max(55.f,GetCapsuleComponent()->GetScaledCapsuleRadius()+33.f);
    bool bFound=false;Length=TNumericLimits<float>::Max();
    // Test adjacent points, never the pot centre. Navigation must reach the complete path.
    for(int32 I=0;I<8;++I)
    {
        const float A=I*PI/4;FVector Candidate=N.SoilPoint+FVector(FMath::Cos(A),FMath::Sin(A),0)*Reach;
        FNavLocation Projected;if(!Nav->ProjectPointToNavigation(Candidate,Projected,FVector(45,45,160)))continue;
        if(FVector::Dist2D(Projected.Location,N.SoilPoint)<MinimumDistance || FVector::Dist2D(Projected.Location,N.SoilPoint)>Reach+35)continue;
        FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(RobotApproach),false,this);Q.AddIgnoredActor(Pot);if(Base)Q.AddIgnoredActor(Base);
        if(GetWorld()->LineTraceSingleByChannel(Hit,Projected.Location+FVector(0,0,60),N.SoilPoint,ECC_Visibility,Q))continue;
        float L=0;if(Path(GetActorLocation(),Projected.Location,L) && L<Length){Length=L;Point=Projected.Location;bFound=true;}
    }
    return bFound;
}
bool ABotanicusGardeningRobot::CanAfford(float Distance,float Water) const
{
    // World clock advances one game minute per world second. Time dilation affects navigation and clock equally.
    const float Minutes=Distance/FMath::Max(1.f,Config.MovementSpeed)+Water/FMath::Max(.001f,FMath::Min(.12f,Config.WateringRate))+2;
    return Battery>Config.BatteryReturnThreshold+Minutes/(FMath::Max(.1f,Config.BatteryRuntimeHours)*60);
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
    const float Acceptance=NextState==EBotanicusGardeningState::ReturningToBase?5.f:8.f;
    auto Result=AI->MoveToLocation(MovePoint,Acceptance,false,true,false,false,nullptr,false);
    if(Result==EPathFollowingRequestResult::Failed)return false;
    SetActivity(NextState==EBotanicusGardeningState::ReturningToBase?EBotanicusRobotActivity::ApproachingDock:EBotanicusRobotActivity::None);
    Destination=MovePoint;State=NextState;MoveStarted=LastProgress=GetWorld()->GetTimeSeconds();LastProgressLocation=GetActorLocation();return true;
}
void ABotanicusGardeningRobot::Search()
{
    NextSearch=GetWorld()->GetTimeSeconds()+FMath::Max(.5f,Config.PlantSearchInterval);
    for(auto I=RetryAfter.CreateIterator();I;++I)if(!I.Key().IsValid() || I.Value()<=GetWorld()->GetTimeSeconds())I.RemoveCurrent();
    if(!Base || !Base->IsStationAvailable()){State=EBotanicusGardeningState::Blocked;return;}
    if(IsDocked() && CurrentWater<Config.WaterCapacity){State=EBotanicusGardeningState::RefillingWater;return;}
    if(Battery<=Config.BatteryReturnThreshold || CurrentWater<=KINDA_SMALL_NUMBER){ReturnToBase(Battery<=Config.BatteryReturnThreshold);return;}
    bTaskPowerLimited=false;
    auto* Registry=GetWorld()->GetSubsystem<UBotanicusGardeningSubsystem>();if(!Registry)return;
    struct FCandidate {AActor* Pot;int32 Slot;float Deficit,Distance,Water;};
    TArray<FCandidate> Candidates;
    for(auto& Weak:Registry->GetPots())
    {
        auto* P=Weak.Get();if(!P || FVector::DistSquared(P->GetActorLocation(),Base->DockLocation())>FMath::Square(Config.PlantSearchRadius) || RetryAfter.FindRef(P)>GetWorld()->GetTimeSeconds())continue;
        for(int32 I=0;I<BotanicusAutomaticWatering::SlotCount(P);++I)
        {
            FBotanicusAutomaticWateringNeed N;
            if(!BotanicusAutomaticWatering::Read(P,I,1.f /* species maximum */,N) || N.Current>=N.Minimum-KINDA_SMALL_NUMBER || !Registry->IsAvailable(P,I,this))continue;
            Candidates.Add({P,I,(N.Minimum-N.Current)/FMath::Max(.01f,N.Minimum),static_cast<float>(FVector::Dist2D(GetActorLocation(),N.SoilPoint)),N.UnitsNeeded()});
        }
    }
    Candidates.Sort([](const FCandidate& A,const FCandidate& B){return A.Deficit==B.Deficit?A.Distance<B.Distance:A.Deficit>B.Deficit;});
    AActor* Best=nullptr;int32 Slot=0;FVector Point;
    // Bound costly navigation work. Unreachable candidates enter cooldown, allowing later candidates next search.
    const int32 Count=FMath::Min(8,Candidates.Num());
    for(int32 I=0;I<Count;++I)
    {
        const auto& C=Candidates[I];FVector At;float Distance=0,Back=0;
        if(!Approach(C.Pot,C.Slot,At,Distance) || !Path(At,Base->DockLocation(),Back))
        {RetryAfter.Add(C.Pot,GetWorld()->GetTimeSeconds()+Config.UnreachableRetrySeconds);continue;}
        if(!CanAfford(Distance+Back,FMath::Min(CurrentWater,C.Water))){bTaskPowerLimited=true;continue;}
        bTaskPowerLimited=false;
        Best=C.Pot;Slot=C.Slot;Point=At;break;
    }
    if(Best && Registry->Reserve(Best,Slot,this))
    {
        Target=Best;TargetSlot=Slot;bFirstWaterPulse=true;NextReturnPathCheck=0;
        if(!Navigate(Point,EBotanicusGardeningState::MovingToPlant))FailMove();
    }
    else if(IsDocked())State=EBotanicusGardeningState::IdleAtBase;
    else ReturnToBase();
}
void ABotanicusGardeningRobot::ReturnToBase(bool bLowBattery)
{
    ReleaseTarget();bAwaitFullCharge|=bLowBattery;
    if(IsDocked()){State=bAwaitFullCharge?EBotanicusGardeningState::Charging:EBotanicusGardeningState::RefillingWater;return;}
    if(!Base || !Base->IsStationAvailable() || Battery<=0 || !Navigate(Base->DockLocation(),EBotanicusGardeningState::ReturningToBase))
    {State=EBotanicusGardeningState::Blocked;NextSearch=GetWorld()->GetTimeSeconds()+Config.UnreachableRetrySeconds;}
}
void ABotanicusGardeningRobot::FailMove()
{
    if(Target){RetryAfter.Add(Target,GetWorld()->GetTimeSeconds()+Config.UnreachableRetrySeconds);ReleaseTarget();ReturnToBase();return;}
    if(auto* AI=Cast<AAIController>(GetController()))AI->StopMovement();
    State=EBotanicusGardeningState::Blocked;NextSearch=GetWorld()->GetTimeSeconds()+Config.UnreachableRetrySeconds;
}
void ABotanicusGardeningRobot::UpdateRobot()
{
    if(!HasAuthority()){UpdateCosmetics();return;}
    const double Now=GetWorld()->GetTimeSeconds();const float Seconds=FMath::Max(0.,Now-LastUpdate);LastUpdate=Now;
    float Minutes=0;
    if(auto* Clock=GetWorld()->GetGameState<ABotanicusGameState>())
    {
        const float Current=Clock->GetDayTimeMinutes();if(LastClock>=0){Minutes=Current-LastClock;if(Minutes< -720)Minutes+=1440;Minutes=FMath::Max(0.f,Minutes);}LastClock=Current;
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
    if(UpdateActivity(Now)){UpdateCosmetics();return;}
    if(!IsValid(Base) || !bEnabled || Battery<=0)
    {
        if(Presentation.Activity==EBotanicusRobotActivity::Watering)FinishWatering();
        if(auto* AI=Cast<AAIController>(GetController()))AI->StopMovement();ReleaseTarget();
        State=Battery<=0?EBotanicusGardeningState::Blocked:EBotanicusGardeningState::Inactive;UpdateCosmetics();return;
    }
    if(!Base->IsStationAvailable()){if(Presentation.Activity==EBotanicusRobotActivity::Watering)FinishWatering();if(auto* AI=Cast<AAIController>(GetController()))AI->StopMovement();ReleaseTarget();State=EBotanicusGardeningState::Blocked;NextSearch=Now+1;UpdateCosmetics();return;}
    if(IsDocked() && State!=EBotanicusGardeningState::MovingToPlant && State!=EBotanicusGardeningState::Watering)
    {
        if(auto* AI=Cast<AAIController>(GetController()))AI->StopMovement();MoveFailures=0;
        if(Battery<=Config.BatteryReturnThreshold)bAwaitFullCharge=true;
        if(bAwaitFullCharge)State=EBotanicusGardeningState::Charging;
        else if(CurrentWater<Config.WaterCapacity)State=EBotanicusGardeningState::RefillingWater;
        else if(Now>=NextSearch){State=EBotanicusGardeningState::SearchingPlant;Search();}
        else State=CurrentWater<Config.WaterCapacity-KINDA_SMALL_NUMBER?EBotanicusGardeningState::RefillingWater:EBotanicusGardeningState::IdleAtBase;
    }
    else if(State==EBotanicusGardeningState::Watering)
    {
        FBotanicusAutomaticWateringNeed N;bool bReturnPath=true;
        if(Now>=NextReturnPathCheck){NextReturnPathCheck=Now+1;bReturnPath=Path(GetActorLocation(),Base->DockLocation(),ReturnPathLength);}
        if(!IsValid(Target) || !BotanicusAutomaticWatering::Read(Target,TargetSlot,1.f /* species maximum */,N) || N.Current>=N.Target-KINDA_SMALL_NUMBER)
        {FinishWatering();}
        else if(!bReturnPath || !CanAfford(ReturnPathLength,0))FinishWatering(true,true);
        else if(CurrentWater<=KINDA_SMALL_NUMBER)FinishWatering(true);
        else if(FVector::Dist2D(GetActorLocation(),N.SoilPoint)>WateringApproachDistance()+45 || !HasWaterLine(N.SoilPoint))
        {if(Target)RetryAfter.Add(Target,Now+Config.UnreachableRetrySeconds);FinishWatering(true);}
        else
        {
            WaterTarget=N.SoilPoint;SetActorRotation(FRotator(0,(N.SoilPoint-GetActorLocation()).Rotation().Yaw,0));
            const float Used=BotanicusAutomaticWatering::Apply(Target,TargetSlot,1.f /* species maximum */,FMath::Min(CurrentWater,FMath::Min(.12f,Config.WateringRate)*Seconds),bFirstWaterPulse);
            CurrentWater=FMath::Max(0.f,CurrentWater-Used);if(Used>0)bFirstWaterPulse=false;
            // Finish on the accepted transfer, before natural drying can lower the plant again next tick.
            const float RemainingMoisture=FMath::Max(0.f,N.Target-N.Current-Used/N.UnitsPerMoisture);
            if(Used>0 && (RemainingMoisture<=KINDA_SMALL_NUMBER || CurrentWater<=KINDA_SMALL_NUMBER))
                FinishWatering(CurrentWater<=KINDA_SMALL_NUMBER);
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
            else if(Battery<=Config.BatteryReturnThreshold)ReturnToBase(true);
            else if(AI && AI->GetMoveStatus()==EPathFollowingStatus::Idle)
            {
                WaterTarget=N.SoilPoint;
                if(FVector::Dist2D(GetActorLocation(),Destination)<35 && HasWaterLine(N.SoilPoint))BeginWaterPreparation();
                else FailMove();
            }
        }
        else if(AI && AI->GetMoveStatus()==EPathFollowingStatus::Idle && !IsDocked())
        {
            if(FVector::Dist2D(GetActorLocation(),Destination)<25){GetCharacterMovement()->StopMovementImmediately();SetActivity(EBotanicusRobotActivity::StoppingAtDock,StopRunDuration());}
            else FailMove();
        }
        if(FVector::DistSquared(GetActorLocation(),LastProgressLocation)>FMath::Square(8.f)){LastProgress=Now;LastProgressLocation=GetActorLocation();}
        if((State==EBotanicusGardeningState::MovingToPlant || State==EBotanicusGardeningState::ReturningToBase) && (Now-LastProgress>5 || Now-MoveStarted>60))
        {
            if(++MoveFailures<=2 && AI && Navigate(Destination,State)){}else FailMove();
        }
    }
    else if(State==EBotanicusGardeningState::Blocked && Now>=NextSearch)ReturnToBase(Battery<=Config.BatteryReturnThreshold);
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
        !bAwaitFullCharge && !bTaskPowerLimited && State!=EBotanicusGardeningState::Charging && CanAfford(0,0);
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
