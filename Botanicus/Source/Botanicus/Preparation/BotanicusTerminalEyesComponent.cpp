#include "Preparation/BotanicusTerminalEyesComponent.h"

#include "Components/MeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

UBotanicusTerminalEyesComponent::UBotanicusTerminalEyesComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	EyeMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(
		TEXT("/Game/Botanicus/Items/PanneauDeCommande/M_CommandTerminalEyes.M_CommandTerminalEyes")));
}

void UBotanicusTerminalEyesComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UBotanicusTerminalEyesComponent, LookTarget);
}

bool UBotanicusTerminalEyesComponent::IsWorldTerminal() const
{
	const AActor* Owner = GetOwner();
	return IsValid(Owner) && Owner->GetIsReplicated() &&
		!Owner->ActorHasTag(TEXT("BotanicusPlacementPreview")) &&
		!Owner->ActorHasTag(TEXT("BotanicusEquippedPreview")) &&
		!Owner->ActorHasTag(TEXT("BotanicusJuiceRemotePreview"));
}

void UBotanicusTerminalEyesComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!IsWorldTerminal()) return;
	UMaterialInterface* Material = EyeMaterial.LoadSynchronous();
	TInlineComponentArray<UMeshComponent*> Meshes(GetOwner());
	for (UMeshComponent* Candidate : Meshes)
	{
		const int32 Index = Candidate->GetMaterialIndex(EyeMaterialSlot);
		if (Index != INDEX_NONE && Material)
		{
			EyeMesh = Candidate;
			EyeMaterialIndex = Index;
			OriginalEyeMaterial = Candidate->GetMaterial(Index);
			break;
		}
	}
	if (!EyeMesh) return; // Old computer models and placement holograms remain untouched.
	if (GetNetMode() != NM_DedicatedServer)
	{
		EyeMID = UMaterialInstanceDynamic::Create(Material, this);
		EyeMesh->SetMaterial(EyeMaterialIndex, EyeMID);
	}
	if (GetOwner()->HasAuthority())
	{
		GetWorld()->GetTimerManager().SetTimer(TargetSearchTimer, this,
			&ThisClass::SelectLookTarget, FMath::Max(0.1f, TargetSearchInterval), true);
		SelectLookTarget();
	}
	OnRep_LookTarget();
}

FVector UBotanicusTerminalEyesComponent::GetEyeWorldOrigin() const
{
	return EyeMesh->GetComponentTransform().TransformPosition(EyeOriginLocal);
}

bool UBotanicusTerminalEyesComponent::IsEligible(const APawn* Pawn, float& OutDistance) const
{
	if (!IsValid(Pawn) || !Pawn->IsPlayerControlled() || !EyeMesh) return false;
	const FVector Delta = Pawn->GetPawnViewLocation() - GetEyeWorldOrigin();
	OutDistance = Delta.Size();
	const FVector Forward = EyeMesh->GetComponentTransform().TransformVectorNoScale(EyeForwardLocal).GetSafeNormal();
	return OutDistance > 1.0f && OutDistance <= DetectionDistance &&
		FVector::DotProduct(Delta / OutDistance, Forward) >= FMath::Cos(FMath::DegreesToRadians(FrontHalfAngle));
}

void UBotanicusTerminalEyesComponent::SelectLookTarget()
{
	APawn* Best = nullptr;
	float BestDistance = TNumericLimits<float>::Max();
	if (IsWorldTerminal())
	{
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			APawn* Pawn = It->Get()->GetPawn();
			float Distance = 0;
			if (IsEligible(Pawn, Distance) && Distance < BestDistance) { Best = Pawn; BestDistance = Distance; }
		}
		float CurrentDistance = 0;
		if (IsEligible(LookTarget, CurrentDistance) &&
			(!Best || CurrentDistance <= BestDistance + TargetSwitchHysteresis)) Best = LookTarget;
	}
	if (LookTarget != Best)
	{
		LookTarget = Best;
		GetOwner()->ForceNetUpdate();
		OnRep_LookTarget();
	}
}

void UBotanicusTerminalEyesComponent::OnRep_LookTarget()
{
	if (!EyeMID || !GetWorld()) return;
	if (IsValid(LookTarget))
	{
		if (!GetWorld()->GetTimerManager().IsTimerActive(VisualAimTimer))
		{
			// Sampling one known head at 10 Hz; no traces or client player search.
			GetWorld()->GetTimerManager().SetTimer(VisualAimTimer, this,
				&ThisClass::RefreshDesiredOffset, 0.1f, true);
		}
	}
	else GetWorld()->GetTimerManager().ClearTimer(VisualAimTimer);
	RefreshDesiredOffset();
}

void UBotanicusTerminalEyesComponent::RefreshDesiredOffset()
{
	DesiredOffset = FVector2D::ZeroVector;
	float Distance = 0;
	if (IsWorldTerminal() && EyeMID && EyeMesh->GetMaterial(EyeMaterialIndex) == EyeMID &&
		IsEligible(LookTarget, Distance))
	{
		const FTransform Frame = EyeMesh->GetComponentTransform();
		const FVector Forward = Frame.TransformVectorNoScale(EyeForwardLocal).GetSafeNormal();
		const FVector Up = Frame.TransformVectorNoScale(FVector::UpVector).GetSafeNormal();
		const FVector Right = FVector::CrossProduct(Forward, Up).GetSafeNormal();
		const FVector Delta = LookTarget->GetPawnViewLocation() - GetEyeWorldOrigin();
		const float Ahead = FVector::DotProduct(Delta, Forward);
		const float Horizontal = FVector::DotProduct(Delta, Right);
		DesiredOffset.X = FMath::Clamp(FMath::Atan2(Horizontal, Ahead) / FMath::DegreesToRadians(60.0f), -1.0f, 1.0f) * FMath::Clamp(EyeMovementRange.X, 0.0f, 0.2f);
		DesiredOffset.Y = FMath::Clamp(FMath::Atan2(FVector::DotProduct(Delta, Up), FMath::Sqrt(Ahead*Ahead + Horizontal*Horizontal)) / FMath::DegreesToRadians(40.0f), -1.0f, 1.0f) * FMath::Clamp(EyeMovementRange.Y, 0.0f, 0.2f);
	}
	SetComponentTickEnabled(!CurrentOffset.Equals(DesiredOffset, 0.0001f));
}

void UBotanicusTerminalEyesComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!EyeMID || !IsWorldTerminal() || EyeMesh->GetMaterial(EyeMaterialIndex) != EyeMID)
	{
		SetComponentTickEnabled(false);
		return;
	}
	CurrentOffset += (DesiredOffset - CurrentOffset) * (1.0f - FMath::Exp(-FMath::Max(0.1f, EyeSmoothSpeed) * DeltaTime));
	if (CurrentOffset.Equals(DesiredOffset, 0.0001f))
	{
		CurrentOffset = DesiredOffset;
		SetComponentTickEnabled(false);
	}
	EyeMID->SetScalarParameterValue(TEXT("EyeOffsetX"), CurrentOffset.X);
	EyeMID->SetScalarParameterValue(TEXT("EyeOffsetY"), CurrentOffset.Y);
}

void UBotanicusTerminalEyesComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(TargetSearchTimer);
		GetWorld()->GetTimerManager().ClearTimer(VisualAimTimer);
	}
	if (EyeMesh && EyeMID && EyeMesh->GetMaterial(EyeMaterialIndex) == EyeMID)
		EyeMesh->SetMaterial(EyeMaterialIndex, OriginalEyeMaterial);
	Super::EndPlay(EndPlayReason);
}
