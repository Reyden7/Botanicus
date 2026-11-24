#include "Building/BotanicusPlacementSurfaceComponent.h"

#include "Delivery/BotanicusDeliveryParcelActor.h"
#include "Delivery/BotanicusLargeEquipmentActor.h"
#include "Delivery/BotanicusPlaceableItemActor.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Preparation/BotanicusPreparationWorkbenchActor.h"
#include "Sales/BotanicusSalesDisplayActor.h"
#include "Sales/BotanicusSalesShelfActor.h"
#include "Storage/BotanicusStorageShelfActor.h"

UBotanicusPlacementSurfaceComponent::UBotanicusPlacementSurfaceComponent()
{
	InitBoxExtent(FVector(40, 25, 1));
	SetHiddenInGame(true);
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
}

void UBotanicusPlacementSurfaceComponent::OnRegister()
{
	SetCollisionEnabled(bSupportPhysics ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::QueryOnly);
	SetCollisionResponseToAllChannels(ECR_Ignore);
	SetCollisionResponseToChannel(PlacementChannel, ECR_Block);
	if (bSupportPhysics)
	{
		SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
	}
	Super::OnRegister();
}

bool UBotanicusPlacementSurfaceComponent::Accepts(FName Key, EBotanicusItemCategory Category) const
{
	const auto* Owner = GetOwner();
	const auto* Equipment = Cast<ABotanicusLargeEquipmentActor>(Owner);
	return IsValid(Owner) && (!Equipment || (!Equipment->GetCarrier() && !Equipment->IsInPlacementMode())) &&
		!Owner->IsA<ABotanicusPreparationWorkbenchActor>() && !Owner->IsA<ABotanicusSalesDisplayActor>() &&
		!Owner->IsA<ABotanicusSalesShelfActor>() && !Owner->IsA<ABotanicusStorageShelfActor>() &&
		(AllowedItemKeys.IsEmpty() || AllowedItemKeys.Contains(Key)) &&
		(AllowedCategories.IsEmpty() || AllowedCategories.Contains(Category));
}

bool UBotanicusPlacementSurfaceComponent::HasSupportedItems(const AActor* Furniture)
{
	if (!IsValid(Furniture))
		return false;

	TInlineComponentArray<UBotanicusPlacementSurfaceComponent*> Surfaces(Furniture);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(BotanicusFurnitureContents), false, Furniture);
	for (const auto* Surface : Surfaces)
	{
		const FVector Extent = Surface->GetScaledBoxExtent();
		const FVector Top = Surface->GetComponentLocation() + Surface->GetUpVector() * Extent.Z;
		TArray<FOverlapResult> Hits;
		Furniture->GetWorld()->OverlapMultiByObjectType(Hits, Top + Surface->GetUpVector(),
			Surface->GetComponentQuat(), Objects, FCollisionShape::MakeBox(FVector(Extent.X, Extent.Y, 4.)), Query);
		for (const auto& Hit : Hits)
		{
			const auto* Item = Cast<ABotanicusPlaceableItemActor>(Hit.GetActor());
			const auto* Parcel = Cast<ABotanicusDeliveryParcelActor>(Hit.GetActor());
			if ((IsValid(Item) && Item->GetIsReplicated() && !Item->ActorHasTag(TEXT("BotanicusPlacementPreview")) &&
				!Item->ActorHasTag(TEXT("BotanicusEquippedPreview")) &&
				!Item->ActorHasTag(TEXT("BotanicusJuiceRemotePreview"))) || (IsValid(Parcel) && !Parcel->GetCarrier()))
			{
				return true;
			}
		}
	}
	return false;
}
