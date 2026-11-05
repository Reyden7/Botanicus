#include "Editor/BotanicusJuiceValidation.h"
#include "Visuals/BotanicusJuiceWorldSubsystem.h"
#include "Visuals/BotanicusJuiceSettings.h"
#include "Visuals/BotanicusPotSoilVisualActor.h"
#include "Audio/BotanicusAudioWorldSubsystem.h"
#include "BotanicusCharacter.h"
#include "BotanicusPlayerController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Growing/BotanicusPlantPotActor.h"
#include "Growing/BotanicusMultiPlantPotActor.h"
#include "Growing/BotanicusPlantSubsystem.h"
#include "IllegalTrade/BotanicusIllegalPlanterActor.h"
#include "IllegalTrade/BotanicusIllegalTradeSettings.h"
#include "QuickBar/BotanicusQuickBarComponent.h"
#include "Components/MeshComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#if WITH_EDITOR
#include "UnrealClient.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "HighResScreenshot.h"
#endif
namespace
{
FName TestTargetName;
bool IsJuiceTest(UWorld* World)
{
#if WITH_EDITOR
	FString Saved=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());FPaths::NormalizeDirectoryName(Saved);
	return World && World->WorldType==EWorldType::PIE && FParse::Param(FCommandLine::Get(),TEXT("BotanicusJuiceValidation")) && Saved.Contains(TEXT("/isolated_user/"));
#else
	return false;
#endif
}
}
UBotanicusJuiceWorldSubsystem* UBotanicusJuiceValidation::GetJuice(UObject* Context)
{
	UWorld* World=GEngine->GetWorldFromContextObject(Context,EGetWorldErrorMode::ReturnNull);return World ? World->GetSubsystem<UBotanicusJuiceWorldSubsystem>() : nullptr;
}
float UBotanicusJuiceValidation::ReadWetness(UObject* Context)
{
#if WITH_EDITOR
	UWorld* World=GEngine->GetWorldFromContextObject(Context,EGetWorldErrorMode::ReturnNull);if(!IsJuiceTest(World)) return -1;
	for(TActorIterator<AActor> It(World);It;++It) if(It->GetFName()==TestTargetName)
	{
		TArray<UMeshComponent*> Meshes;It->GetComponents(Meshes);
		for(auto* Mesh:Meshes) if(Mesh->GetFName()==TEXT("MultiSoil")) if(auto* MID=Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0))) return MID->K2_GetScalarParameterValue(TEXT("Wetness"));
		TArray<UChildActorComponent*> Children;It->GetComponents(Children);
		for(auto* C:Children) if(auto* Soil=Cast<ABotanicusPotSoilVisualActor>(C->GetChildActor()))
		{
			Soil->GetComponents(Meshes);for(auto* Mesh:Meshes) if(auto* MID=Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0))) return MID->K2_GetScalarParameterValue(TEXT("Wetness"));
		}
	}
