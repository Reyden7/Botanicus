#include "Gardening/BotanicusGardeningRobot.h"
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

ABotanicusGardeningRobot::ABotanicusGardeningRobot()
{
    bReplicates=true;SetReplicateMovement(true);SetNetUpdateFrequency(10);
    PrimaryActorTick.bCanEverTick=false;
    AutoPossessAI=EAutoPossessAI::PlacedInWorldOrSpawned;AIControllerClass=AAIController::StaticClass();
    GetCapsuleComponent()->InitCapsuleSize(22,40);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
    GetCapsuleComponent()->SetCanEverAffectNavigation(false);
    GetCharacterMovement()->MaxWalkSpeed=180;GetCharacterMovement()->bOrientRotationToMovement=true;
    GetCharacterMovement()->RotationRate=FRotator(0,200,0);GetCharacterMovement()->bUseRVOAvoidance=true;
    GetCharacterMovement()->AvoidanceConsiderationRadius=160;
    Body=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RobotBody"));Body->SetupAttachment(GetRootComponent());
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Shape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if(Shape.Succeeded())Body->SetStaticMesh(Shape.Object);
    Body->SetRelativeScale3D(FVector(.4f,.4f,.55f));Body->SetRelativeLocation(FVector(0,0,-12.5f));
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);Body->SetCanEverAffectNavigation(false);
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
void ABotanicusGardeningRobot::BeginPlay()
{
    Super::BeginPlay();GetCharacterMovement()->MaxWalkSpeed=Config.MovementSpeed;
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
    DOREPLIFETIME(ThisClass,Config);DOREPLIFETIME(ThisClass,RobotId);DOREPLIFETIME(ThisClass,Base);
    DOREPLIFETIME(ThisClass,Battery);DOREPLIFETIME(ThisClass,CurrentWater);DOREPLIFETIME(ThisClass,bEnabled);
    DOREPLIFETIME(ThisClass,State);DOREPLIFETIME(ThisClass,Target);DOREPLIFETIME(ThisClass,TargetSlot);
    DOREPLIFETIME(ThisClass,WaterTarget);DOREPLIFETIME(ThisClass,Carrier);
}
void ABotanicusGardeningRobot::InitializeRobot(ABotanicusGardeningRobotBase* B,const FBotanicusGardeningSave& S)
{
    if(!HasAuthority())return;
    Base=B;RobotId=S.RobotId.IsValid()?S.RobotId:FGuid::NewGuid();
    Battery=FMath::Clamp(S.Battery,0.f,1.f);CurrentWater=FMath::Clamp(S.Water,0.f,Config.WaterCapacity);
    bEnabled=S.bEnabled;bAwaitFullCharge=S.bAwaitFullCharge;
    // No saved target is trusted. Navigation and demand are re-evaluated after all actors restore.
    State=bEnabled?EBotanicusGardeningState::SearchingPlant:EBotanicusGardeningState::Inactive;
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
    return IsValid(Base) && Base->IsStationAvailable() && !Carrier &&
        FVector::Dist2D(GetActorLocation(),Base->DockLocation())<=Config.DockingDistance &&
        FMath::Abs(GetActorLocation().Z-40-Base->DockLocation().Z)<25;
}
void ABotanicusGardeningRobot::ReleaseTarget()
{
    if(HasAuthority())if(auto* R=GetWorld()->GetSubsystem<UBotanicusGardeningSubsystem>())R->Release(this);
    Target=nullptr;bFirstWaterPulse=true;
}
void ABotanicusGardeningRobot::SetEnabled(bool bValue)
{
    if(!HasAuthority())return;
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
bool ABotanicusGardeningRobot::Approach(ABotanicusPlantPotActor* Pot,int32 Slot,FVector& Point,float& Length) const
{
    auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());if(!Nav)return false;
    FBotanicusAutomaticWateringNeed N;if(!Pot->GetAutomaticWateringNeed(Slot,Config.TargetWaterRatio,N))return false;
    bool bFound=false;Length=TNumericLimits<float>::Max();
    // Test adjacent points, never the pot centre. Navigation must reach the complete path.
    for(int32 I=0;I<8;++I)
    {
        const float A=I*PI/4;FVector Candidate=N.SoilPoint+FVector(FMath::Cos(A),FMath::Sin(A),0)*Config.WateringDistance;
        FNavLocation Projected;if(!Nav->ProjectPointToNavigation(Candidate,Projected,FVector(45,45,160)))continue;
        if(FVector::Dist2D(Projected.Location,N.SoilPoint)<55 || FVector::Dist2D(Projected.Location,N.SoilPoint)>Config.WateringDistance+35)continue;
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
    const float Acceptance=NextState==EBotanicusGardeningState::ReturningToBase?FMath::Max(2.f,Config.DockingDistance*.5f):8.f;
    auto Result=AI->MoveToLocation(Point,Acceptance,false,true,false,false,nullptr,false);
    if(Result==EPathFollowingRequestResult::Failed)return false;
    Destination=Point;State=NextState;MoveStarted=LastProgress=GetWorld()->GetTimeSeconds();LastProgressLocation=GetActorLocation();return true;
}
void ABotanicusGardeningRobot::Search()
{
    NextSearch=GetWorld()->GetTimeSeconds()+FMath::Max(.5f,Config.PlantSearchInterval);
    for(auto I=RetryAfter.CreateIterator();I;++I)if(!I.Key().IsValid() || I.Value()<=GetWorld()->GetTimeSeconds())I.RemoveCurrent();
    if(!Base || !Base->IsStationAvailable()){State=EBotanicusGardeningState::Blocked;return;}
    if(Battery<=Config.BatteryReturnThreshold || CurrentWater<=KINDA_SMALL_NUMBER){ReturnToBase(Battery<=Config.BatteryReturnThreshold);return;}
    auto* Registry=GetWorld()->GetSubsystem<UBotanicusGardeningSubsystem>();if(!Registry)return;
    struct FCandidate {ABotanicusPlantPotActor* Pot;int32 Slot;float Deficit,Distance,Water;};
    TArray<FCandidate> Candidates;
    for(auto& Weak:Registry->GetPots())
    {
        auto* P=Weak.Get();if(!P || FVector::DistSquared(P->GetActorLocation(),Base->DockLocation())>FMath::Square(Config.PlantSearchRadius) || RetryAfter.FindRef(P)>GetWorld()->GetTimeSeconds())continue;
        for(int32 I=0;I<P->GetAutomaticWateringSlotCount();++I)
        {
            FBotanicusAutomaticWateringNeed N;
            if(!P->GetAutomaticWateringNeed(I,Config.TargetWaterRatio,N) || N.Current>=N.Minimum-KINDA_SMALL_NUMBER || !Registry->IsAvailable(P,I,this))continue;
            Candidates.Add({P,I,(N.Minimum-N.Current)/FMath::Max(.01f,N.Minimum),static_cast<float>(FVector::Dist2D(GetActorLocation(),N.SoilPoint)),N.UnitsNeeded()});
        }
    }
    Candidates.Sort([](const FCandidate& A,const FCandidate& B){return A.Deficit==B.Deficit?A.Distance<B.Distance:A.Deficit>B.Deficit;});
    ABotanicusPlantPotActor* Best=nullptr;int32 Slot=0;FVector Point;
    // Bound costly navigation work. Unreachable candidates enter cooldown, allowing later candidates next search.
    const int32 Count=FMath::Min(8,Candidates.Num());
    for(int32 I=0;I<Count;++I)
    {
        const auto& C=Candidates[I];FVector At;float Distance=0,Back=0;
        if(!Approach(C.Pot,C.Slot,At,Distance) || !Path(At,Base->DockLocation(),Back))
        {RetryAfter.Add(C.Pot,GetWorld()->GetTimeSeconds()+Config.UnreachableRetrySeconds);continue;}
        if(!CanAfford(Distance+Back,FMath::Min(CurrentWater,C.Water)))continue;
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
    if(!IsValid(Base) || !bEnabled || Battery<=0)
    {
        if(auto* AI=Cast<AAIController>(GetController()))AI->StopMovement();ReleaseTarget();
        State=Battery<=0?EBotanicusGardeningState::Blocked:EBotanicusGardeningState::Inactive;UpdateCosmetics();return;
    }
    if(!Base->IsStationAvailable()){if(auto* AI=Cast<AAIController>(GetController()))AI->StopMovement();ReleaseTarget();State=EBotanicusGardeningState::Blocked;NextSearch=Now+1;UpdateCosmetics();return;}
    if(IsDocked() && State!=EBotanicusGardeningState::MovingToPlant && State!=EBotanicusGardeningState::Watering)
    {
        if(auto* AI=Cast<AAIController>(GetController()))AI->StopMovement();MoveFailures=0;
        const FVector Dock=Base->DockLocation();
        if(FVector::Dist2D(GetActorLocation(),Dock)>.5f)SetActorLocation(FVector(Dock.X,Dock.Y,GetActorLocation().Z),true);
        SetActorRotation(FRotator(0,Base->GetActorRotation().Yaw,0));
        if(Battery<=Config.BatteryReturnThreshold)bAwaitFullCharge=true;
        if(bAwaitFullCharge)State=EBotanicusGardeningState::Charging;
        else if(CurrentWater<Config.WaterCapacity-KINDA_SMALL_NUMBER)State=EBotanicusGardeningState::RefillingWater;
        else if(Now>=NextSearch){State=EBotanicusGardeningState::SearchingPlant;Search();}
    }
    else if(State==EBotanicusGardeningState::Watering)
    {
        FBotanicusAutomaticWateringNeed N;bool bReturnPath=true;
        if(Now>=NextReturnPathCheck){NextReturnPathCheck=Now+1;bReturnPath=Path(GetActorLocation(),Base->DockLocation(),ReturnPathLength);}
        if(!IsValid(Target) || !Target->GetAutomaticWateringNeed(TargetSlot,Config.TargetWaterRatio,N) || N.UnitsNeeded()<=KINDA_SMALL_NUMBER)
        {ReleaseTarget();State=EBotanicusGardeningState::SearchingPlant;NextSearch=Now;}
        else if(!bReturnPath || !CanAfford(ReturnPathLength,0))ReturnToBase(true);
        else if(CurrentWater<=KINDA_SMALL_NUMBER)ReturnToBase();
        else if(FVector::Dist2D(GetActorLocation(),N.SoilPoint)>Config.WateringDistance+45 || !HasWaterLine(N.SoilPoint))FailMove();
        else
        {
            WaterTarget=N.SoilPoint;SetActorRotation(FRotator(0,(N.SoilPoint-GetActorLocation()).Rotation().Yaw,0));
            const float Used=Target->ApplyAutomaticWater(TargetSlot,Config.TargetWaterRatio,FMath::Min(CurrentWater,FMath::Min(.12f,Config.WateringRate)*Seconds),bFirstWaterPulse);
            CurrentWater=FMath::Max(0.f,CurrentWater-Used);if(Used>0)bFirstWaterPulse=false;
        }
    }
    else if(State==EBotanicusGardeningState::MovingToPlant || State==EBotanicusGardeningState::ReturningToBase)
    {
        auto* AI=Cast<AAIController>(GetController());
        if(State==EBotanicusGardeningState::MovingToPlant)
        {
            FBotanicusAutomaticWateringNeed N;
            if(!IsValid(Target) || !Target->GetAutomaticWateringNeed(TargetSlot,Config.TargetWaterRatio,N) || N.Current>=N.Minimum-KINDA_SMALL_NUMBER)
            {if(AI)AI->StopMovement();ReleaseTarget();State=EBotanicusGardeningState::SearchingPlant;NextSearch=Now;}
            else if(Battery<=Config.BatteryReturnThreshold)ReturnToBase(true);
            else if(AI && AI->GetMoveStatus()==EPathFollowingStatus::Idle)
            {
                WaterTarget=N.SoilPoint;
                if(FVector::Dist2D(GetActorLocation(),Destination)<35 && HasWaterLine(N.SoilPoint))State=EBotanicusGardeningState::Watering;
                else FailMove();
            }
        }
        else if(AI && AI->GetMoveStatus()==EPathFollowingStatus::Idle && !IsDocked())FailMove();
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
void ABotanicusGardeningRobot::UpdateCosmetics()
{
    if(GetNetMode()==NM_DedicatedServer)return;
    const bool bWater=State==EBotanicusGardeningState::Watering && IsValid(Target) && !Carrier && CurrentWater>0;
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
    ReleaseTarget();if(auto* AI=Cast<AAIController>(GetController()))AI->StopMovement();
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
    if(bDockDrop)SetActorRotation(FRotator(0,Base->GetActorRotation().Yaw,0));ForceNetUpdate();
    if(auto* GM=GetWorld()->GetAuthGameMode<ABotanicusGameMode>())GM->ScheduleInventoryAutosave();return true;
}
