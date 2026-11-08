#include "Environment/BotanicusClandestineDoor.h"
#include "Environment/BotanicusAmbienceDirector.h"
#include "Audio/BotanicusAudioWorldSubsystem.h"
#include "BotanicusCharacter.h"
#include "BotanicusGameState.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NavAreas/NavArea_Null.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace {float Ease(float T){T=FMath::Clamp(T,0.f,1.f);return T*T*(3.f-2.f*T);}}
ABotanicusClandestineDoor::ABotanicusClandestineDoor()
{
    bReplicates=true;bAlwaysRelevant=true;SetReplicateMovement(false);SetNetUpdateFrequency(5.f);
    PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.bStartWithTickEnabled=false;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("DoorRoot")));
    PassageBlocker=CreateDefaultSubobject<UBoxComponent>(TEXT("AuthoritativePassage"));PassageBlocker->SetupAttachment(RootComponent);
    PassageBlocker->SetCollisionProfileName(TEXT("BlockAll"));PassageBlocker->SetGenerateOverlapEvents(false);
    PassageBlocker->SetCanEverAffectNavigation(false);
    // Wider than a Recast cell and reaching below the floor; the thin physical panel is unsuitable as a rasterized nav modifier.
    NavigationObstacle=CreateDefaultSubobject<UBoxComponent>(TEXT("NavigationObstacle"));NavigationObstacle->SetupAttachment(RootComponent);
    NavigationObstacle->SetCollisionEnabled(ECollisionEnabled::NoCollision);NavigationObstacle->SetGenerateOverlapEvents(false);
    NavigationObstacle->bDynamicObstacle=true;NavigationObstacle->SetAreaClassOverride(UNavArea_Null::StaticClass());
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));SectionMesh=Cube.Object;
    auto Make=[&](FName Name,bool bShadow)
    {
        auto* C=CreateDefaultSubobject<UStaticMeshComponent>(Name);C->SetupAttachment(RootComponent);C->SetStaticMesh(Cube.Object);
        C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCanEverAffectNavigation(false);C->SetCastShadow(bShadow);return C;
    };
    TopInfill=Make(TEXT("FixedTopInfill"),true);
    for(int32 I=0;I<12;++I)Sections.Add(Make(*FString::Printf(TEXT("Section_%02d"),I),true));
    for(int32 I=0;I<13;++I)Rails.Add(Make(*FString::Printf(TEXT("Track_%02d"),I),false));
    for(int32 I=0;I<2;++I)NeonStrips.Add(Make(*FString::Printf(TEXT("LavenderStrip_%02d"),I),false));
}
void ABotanicusClandestineDoor::OnConstruction(const FTransform& Transform){Super::OnConstruction(Transform);BuildGeometry();ApplyPanels();ApplyAccess();}
float ABotanicusClandestineDoor::MovingHeight() const {return FMath::Clamp(Height-TopInfillHeight,220.f,Height);}
float ABotanicusClandestineDoor::OpenTravel() const {return MovingHeight()+FMath::Max(25.f,BendRadius)*HALF_PI;}
FVector ABotanicusClandestineDoor::TrackPoint(float Distance,float& Pitch) const
{
    const float H=MovingHeight(),R=FMath::Max(25.f,BendRadius);Pitch=0;
    if(Distance<=H)return FVector(0,0,Distance);
    if(Distance<H+R*HALF_PI)
    {
        const float A=(Distance-H)/R;Pitch=-FMath::RadiansToDegrees(A);return FVector(R*(1-FMath::Cos(A)),0,H+R*FMath::Sin(A));
    }
    Pitch=-90;return FVector(R+Distance-H-R*HALF_PI,0,H+R);
}
void ABotanicusClandestineDoor::BuildGeometry()
{
    Width=FMath::Max(100.f,Width);Height=FMath::Max(220.f,Height);SectionCount=FMath::Clamp(SectionCount,3,12);Thickness=FMath::Clamp(Thickness,2.f,15.f);
    TopInfillHeight=FMath::Clamp(TopInfillHeight,0.f,Height-220.f);
    const float H=MovingHeight();
    TopInfill->SetVisibility(TopInfillHeight>0);
    TopInfill->SetRelativeLocation(FVector(0,0,H+TopInfillHeight*.5f));
    TopInfill->SetRelativeScale3D(FVector(Thickness/100,Width/100,TopInfillHeight/100));
    if(PanelMaterial)TopInfill->SetMaterial(0,PanelMaterial);
    TopInfill->SetCollisionProfileName(TEXT("BlockAll"));
    TopInfill->SetCollisionEnabled(TopInfillHeight>0 ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    PassageBlocker->SetRelativeLocation(FVector(0,0,Height*.5f));PassageBlocker->SetBoxExtent(FVector(Thickness*.5f,Width*.5f,Height*.5f));
    NavigationObstacle->SetRelativeLocation(FVector(0,0,Height*.5f));NavigationObstacle->SetBoxExtent(FVector(50,Width*.5f+5,Height*.5f+60));
    for(int32 I=0;I<Sections.Num();++I)
    {
        auto* C=Sections[I].Get();C->SetVisibility(I<SectionCount);C->SetStaticMesh(SectionMesh);if(PanelMaterial)C->SetMaterial(0,PanelMaterial);
    }
    auto SetBar=[&](int32 I,FVector A,FVector B,float Gauge)
    {
        auto* C=Rails[I].Get();const FVector D=B-A;C->SetRelativeLocation((A+B)*.5f);C->SetRelativeRotation(FRotationMatrix::MakeFromZ(D.GetSafeNormal()).Rotator());
        C->SetRelativeScale3D(FVector(Gauge/100,Gauge/100,D.Size()/100));
    };
    for(int32 Side=0;Side<2;++Side)
    {
        const float Y=(Side ? 1.f:-1.f)*(Width*.5f+3.f);const int32 Base=Side*6;
        SetBar(Base,FVector(0,Y,0),FVector(0,Y,H),5);
        for(int32 J=0;J<4;++J)
        {
            float P;FVector A=TrackPoint(H+BendRadius*HALF_PI*J/4,P),B=TrackPoint(H+BendRadius*HALF_PI*(J+1)/4,P);A.Y=B.Y=Y;SetBar(Base+1+J,A,B,5);
        }
        SetBar(Base+5,FVector(BendRadius,Y,H+BendRadius),FVector(BendRadius+H+10,Y,H+BendRadius),5);
        NeonStrips[Side]->SetRelativeLocation(FVector(-15,Y+(Side ? -22:22),Height*.64f));NeonStrips[Side]->SetRelativeScale3D(FVector(.025,.025,.9));
        if(NeonMaterial)NeonStrips[Side]->SetMaterial(0,NeonMaterial);
    }
    SetBar(12,FVector(0,-Width*.5f-6,H+5),FVector(0,Width*.5f+6,H+5),10);
}
double ABotanicusClandestineDoor::ServerNow() const
{
    const auto* GS=GetWorld() ? GetWorld()->GetGameState<ABotanicusGameState>() : nullptr;
    return GS ? GS->GetServerWorldTimeSeconds() : GetWorld() ? GetWorld()->GetTimeSeconds() : 0.;
}
float ABotanicusClandestineDoor::GetOpenFraction() const
{
    if(Presentation.MotionDuration<=0)return Presentation.bTargetOpen ? 1.f:0.f;
    float T=FMath::Clamp(float((ServerNow()-Presentation.MotionStart)/Presentation.MotionDuration),0.f,1.f);
    T=T<=0 ? 0.f:T>=1 ? 1.f:MotionCurve ? FMath::Clamp(MotionCurve->GetFloatValue(T),0.f,1.f) : Ease(T);
    return FMath::Lerp(Presentation.From,Presentation.bTargetOpen ? 1.f:0.f,T);
}
float ABotanicusClandestineDoor::GetPublicLightWeight() const
{
    const float T=Presentation.PublicDuration>0 ? float((ServerNow()-Presentation.PublicStart)/Presentation.PublicDuration) : 1.f;
    return FMath::Lerp(Presentation.PublicFrom,Presentation.PublicTo,Ease(T));
}
float ABotanicusClandestineDoor::GetNeonWeight() const
{
    if(Presentation.NeonStart<0)return 0;
    const double Now=ServerNow();
    if(Presentation.NeonOff>=0)return Presentation.NeonAtOff*(1-Ease(float((Now-Presentation.NeonOff)/FMath::Max(.1f,NeonFadeSeconds))));
    if(Presentation.bNeonInitiallyStable)return 1.f;
    const float T=float((Now-Presentation.NeonStart)/FMath::Max(.2f,NeonIgnitionSeconds));if(T<=0)return 0;if(T>=1)return 1;
    const float Base=FMath::Lerp(.12f,1.f,Ease(T));
    const float Dip=bCosmeticEffects ? FMath::Clamp(FlickerDepth,0.f,.5f)*FMath::Square(FMath::Sin(PI*FMath::Clamp(GentleFlickers,0,3)*T)) : 0.f;
    return Base*(1-Dip);
}
void ABotanicusClandestineDoor::ApplyPanels()
{
    if(GetWorld() && GetWorld()->IsGameWorld() && GetNetMode()==NM_DedicatedServer)return;
    const float PanelHeight=MovingHeight()/FMath::Clamp(SectionCount,3,12);const float Offset=GetOpenFraction()*OpenTravel();
    for(int32 I=0;I<SectionCount;++I)
    {
        float Pitch;const FVector Center=TrackPoint(Offset+(I+.5f)*PanelHeight,Pitch);const FRotator Rotation(Pitch,0,0);
        const UStaticMesh* Mesh=Sections[I]->GetStaticMesh();if(!Mesh)continue;const FBoxSphereBounds Bounds=Mesh->GetBounds();
        const FVector Size=Bounds.BoxExtent*2;const FVector Scale(Thickness/FMath::Max(1.f,Size.X),Width/FMath::Max(1.f,Size.Y),(PanelHeight-.35f)/FMath::Max(1.f,Size.Z));
        Sections[I]->SetRelativeScale3D(Scale);Sections[I]->SetRelativeRotation(Rotation);Sections[I]->SetRelativeLocation(Center-Rotation.RotateVector(Bounds.Origin*Scale));
    }
}
float ABotanicusClandestineDoor::MinimumSectionHeight() const
{
    float Pitch;const float PH=MovingHeight()/FMath::Clamp(SectionCount,3,12);const FVector P=TrackPoint(GetOpenFraction()*OpenTravel()+PH*.5f,Pitch);const float A=FMath::DegreesToRadians(Pitch);
    return P.Z-FMath::Abs(FMath::Cos(A))*PH*.5f-FMath::Abs(FMath::Sin(A))*Thickness*.5f;
}
void ABotanicusClandestineDoor::BeginPlay()
{
    Super::BeginPlay();BuildGeometry();
    if(GetNetMode()!=NM_DedicatedServer && NeonMaterial)
    {
        NeonMID=UMaterialInstanceDynamic::Create(NeonMaterial,this);for(UStaticMeshComponent* C:NeonStrips)C->SetMaterial(0,NeonMID);
    }
    ApplyAccess();ApplyPanels();UpdateLocalEffects();
    if(HasAuthority())GetWorldTimerManager().SetTimer(AuthorityTimer,this,&ThisClass::RefreshAuthority,.1f,true);
}
bool ABotanicusClandestineDoor::HasPawnInBox(const FBox& Box,bool bPlayersOnly) const
{
    TArray<FOverlapResult> Hits;FCollisionObjectQueryParams Objects(ECC_Pawn);FCollisionQueryParams Query(SCENE_QUERY_STAT(DoorSafety),false,this);
    GetWorld()->OverlapMultiByObjectType(Hits,GetActorTransform().TransformPosition(Box.GetCenter()),GetActorQuat(),Objects,FCollisionShape::MakeBox(Box.GetExtent()*GetActorScale3D().GetAbs()),Query);
    for(const auto& Hit:Hits)
    {
        const APawn* Pawn=Cast<APawn>(Hit.GetActor());
        if(!Pawn || (bPlayersOnly && !Pawn->IsA<ABotanicusCharacter>()))continue;
        // Cosmetic body/hand collision must not grant emergency access through a closed door.
        if(const ACharacter* Character=Cast<ACharacter>(Pawn); Character && Hit.GetComponent()!=Character->GetCapsuleComponent())continue;
        // The portal overlap protects straddling characters. Rear-room escape is only for a player whose capsule centre is actually inside.
        if(bPlayersOnly && !Box.IsInsideOrOn(GetActorTransform().InverseTransformPosition(Pawn->GetActorLocation())))continue;
        return true;
    }
    return false;
}
bool ABotanicusClandestineDoor::PortalOccupied() const{return HasPawnInBox(FBox(FVector(-85,-Width*.5f-35,-10),FVector(85,Width*.5f+35,Height+50)),false);}
bool ABotanicusClandestineDoor::RearOccupied() const{for(const FBox& B:RearSafetyBoxes)if(B.IsValid && HasPawnInBox(B,true))return true;return false;}
void ABotanicusClandestineDoor::ApplyAccess()
{
    // Reassert the authoritative physical contract even if an inherited Blueprint/instance had different responses.
    SetActorEnableCollision(true);
    PassageBlocker->SetCollisionProfileName(TEXT("BlockAll"));
    PassageBlocker->SetCollisionObjectType(ECC_WorldStatic);
    PassageBlocker->SetCollisionResponseToChannel(ECC_Pawn,ECR_Block);
    PassageBlocker->SetCollisionEnabled(Presentation.bPassageBlocked ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    NavigationObstacle->SetCanEverAffectNavigation(HasAuthority() && Presentation.bPassageBlocked);
}
void ABotanicusClandestineDoor::SetBlocked(bool bBlocked)
{
    if(Presentation.bPassageBlocked==bBlocked)return;Presentation.bPassageBlocked=bBlocked;ApplyAccess();Publish();
}
void ABotanicusClandestineDoor::Publish(){++Presentation.Revision;ForceNetUpdate();OnRep_Presentation();}
void ABotanicusClandestineDoor::BeginMotion(bool bOpen)
{
    const float From=GetOpenFraction();if(Presentation.bTargetOpen==bOpen && Presentation.MotionDuration>0)return;
    Presentation.From=From;Presentation.bTargetOpen=bOpen;Presentation.MotionStart=ServerNow();
    Presentation.MotionDuration=FMath::Max(.25f,bOpen ? OpenSeconds:CloseSeconds)*FMath::Max(.02f,FMath::Abs((bOpen ? 1.f:0.f)-From));
    if(!bOpen)Presentation.bPassageBlocked=true;
    // Opening never disables the blocker early, including an occupied/reversed motion.
    ApplyAccess();Publish();SetActorTickEnabled(true);
}
void ABotanicusClandestineDoor::RefreshAuthority()
{
    if(!HasAuthority())return;const auto* GS=GetWorld()->GetGameState<ABotanicusGameState>();if(!GS)return;
    const double Now=ServerNow();const bool bNight=GS->IsIllegalTradeActive();const float Public=GS->IsMainShopOpen() ? 1.f:0.f;
    if(!Presentation.bReady)
    {
        Presentation.bReady=true;Presentation.bTargetOpen=bNight || RearOccupied();Presentation.From=Presentation.bTargetOpen ? 1.f:0.f;
        Presentation.bPassageBlocked=!Presentation.bTargetOpen;Presentation.bWaitingForExit=!bNight && Presentation.bTargetOpen;
        Presentation.MotionStart=Now;Presentation.PublicFrom=Presentation.PublicTo=Public;Presentation.PublicStart=Now;
        Presentation.NeonStart=bNight ? 0. : -1.;Presentation.bNeonInitiallyStable=bNight;
        ApplyAccess();Publish();return;
    }
    if(Presentation.PublicTo!=Public)
    {
        Presentation.PublicFrom=GetPublicLightWeight();Presentation.PublicTo=Public;Presentation.PublicStart=Now;Presentation.PublicDuration=Ambience ? Ambience->ShopLightTransitionSeconds : 3.f;Publish();
    }
    if(!bNight && Presentation.NeonStart>=0 && Presentation.NeonOff<0)
    {
        Presentation.NeonAtOff=GetNeonWeight();Presentation.NeonOff=Now;Publish();
    }
    if(bNight)
    {
        if(Presentation.bWaitingForExit){Presentation.bWaitingForExit=false;Publish();}
        if(!Presentation.bTargetOpen)BeginMotion(true);
        else if(Presentation.MotionDuration<=0 && (Presentation.NeonStart<0 || Presentation.NeonOff>=0))
        {Presentation.NeonStart=Now+FMath::Max(0.f,NeonDelay);Presentation.bNeonInitiallyStable=false;Presentation.NeonOff=-1;Publish();}
        return;
    }
    const bool bInside=RearOccupied();const bool bCanClose=!bInside && !PortalOccupied();
    const bool bClosed=!Presentation.bTargetOpen && Presentation.MotionDuration<=0;
    if(bClosed)
    {
        if(bInside){Presentation.bWaitingForExit=true;BeginMotion(true);}return;
    }
    if(!bCanClose)
    {
        if(!Presentation.bWaitingForExit){Presentation.bWaitingForExit=true;Publish();}
        if(!Presentation.bTargetOpen)BeginMotion(true);
    }
    else
    {
        if(Presentation.bWaitingForExit){Presentation.bWaitingForExit=false;Publish();}
        if(Presentation.bTargetOpen && GetNeonWeight()<=.005f)BeginMotion(false);
    }
}
UAudioComponent* ABotanicusClandestineDoor::Sound(USoundBase* Asset,float Volume,float StartAt)
{
    if(!bCosmeticEffects || !Asset || GetNetMode()==NM_DedicatedServer)return nullptr;
    if(auto* Audio=GetWorld()->GetSubsystem<UBotanicusAudioWorldSubsystem>())return Audio->PlaySpatialTransition(this,Asset,GetActorTransform().TransformPosition(FVector(0,0,Height*.6f)),Volume,StartAt);
    return nullptr;
}
void ABotanicusClandestineDoor::OnRep_Presentation()
{
    ApplyAccess();ApplyPanels();
    if(!bHadSnapshot)
    {bHadSnapshot=Presentation.bReady;HeardMotion=Presentation.MotionStart;HeardNeon=Presentation.NeonStart;}
    else if(HeardMotion!=Presentation.MotionStart)
    {
        if(MotorVoice.IsValid()){MotorVoice->FadeOut(.12f,0);MotorVoice.Reset();}
        // A received timestamp can be slightly ahead of the client's clock. Leave it pending until local synchronized time reaches it.
    }
    if(Presentation.NeonOff>=0 && NeonVoice.IsValid()){NeonVoice->FadeOut(.15f,0);NeonVoice.Reset();}
    UpdateLocalEffects();SetActorTickEnabled(true);
}
void ABotanicusClandestineDoor::UpdateLocalEffects()
{
    if(GetNetMode()==NM_DedicatedServer)return;
    if(NeonMID){NeonMID->SetVectorParameterValue(TEXT("Tint"),NeonColor);NeonMID->SetScalarParameterValue(TEXT("Strength"),GetNeonWeight()*FMath::Max(0.f,EmissiveStrength));}
    const double Now=ServerNow();
    if(bHadSnapshot && HeardMotion!=Presentation.MotionStart && Now>=Presentation.MotionStart)
    {
        HeardMotion=Presentation.MotionStart;
        const double Elapsed=Now-Presentation.MotionStart;
        if(Presentation.MotionDuration>0 && Elapsed<Presentation.MotionDuration)
        {
            PendingContactEnd=Presentation.MotionStart+Presentation.MotionDuration;
            MotorVoice=Sound(Presentation.bTargetOpen ? OpenSound:CloseSound,MotorVolume,float(Elapsed));
        }
        else PendingContactEnd=-1.;
    }
    if(Presentation.NeonStart>=0 && Presentation.NeonOff<0 && HeardNeon!=Presentation.NeonStart && Now>=Presentation.NeonStart)
    {
        HeardNeon=Presentation.NeonStart;
        if(Now-Presentation.NeonStart<NeonIgnitionSeconds)NeonVoice=Sound(NeonSound,NeonVolume,NeonSoundOffset+float(Now-Presentation.NeonStart));
    }
    if(NeonVoice.IsValid() && (Presentation.NeonOff>=0 || Now>Presentation.NeonStart+NeonIgnitionSeconds)){NeonVoice->FadeOut(.15f,0);NeonVoice.Reset();}
}
void ABotanicusClandestineDoor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);const double Now=ServerNow();
    if(HasAuthority() && Presentation.MotionDuration>0)
    {
        if(!Presentation.bTargetOpen && (RearOccupied() || PortalOccupied())){Presentation.bWaitingForExit=true;BeginMotion(true);}
        if(Now>=Presentation.MotionStart+Presentation.MotionDuration)
        {
            const double End=Presentation.MotionStart+Presentation.MotionDuration;Presentation.MotionDuration=0;Presentation.From=Presentation.bTargetOpen ? 1.f:0.f;
            if(Presentation.bTargetOpen)
            {
                SetBlocked(MinimumSectionHeight()<FMath::Max(200.f,PawnClearance));const auto* GS=GetWorld()->GetGameState<ABotanicusGameState>();
                if(GS && GS->IsIllegalTradeActive()){Presentation.NeonStart=End+FMath::Max(0.f,NeonDelay);Presentation.bNeonInitiallyStable=false;Presentation.NeonOff=-1;}
            }
            Publish();if(MotorVoice.IsValid()){MotorVoice->FadeOut(.12f,0);MotorVoice.Reset();}
        }
    }
    if(Presentation.MotionDuration>0)ApplyPanels();UpdateLocalEffects();
    const bool Motion=Presentation.MotionDuration>0 && Now<Presentation.MotionStart+Presentation.MotionDuration;
    const bool Neon=(Presentation.NeonOff>=0 && Now<Presentation.NeonOff+NeonFadeSeconds) || (Presentation.NeonOff<0 && !Presentation.bNeonInitiallyStable && Presentation.NeonStart>=0 && Now<Presentation.NeonStart+NeonIgnitionSeconds);
    if(!Motion && PendingContactEnd>=0 && Now>=PendingContactEnd){PendingContactEnd=-1.;Sound(EndContactSound,MotorVolume*.5f);}
    if(!Motion && (!Neon || GetNetMode()==NM_DedicatedServer))SetActorTickEnabled(false);
    // Clients stop their local motor at the synchronized end even before the final snapshot.
    if(!Motion && MotorVoice.IsValid()){MotorVoice->FadeOut(.12f,0);MotorVoice.Reset();}
}
void ABotanicusClandestineDoor::EndPlay(const EEndPlayReason::Type Reason)
{
    GetWorldTimerManager().ClearTimer(AuthorityTimer);if(MotorVoice.IsValid())MotorVoice->Stop();if(NeonVoice.IsValid())NeonVoice->Stop();Super::EndPlay(Reason);
}
void ABotanicusClandestineDoor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const{Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(ABotanicusClandestineDoor,Presentation);}
