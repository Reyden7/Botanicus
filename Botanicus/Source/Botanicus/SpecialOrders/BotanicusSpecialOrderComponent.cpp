#include "SpecialOrders/BotanicusSpecialOrderComponent.h"

#include "BotanicusCharacter.h"
#include "BotanicusGameMode.h"
#include "BotanicusGameState.h"
#include "Catalog/BotanicusItemCatalogSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Growing/BotanicusPlantSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "QuickBar/BotanicusQuickBarComponent.h"
#include "Sales/BotanicusCashRegisterActor.h"
#include "Visitors/BotanicusVisitorCharacter.h"

UBotanicusSpecialOrderComponent::UBotanicusSpecialOrderComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.25f;
}

UBotanicusSpecialOrderComponent* UBotanicusSpecialOrderComponent::Get(const UWorld* World)
{
	return World && World->GetGameState()
		? World->GetGameState()->FindComponentByClass<UBotanicusSpecialOrderComponent>() : nullptr;
}

void UBotanicusSpecialOrderComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UBotanicusSpecialOrderComponent, Orders);
}

float UBotanicusSpecialOrderComponent::ServerTime() const
{
	const AGameStateBase* State = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	return State ? State->GetServerWorldTimeSeconds() : 0.0f;
}

const FBotanicusSpecialOrder* UBotanicusSpecialOrderComponent::FindOrder(FGuid Id) const
{
	return Orders.FindByPredicate([Id](const auto& Order) { return Order.OrderId == Id; });
}

float UBotanicusSpecialOrderComponent::GetRemainingSeconds(const FBotanicusSpecialOrder& Order) const
{
	return FMath::Max(0.0f, Order.DeadlineServerTime - ServerTime());
}

void UBotanicusSpecialOrderComponent::Changed(bool bSave)
{
	GetOwner()->ForceNetUpdate();
	if (bSave && GetWorld())
	{
		if (auto* Mode = GetWorld()->GetAuthGameMode<ABotanicusGameMode>()) Mode->ScheduleInventoryAutosave();
	}
}

bool UBotanicusSpecialOrderComponent::ResolvePlant(FName ItemKey, bool bPreparedPot,
	FBotanicusSpecialOrderPlant& OutPlant) const
{
	const auto* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const auto* Catalog = Instance ? Instance->GetSubsystem<UBotanicusItemCatalogSubsystem>() : nullptr;
	const auto* Plants = Instance ? Instance->GetSubsystem<UBotanicusPlantSubsystem>() : nullptr;
	const auto* Item = Catalog ? Catalog->FindItem(ItemKey) : nullptr;
	if (!Item || !Item->bWholePlant || !Plants) return false;
	const auto* Traits = GetDefault<UBotanicusSpecialOrderSettings>()->ItemTraits.Find(ItemKey);
	const auto* Species = Traits && !Traits->PlantKey.IsNone()
		? Plants->FindPlant(Traits->PlantKey) : Plants->FindPlantByHarvestItem(ItemKey);
	if (!Species) return false;
	OutPlant = FBotanicusSpecialOrderPlant();
	OutPlant.PlantKey = Species->PlantKey;
	OutPlant.Element = Species->Element;
	OutPlant.ColorTag = Item->PlantColorTag;
	OutPlant.Quality = Item->PlantQualityTag == TEXT("Exceptional") ? 2 :
		(Item->PlantQualityTag == TEXT("Beautiful") ? 1 : 0);
	OutPlant.bPreparedPot = bPreparedPot;
	if (Traits) { OutPlant.RarityTag = Traits->RarityTag; OutPlant.MutationTags = Traits->MutationTags; }
	return true;
}

bool UBotanicusSpecialOrderComponent::ResolveSlot(const FBotanicusQuickBarSlot& Slot,
	FBotanicusSpecialOrderPlant& OutPlant) const
{
	if (Slot.IsEmpty()) return false;
	const bool bPreparedPot = Slot.CarriedState.bHasSalePotState &&
		!Slot.CarriedState.SaleSoilItemKey.IsNone() && !Slot.CarriedState.SalePlantItemKey.IsNone();
	return ResolvePlant(bPreparedPot ? Slot.CarriedState.SalePlantItemKey : Slot.ItemKey, bPreparedPot, OutPlant);
}

