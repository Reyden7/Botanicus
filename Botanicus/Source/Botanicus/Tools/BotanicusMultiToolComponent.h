#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Tools/BotanicusMultiToolActor.h"
#include "Engine/NetSerialization.h"
#include "BotanicusMultiToolComponent.generated.h"

USTRUCT()
struct FBotanicusMultiToolPresentation
{
    GENERATED_BODY()
    UPROPERTY() EBotanicusMultiToolMode Mode=EBotanicusMultiToolMode::Neutral;
    UPROPERTY() bool bEquipped=false;
    UPROPERTY() TObjectPtr<AActor> Target;
    UPROPERTY() int32 Slot=INDEX_NONE;
};

/** Contextual selection only. Inventory ownership and original actions remain authoritative. */
UCLASS(ClassGroup=(Botanicus),meta=(BlueprintSpawnableComponent))
class BOTANICUS_API UBotanicusMultiToolComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UBotanicusMultiToolComponent();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    static FName ItemKey(){return TEXT("MultiTool_L1");}
    /** Legacy tools continue to work; only an owned, selected multi-tool with the right context substitutes them. */
    static bool HasTool(const AActor* Interactor,FName RequiredTool,const AActor* Target,int32 Slot=0);
    EBotanicusMultiToolMode GetMode() const {return Presentation.Mode;}
    void SetActionVisualMode(EBotanicusMultiToolMode Mode);
    static bool IsRobotItem(const UObject* Context,FName Key);
    bool TryWhip();
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="MultiTool|Whip",meta=(ClampMin="25",Units="cm")) float WhipInteractionDistance=200;
    bool ShouldShowEquippedVisual() const {return !IsBlocked();}
    AActor* GetPreparedTarget(EBotanicusMultiToolMode RequiredMode) const;
    /** Called only from explicit hotbar input, never from inventory quantity notifications. */
    void NotifyManualSelection();
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="MultiTool",meta=(ClampMin="0.05")) float TargetStabilization=.15f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="MultiTool",meta=(ClampMin="50",Units="cm")) float InteractionDistance=450;
    UPROPERTY(EditAnywhere,Category="MultiTool|Presentation") TSoftClassPtr<ABotanicusMultiToolActor> VisualClass;
    UPROPERTY(EditAnywhere,Category="MultiTool|Presentation") FName ThirdPersonSocket=TEXT("hand_r");
    UPROPERTY(EditAnywhere,Category="MultiTool|Presentation") FTransform ThirdPersonOffset=FTransform::Identity;
private:
    bool ResolveAim(AActor*& Target,int32& Slot,EBotanicusMultiToolMode& Mode) const;
    bool IsBlocked() const;
    int32 FindOwnedSlot() const;
    void UpdateContext();
    void ReleaseContext(bool bRestore);
    void Publish();
    UFUNCTION() void OnRep_Presentation();
    UFUNCTION(Server,Reliable) void ServerPrepareContext(AActor* Target,int32 Slot);
    UFUNCTION(Server,Reliable) void ServerCancelContext(bool bManual);
    UFUNCTION(Server,Reliable) void ServerWhip(class ABotanicusGardeningRobot* Robot);
    UFUNCTION(NetMulticast,Unreliable) void MulticastWhipSwing(FVector_NetQuantize10 Target, double StartedAt);
    UPROPERTY(ReplicatedUsing=OnRep_Presentation) FBotanicusMultiToolPresentation Presentation;
    UPROPERTY(Transient) TObjectPtr<ABotanicusMultiToolActor> RemoteVisual;
    EBotanicusMultiToolMode ActionVisualMode = EBotanicusMultiToolMode::Neutral;
    TWeakObjectPtr<AActor> Candidate,LastRequested,SuppressedTarget;
    int32 CandidateSlot=INDEX_NONE,LastRequestedSlot=INDEX_NONE,SuppressedSlot=INDEX_NONE;
    bool bSuppressed=false,bAutoSelected=false;
    FGuid AutoInstance,PreviousInstance;
    int32 PreviousSlot=INDEX_NONE;
    double CandidateSince=0,ServerInvalidSince=-1;
    FTimerHandle UpdateTimer;
};
