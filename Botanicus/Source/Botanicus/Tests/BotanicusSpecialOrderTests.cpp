#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "BotanicusCharacter.h"
#include "BotanicusGameState.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "QuickBar/BotanicusQuickBarComponent.h"
#include "Save/BotanicusWorldSaveGame.h"
#include "SpecialOrders/BotanicusSpecialOrderComponent.h"
#include "UObject/StrongObjectPtr.h"
#include "Visitors/BotanicusVisitorCharacter.h"
#include "Visitors/BotanicusVisitorZoneActor.h"
#include "Sales/BotanicusCashRegisterActor.h"
#include "Components/CapsuleComponent.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/NamedSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "UI/BotanicusHudLayoutWidget.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundAttenuation.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBotanicusSpecialOrderCriteriaTest,
	"Botanicus.SpecialOrders.Criteria", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBotanicusSpecialOrderCriteriaTest::RunTest(const FString& Parameters)
{
	FBotanicusSpecialOrderRequirement R;
	R.PlantKey = TEXT("CoralyneBrumes");
	R.bFilterElement = true; R.Element = EBotanicusPlantElement::Water;
	R.MinimumQuality = 1; R.RarityTag = TEXT("Rare"); R.MutationTag = TEXT("Luminous");
	R.ColorTag = TEXT("Blue"); R.bRequirePreparedPot = true;
	FBotanicusSpecialOrderPlant P;
	P.PlantKey = R.PlantKey; P.Element = R.Element; P.Quality = 2;
	P.RarityTag = R.RarityTag; P.MutationTags.Add(R.MutationTag); P.ColorTag = R.ColorTag; P.bPreparedPot = true;
	TestTrue(TEXT("All criteria and better quality accepted"), MatchesBotanicusSpecialOrder(R, P));
	auto Wrong = P; Wrong.MutationTags.Reset();
	TestFalse(TEXT("Missing mutation rejected"), MatchesBotanicusSpecialOrder(R, Wrong));
	Wrong = P; Wrong.RarityTag = TEXT("Common");
	TestFalse(TEXT("Quality is not rarity"), MatchesBotanicusSpecialOrder(R, Wrong));
	Wrong = P; Wrong.Element = EBotanicusPlantElement::Fire;
	TestFalse(TEXT("Wrong element rejected"), MatchesBotanicusSpecialOrder(R, Wrong));
	Wrong = P; Wrong.Quality = 0;
	TestFalse(TEXT("Low quality rejected"), MatchesBotanicusSpecialOrder(R, Wrong));
	Wrong = P; Wrong.bPreparedPot = false;
	TestFalse(TEXT("Bare harvest rejected when pot required"), MatchesBotanicusSpecialOrder(R, Wrong));
	Wrong = P; Wrong.PlantKey = TEXT("AureliaSweet");
	TestFalse(TEXT("Wrong species rejected"), MatchesBotanicusSpecialOrder(R, Wrong));

	FBotanicusSpecialOrderRequirement Generic;
	Generic.bFilterElement = true; Generic.Element = P.Element;
	TArray<FBotanicusSpecialOrderRequirement> Lines = {Generic, R};
	TestEqual(TEXT("Specific line has priority over generic line"), FindBotanicusSpecialOrderDeliveryLine(Lines, P), 1);
	Lines[1].Delivered = 1;
	TestEqual(TEXT("Completed line cannot consume another item"), FindBotanicusSpecialOrderDeliveryLine(Lines, P), 0);
	Lines[0].Delivered = 1;
	TestEqual(TEXT("A plant is never counted twice"), FindBotanicusSpecialOrderDeliveryLine(Lines, P), INDEX_NONE);
	FBotanicusSpecialOrder Empty;
	TestFalse(TEXT("Empty request cannot complete"), Empty.IsComplete());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBotanicusSpecialOrderLifecycleTest,
	"Botanicus.SpecialOrders.Lifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBotanicusSpecialOrderLifecycleTest::RunTest(const FString& Parameters)
{
	// An isolated transient world: no real map, save slot, GameMode or user progress is loaded.
	TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>(GEngine));
	Instance->InitializeStandalone();
	UWorld* World = Instance->GetWorld();
	if (!TestNotNull(TEXT("Isolated world"), World)) return false;
	ON_SCOPE_EXIT
	{
		Instance->Shutdown();
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	};
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* State = World->SpawnActor<ABotanicusGameState>(FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
	if (!TestNotNull(TEXT("Shared state"), State)) return false;
	World->SetGameState(State);
	auto* Orders = State->SpecialOrders.Get();
	auto* Visitor = World->SpawnActor<ABotanicusVisitorCharacter>(FVector(0,0,100), FRotator::ZeroRotator, Spawn);
	UClass* PlayerClass = LoadClass<ABotanicusCharacter>(nullptr,
		TEXT("/Game/Botanicus/Building/BP_BotanicusBuilderCharacter.BP_BotanicusBuilderCharacter_C"));
	if (!TestNotNull(TEXT("Concrete project player class"), PlayerClass)) return false;
	auto* PlayerA = World->SpawnActor<ABotanicusCharacter>(PlayerClass, FVector(100,0,100), FRotator::ZeroRotator, Spawn);
	auto* PlayerB = World->SpawnActor<ABotanicusCharacter>(PlayerClass, FVector(150,0,100), FRotator::ZeroRotator, Spawn);
	if (!TestNotNull(TEXT("First player"), PlayerA) || !TestNotNull(TEXT("Second player"), PlayerB) ||
		!TestNotNull(TEXT("Customer"), Visitor)) return false;
	Visitor->VisitorState = EBotanicusVisitorState::SpecialOrderWaiting;
	FBotanicusSpecialOrder Order;
	Order.OrderId = FGuid::NewGuid(); Order.Number = 1;
	Order.Status = EBotanicusSpecialOrderStatus::Offered;
	Order.DeadlineServerTime = Orders->ServerTime() + 90.0f;
	Order.TimeLimitSeconds = 360.0f; Order.RewardCredits = 200; Order.Customer = Visitor;
	FBotanicusSpecialOrderRequirement Line;
	Line.PlantKey = TEXT("AureliaSweet"); Line.Quantity = 2;
	Order.Requirements.Add(Line);
	Visitor->SpecialOrderId = Order.OrderId;
	Orders->Orders.Add(Order);
	const FGuid Id = Order.OrderId;
	const int32 InitialFunds = State->GetSharedFunds();
	const int32 InitialReviews = State->GetTotalVisitorReviews();
	TestTrue(TEXT("Offer can be accepted at counter"), Orders->CanInteract(Id, PlayerA));
	Orders->Interact(Id, PlayerA);
	TestTrue(TEXT("Acceptance activates shared order"), Orders->FindOrder(Id)->Status == EBotanicusSpecialOrderStatus::Active);
	TestEqual(TEXT("Acceptance alone never rewards credits"), State->GetSharedFunds(), InitialFunds);
	TestFalse(TEXT("Empty inventory cannot deliver"), Orders->CanInteract(Id, PlayerA));
	TestTrue(TEXT("Give first harvested plant"), PlayerA->GetQuickBarComponent()->SetSlotItem(0, TEXT("Harvest_AureliaSweet")));
	TestTrue(TEXT("Give second harvested plant"), PlayerB->GetQuickBarComponent()->SetSlotItem(0, TEXT("Harvest_AureliaSweet")));
	PlayerB->SetActorLocation(FVector(2000,0,100));
	TestFalse(TEXT("Remote player rejected"), Orders->CanInteract(Id, PlayerB));
	PlayerB->SetActorLocation(FVector(150,0,100));
	Orders->Interact(Id, PlayerA);
	TestEqual(TEXT("First contribution is shared"), Orders->FindOrder(Id)->Requirements[0].Delivered, 1);
	TestTrue(TEXT("First contributor pays exactly one item"), PlayerA->GetQuickBarComponent()->GetSlot(0).IsEmpty());
	TestFalse(TEXT("Other player's inventory is untouched"), PlayerB->GetQuickBarComponent()->GetSlot(0).IsEmpty());
	TestEqual(TEXT("Partial order has no full reward"), State->GetSharedFunds(), InitialFunds);

	const auto Saved = Orders->CaptureSaveData();
	TestEqual(TEXT("Accepted order saved"), Saved.Num(), 1);
	TestNull(TEXT("No NPC reference in save"), Saved[0].Order.Customer.Get());
	auto* SaveObject = NewObject<UBotanicusWorldSaveGame>();
	SaveObject->SpecialOrders = Saved;
	TArray<uint8> Bytes;
	TestTrue(TEXT("Save serialization succeeds"), UGameplayStatics::SaveGameToMemory(SaveObject, Bytes));
	auto* Loaded = Cast<UBotanicusWorldSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	if (TestNotNull(TEXT("Save deserialization succeeds"), Loaded))
	{
		Orders->RestoreSaveData(Loaded->SpecialOrders);
		TestEqual(TEXT("Reload preserves partial deliveries"), Orders->FindOrder(Id)->Requirements[0].Delivered, 1);
		TestTrue(TEXT("Reload preserves remaining time"), FMath::IsNearlyEqual(Orders->GetRemainingSeconds(*Orders->FindOrder(Id)), Saved[0].RemainingSeconds, 0.01f));
		TestFalse(TEXT("Cannot hand in to an unassigned restored customer"), Orders->CanInteract(Id, PlayerB));
		Orders->Orders[0].Customer = Visitor;
	}
	Orders->Interact(Id, PlayerB);
	TestTrue(TEXT("Second player completes same order"), Orders->FindOrder(Id)->Status == EBotanicusSpecialOrderStatus::Completed);
	TestEqual(TEXT("Shared reward paid once"), State->GetSharedFunds(), InitialFunds + 200);
	TestEqual(TEXT("Both plants count in sales"), State->GetTotalPlantsSold(), 2);
	TestEqual(TEXT("One satisfaction review"), State->GetTotalVisitorReviews(), InitialReviews + 1);
	Orders->Interact(Id, PlayerB);
	Orders->FinishOrder(Orders->Orders[0], EBotanicusSpecialOrderStatus::Completed);
	TestEqual(TEXT("Repeated/reentrant completion cannot duplicate reward"), State->GetSharedFunds(), InitialFunds + 200);
	TestEqual(TEXT("Completed order excluded from save"), Orders->CaptureSaveData().Num(), 0);

	// A late delivery must fail even if the periodic expiry tick has not run yet.
	Order.OrderId = FGuid::NewGuid(); Order.Status = EBotanicusSpecialOrderStatus::Active;
	Order.DeadlineServerTime = Orders->ServerTime(); Order.Requirements[0].Delivered = 0;
	Order.Customer = Visitor; Visitor->SpecialOrderId = Order.OrderId;
	Visitor->VisitorState = EBotanicusVisitorState::SpecialOrderWaiting;
	Orders->Orders.Add(Order);
	PlayerA->GetQuickBarComponent()->SetSlotItem(0, TEXT("Harvest_AureliaSweet"));
	TestFalse(TEXT("Deadline enforced before tick"), Orders->CanInteract(Order.OrderId, PlayerA));
	Orders->Interact(Order.OrderId, PlayerA);
	TestFalse(TEXT("Late delivery does not consume inventory"), PlayerA->GetQuickBarComponent()->GetSlot(0).IsEmpty());
	const int32 BeforePenalty = State->GetShopReputationPoints();
	Orders->TickComponent(0.25f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Expired customer departs"), Orders->FindOrder(Order.OrderId)->Status == EBotanicusSpecialOrderStatus::Expired);
	TestTrue(TEXT("Failure lowers reputation"), State->GetShopReputationPoints() < BeforePenalty);
	const int32 ReviewsAfterExpiry = State->GetTotalVisitorReviews();
	Orders->TickComponent(0.25f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Failure penalty exactly once"), State->GetTotalVisitorReviews(), ReviewsAfterExpiry);

	for (int32 Index = 0; Index < 20; ++Index)
	{
		FBotanicusSpecialOrder Generated;
		TestTrue(TEXT("Current catalogue produces feasible requests"), Orders->BuildRequest(Generated));
		TestTrue(TEXT("Generated request has lines and reward"), !Generated.Requirements.IsEmpty() && Generated.RewardCredits > 0);
	}

	// Exercise the real counter arrival transition, including the one-shot multiplayer bell.
	Orders->Orders.Reset();
	Orders->NextOfferServerTime = 0.0f;
	State->SetMainShopOpen(true);
	auto* Settings = GetMutableDefault<UBotanicusSpecialOrderSettings>();
	TGuardValue<float> GuaranteedOffer(Settings->VisitorChance, 1.0f);
	auto* Arriving = World->SpawnActor<ABotanicusVisitorCharacter>(FVector(0,0,100), FRotator::ZeroRotator, Spawn);
	if (!TestNotNull(TEXT("Arriving customer"), Arriving)) return false;
	// The corner is deliberate: a direct parking-to-counter implementation would
	// skip it and reproduce the wall-crossing regression from the real shop.
	const TArray<FVector> Route = {FVector(0,0,100), FVector(0,500,100), FVector(500,500,100), FVector(1000,500,100), FVector(1000,0,100)};
	Arriving->InitializeQueuedCircuit(Route, 2, Route[0], {Route[0], Route[1]}, {Route[3], Route[4]});
	Arriving->AdmitFromQueue();
	TestFalse(TEXT("No offer without operational counter"), Orders->TryAssignVisitor(Arriving));
	auto* Counter = World->SpawnActor<ABotanicusCashRegisterActor>(FVector(1000,0,0), FRotator::ZeroRotator, Spawn);
	auto* Zone = World->SpawnActor<ABotanicusVisitorZoneActor>(FVector(1000,0,0), FRotator::ZeroRotator, Spawn);
	if (!TestNotNull(TEXT("Counter"), Counter) || !TestNotNull(TEXT("Checkout zone"), Zone)) return false;
	Zone->InitializeZone(EBotanicusVisitorZoneType::Checkout, FVector(500,400,8));
	if (!TestTrue(TEXT("Eligible visitor gets an offer"), Orders->TryAssignVisitor(Arriving))) return false;
	TestEqual(TEXT("Special customer keeps complete NPC route"), Arriving->RoutePoints.Num(), Route.Num());
	TestEqual(TEXT("Special customer first follows parking waypoint"), Arriving->RoutePoints[0], Route[0]);
	TestEqual(TEXT("Special customer follows entrance corner before checkout"), Arriving->RoutePoints[1], Route[1]);
	TestEqual(TEXT("Special customer retains checkout waypoint index"), Arriving->CheckoutWaypointIndex, 2);
	TestFalse(TEXT("No bell before reaching counter"), Arriving->TryRingSpecialOrderBell());
	for (int32 Step = 0; Step < Route.Num() && Arriving->VisitorState == EBotanicusVisitorState::FollowingRoute; ++Step)
	{
		if (!Arriving->RoutePoints.IsValidIndex(Arriving->RouteWaypointIndex)) break;
		Arriving->SetActorLocation(Arriving->RoutePoints[Arriving->RouteWaypointIndex]);
		Arriving->FollowRoute(0.1f);
	}
	TestTrue(TEXT("Checkout waypoint leads to special counter position"), Arriving->VisitorState == EBotanicusVisitorState::SpecialOrderApproaching);
	TestNull(TEXT("Special customer never browses a display"), Arriving->TargetDisplay.Get());
	Arriving->SetActorLocation(Arriving->SpecialOrderStandLocation);
	Arriving->Tick(0.1f);
	TestTrue(TEXT("Customer waits after arrival"), Arriving->IsWaitingForSpecialOrder());
	TestTrue(TEXT("Arrival offers request for acceptance"), Orders->FindOrder(Arriving->SpecialOrderId)->Status == EBotanicusSpecialOrderStatus::Offered);
	const auto OfferPrompt = Arriving->GetInteractionPrompt_Implementation(PlayerA);
	TestEqual(TEXT("HUD interaction offers taking the order"), OfferPrompt.ActionText.ToString(), FString(TEXT("POUR PRENDRE LA COMMANDE")));
	TestTrue(TEXT("HUD interaction identifies special-order customer"), OfferPrompt.TargetName.ToString().Contains(TEXT("commande spéciale")));
	TestTrue(TEXT("Unanswered customer allows exactly two minutes"),
		FMath::IsNearlyEqual(Orders->GetRemainingSeconds(*Orders->FindOrder(Arriving->SpecialOrderId)), 120.0f));
	TestTrue(TEXT("Bell dispatched on arrival"), Arriving->bSpecialOrderBellRung);
	TestFalse(TEXT("Same arrival cannot ring twice"), Arriving->TryRingSpecialOrderBell());
	TestFalse(TEXT("Special customer excluded from normal queue"), Arriving->IsWaitingForCheckoutAssignment());
	TestTrue(TEXT("Waiting customer can be targeted"), Arriving->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Visibility) == ECR_Block);
	Arriving->Tick(6.0f);
	TestTrue(TEXT("Waiting is not treated as stuck movement"), Arriving->IsWaitingForSpecialOrder());
	TestFalse(TEXT("Waiting ticks cannot ring again"), Arriving->TryRingSpecialOrderBell());
	TestTrue(TEXT("First reminder scheduled one minute after arrival"),
		FMath::IsNearlyEqual(Arriving->NextSpecialOrderBellTime - World->GetTimeSeconds(), 60.0f));
	TestFalse(TEXT("No reminder before one minute"), Arriving->TryRingSpecialOrderBellReminder());
	Arriving->NextSpecialOrderBellTime = World->GetTimeSeconds();
	const float OriginalDeadline = Orders->FindOrder(Arriving->SpecialOrderId)->DeadlineServerTime;
	TestTrue(TEXT("Unaccepted order rings again when minute is due"), Arriving->TryRingSpecialOrderBellReminder());
	TestFalse(TEXT("A reminder cannot repeat during the same minute"), Arriving->TryRingSpecialOrderBellReminder());
	TestEqual(TEXT("Reminder does not extend customer patience"),
		Orders->FindOrder(Arriving->SpecialOrderId)->DeadlineServerTime, OriginalDeadline);
	TestTrue(TEXT("Following reminder scheduled another minute later"),
		FMath::IsNearlyEqual(Arriving->NextSpecialOrderBellTime - World->GetTimeSeconds(), 60.0f));
	Arriving->NextSpecialOrderBellTime = World->GetTimeSeconds();
	Orders->Orders[0].DeadlineServerTime = Orders->ServerTime();
	TestFalse(TEXT("No reminder after deadline even before expiry tick"), Arriving->TryRingSpecialOrderBellReminder());
	Orders->Orders[0].DeadlineServerTime = OriginalDeadline;
	PlayerA->SetActorLocation(Arriving->GetActorLocation() + FVector(100,0,0));
	Orders->Interact(Arriving->SpecialOrderId, PlayerA);
	TestTrue(TEXT("Order accepted before due reminder"), Orders->FindOrder(Arriving->SpecialOrderId)->Status == EBotanicusSpecialOrderStatus::Active);
	TestFalse(TEXT("Accepting order stops all reminder rings"), Arriving->TryRingSpecialOrderBellReminder());
	const FGuid ArrivalId = Arriving->SpecialOrderId;
	Counter->Destroy();
	Arriving->Tick(0.1f);
	TestTrue(TEXT("Removing counter cancels pending request"), Orders->FindOrder(ArrivalId)->Status == EBotanicusSpecialOrderStatus::Cancelled);
	TestFalse(TEXT("Departing customer cannot ring"), Arriving->TryRingSpecialOrderBell());
	TestFalse(TEXT("Departing customer cannot trigger reminder"), Arriving->TryRingSpecialOrderBellReminder());

	auto* IgnoredVisitor = World->SpawnActor<ABotanicusVisitorCharacter>(FVector(1000,0,100), FRotator::ZeroRotator, Spawn);
	if (!TestNotNull(TEXT("Unanswered customer"), IgnoredVisitor)) return false;
	FBotanicusSpecialOrder Ignored = Order;
	Ignored.OrderId = FGuid::NewGuid();
	Ignored.Status = EBotanicusSpecialOrderStatus::Offered;
	Ignored.DeadlineServerTime = Orders->ServerTime(); // The two-minute acceptance window has elapsed.
	Ignored.Customer = IgnoredVisitor;
	IgnoredVisitor->VisitorState = EBotanicusVisitorState::SpecialOrderWaiting;
	IgnoredVisitor->SpecialOrderId = Ignored.OrderId;
	Orders->Orders.Add(Ignored);
	const int32 ReputationBeforeIgnored = State->GetShopReputationPoints();
	const int32 ReviewsBeforeIgnored = State->GetTotalVisitorReviews();
	Orders->TickComponent(0.25f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Unanswered order expires at the patience deadline"), Orders->FindOrder(Ignored.OrderId)->Status == EBotanicusSpecialOrderStatus::Expired);
	TestFalse(TEXT("Unanswered customer leaves the counter"), IgnoredVisitor->IsWaitingForSpecialOrder());
	TestEqual(TEXT("Unanswered customer leaves unhappy with minus two reputation"), State->GetShopReputationPoints(), ReputationBeforeIgnored - 2);
	Orders->TickComponent(0.25f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Unanswered customer records exactly one review"), State->GetTotalVisitorReviews(), ReviewsBeforeIgnored + 1);

	auto* Bell = Cast<USoundWave>(Arriving->SpecialOrderBellSound);
	if (TestNotNull(TEXT("Provided bell is a hard-referenced cooked asset"), Bell))
	{
		TestTrue(TEXT("Bell has audio data and is not looping"), Bell->GetDuration() > 0.0f && !Bell->bLooping);
		if (TestNotNull(TEXT("Bell has distance attenuation"), Bell->AttenuationSettings.Get()))
		{
			const auto& Attenuation = Bell->AttenuationSettings->Attenuation;
			TestTrue(TEXT("Bell is spatialized with finite audible range"), Attenuation.bSpatialize && Attenuation.bAttenuate &&
				Attenuation.FalloffDistance > 0.0f && Attenuation.FalloffDistance <= 3000.0f);
		}
	}
	UClass* HudClass = LoadClass<UBotanicusHudLayoutWidget>(nullptr,
		TEXT("/Game/Botanicus/UI/HUD/WBP_BotanicusHUD.WBP_BotanicusHUD_C"));
	auto* HudGenerated = Cast<UWidgetBlueprintGeneratedClass>(HudClass);
	if (TestNotNull(TEXT("Authored HUD loads"), HudGenerated))
	{
		auto* TreeOwner = HudGenerated->FindWidgetTreeOwningClass();
		auto* Tree = TreeOwner ? TreeOwner->GetWidgetTreeArchetype() : nullptr;
		TestTrue(TEXT("Authored HUD supports special-order slot or canvas fallback"), Tree &&
			(Tree->FindWidget(TEXT("SpecialOrdersSlot")) || Cast<UCanvasPanel>(Tree->RootWidget)));
		auto* ObjectiveHost = Tree ? Cast<UNamedSlot>(Tree->FindWidget(TEXT("ObjectivesSlot"))) : nullptr;
		TestTrue(TEXT("Accepted orders can anchor directly below authored objectives"), ObjectiveHost &&
			Cast<UCanvasPanel>(ObjectiveHost->GetParent()) && Cast<UCanvasPanelSlot>(ObjectiveHost->Slot));
	}
	return true;
}
#endif
