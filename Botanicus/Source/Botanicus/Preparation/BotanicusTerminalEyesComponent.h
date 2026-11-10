#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BotanicusTerminalEyesComponent.generated.h"

class APawn;
class UMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/** Per-terminal cosmetic gaze. Only the chosen player is replicated. */
UCLASS(ClassGroup=(Botanicus), meta=(BlueprintSpawnableComponent))
class BOTANICUS_API UBotanicusTerminalEyesComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UBotanicusTerminalEyesComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Botanicus|Eyes", meta=(ClampMin="0", Units="cm"))
	float DetectionDistance = 500.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Botanicus|Eyes", meta=(ClampMin="5", ClampMax="85", Units="deg"))
	float FrontHalfAngle = 75.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Botanicus|Eyes", meta=(ClampMin="0", Units="cm"))
	float TargetSwitchHysteresis = 50.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Botanicus|Eyes", meta=(ClampMin="0.1", ClampMax="1", Units="s"))
	float TargetSearchInterval = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Botanicus|Eyes", meta=(ClampMin="0", ClampMax="0.2"))
	FVector2D EyeMovementRange = FVector2D(0.13f, 0.10f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Botanicus|Eyes", meta=(ClampMin="0.1", ClampMax="30"))
	float EyeSmoothSpeed = 6.0f;
	/** Coordinates of the existing eye surfaces in the visual mesh, not actor space. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Botanicus|Eyes|Surface")
	FVector EyeOriginLocal = FVector(2.4339f, 81.75777f, 525.5372f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Botanicus|Eyes|Surface")
	FVector EyeForwardLocal = FVector(0, 1, 0);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Botanicus|Eyes|Surface")
	FName EyeMaterialSlot = TEXT("yeux");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Botanicus|Eyes|Surface")
	TSoftObjectPtr<UMaterialInterface> EyeMaterial;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool IsWorldTerminal() const;
	bool IsEligible(const APawn* Pawn, float& OutDistance) const;
	FVector GetEyeWorldOrigin() const;
	void SelectLookTarget();
	void RefreshDesiredOffset();
	UFUNCTION()
	void OnRep_LookTarget();
	UPROPERTY(ReplicatedUsing=OnRep_LookTarget)
	TObjectPtr<APawn> LookTarget;
	UPROPERTY(Transient)
	TObjectPtr<UMeshComponent> EyeMesh;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> OriginalEyeMaterial;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> EyeMID;
	int32 EyeMaterialIndex = INDEX_NONE;
	FVector2D CurrentOffset = FVector2D::ZeroVector;
	FVector2D DesiredOffset = FVector2D::ZeroVector;
	FTimerHandle TargetSearchTimer;
	FTimerHandle VisualAimTimer;
};
