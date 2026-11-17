#include "Gardening/BotanicusGardeningRobotBase.h"
#include "Gardening/BotanicusGardeningRobot.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "BotanicusGameMode.h"
#include "Gardening/BotanicusRobotBatteryWidget.h"
#include "Components/WidgetComponent.h"
#include "Components/BoxComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ABotanicusGardeningRobotBase::ABotanicusGardeningRobotBase()
{
    bUseBlueprintAppearance=true;
    BaseVisualPivot=CreateDefaultSubobject<USceneComponent>(TEXT("BaseVisualPivot"));BaseVisualPivot->SetupAttachment(SceneRoot);BaseVisualPivot->bEditableWhenInherited=true;
    Mesh->SetupAttachment(BaseVisualPivot);Mesh->bEditableWhenInherited=true;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> StationModel(TEXT("/Game/Botanicus/Items/robot_aroseur/Base/base.base"));
    if(StationModel.Succeeded())Mesh->SetStaticMesh(StationModel.Object);
    const FTransform StationTransform(FRotator(0,-90,0),FVector(0,0,-5.46972f),FVector(.11f));
    // Visual transform is authored in the Blueprint, never reapplied here or during play.
    // The imported single hull fills the C-shaped opening. Keep physics on three fitted primitives instead.
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);Mesh->SetGenerateOverlapEvents(false);
    StationCollisionRoot=CreateDefaultSubobject<USceneComponent>(TEXT("StationCollisionRoot"));StationCollisionRoot->SetupAttachment(SceneRoot);StationCollisionRoot->SetRelativeTransform(StationTransform);
    auto Collision=[&](const TCHAR* Name,FVector Position,FVector Extent)
    {
        auto* Part=CreateDefaultSubobject<UBoxComponent>(Name);Part->SetupAttachment(StationCollisionRoot);Part->SetRelativeLocation(Position);Part->SetBoxExtent(Extent);
        Part->SetCollisionProfileName(TEXT("BlockAllDynamic"));Part->SetGenerateOverlapEvents(false);Part->SetCanEverAffectNavigation(false);return Part;
    };
    StationBackCollision=Collision(TEXT("StationBackCollision"),FVector(1.36,-585,345),FVector(120,37,350));
    StationRoofCollision=Collision(TEXT("StationRoofCollision"),FVector(1.36,-472,673),FVector(120,150,22));
    StationFootCollision=Collision(TEXT("StationFootCollision"),FVector(1.36,-472,7),FVector(120,150,12));
    // Existing interaction validation traces to the actor origin, which is the preserved dock reference.
    // A query-only target keeps this reference usable without any invisible Pawn/physics obstruction.
    StationInteractionTarget=CreateDefaultSubobject<UBoxComponent>(TEXT("StationInteractionTarget"));StationInteractionTarget->SetupAttachment(SceneRoot);
    StationInteractionTarget->SetBoxExtent(FVector(8,8,5));StationInteractionTarget->SetCollisionEnabled(ECollisionEnabled::QueryOnly);StationInteractionTarget->SetCollisionResponseToAllChannels(ECR_Ignore);
    StationInteractionTarget->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);StationInteractionTarget->SetGenerateOverlapEvents(false);StationInteractionTarget->SetCanEverAffectNavigation(false);
    Mesh->SetCanEverAffectNavigation(false);
    DockPoint=CreateDefaultSubobject<USceneComponent>(TEXT("DockPoint"));
    DockPoint->SetupAttachment(SceneRoot); DockPoint->SetRelativeLocation(FVector(0,0,6));
    ApproachPoint=CreateDefaultSubobject<USceneComponent>(TEXT("ApproachPoint"));ApproachPoint->SetupAttachment(SceneRoot);ApproachPoint->bEditableWhenInherited=true;
    Indicator=CreateDefaultSubobject<UPointLightComponent>(TEXT("ChargeIndicator"));
    Indicator->SetupAttachment(SceneRoot);Indicator->SetRelativeLocation(FVector(0,0,10));
    // Retain the inherited component identity for existing Blueprints, but render no point light.
    Indicator->SetIntensity(0);Indicator->SetVisibility(false);Indicator->SetCastShadows(false);
    BatteryDisplay=CreateDefaultSubobject<UWidgetComponent>(TEXT("BatteryDisplay"));BatteryDisplay->SetupAttachment(SceneRoot);
    BatteryDisplay->SetWidgetSpace(EWidgetSpace::World);BatteryDisplay->SetWidgetClass(UBotanicusRobotBatteryWidget::StaticClass());
    BatteryDisplay->SetDrawSize(FVector2D(256,64));BatteryDisplay->SetTwoSided(true);BatteryDisplay->SetManuallyRedraw(true);
    BatteryDisplay->bEditableWhenInherited=true;
    BatteryDisplay->SetCollisionEnabled(ECollisionEnabled::NoCollision);BatteryDisplay->SetCanEverAffectNavigation(false);
    BatteryDisplay->SetCastShadow(false);BatteryDisplay->SetWindowFocusable(false);
    PowerLED=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PowerLED"));PowerLED->SetupAttachment(SceneRoot);PowerLED->bEditableWhenInherited=true;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> LEDShape(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> LEDBaseMaterial(TEXT("/Game/Botanicus/Materials/Robot/M_RobotStationLED.M_RobotStationLED"));
    if(LEDShape.Succeeded())PowerLED->SetStaticMesh(LEDShape.Object);if(LEDBaseMaterial.Succeeded())PowerLED->SetMaterial(0,LEDBaseMaterial.Object);
    PowerLED->SetRelativeScale3D(FVector(.035f));PowerLED->SetCollisionEnabled(ECollisionEnabled::NoCollision);PowerLED->SetCanEverAffectNavigation(false);PowerLED->SetCastShadow(false);
    RobotClass=ABotanicusGardeningRobot::StaticClass();
}
void ABotanicusGardeningRobotBase::BeginPlay()
{
    Super::BeginPlay();SetupIndicators();UpdateIndicator();
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
FVector ABotanicusGardeningRobotBase::DockForward() const {return DockPoint->GetForwardVector().GetSafeNormal2D();}
FVector ABotanicusGardeningRobotBase::ApproachLocation() const {return ApproachPoint->GetComponentLocation();}
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
    UClass* Class=Saved.RobotClass.IsValid()?Saved.RobotClass.TryLoadClass<ABotanicusGardeningRobot>():RobotClass.Get();
    if(!Class || !Class->IsChildOf(ABotanicusGardeningRobot::StaticClass()))return;
    if(!Saved.RobotId.IsValid())
    {
        Saved.RobotId=FGuid::NewGuid();Saved.Water=Config.WaterCapacity;
        const float HalfHeight=Class->GetDefaultObject<ABotanicusGardeningRobot>()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        Saved.RobotTransform=FTransform(GetActorRotation(),DockLocation()+FVector(0,0,HalfHeight));
    }
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
    if(GetNetMode()!=NM_DedicatedServer)
    {
        const bool bAvailable=IsValid(Robot) && IsStationAvailable();
        BatteryDisplay->SetVisibility(bAvailable);PowerLED->SetVisibility(bAvailable);
        if(LEDMaterial)LEDMaterial->SetScalarParameterValue(TEXT("LEDIntensity"),bAvailable?LEDIntensity:0);
        if(bAvailable)
        {
            if(auto* Display=Cast<UBotanicusRobotBatteryWidget>(BatteryDisplay->GetUserWidgetObject()))
            {Display->SetBattery(Robot->Battery,true);BatteryDisplay->RequestRedraw();}
            const bool bReady=Robot->HasPowerForTask() && !(Robot->IsDocked() && Robot->Battery<1-KINDA_SMALL_NUMBER);
            if(LEDMaterial && (!bHasLEDState || bReady!=bLastPowerReady))
            {LEDMaterial->SetVectorParameterValue(TEXT("LEDColor"),bReady?FullColor:ChargingColor);bLastPowerReady=bReady;bHasLEDState=true;}
        }
    }
    SetInteractionIndicatorVisibility(false);
}
FBotanicusInteractionPrompt ABotanicusGardeningRobotBase::GetInteractionPrompt_Implementation(AActor* Interactor) const
{
    if(!Robot)return Super::GetInteractionPrompt_Implementation(Interactor);
    FBotanicusInteractionPrompt P;P.ActionText=Robot->bEnabled?NSLOCTEXT("Robot","LaunchRound","Lancer une ronde"):NSLOCTEXT("Robot","Enable","Activer le robot");
    const float Remaining=(1-Robot->Battery)*Config.BatteryRechargeHours*60;
    P.TargetName=FText::Format(NSLOCTEXT("Robot","BaseInfo","Station jardinier — batterie {0}% | eau {1}%\nRecharge restante : {2} min de jeu"),FMath::RoundToInt(Robot->Battery*100),FMath::RoundToInt(Robot->CurrentWater/FMath::Max(.01f,Config.WaterCapacity)*100),FMath::CeilToInt(Remaining));
    P.bCanInteract=CanInteract_Implementation(Interactor);return P;
}
bool ABotanicusGardeningRobotBase::CanInteract_Implementation(AActor* Interactor) const
{
    if(!Robot)return Super::CanInteract_Implementation(Interactor);
    return IsStationAvailable() && IsValid(Interactor) && (!Robot->bEnabled || Robot->CanLaunchRound()) && FVector::DistSquared(GetActorLocation(),Interactor->GetActorLocation())<FMath::Square(250.f);
}
void ABotanicusGardeningRobotBase::Interact_Implementation(AActor* Interactor)
{
    if(!HasAuthority() || !CanInteract_Implementation(Interactor))return;
    if(Robot){if(!Robot->bEnabled)Robot->SetEnabled(true);else Robot->LaunchRound();}else Super::Interact_Implementation(Interactor);
}

