#include "Audio/BotanicusAudioValidation.h"
#include "Audio/BotanicusAudioWorldSubsystem.h"
#include "Audio/BotanicusAudioRelay.h"
#include "BotanicusCharacter.h"
#include "BotanicusPlayerController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Growing/BotanicusPlantPotActor.h"
#include "Growing/BotanicusPlantSubsystem.h"
#include "IllegalTrade/BotanicusIllegalPlanterActor.h"
#include "QuickBar/BotanicusQuickBarComponent.h"
#include "Sales/BotanicusSalesDisplayActor.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
UBotanicusAudioWorldSubsystem* UBotanicusAudioValidation::GetAudio(UObject* Context)
{
	UWorld* World=GEngine->GetWorldFromContextObject(Context,EGetWorldErrorMode::ReturnNull);
	return World ? World->GetSubsystem<UBotanicusAudioWorldSubsystem>() : nullptr;
}
bool UBotanicusAudioValidation::Step(UObject* Context,FName Action)
{
#if WITH_EDITOR
	UWorld* World=GEngine->GetWorldFromContextObject(Context,EGetWorldErrorMode::ReturnNull);
	FString Saved=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());FPaths::NormalizeDirectoryName(Saved);
	if (!World || World->WorldType!=EWorldType::PIE || !FParse::Param(FCommandLine::Get(),TEXT("BotanicusAudioValidation")) || !Saved.Contains(TEXT("/isolated_user/"))) return false;
	// Python editor callbacks set this global and force every Actor RPC local.
	// Restore normal PIE routing for the duration of this isolated native scenario.
	TGuardValue<bool> RuntimeRouting(GAllowActorScriptExecutionInEditor,false);
	using Audio=UBotanicusAudioWorldSubsystem;
	Audio* System=GetAudio(Context);if(!System) return false;
	APlayerController* BasePC=World->GetFirstPlayerController();
	ABotanicusPlayerController* PC=Cast<ABotanicusPlayerController>(BasePC);
	ABotanicusCharacter* Pawn=PC ? Cast<ABotanicusCharacter>(PC->GetPawn()) : nullptr;
	if (!Pawn) return false;
	if (Action==TEXT("ClientEmitRejected")) {Audio::Emit(Pawn,TEXT("EquipmentPlace"),Pawn->GetActorLocation());return true;}
	if (World->GetNetMode()==NM_Client) return false;
	ABotanicusAudioRelay* Relay=System->GetRelay(); if(!Relay) return false;
	if(Action==TEXT("Quiesce")) {for(TActorIterator<ABotanicusPlantPotActor> It(World);It;++It) It->SetActorTickEnabled(false);for(TActorIterator<ABotanicusIllegalPlanterActor> It(World);It;++It) It->SetActorTickEnabled(false);return true;}
	if (Action==TEXT("Relay")) {Relay->Emit(TEXT("EquipmentPlace"),Pawn->GetActorLocation());return true;}
	if (Action==TEXT("Dedupe")) {System->ReceiveFeedback(100000,TEXT("PotPlace"),Pawn->GetActorLocation());System->ReceiveFeedback(100000,TEXT("PotPlace"),Pawn->GetActorLocation());return true;}
	if (Action==TEXT("DedupeDelayed")) {System->ReceiveFeedback(100000,TEXT("PotPlace"),Pawn->GetActorLocation());return true;}
	if (Action==TEXT("SaleOwner")) {PC->CreditPlantSale(TEXT("AudioTestPlant"),1);return true;}
	if (Action==TEXT("SaleRemote")) {for(FConstPlayerControllerIterator It=World->GetPlayerControllerIterator();It;++It) if(auto* Remote=Cast<ABotanicusPlayerController>(It->Get());Remote && !Remote->IsLocalController()) {Remote->CreditPlantSale(TEXT("AudioTestPlant"),1);return true;}return false;}
	if (Action==TEXT("Purchase")) {const int32 Before=PC->GetPendingOrders().Num();PC->BotanicusOrder(TEXT("PottingSoil"));return PC->GetPendingOrders().Num()>Before;}
	if (Action==TEXT("WaterPulse")) {Audio::WaterPulse(Pawn);return true;}
	if (Action==TEXT("WaterEnd")) {Audio::WaterEnd(Pawn);return true;}
	if (Action==TEXT("WaterFourPulse") || Action==TEXT("WaterFourEnd")) {for(FConstPlayerControllerIterator It=World->GetPlayerControllerIterator();It;++It) if(APawn* Source=It->Get()->GetPawn()) {if(Action==TEXT("WaterFourPulse")) Audio::WaterPulse(Source);else Audio::WaterEnd(Source);}return true;}
	UBotanicusQuickBarComponent* Bar=Pawn->GetQuickBarComponent();
	UBotanicusPlantSubsystem* Plants=World->GetGameInstance()->GetSubsystem<UBotanicusPlantSubsystem>();
	if (!Bar || !Plants) return false;
	const auto Definitions=Plants->GetAllPlants();
	const auto* Definition=Definitions.FindByPredicate([](const auto& D){return D.Element==EBotanicusPlantElement::Normal && !D.HarvestToolItemKey.IsNone();});
	if(!Definition) return false;
	ABotanicusPlantPotActor* Pot=nullptr;
	for(TActorIterator<ABotanicusPlantPotActor> It(World);It;++It) if(It->ActorHasTag(TEXT("BotanicusAudioTest"))) {Pot=*It;break;}
	if(Action==TEXT("Plant"))
	{
		for(int32 I=0;I<Bar->SlotCount;++I) {const auto S=Bar->GetSlot(I);if(!S.IsEmpty()) Bar->RemoveQuantity(I,S.Quantity);}
		Pot=World->SpawnActor<ABotanicusPlantPotActor>();if(!Pot) return false;
		Pot->Tags.Add(TEXT("BotanicusAudioTest"));Pot->InitializePlacedItem(TEXT("PlantPot"),1);Pot->SetActorTickEnabled(false);
		Pot->SetActorLocation(Pawn->GetPawnViewLocation()+Pawn->GetActorForwardVector()*150.f-FVector(0,0,30));
		Pot->RestoreGrowingState(true,NAME_None,.3f,0,0);
		int32 Slot; if(!Bar->AddItem(Definition->SeedItemKey,2,Slot)) return false;Bar->SelectSlotAuthoritative(Slot);
		Pot->BeginPrimaryUse(Pawn);return Pot->GetPlantKey()==Definition->PlantKey;
	}
	if(!Pot) return false;
	if(Action==TEXT("GrowthYoung") || Action==TEXT("GrowthLarge") || Action==TEXT("Mature"))
	{
		const float Target=Action==TEXT("GrowthYoung") ? .3f : Action==TEXT("GrowthLarge") ? .7f : 1.f;
		Pot->RestoreGrowingState(true,Definition->PlantKey,(Definition->MinimumHealthyWater+Definition->MaximumHealthyWater)*.5f,Target-.00015f,.5f);
		Pot->Tick(1.f);return Pot->GetGrowthProgress()>=Target-KINDA_SMALL_NUMBER;
	}
	FVector Origin,Extent;Pot->GetActorBounds(true,Origin,Extent);PC->SetControlRotation((Origin-Pawn->GetPawnViewLocation()).Rotation());
	if(Action==TEXT("RejectedPlant")) {Pot->BeginPrimaryUse(Pawn);return true;}
	if(Action==TEXT("WaterGameplay"))
	{
		FBotanicusCarriedItemState State;State.bHasWateringCanState=true;State.WateringCanWaterLevel=1;
		int32 Slot;if(!Bar->AddUniqueItem(TEXT("WateringCan"),State,Slot)) return false;Bar->SelectSlotAuthoritative(Slot);
		const float Before=Pot->GetWaterLevel();Pot->BeginPrimaryUse(Pawn);Pot->Tick(.1f);return Pot->GetWaterLevel()>Before;
	}
	if(Action==TEXT("WaterGameplayEnd")) {Pot->EndPrimaryUse(Pawn);return true;}
	if(Action==TEXT("Harvest"))
	{
		Pot->EndPrimaryUse(Pawn);Pot->RestoreGrowingState(true,Definition->PlantKey,.5f,1.f,1.f);
		int32 Slot;if(!Bar->AddItem(Definition->HarvestToolItemKey,1,Slot)) return false;Bar->SelectSlotAuthoritative(Slot);
		Pot->BeginPrimaryUse(Pawn);return Pot->GetPlantKey().IsNone();
	}
	if(Action==TEXT("Listed"))
	{
		auto* Display=World->SpawnActor<ABotanicusSalesDisplayActor>();if(!Display) return false;
		Display->SetActorLocation(Pot->GetActorLocation());return Display->TryMountSalePotState(Definition->CompatibleSaleSoilItemKey,Definition->HarvestItemKey,PC,TEXT("SalePot"));
	}
#endif
	return false;
}