bool UBotanicusSpecialOrderComponent::BuildRequest(FBotanicusSpecialOrder& OutOrder) const
{
	const auto* State = Cast<ABotanicusGameState>(GetOwner());
	const auto* Instance = GetWorld()->GetGameInstance();
	const auto* Catalog = Instance ? Instance->GetSubsystem<UBotanicusItemCatalogSubsystem>() : nullptr;
	const auto* Plants = Instance ? Instance->GetSubsystem<UBotanicusPlantSubsystem>() : nullptr;
	if (!State || !Catalog || !Plants) return false;
	const auto* Settings = GetDefault<UBotanicusSpecialOrderSettings>();
	struct FCandidate { FBotanicusSpecialOrderPlant Plant; int32 Price; float GrowthSeconds; };
	TArray<FCandidate> Candidates;
	for (const auto& Item : Catalog->GetAllItems())
	{
		FBotanicusSpecialOrderPlant Plant;
		if (!ResolvePlant(Item.ItemKey, true, Plant)) continue;
		const auto* Species = Plants->FindPlant(Plant.PlantKey);
		const auto* Seed = Species ? Catalog->FindItem(Species->SeedItemKey) : nullptr;
		const auto* ResolvedItem = Catalog->FindItem(Item.ItemKey);
		if (Seed && Seed->bPurchasable && ResolvedItem)
			Candidates.Add({Plant, FMath::Max(1, ResolvedItem->SalePrice), Species->GrowthDurationSeconds});
	}
	if (Candidates.IsEmpty()) return false;

	TArray<FBotanicusSpecialOrderTemplate> Templates = Settings->Templates;
	const auto& Pick = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
	FBotanicusSpecialOrderRequirement Line;
	Line.PlantKey = Pick.Plant.PlantKey;
	FBotanicusSpecialOrderTemplate SpeciesRequest;
	SpeciesRequest.Title = NSLOCTEXT("BotanicusOrders", "Species", "Une plante bien précise");
	SpeciesRequest.Requirements.Add(Line);
	SpeciesRequest.TimeLimitSeconds = FMath::Max(Settings->MinimumProductionSeconds, Pick.GrowthSeconds + 120.0f);
	Templates.Add(SpeciesRequest);

	FBotanicusSpecialOrderTemplate Bouquet = SpeciesRequest;
	Bouquet.Title = NSLOCTEXT("BotanicusOrders", "Bouquet", "Une collection à préparer");
	for (const auto& Candidate : Candidates)
	{
		if (Candidate.Plant.PlantKey != Line.PlantKey)
		{
			auto Second = Line; Second.PlantKey = Candidate.Plant.PlantKey;
			Bouquet.Requirements.Add(Second);
			Bouquet.TimeLimitSeconds = FMath::Max(Bouquet.TimeLimitSeconds, Candidate.GrowthSeconds + 180.0f);
			Templates.Add(Bouquet);
			break;
		}
	}
	FBotanicusSpecialOrderTemplate Elemental = SpeciesRequest;
	Elemental.Title = NSLOCTEXT("BotanicusOrders", "Elemental", "Une touche élémentaire");
	Elemental.Requirements[0].PlantKey = NAME_None;
	Elemental.Requirements[0].bFilterElement = true;
	Elemental.Requirements[0].Element = Pick.Plant.Element;
	Elemental.Requirements[0].Quantity = 2;
	Elemental.TimeLimitSeconds += 60.0f;
	Templates.Add(Elemental);
	FBotanicusSpecialOrderTemplate Quality = SpeciesRequest;
	Quality.Title = NSLOCTEXT("BotanicusOrders", "Quality", "Un cadeau soigné");
	Quality.Requirements[0].PlantKey = NAME_None;
	Quality.Requirements[0].MinimumQuality = 1;
	Quality.Requirements[0].bRequirePreparedPot = true;
	Quality.TimeLimitSeconds += 60.0f;
	Templates.Add(Quality);
	for (const auto& Candidate : Candidates)
	{
		if (Candidate.Plant.RarityTag != NAME_None && Candidate.Plant.RarityTag != TEXT("Common"))
		{
			auto Rare = SpeciesRequest; Rare.Title = FText::FromString(TEXT("Une plante de collection"));
			Rare.Requirements[0].PlantKey = NAME_None;
			Rare.Requirements[0].RarityTag = Candidate.Plant.RarityTag;
			Templates.Add(Rare); break;
		}
	}
	for (const auto& Candidate : Candidates)
	{
		if (!Candidate.Plant.MutationTags.IsEmpty())
		{
			auto Mutated = SpeciesRequest; Mutated.Title = FText::FromString(TEXT("Une mutation recherchée"));
			Mutated.Requirements[0].PlantKey = NAME_None;
			Mutated.Requirements[0].MutationTag = Candidate.Plant.MutationTags[0];
			Templates.Add(Mutated); break;
		}
	}

	// Filter impossible or malformed authored requests before making any offer.
	TArray<FBotanicusSpecialOrder> ValidOrders;
	for (const auto& Template : Templates)
	{
		if (Template.MinimumShopLevel > State->GetMainShopLevel() || Template.Requirements.IsEmpty() ||
			Template.Requirements.Num() > 4 || !FMath::IsFinite(Template.TimeLimitSeconds) || Template.TimeLimitSeconds < 30.0f) continue;
		FBotanicusSpecialOrder Order;
		Order.Title = Template.Title;
		Order.TimeLimitSeconds = FMath::Clamp(Template.TimeLimitSeconds, 30.0f, 1800.0f);
		Order.Requirements = Template.Requirements;
		int64 BaseReward = 0;
		bool bFeasible = true;
		for (auto& R : Order.Requirements)
		{
			R.Delivered = 0;
			if (R.Quantity < 1 || R.Quantity > 10 || R.MinimumQuality < 0 || R.MinimumQuality > 2) { bFeasible = false; break; }
			int32 LowestPrice = MAX_int32;
			for (const auto& Candidate : Candidates)
				if (MatchesBotanicusSpecialOrder(R, Candidate.Plant)) LowestPrice = FMath::Min(LowestPrice, Candidate.Price);
			if (LowestPrice == MAX_int32) { bFeasible = false; break; }
			BaseReward += static_cast<int64>(LowestPrice) * R.Quantity;
			FString Description;
			const auto* Species = Plants->FindPlant(R.PlantKey);
			Description = Species ? Species->DisplayName.ToString() : TEXT("Plante");
			if (R.bFilterElement) Description += TEXT(" · ") + GetBotanicusSpecialOrderElementLabel(R.Element);
			if (R.MinimumQuality > 0) Description += R.MinimumQuality == 2 ? TEXT(" · exceptionnelle") : TEXT(" · belle ou mieux");
			if (!R.RarityTag.IsNone()) Description += TEXT(" · rareté : ") + R.RarityTag.ToString();
			if (!R.MutationTag.IsNone()) Description += TEXT(" · mutation : ") + R.MutationTag.ToString();
			if (!R.ColorTag.IsNone()) Description += TEXT(" · couleur : ") + R.ColorTag.ToString();
			if (R.bRequirePreparedPot) Description += TEXT(" · en pot de vente");
			R.Description = FText::FromString(Description);
		}
		if (!bFeasible) continue;
		const float Multiplier = FMath::IsFinite(Settings->RewardMultiplier) ? FMath::Clamp(Settings->RewardMultiplier, 1.0f, 5.0f) : 1.5f;
		Order.RewardCredits = FMath::RoundToInt(FMath::Clamp(static_cast<double>(BaseReward) * Multiplier, 1.0, 1000000.0));
		// The customer must be able to wait until the promised deadline before closing.
		if (State->GetDayTimeMinutes() + Order.TimeLimitSeconds + Settings->AcceptanceWaitSeconds + 30.0f >= 1140.0f) continue;
		ValidOrders.Add(MoveTemp(Order));
	}
	if (ValidOrders.IsEmpty()) return false;
	OutOrder = ValidOrders[FMath::RandRange(0, ValidOrders.Num() - 1)];
	return true;
}