#endif
	return -1;
}
bool UBotanicusJuiceValidation::Step(UObject* Context,FName Action)
{
#if WITH_EDITOR
	UWorld* World=GEngine->GetWorldFromContextObject(Context,EGetWorldErrorMode::ReturnNull);if(!IsJuiceTest(World)) return false;
	TGuardValue<bool> Routing(GAllowActorScriptExecutionInEditor,false);
	auto* PC=Cast<ABotanicusPlayerController>(World->GetFirstPlayerController());
	auto* Pawn=PC ? Cast<ABotanicusCharacter>(PC->GetPawn()) : nullptr;if(!Pawn) return false;
	if(Action==TEXT("InputOn") || Action==TEXT("InputOff")) {PC->bWateringCanSprayHeld=Action==TEXT("InputOn");return true;}
	if(Action==TEXT("EffectsOn") || Action==TEXT("EffectsOff")) {GetMutableDefault<UBotanicusJuiceSettings>()->bEnabled=Action==TEXT("EffectsOn");return true;}
	if(World->GetNetMode()==NM_Client) return false;
	if(Action==TEXT("Quiesce"))
	{
		for(TActorIterator<ABotanicusPlantPotActor> It(World);It;++It) It->SetActorTickEnabled(false);
		for(TActorIterator<ABotanicusIllegalPlanterActor> It(World);It;++It) It->SetActorTickEnabled(false);
		return true;
	}
	using Juice=UBotanicusJuiceWorldSubsystem;
	using Audio=UBotanicusAudioWorldSubsystem;
	UBotanicusQuickBarComponent* Bar=Pawn->GetQuickBarComponent();if(!Bar) return false;
	auto Clear=[Bar](){for(int32 I=0;I<Bar->SlotCount;++I) {const auto S=Bar->GetSlot(I);if(!S.IsEmpty()) Bar->RemoveQuantity(I,S.Quantity);}};
	auto Select=[Bar](FName Key,int32 Quantity=1){int32 Slot;if(!Bar->AddItem(Key,Quantity,Slot)) return false;Bar->SelectSlotAuthoritative(Slot);return true;};
	AActor* Target=nullptr;for(TActorIterator<AActor> It(World);It;++It) if(It->GetFName()==TestTargetName) {Target=*It;break;}
	const auto Definitions=World->GetGameInstance()->GetSubsystem<UBotanicusPlantSubsystem>()->GetAllPlants();
	const auto* Definition=Definitions.FindByPredicate([](const auto& D){return D.Element==EBotanicusPlantElement::Normal;});if(!Definition) return false;
	if(Action==TEXT("SetupSimple") || Action==TEXT("SetupMulti") || Action==TEXT("SetupIllegal"))
	{
		if(Target) Target->Destroy();Clear();
		UClass* Class=Action==TEXT("SetupMulti") ? ABotanicusMultiPlantPotActor::StaticClass() : Action==TEXT("SetupIllegal") ? ABotanicusIllegalPlanterActor::StaticClass() : LoadClass<ABotanicusPlantPotActor>(nullptr,TEXT("/Game/Botanicus/blueprints/BP_Item_PlantPot.BP_Item_PlantPot_C"));
		Target=World->SpawnActor<AActor>(Class);if(!Target) return false;
		Target->Tags.Add(TEXT("JuiceTarget"));TestTargetName=Target->GetFName();Target->SetActorTickEnabled(false);
		Target->SetActorLocation(Pawn->GetPawnViewLocation()+Pawn->GetActorForwardVector()*180.f-FVector(0,0,80));
		if(auto* Multi=Cast<ABotanicusMultiPlantPotActor>(Target)) {Multi->InitializePlacedItem(TEXT("PreparationPlanter2"),1);Multi->RestoreMultiPlantState(2,{});}
		else if(auto* Pot=Cast<ABotanicusPlantPotActor>(Target)) {Pot->InitializePlacedItem(TEXT("PlantPot"),1);Pot->RestoreGrowingState(true,NAME_None,0,0,0);}
		else if(auto* Illegal=Cast<ABotanicusIllegalPlanterActor>(Target)) {Illegal->InitializePlacedItem(TEXT("IllegalPlanter"),1);Illegal->RestoreIllegalPlanterState(1,4,0,{});}
		const FVector Surface=Juice::SoilSurface(Target,Target->GetActorLocation()+FVector(0,0,25));PC->SetControlRotation((Surface-Pawn->GetPawnViewLocation()).Rotation());
		return Select(Action==TEXT("SetupIllegal") ? FName(TEXT("SeedPacket_Noctiflore")) : Definition->SeedItemKey,3);
	}
	if(Action==TEXT("WaterFourPulse"))
	{
		for(FConstPlayerControllerIterator It=World->GetPlayerControllerIterator();It;++It) if(auto* Character=Cast<ABotanicusCharacter>(It->Get()->GetPawn())) Audio::WaterPulse(Character);
		return true;
	}
	if(Action==TEXT("WaterFourPrepare"))
	{
		for(FConstPlayerControllerIterator It=World->GetPlayerControllerIterator();It;++It) if(auto* Character=Cast<ABotanicusCharacter>(It->Get()->GetPawn()))
		{
			auto* Inventory=Character->GetQuickBarComponent();FBotanicusCarriedItemState State;State.bHasWateringCanState=true;State.WateringCanWaterLevel=1;int32 Slot;
			if(!Inventory->AddUniqueItem(TEXT("WateringCan"),State,Slot)) return false;Inventory->SelectSlotAuthoritative(Slot);
		}
		return true;
	}
	if(Action==TEXT("WaterFourEnd")) {for(FConstPlayerControllerIterator It=World->GetPlayerControllerIterator();It;++It) Audio::WaterEnd(It->Get()->GetPawn());return true;}
	if(Action==TEXT("RefillFour")) {for(FConstPlayerControllerIterator It=World->GetPlayerControllerIterator();It;++It) if(auto* Character=Cast<ABotanicusCharacter>(It->Get()->GetPawn())) {auto* Inventory=Character->GetQuickBarComponent();auto State=Inventory->GetSelectedSlot().CarriedState;State.bHasWateringCanState=true;State.WateringCanWaterLevel=1;Inventory->SetCarriedItemState(Inventory->GetSelectedSlotIndex(),State);}return true;}
	if(!Target) return false;
	const FVector Surface=Juice::SoilSurface(Target,Target->GetActorLocation()+FVector(0,0,25));PC->SetControlRotation((Surface-Pawn->GetPawnViewLocation()).Rotation());
	auto Begin=[&](){if(auto* Illegal=Cast<ABotanicusIllegalPlanterActor>(Target)) Illegal->BeginPrimaryUse(Pawn);else CastChecked<ABotanicusPlantPotActor>(Target)->BeginPrimaryUse(Pawn);};
	auto End=[&](){if(auto* Illegal=Cast<ABotanicusIllegalPlanterActor>(Target)) Illegal->EndPrimaryUse(Pawn);else CastChecked<ABotanicusPlantPotActor>(Target)->EndPrimaryUse(Pawn);};
	if(Action==TEXT("Plant"))
	{
		const int32 Quantity=Bar->GetSelectedSlot().Quantity;Begin();return Bar->GetSelectedSlot().Quantity==Quantity-1;
	}
	if(Action==TEXT("RejectedPlant")) {Begin();return true;}
	if(Action==TEXT("WaterPrepare"))
	{
		FBotanicusCarriedItemState State;State.bHasWateringCanState=true;State.WateringCanWaterLevel=1;int32 Slot;
		if(!Bar->AddUniqueItem(TEXT("WateringCan"),State,Slot)) return false;Bar->SelectSlotAuthoritative(Slot);return true;
	}
	if(Action==TEXT("WaterBegin")) {PC->LocalActivePlantPot=Cast<ABotanicusPlantPotActor>(Target);PC->LocalActiveIllegalPlanter=Cast<ABotanicusIllegalPlanterActor>(Target);Begin();Target->Tick(.1f);return true;}
	if(Action==TEXT("WaterMaintain")) {Target->Tick(.01f);return true;}
	if(Action==TEXT("WaterEnd")) {PC->bWateringCanSprayHeld=false;End();return true;}
	if(Action==TEXT("ToolChange")) {PC->bWateringCanSprayHeld=false;Select(TEXT("GardenTrowel"));Target->Tick(.1f);return true;}
	if(Action==TEXT("DestroyPot")) {PC->bWateringCanSprayHeld=false;Target->Destroy();return true;}
	if(Action==TEXT("WaterFree")) {Audio::WaterPulse(Pawn);return true;}
	if(Action==TEXT("Wet") || Action==TEXT("Dry"))
	{
		const float Level=Action==TEXT("Wet") ? 1.f : 0.f;
		if(auto* Multi=Cast<ABotanicusMultiPlantPotActor>(Target)) {auto Slots=Multi->GetPlantSlots();for(auto& Slot:Slots) if(!Slot.PlantKey.IsNone()) Slot.WaterLevel=Level;Multi->RestoreMultiPlantState(Multi->GetSoilUnits(),Slots);}
		else if(auto* Pot=Cast<ABotanicusPlantPotActor>(Target)) Pot->RestoreGrowingState(true,Pot->GetPlantKey(),Level,Pot->GetGrowthProgress(),.1f);
		else if(auto* Illegal=Cast<ABotanicusIllegalPlanterActor>(Target)) {auto Slots=Illegal->GetPlantSlots();for(auto& Slot:Slots) Slot.WaterAmount=Level;Illegal->RestoreIllegalPlanterState(Illegal->GetPlanterLevel(),Illegal->GetSoilUnits(),0,Slots);}
		return true;
	}
#endif
	return false;
}
void UBotanicusJuiceValidation::Capture(UObject* Context,const FString& Filename)
{
#if WITH_EDITOR
	UWorld* World=GEngine->GetWorldFromContextObject(Context,EGetWorldErrorMode::ReturnNull);if(!IsJuiceTest(World)) return;
	if(auto* PC=World->GetFirstPlayerController()) if(auto* Local=PC->GetLocalPlayer()) if(UGameViewportClient* Viewport=Local->ViewportClient)
	{
		auto& Config=GetHighResScreenshotConfig();Config.FilenameOverride=Filename;Config.SetResolution(1280,720,1);Viewport->Viewport->TakeHighResScreenShot();
	}
#endif
}
TArray<float> UBotanicusJuiceValidation::ReadFrameMetrics(UObject* Context)
{
#if WITH_EDITOR
	UWorld* World=GEngine->GetWorldFromContextObject(Context,EGetWorldErrorMode::ReturnNull);if(!IsJuiceTest(World)) return {};
	if(auto* PC=World->GetFirstPlayerController()) if(auto* Local=PC->GetLocalPlayer()) if(UGameViewportClient* View=Local->ViewportClient) if(auto* Data=View->GetStatUnitData()) return {Data->RawFrameTime,Data->RawGameThreadTime,Data->RawRenderThreadTime,Data->RawGPUFrameTime[0]};
#endif
	return {};
}