void ABotanicusGardeningRobotBase::OnConstruction(const FTransform& Transform)
{Super::OnConstruction(Transform);SetupIndicators();}
void ABotanicusGardeningRobotBase::SetupIndicators()
{
    Indicator->SetIntensity(0);Indicator->SetVisibility(false);
    // Authored component transform and DrawSize are deliberately untouched.
    if(GetNetMode()==NM_DedicatedServer){BatteryDisplay->SetVisibility(false);PowerLED->SetVisibility(false);return;}
    if(!LEDMaterial)LEDMaterial=PowerLED->CreateDynamicMaterialInstance(0);
    if(LEDMaterial){LEDMaterial->SetScalarParameterValue(TEXT("LEDIntensity"),LEDIntensity);bHasLEDState=false;}
    // The movable LED is independent. Neutralise only the model's old integrated indicator slot.
    const int32 Slot=Mesh->GetMaterialIndex(TEXT("indicateur_de_charge"));
    if(Slot!=INDEX_NONE && LEDMaterial)
    {
        if(!IntegratedLEDOffMaterial){IntegratedLEDOffMaterial=UMaterialInstanceDynamic::Create(LEDMaterial->Parent,this);IntegratedLEDOffMaterial->SetScalarParameterValue(TEXT("LEDIntensity"),0);}
        Mesh->SetMaterial(Slot,IntegratedLEDOffMaterial);
    }
    if(HasActorBegunPlay())BatteryDisplay->InitWidget();
}