ABotanicusCashRegisterActor* UBotanicusSpecialOrderComponent::FindCounter() const
{
	for (TActorIterator<ABotanicusCashRegisterActor> It(GetWorld()); It; ++It)
	{
		if (!It->IsOperational() || It->ActorHasTag(TEXT("BotanicusPlacementPreview"))) continue;
		int32 Waiting = 0;
		for (const auto& Order : Orders)
			if (Order.IsPending() && IsValid(Order.Customer) && Order.Customer->GetSpecialOrderCounter() == *It) ++Waiting;
		if (Waiting < 2) return *It;
	}
	return nullptr;
}

bool UBotanicusSpecialOrderComponent::TryAssignVisitor(ABotanicusVisitorCharacter* Visitor)
{
	const auto* State = Cast<ABotanicusGameState>(GetOwner());
	if (!State || !State->HasAuthority() || !State->IsMainShopOpen() || !IsValid(Visitor)) return false;
	auto* Counter = FindCounter();
	if (!Counter) return false;
	for (auto& Order : Orders)
	{
		if (Order.Status == EBotanicusSpecialOrderStatus::Active && !IsValid(Order.Customer) && GetRemainingSeconds(Order) > 0.0f)
		{
			Order.Customer = Visitor;
			Visitor->BeginSpecialOrderVisit(Order.OrderId, Counter);
			Changed(false);
			return true;
		}
	}
	const auto* Settings = GetDefault<UBotanicusSpecialOrderSettings>();
	const int32 Pending = Orders.FilterByPredicate([](const auto& O) { return O.IsPending(); }).Num();
	if (!Settings->bEnabled || Pending >= FMath::Clamp(Settings->MaximumConcurrentOrders, 1, 4) ||
		ServerTime() < NextOfferServerTime || FMath::FRand() >= FMath::Clamp(Settings->VisitorChance, 0.0f, 1.0f)) return false;
	FBotanicusSpecialOrder Order;
	if (!BuildRequest(Order)) return false;
	Order.OrderId = FGuid::NewGuid();
	Order.Number = NextOrderNumber++;
	Order.Customer = Visitor;
	Order.DeadlineServerTime = ServerTime() + 120.0f; // Bounded walk to the counter.
	const FGuid Id = Order.OrderId;
	Orders.Add(MoveTemp(Order));
	Visitor->BeginSpecialOrderVisit(Id, Counter);
	NextOfferServerTime = ServerTime() + FMath::Max(10.0f, Settings->OfferIntervalSeconds);
	Changed(false);
	return true;
}

