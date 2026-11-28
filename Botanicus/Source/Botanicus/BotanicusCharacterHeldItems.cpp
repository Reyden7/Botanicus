#include "BotanicusCharacter.h"
#include "Components/PrimitiveComponent.h"

#include "Catalog/BotanicusItemCatalogSubsystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Delivery/BotanicusPlaceableItemActor.h"
#include "Engine/GameInstance.h"
#include "Presentation/BotanicusHeldItemComponent.h"

void ABotanicusCharacter::EndPlay(const EEndPlayReason::Type Reason)
{
	ResetRobotEmoteAudio();
	UpdateHeldItemVisibility(nullptr);
	if (ABotanicusPlaceableItemActor* Item = ObservedQuickBarItem.Get())
		Item->Destroy();
	ObservedQuickBarItem.Reset();
	Super::EndPlay(Reason);
}

void ABotanicusCharacter::RegisterHeldItem(UBotanicusHeldItemComponent* Item, bool bRegister)
{
	if (bRegister)
		HeldItemSources.AddUnique(Item);
	else
		HeldItemSources.Remove(Item);
}

UBotanicusHeldItemComponent* ABotanicusCharacter::GetRobotHeldGrip() const
{
	UBotanicusHeldItemComponent* Active = nullptr;
	if (!IsLocallyControlled() && !GetCarriedParcelForAnimation() && !IsCarryingEquipment())
	{
		for (const TWeakObjectPtr<UBotanicusHeldItemComponent>& Source : HeldItemSources)
		{
			UBotanicusHeldItemComponent* Grip = Source.Get();
			if (Grip && Grip->HasMount() && (Grip->GetSource() == EBotanicusHeldItemSource::QuickBar || !Grip->GetOwner()->IsHidden())
				&& (!Active || Grip->GetSource() > Active->GetSource()))
			{
				Active = Grip;
			}
		}
	}

	return Active;
}

FBotanicusHeldItemPose ABotanicusCharacter::UpdateHeldItemPose(float DeltaSeconds, const FBotanicusRobotToolMotion& Motion,
	bool bSuppressHeldItem)
{
	UBotanicusHeldItemComponent* Active = GetRobotHeldGrip();
	const bool bHideItem = IsUsingOrderComputer() || bSuppressHeldItem;
	if (ABotanicusPlaceableItemActor* Item = ObservedQuickBarItem.Get())
		Item->SetActorHiddenInGame(bHideItem || Item->GetHeldItemGrip() != Active);
	AActor* ItemToHide = bHideItem && Active && Active->GetSource() != EBotanicusHeldItemSource::QuickBar
		? Active->GetOwner() : nullptr;
	if (!IsLocallyControlled())
		UpdateHeldItemVisibility(ItemToHide);
	if (!Active || bHideItem)
		return {};
	Active->SetToolMotion(Motion);
	return Active->UpdateMount(DeltaSeconds);
}

void ABotanicusCharacter::UpdateHeldItemVisibility(AActor* ItemToHide)
{
	if (HiddenHeldItem.Get() != ItemToHide || (!ItemToHide && !HiddenHeldPrimitives.IsEmpty()))
	{
		for (const FHiddenHeldPrimitive& Hidden : HiddenHeldPrimitives)
		{
			if (auto* Primitive = Hidden.Component.Get())
			{
				Primitive->SetRenderInMainPass(Hidden.bMainPass);
				Primitive->SetRenderInDepthPass(Hidden.bDepthPass);
				Primitive->SetRenderCustomDepth(Hidden.bCustomDepth);
				Primitive->SetCastShadow(Hidden.bCastShadow);
			}
		}
		HiddenHeldPrimitives.Reset();
		HiddenHeldItem = ItemToHide;
		if (ItemToHide)
		{
			TInlineComponentArray<UPrimitiveComponent*> Meshes(ItemToHide);
			for (auto* Primitive : Meshes)
			{
				HiddenHeldPrimitives.Add({Primitive, !!Primitive->bRenderInMainPass, !!Primitive->bRenderInDepthPass,
					!!Primitive->bRenderCustomDepth, !!Primitive->CastShadow});
				// Disabling the colour pass alone still leaves a depth silhouette.
				Primitive->SetRenderInMainPass(false);
				Primitive->SetRenderInDepthPass(false);
				Primitive->SetRenderCustomDepth(false);
				Primitive->SetCastShadow(false);
			}
		}
	}
}

ABotanicusPlaceableItemActor* ABotanicusCharacter::GetObservedQuickBarItem() const
{
	ABotanicusPlaceableItemActor* Item = ObservedQuickBarItem.Get();
	return Item && !Item->IsHidden() ? Item : nullptr;
}

void ABotanicusCharacter::SetObservedQuickBarItem(FName ItemKey)
{
	if (ABotanicusPlaceableItemActor* Previous = ObservedQuickBarItem.Get())
	{
		if (Previous->GetItemKey() == ItemKey && !IsLocallyControlled())
			return;
		ObservedQuickBarItem.Reset();
		Previous->Destroy();
	}
	if (ItemKey.IsNone() || IsLocallyControlled() || GetNetMode() == NM_DedicatedServer)
		return;

	const UGameInstance* Instance = GetGameInstance();
	const auto* Catalog = Instance ? Instance->GetSubsystem<UBotanicusItemCatalogSubsystem>() : nullptr;
	const FBotanicusItemDefinition* Definition = Catalog ? Catalog->FindItem(ItemKey) : nullptr;
	if (!Definition || Definition->WeightClass != EBotanicusItemWeightClass::Hotbar)
		return;
	UClass* Class = Definition->WorldActorClass.LoadSynchronous();
	if (!Class || !Class->IsChildOf(ABotanicusPlaceableItemActor::StaticClass()))
		Class = ABotanicusPlaceableItemActor::StaticClass();

	const auto* Defaults = Class->GetDefaultObject<ABotanicusPlaceableItemActor>();
	if (Defaults->UsesDedicatedHeldPresentation()
		|| Defaults->GetHeldItemGrip()->ResolveSettings(Definition->HeldItemSettings).Profile == EBotanicusHeldItemProfile::None)
		return;

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.CustomPreSpawnInitialization = [](AActor* Actor)
	{
		Actor->Tags.AddUnique(TEXT("BotanicusEquippedPreview"));
		Actor->Tags.AddUnique(TEXT("BotanicusObservedHeldItem"));
		Actor->SetReplicates(false);
		Actor->SetActorEnableCollision(false);
	};
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Item = GetWorld()->SpawnActor<ABotanicusPlaceableItemActor>(Class, GetActorTransform(), Params);
	if (!Item)
		return;
	ObservedQuickBarItem = Item;
	Item->InitializePlacedItem(ItemKey);
	Item->ConfigureAsLocalPreview(true);
	Item->ConfigureAsLocalInspection();
	Item->SetActorTickEnabled(false);
	Item->AttachToComponent(GetMesh(), FAttachmentTransformRules::KeepWorldTransform);
	Item->EquipForRobot(this, EBotanicusHeldItemSource::QuickBar, Definition->HeldItemSettings);
	UpdateHeldItemPose(0.f);
}

