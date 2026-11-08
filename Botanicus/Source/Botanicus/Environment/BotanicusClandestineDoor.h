#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BotanicusClandestineDoor.generated.h"
class UBoxComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UCurveFloat;
class USoundBase;
class UAudioComponent;
class ABotanicusAmbienceDirector;

/** One atomic snapshot. Times use GameState's synchronized server time, not another day clock. */
USTRUCT()
struct FBotanicusDoorPresentation
{
    GENERATED_BODY()
    UPROPERTY() bool bReady=false;
    UPROPERTY() bool bTargetOpen=false;
    UPROPERTY() bool bPassageBlocked=true;
    UPROPERTY() bool bWaitingForExit=false;
    UPROPERTY() float From=0.f;
    UPROPERTY() double MotionStart=0.;
    UPROPERTY() float MotionDuration=0.f;
    UPROPERTY() double NeonStart=-1.;
    UPROPERTY() bool bNeonInitiallyStable=false;
    UPROPERTY() double NeonOff=-1.;
    UPROPERTY() float NeonAtOff=0.f;
    UPROPERTY() float PublicFrom=0.f;
    UPROPERTY() float PublicTo=0.f;
    UPROPERTY() double PublicStart=0.;
    UPROPERTY() float PublicDuration=3.f;
    UPROPERTY() uint32 Revision=0;
};

/** Server-authoritative sectional door; panel transforms and gentle lighting are local presentation. */
UCLASS(Blueprintable)
class BOTANICUS_API ABotanicusClandestineDoor : public AActor
{
    GENERATED_BODY()
public:
    ABotanicusClandestineDoor();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Door|Dimensions",meta=(ClampMin="100")) float Width=400.f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Door|Dimensions",meta=(ClampMin="220")) float Height=300.f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Door|Dimensions",meta=(ClampMin="3",ClampMax="12")) int32 SectionCount=8;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Door|Dimensions",meta=(ClampMin="2",ClampMax="15")) float Thickness=6.f;
    /** Fixed upper infill provides headroom for the curved track under low ceilings. */
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Door|Track",meta=(ClampMin="0")) float TopInfillHeight=0.f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Door|Track",meta=(ClampMin="25",ClampMax="150")) float BendRadius=55.f;
    /** Local +X is the side used for overhead retraction. Keep the actor upright. */
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Door|Track") TObjectPtr<UCurveFloat> MotionCurve;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Door|Motion",meta=(ClampMin=".25")) float OpenSeconds=3.f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Door|Motion",meta=(ClampMin=".25")) float CloseSeconds=3.f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Door|Meshes") TObjectPtr<UStaticMesh> SectionMesh;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Door|Meshes") TObjectPtr<UMaterialInterface> PanelMaterial;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Door|Meshes") TObjectPtr<UMaterialInterface> NeonMaterial;
    /** Local-space boxes covering the actual rear rooms, excluding the public preparation room. */
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Door|Safety") TArray<FBox> RearSafetyBoxes;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Door|Safety",meta=(ClampMin="200")) float PawnClearance=220.f;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Door|Lighting") TObjectPtr<ABotanicusAmbienceDirector> Ambience;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Door|Neon",meta=(ClampMin="0",ClampMax="5")) float NeonDelay=1.75f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Door|Neon",meta=(ClampMin=".2",ClampMax="4")) float NeonIgnitionSeconds=1.4f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Door|Neon",meta=(ClampMin=".1",ClampMax="4")) float NeonFadeSeconds=1.2f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Door|Neon",meta=(ClampMin="0",ClampMax="3")) int32 GentleFlickers=3;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Door|Neon",meta=(ClampMin="0",ClampMax=".5")) float FlickerDepth=.32f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Door|Neon") FLinearColor NeonColor=FLinearColor(.36f,.42f,.78f);
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Door|Neon",meta=(ClampMin="0",ClampMax="10")) float EmissiveStrength=2.5f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Door|Cosmetics") bool bCosmeticEffects=true;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Door|Audio") TObjectPtr<USoundBase> OpenSound;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Door|Audio") TObjectPtr<USoundBase> CloseSound;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Door|Audio") TObjectPtr<USoundBase> NeonSound;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Door|Audio") TObjectPtr<USoundBase> EndContactSound;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Door|Audio",meta=(ClampMin="0",ClampMax="1")) float MotorVolume=.25f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Door|Audio",meta=(ClampMin="0",ClampMax="1")) float NeonVolume=.65f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Door|Audio",meta=(ClampMin="0")) float NeonSoundOffset=0.f;
    UFUNCTION(BlueprintPure,Category="Door") float GetOpenFraction() const;
    UFUNCTION(BlueprintPure,Category="Door") float GetNeonWeight() const;
    UFUNCTION(BlueprintPure,Category="Door") float GetPublicLightWeight() const;
    UFUNCTION(BlueprintPure,Category="Door") bool IsPassageBlocked() const {return Presentation.bPassageBlocked;}
    UFUNCTION(BlueprintPure,Category="Door") bool IsWaitingForExit() const {return Presentation.bWaitingForExit;}
    UFUNCTION(BlueprintPure,Category="Door") bool IsPresentationReady() const {return Presentation.bReady;}
    const FBotanicusDoorPresentation& GetPresentation() const {return Presentation;}
    UBoxComponent* GetBlocker() const {return PassageBlocker;}
    const TArray<TObjectPtr<UStaticMeshComponent>>& GetSections() const {return Sections;}
    /** Shared server-state refresh, also used by the opt-in integration test. */
    void RefreshAuthority();
private:
    UFUNCTION() void OnRep_Presentation();
    void BuildGeometry();
    void ApplyPanels();
    void ApplyAccess();
    void UpdateLocalEffects();
    void BeginMotion(bool bOpen);
    void Publish();
    bool PortalOccupied() const;
    bool RearOccupied() const;
    bool HasPawnInBox(const FBox& Box,bool bPlayersOnly) const;
    void SetBlocked(bool bBlocked);
    double ServerNow() const;
    FVector TrackPoint(float Distance,float& Pitch) const;
    float MinimumSectionHeight() const;
    float OpenTravel() const;
    float MovingHeight() const;
    UAudioComponent* Sound(USoundBase* Asset,float Volume,float StartAt=0.f);
    UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> PassageBlocker;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> TopInfill;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> NavigationObstacle;
    UPROPERTY(VisibleAnywhere) TArray<TObjectPtr<UStaticMeshComponent>> Sections;
    UPROPERTY(VisibleAnywhere) TArray<TObjectPtr<UStaticMeshComponent>> Rails;
    UPROPERTY(VisibleAnywhere) TArray<TObjectPtr<UStaticMeshComponent>> NeonStrips;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> NeonMID;
    UPROPERTY(ReplicatedUsing=OnRep_Presentation) FBotanicusDoorPresentation Presentation;
    TWeakObjectPtr<UAudioComponent> MotorVoice;
    TWeakObjectPtr<UAudioComponent> NeonVoice;
    FTimerHandle AuthorityTimer;
    bool bHadSnapshot=false;
    double HeardMotion=-1.;
    double HeardNeon=-1.;
    double PendingContactEnd=-1.;
};