void UBotanicusSpecialOrderComponent::CustomerArrived(FGuid Id, ABotanicusVisitorCharacter* Visitor)
{
	if (!GetOwner()->HasAuthority()) return;
	auto* Order = Orders.FindByPredicate([Id](const auto& O) { return O.OrderId == Id; });
	if (!Order || Order->Customer != Visitor || Order->Status != EBotanicusSpecialOrderStatus::Travelling) return;
	Order->Status = EBotanicusSpecialOrderStatus::Offered;
	Order->DeadlineServerTime = ServerTime() + FMath::Max(10.0f, GetDefault<UBotanicusSpecialOrderSettings>()->AcceptanceWaitSeconds);
	Changed(false);
}

bool UBotanicusSpecialOrderComponent::ValidateInteraction(const FBotanicusSpecialOrder& Order, AActor* Interactor) const
{
	const auto* Character = Cast<ABotanicusCharacter>(Interactor);
	return Character && Character->GetQuickBarComponent() && IsValid(Order.Customer) &&
		Order.Customer->IsWaitingForSpecialOrder() && GetRemainingSeconds(Order) > 0.0f &&
		FVector::DistSquared(Character->GetActorLocation(), Order.Customer->GetActorLocation()) <= FMath::Square(350.0f);
}

bool UBotanicusSpecialOrderComponent::CanInteract(FGuid Id, AActor* Interactor) const
{
	const auto* Order = FindOrder(Id);
	if (!Order || !ValidateInteraction(*Order, Interactor)) return false;
	if (Order->Status == EBotanicusSpecialOrderStatus::Offered) return true;
	if (Order->Status != EBotanicusSpecialOrderStatus::Active) return false;
	FBotanicusSpecialOrderPlant Plant;
	const auto* Character = Cast<ABotanicusCharacter>(Interactor);
	return ResolveSlot(Character->GetQuickBarComponent()->GetSelectedSlot(), Plant) &&
		FindBotanicusSpecialOrderDeliveryLine(Order->Requirements, Plant) != INDEX_NONE;
}

