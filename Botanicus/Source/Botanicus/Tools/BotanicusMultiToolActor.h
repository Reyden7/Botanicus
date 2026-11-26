#pragma once
#include "CoreMinimal.h"
#include "Delivery/BotanicusPlaceableItemActor.h"
#include "BotanicusMultiToolActor.generated.h"

UENUM(BlueprintType)
enum class EBotanicusMultiToolMode : uint8 {Neutral,Cutter,Shovel,ElectricWhip};

/** One inventory item. Mode changes only affect this reusable presentation. */
UCLASS()
class BOTANICUS_API ABotanicusMultiToolActor : public ABotanicusPlaceableItemActor
{
    GENERATED_BODY()
public:
    ABotanicusMultiToolActor();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void ApplyItemDefinition() override;
    float GetHeldVisualRadius() const;
    virtual bool UsesDedicatedHeldPresentation() const override { return true; }
    virtual void EquipForRobot(ABotanicusCharacter* Carrier, EBotanicusHeldItemSource Source,
        const FBotanicusHeldItemSettings& Catalogue = {}) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MultiTool|Grip")
    TObjectPtr<USceneComponent> ShovelRightHandGrip;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MultiTool|Grip")
    TObjectPtr<USceneComponent> WhipRightHandGrip;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MultiTool|Grip")
    TObjectPtr<USceneComponent> AnimatedRightHandGrip;
    void PlayWhipSwing();
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="MultiTool") TObjectPtr<USceneComponent> WhipVisualRoot;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="MultiTool") TArray<TObjectPtr<UStaticMeshComponent>> WhipSegments;
    UFUNCTION(BlueprintCallable,Category="MultiTool") void SetMultiToolMode(EBotanicusMultiToolMode NewMode);
    UFUNCTION(BlueprintPure,Category="MultiTool") EBotanicusMultiToolMode GetMultiToolMode() const {return Mode;}
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="MultiTool") TObjectPtr<USceneComponent> ToolVisualRoot;
    /** Reuses the animated component provided by the existing item base class. */
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="MultiTool") TObjectPtr<USkeletalMeshComponent> MultiToolSkeletalMesh;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="MultiTool") TObjectPtr<UStaticMeshComponent> CutterMesh_Temporary;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="MultiTool") TObjectPtr<UStaticMeshComponent> ShovelMesh_Temporary;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="MultiTool") TObjectPtr<USceneComponent> InteractionPoint;
    UFUNCTION(BlueprintImplementableEvent,Category="MultiTool") void OnMultiToolModeChanged(EBotanicusMultiToolMode PreviousMode,EBotanicusMultiToolMode NewMode);
private:
    void RefreshVisuals();
    void UpdateWhipSwing();
    double SwingStarted=-1;
    FTimerHandle SwingTimer;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="MultiTool",meta=(AllowPrivateAccess="true")) EBotanicusMultiToolMode Mode=EBotanicusMultiToolMode::Neutral;
};