void UBotanicusSpecialOrderComponent::Interact(FGuid Id, AActor* Interactor)
{
	if (!GetOwner()->HasAuthority() || bInteractionInProgress || !CanInteract(Id, Interactor)) return;
	TGuardValue<bool> Guard(bInteractionInProgress, true);
	auto* Order = Orders.FindByPredicate([Id](const auto& O) { return O.OrderId == Id; });
	if (!Order) return;
	if (Order->Status == EBotanicusSpecialOrderStatus::Offered)
	{
		Order->Status = EBotanicusSpecialOrderStatus::Active;
		Order->bWasAccepted = true;
		Order->DeadlineServerTime = ServerTime() + Order->TimeLimitSeconds;
		Order->Customer->RefreshSpecialOrderSpeech();
		Changed();
		return;
	}
	auto* Character = CastChecked<ABotanicusCharacter>(Interactor);
	auto* Inventory = Character->GetQuickBarComponent();
	FBotanicusSpecialOrderPlant Plant;
	if (!ResolveSlot(Inventory->GetSelectedSlot(), Plant)) return;
	const int32 Line = FindBotanicusSpecialOrderDeliveryLine(Order->Requirements, Plant);
	if (Line == INDEX_NONE || !Inventory->RemoveQuantity(Inventory->GetSelectedSlotIndex(), 1)) return;
	++Order->Requirements[Line].Delivered;
	if (Order->IsComplete()) FinishOrder(*Order, EBotanicusSpecialOrderStatus::Completed);
	else Order->Customer->RefreshSpecialOrderSpeech();
	Changed();
}

void UBotanicusSpecialOrderComponent::FinishOrder(FBotanicusSpecialOrder& Order, EBotanicusSpecialOrderStatus Status)
{
	if (!Order.IsPending()) return;
	const auto Previous = Order.Status;
	Order.Status = Status; // Transition before calling other systems: rewards are exactly once.
	Order.DeadlineServerTime = ServerTime() + 8.0f;
	auto* State = CastChecked<ABotanicusGameState>(GetOwner());
	const int32 PreviousReputation = State->GetShopReputationPoints();
	if (Status == EBotanicusSpecialOrderStatus::Completed)
	{
		State->AddSharedFunds(Order.RewardCredits);
		bool bRevenueRecorded = false;
		for (const auto& R : Order.Requirements)
			for (int32 Index = 0; Index < R.Quantity; ++Index)
			{
				State->RecordPlantSale(bRevenueRecorded ? 0 : Order.RewardCredits);
				bRevenueRecorded = true;
			}
		State->RecordVisitorSatisfaction(95);
	}
	else if (Previous == EBotanicusSpecialOrderStatus::Active || Previous == EBotanicusSpecialOrderStatus::Offered)
	{
		State->RecordVisitorSatisfaction(Previous == EBotanicusSpecialOrderStatus::Active ? 30 : 50);
	}
	Order.ResultReputationDelta = State->GetShopReputationPoints() - PreviousReputation;
	if (IsValid(Order.Customer)) Order.Customer->FinishSpecialOrderVisit(Status == EBotanicusSpecialOrderStatus::Completed);
	Order.Customer = nullptr;
}

void UBotanicusSpecialOrderComponent::CustomerDeparted(FGuid Id, ABotanicusVisitorCharacter* Visitor)
{
	if (!GetOwner()->HasAuthority()) return;
	auto* Order = Orders.FindByPredicate([Id](const auto& O) { return O.OrderId == Id; });
	if (Order && Order->IsPending() && Order->Customer == Visitor)
	{
		FinishOrder(*Order, EBotanicusSpecialOrderStatus::Cancelled);
		Changed();
	}
}

void UBotanicusSpecialOrderComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!GetOwner()->HasAuthority()) return;
	bool bChanged = false;
	for (auto& Order : Orders)
	{
		if (Order.IsPending() && GetRemainingSeconds(Order) <= 0.0f)
		{
			FinishOrder(Order, EBotanicusSpecialOrderStatus::Expired);
			bChanged = true;
		}
	}
	bChanged |= Orders.RemoveAll([this](const auto& O) { return !O.IsPending() && GetRemainingSeconds(O) <= 0.0f; }) > 0;
	if (bChanged) Changed();
}

FText UBotanicusSpecialOrderComponent::GetRequestText(FGuid Id) const
{
	const auto* Order = FindOrder(Id);
	if (!Order) return FText::GetEmpty();
	FString Text = FString::Printf(TEXT("Commande #%d\n"), Order->Number);
	for (const auto& R : Order->Requirements)
		Text += FString::Printf(TEXT("%d/%d %s\n"), R.Delivered, R.Quantity, *R.Description.ToString());
	Text += FString::Printf(TEXT("Récompense : %d crédits · délai : %d min"),
		Order->RewardCredits, FMath::CeilToInt(Order->TimeLimitSeconds / 60.0f));
	if (Order->Status == EBotanicusSpecialOrderStatus::Offered)
		Text += FString::Printf(TEXT("\nLe client attend votre réponse : %d s"), FMath::CeilToInt(GetRemainingSeconds(*Order)));
	return FText::FromString(Text);
}

TArray<FBotanicusSpecialOrderSaveData> UBotanicusSpecialOrderComponent::CaptureSaveData() const
{
	TArray<FBotanicusSpecialOrderSaveData> Result;
	for (const auto& Order : Orders)
	{
		if (Order.Status != EBotanicusSpecialOrderStatus::Active) continue;
		FBotanicusSpecialOrderSaveData Saved;
		Saved.Order = Order;
		Saved.Order.Customer = nullptr;
		Saved.Order.DeadlineServerTime = 0.0f;
		Saved.RemainingSeconds = GetRemainingSeconds(Order);
		Result.Add(MoveTemp(Saved));
	}
	return Result;
}

void UBotanicusSpecialOrderComponent::RestoreSaveData(const TArray<FBotanicusSpecialOrderSaveData>& Saved)
{
	if (!GetOwner()->HasAuthority()) return;
	Orders.Reset();
	for (const auto& Entry : Saved)
	{
		if (Orders.Num() >= 4) break;
		if (!Entry.Order.OrderId.IsValid() || FindOrder(Entry.Order.OrderId) || Entry.Order.Requirements.IsEmpty() ||
			Entry.Order.Requirements.Num() > 4 || Entry.Order.IsComplete() || !FMath::IsFinite(Entry.RemainingSeconds)) continue;
		auto Order = Entry.Order;
		Order.Customer = nullptr;
		Order.Status = EBotanicusSpecialOrderStatus::Active;
		Order.bWasAccepted = true;
		Order.DeadlineServerTime = ServerTime() + FMath::Clamp(Entry.RemainingSeconds, 0.0f, 1800.0f);
		Order.RewardCredits = FMath::Clamp(Order.RewardCredits, 1, 1000000);
		for (auto& R : Order.Requirements) { R.Quantity = FMath::Clamp(R.Quantity, 1, 10); R.Delivered = FMath::Clamp(R.Delivered, 0, R.Quantity); }
		NextOrderNumber = FMath::Max(NextOrderNumber, Order.Number + 1);
		Orders.Add(MoveTemp(Order));
	}
	Changed(false);
}
