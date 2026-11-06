#include "Editor/BotanicusEquipmentValidation.h"
#include "BotanicusPlayerController.h"
#include "BotanicusCharacter.h"
#include "Camera/PlayerCameraManager.h"
#include "BotanicusGameMode.h"
#include "Catalog/BotanicusItemCatalogSubsystem.h"
#include "Growing/BotanicusPlantPotActor.h"
#include "Growing/BotanicusWaterReserveActor.h"
#include "Growing/BotanicusPlantSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/NetConnection.h"
#include "Visuals/BotanicusJuiceWorldSubsystem.h"
#include "Audio/BotanicusAudioWorldSubsystem.h"
#include "Growing/BotanicusWateringCanActor.h"
#include "QuickBar/BotanicusQuickBarComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#if WITH_EDITOR
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#endif

#if WITH_EDITOR
namespace
{
struct FEquipmentTrace
{
	TMap<FString,int64> Counts;
	FDelegateHandle Spawn,Destroy;
};
TMap<TWeakObjectPtr<UWorld>,FEquipmentTrace> Traces;
bool bLegacyMode=true;
bool bComponentCensus=true;
FName ReserveName;
TWeakObjectPtr<ABotanicusPlayerController> Victim;
FBotanicusItemDefinition OriginalDefinition;
bool bDefinitionChanged=false;
bool IsEquipmentTest(UWorld* World)
{
	FString Saved=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());FPaths::NormalizeDirectoryName(Saved);
	return World && World->WorldType==EWorldType::PIE && FParse::Param(FCommandLine::Get(),TEXT("BotanicusEquipmentValidation")) && Saved.Contains(TEXT("/isolated_user/"));
}
UWorld* TestWorld(UObject* Context)
{
	UWorld* W=GEngine ? GEngine->GetWorldFromContextObject(Context,EGetWorldErrorMode::ReturnNull) : nullptr;return IsEquipmentTest(W) ? W : nullptr;
}
void Increment(UWorld* W,const TCHAR* Event,int64 Amount=1)
{
	if(auto* Trace=Traces.Find(W)) Trace->Counts.FindOrAdd(Event)+=Amount;
}
FString Name(const UObject* Object) {return IsValid(Object) ? Object->GetName() : FString();}
}
bool UBotanicusEquipmentValidation::UseLegacy(UWorld* World) {return Traces.Contains(World) && bLegacyMode;}
bool UBotanicusEquipmentValidation::IsTracing(UWorld* World) {return Traces.Contains(World);}
void UBotanicusEquipmentValidation::RecordDuration(UWorld* World,double StartSeconds)
{
	Increment(World,TEXT("EquipmentUpdateCalls"));
	Increment(World,TEXT("EquipmentUpdateNanoseconds"),static_cast<int64>((FPlatformTime::Seconds()-StartSeconds)*1.e9));
}

void UBotanicusEquipmentValidation::Record(UWorld* World,const TCHAR* Event,AActor* Actor)
{
	if(!Traces.Contains(World)) return;
	Increment(World,Event);
	if(Actor && bComponentCensus)
	{
		TArray<UActorComponent*> Components;Actor->GetComponents(Components);
		Increment(World,*(FString(Event)+TEXT("Components")),Components.Num());
	}
}
#endif
bool UBotanicusEquipmentValidation::Begin(UObject* Context)
{
#if WITH_EDITOR
	UWorld* W=TestWorld(Context);if(!W) return false;if(Traces.Contains(W)) return true;
	auto& Trace=Traces.Add(W);const TWeakObjectPtr<UWorld> Weak=W;
	Trace.Spawn=W->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateLambda([Weak](AActor* Actor)
	{if(Actor->IsA<ABotanicusWateringCanActor>()) Increment(Weak.Get(),TEXT("CanActorSpawned"));}));
	Trace.Destroy=W->AddOnActorDestroyedHandler(FOnActorDestroyed::FDelegate::CreateLambda([Weak](AActor* Actor)
	{
		if(Actor->IsA<ABotanicusWateringCanActor>())
		{
			Increment(Weak.Get(),TEXT("CanActorDestroyed"));
			Increment(Weak.Get(),Actor->ActorHasTag(TEXT("BotanicusEquippedPreview")) ? TEXT("LocalCanDestroyed") : Actor->ActorHasTag(TEXT("BotanicusJuiceRemotePreview")) ? TEXT("RemoteCanDestroyed") : TEXT("PhysicalCanDestroyed"));
		}
	}));
	return true;
#else
	return false;
#endif
}
void UBotanicusEquipmentValidation::Finish(UObject* Context)
{
#if WITH_EDITOR
	UWorld* W=TestWorld(Context);if(auto* T=Traces.Find(W)) {W->RemoveOnActorSpawnedHandler(T->Spawn);W->RemoveOnActorDestroyedHandler(T->Destroy);Traces.Remove(W);}
#endif
}
bool UBotanicusEquipmentValidation::SetLegacy(UObject* Context,bool bLegacy)
{
#if WITH_EDITOR
	if(!TestWorld(Context)) return false;bLegacyMode=bLegacy;return true;
#else
	return false;
#endif
}
void UBotanicusEquipmentValidation::ResetCounters(UObject* Context)
{
#if WITH_EDITOR
	if(auto* T=Traces.Find(TestWorld(Context))) T->Counts.Reset();
#endif
}
TMap<FString,int64> UBotanicusEquipmentValidation::ReadCounters(UObject* Context)
{
#if WITH_EDITOR
	if(auto* T=Traces.Find(TestWorld(Context))) return T->Counts;
#endif
	return {};
}
FString UBotanicusEquipmentValidation::Snapshot(UObject* Context)
{
#if WITH_EDITOR
	UWorld* W=TestWorld(Context);if(!W) return TEXT("{}");
	TSharedRef<FJsonObject> Row=MakeShared<FJsonObject>();
	if(auto* PC=Cast<ABotanicusPlayerController>(W->GetFirstPlayerController()))
	{
		Row->SetStringField(TEXT("pawn"),Name(PC->GetPawn()));
		ABotanicusPlaceableItemActor* Item=PC->LocalEquippedQuickBarItem;
		Row->SetStringField(TEXT("equipped"),Name(Item));
		Row->SetStringField(TEXT("class"),Item ? Item->GetClass()->GetPathName() : TEXT(""));
		Row->SetStringField(TEXT("scale"),Item ? Item->GetActorScale3D().ToString() : TEXT(""));
		Row->SetBoolField(TEXT("placement"),IsValid(PC->LocalQuickBarItemPreview));
		if(auto* Pot=Cast<ABotanicusPlantPotActor>(Item)) {Row->SetNumberField(TEXT("pot_growth"),Pot->GetGrowthProgress());Row->SetStringField(TEXT("pot_plant"),Pot->GetPlantKey().ToString());}
		Row->SetStringField(TEXT("item"),Item ? Item->GetItemKey().ToString() : TEXT(""));
		Row->SetStringField(TEXT("mesh"),Item && Item->Mesh ? Name(Item->Mesh->GetStaticMesh()) : TEXT(""));
		Row->SetBoolField(TEXT("preview_replicated"),Item && Item->GetIsReplicated());
		Row->SetBoolField(TEXT("preview_collision"),Item && Item->GetActorEnableCollision());
		if(auto* Can=Cast<ABotanicusWateringCanActor>(Item)) Row->SetNumberField(TEXT("preview_water"),Can->GetWaterLevel());
		if(auto* Character=Cast<ABotanicusCharacter>(PC->GetPawn()))
		{
			auto* Bar=Character->GetQuickBarComponent();const auto Selected=Bar->GetSelectedSlot();
			Row->SetNumberField(TEXT("slot"),Bar->GetSelectedSlotIndex());Row->SetNumberField(TEXT("quantity"),Selected.Quantity);
			Row->SetStringField(TEXT("slot_item"),Selected.ItemKey.ToString());Row->SetStringField(TEXT("instance"),Selected.InstanceId.ToString());
			Row->SetNumberField(TEXT("water"),Bar->GetSelectedWateringCanWaterLevel());
		}
	}
	int32 Players=0;for(FConstPlayerControllerIterator It=W->GetPlayerControllerIterator();It;++It) ++Players;
	Row->SetNumberField(TEXT("players"),Players);
	if(auto* J=W->GetSubsystem<UBotanicusJuiceWorldSubsystem>()) {Row->SetNumberField(TEXT("water_sources"),J->GetActiveWaterCount());Row->SetNumberField(TEXT("water_starts"),J->GetWaterStartCount());Row->SetNumberField(TEXT("burst_count"),J->GetActiveBurstCount());}
	if(auto* A=W->GetSubsystem<UBotanicusAudioWorldSubsystem>()) {Row->SetNumberField(TEXT("audio_water"),A->GetActiveWaterVoiceCount());Row->SetNumberField(TEXT("audio_starts"),A->GetWaterStartCount());}
	TArray<TSharedPtr<FJsonValue>> PhysicalRows;
	for(TActorIterator<ABotanicusWateringCanActor> It(W);It;++It) if(!It->ActorHasTag(TEXT("BotanicusPlacementPreview")) && !It->ActorHasTag(TEXT("BotanicusEquippedPreview")) && !It->ActorHasTag(TEXT("BotanicusJuiceRemotePreview")))
	{
		auto Can=MakeShared<FJsonObject>();Can->SetStringField(TEXT("actor"),Name(*It));Can->SetStringField(TEXT("carrier"),Name(It->GetCarrier()));
		Can->SetStringField(TEXT("attached"),Name(It->GetAttachParentActor()));Can->SetBoolField(TEXT("visible"),!It->IsHidden() && It->Mesh && It->Mesh->IsVisible());
		Can->SetBoolField(TEXT("replicated"),It->GetIsReplicated());PhysicalRows.Add(MakeShared<FJsonValueObject>(Can));
	}
	Row->SetArrayField(TEXT("physical"),PhysicalRows);
	int32 Local=0,Remote=0,Physical=0;
	for(TActorIterator<ABotanicusWateringCanActor> It(W);It;++It)
		if(It->ActorHasTag(TEXT("BotanicusEquippedPreview"))) ++Local;else if(It->ActorHasTag(TEXT("BotanicusJuiceRemotePreview"))) ++Remote;else ++Physical;
	Row->SetNumberField(TEXT("local_cans"),Local);Row->SetNumberField(TEXT("remote_cans"),Remote);Row->SetNumberField(TEXT("physical_cans"),Physical);
	FString JSON;auto Writer=TJsonWriterFactory<>::Create(&JSON);FJsonSerializer::Serialize(Row,Writer);return JSON;
#else
	return TEXT("{}");
#endif
}

bool UBotanicusEquipmentValidation::Select(UObject* Context,int32 Slot)
{
#if WITH_EDITOR
	UWorld* W=TestWorld(Context);if(!W || Slot<0 || Slot>=10) return false;
	TGuardValue<bool> Routing(GAllowActorScriptExecutionInEditor,false);
	auto* P=Cast<ABotanicusCharacter>(W->GetFirstPlayerController()->GetPawn());if(!P) return false;
	P->GetQuickBarComponent()->SelectSlot(Slot);return true;
#else
	return false;
#endif
}
bool UBotanicusEquipmentValidation::Step(UObject* Context,FName Action)
{
#if WITH_EDITOR
	UWorld* W=TestWorld(Context);if(!W) return false;
	TGuardValue<bool> Routing(GAllowActorScriptExecutionInEditor,false);
	auto* PC=Cast<ABotanicusPlayerController>(W->GetFirstPlayerController());if(!PC) return false;
	auto* P=Cast<ABotanicusCharacter>(PC->GetPawn());
	if(Action==TEXT("CountersOnly")) {bComponentCensus=false;return true;}
	if(Action==TEXT("InputOn") || Action==TEXT("InputOff")) {PC->bWateringCanSprayHeld=Action==TEXT("InputOn");return true;}
	if(Action==TEXT("DestroyPreview")) {if(!IsValid(PC->LocalEquippedQuickBarItem)) return false;return PC->LocalEquippedQuickBarItem->Destroy();}
	if(Action==TEXT("ChangeRenderedMesh"))
	{
		if(!IsValid(PC->LocalEquippedQuickBarItem)) return false;
		PC->LocalEquippedQuickBarItem->Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere")));
		PC->HandleEquippedQuickBarChanged();return true;
	}
	if(Action==TEXT("PlacementBegin")) {PC->BeginQuickBarItemPlacement();return IsValid(PC->LocalQuickBarItemPreview);}
	if(Action==TEXT("PlacementCancel")) {PC->CancelQuickBarItemPlacement();return !IsValid(PC->LocalQuickBarItemPreview);}
	if(Action==TEXT("RefillPulse") || Action==TEXT("RefillEnd"))
	{
		for(TActorIterator<ABotanicusWaterReserveActor> It(W);It;++It) if(It->GetFName()==ReserveName)
		{
			if(!P) return false;FVector Centre,Extent;It->GetActorBounds(true,Centre,Extent);PC->SetControlRotation((Centre-P->GetPawnViewLocation()).Rotation());
			if(Action==TEXT("RefillEnd")) PC->ServerEndWateringCanRefill(*It);else PC->ServerRefillWateringCan(*It);return true;
		}
		return false;
	}
	if(W->GetNetMode()==NM_Client) return false;
	if(Action==TEXT("Respawn")) {if(!Victim.IsValid()) return false;W->GetAuthGameMode()->RestartPlayer(Victim.Get());return IsValid(Victim->GetPawn());}
	if(Action==TEXT("DestroyRemotePawn") || Action==TEXT("Disconnect"))
	{
		for(FConstPlayerControllerIterator It=W->GetPlayerControllerIterator();It;++It) if(auto* C=Cast<ABotanicusPlayerController>(It->Get());C && !C->IsLocalController())
		{
			Victim=C;
			if(Action==TEXT("Disconnect")) {if(auto* Net=C->GetNetConnection()) {Net->Close();return true;}return false;}
			return C->GetPawn() && C->GetPawn()->Destroy();
		}
		return false;
	}
	if(!P) return false;auto* Bar=P->GetQuickBarComponent();
	auto Clear=[](UBotanicusQuickBarComponent* B){for(int32 I=0;I<10;++I){auto S=B->GetSlot(I);if(!S.IsEmpty()) B->RemoveQuantity(I,S.Quantity);}};
	if(Action==TEXT("SetupAll"))
	{
		for(FConstPlayerControllerIterator It=W->GetPlayerControllerIterator();It;++It) if(auto* Character=Cast<ABotanicusCharacter>(It->Get()->GetPawn()))
		{
			auto* B=Character->GetQuickBarComponent();Clear(B);int32 Index;FBotanicusCarriedItemState S;S.bHasWateringCanState=true;S.WateringCanWaterLevel=.6f;
			if(!B->AddUniqueItem(TEXT("WateringCan"),S,Index) || !B->AddItem(TEXT("GardenTrowel"),1,Index)) return false;
			S.WateringCanWaterLevel=.3f;if(!B->AddUniqueItem(TEXT("WateringCan"),S,Index) || !B->AddItem(TEXT("PottingSoil"),5,Index)) return false;
			S=FBotanicusCarriedItemState();S.bHasPlantPotState=true;if(!B->AddUniqueItem(TEXT("PlantPot"),S,Index)) return false;
			B->SelectSlotAuthoritative(0);
		}
		return true;
	}
	if(Action==TEXT("UnrelatedAll") || Action==TEXT("ConsumeAll") || Action==TEXT("RechargeAll") || Action==TEXT("QuantityAll") || Action==TEXT("EmptyAll") || Action==TEXT("PotStateAll"))
	{
		bool Ok=true;
		for(FConstPlayerControllerIterator It=W->GetPlayerControllerIterator();It;++It) if(auto* Character=Cast<ABotanicusCharacter>(It->Get()->GetPawn()))
		{
			auto* B=Character->GetQuickBarComponent();
			if(Action==TEXT("ConsumeAll")) Ok &= B->ConsumeSelectedWateringCanWater(.05f);
			else if(Action==TEXT("RechargeAll")) Ok &= B->AddSelectedWateringCanWater(.2f);
			else if(Action==TEXT("QuantityAll")) Ok &= B->RemoveQuantity(3,1);
			else
			{
				const int32 Index=Action==TEXT("EmptyAll") ? B->GetSelectedSlotIndex() : 4;auto S=B->GetSlot(Index).CarriedState;
				if(Action==TEXT("UnrelatedAll")) S.CareScore+=.01f;
				else if(Action==TEXT("EmptyAll")) {S.bHasWateringCanState=true;S.WateringCanWaterLevel=0;}
				else
				{
					const auto Definitions=W->GetGameInstance()->GetSubsystem<UBotanicusPlantSubsystem>()->GetAllPlants();
					const auto* D=Definitions.FindByPredicate([](const auto& Def){return Def.Element==EBotanicusPlantElement::Normal;});if(!D) return false;
					S.bHasPlantPotState=true;S.bPlantPotHasSoil=true;S.PlantKey=D->PlantKey;S.GrowthProgress=.7f;S.WaterLevel=.5f;
				}
				Ok &= B->SetCarriedItemState(Index,S);
			}
		}
		return Ok;
	}
	if(Action==TEXT("FillAll"))
	{
		for(FConstPlayerControllerIterator It=W->GetPlayerControllerIterator();It;++It) if(auto* C=Cast<ABotanicusCharacter>(It->Get()->GetPawn()))
		{
			auto* B=C->GetQuickBarComponent();int32 Index;for(int32 I=0;I<10;++I) if(B->GetSlot(I).IsEmpty()) if(!B->AddUniqueItem(TEXT("WateringCan"),{},Index)) return false;
			if(B->AddUniqueItem(TEXT("WateringCan"),{},Index)) return false;
		}
		return true;
	}
	if(Action==TEXT("Reserve"))
	{
		auto* R=W->SpawnActor<ABotanicusWaterReserveActor>();if(!R) return false;ReserveName=R->GetFName();R->InitializePlacedItem(TEXT("WaterReserve"),1);
		R->SetActorLocation(P->GetPawnViewLocation()+P->GetActorForwardVector()*150-FVector(0,0,60));R->SetActorTickEnabled(false);
		// Prepared server views match the headless clients' requested aim; range
		// and reservoir validation still execute in the real refill RPC.
		for(FConstPlayerControllerIterator It=W->GetPlayerControllerIterator();It;++It) if(auto* Character=Cast<ABotanicusCharacter>(It->Get()->GetPawn()))
			It->Get()->SetControlRotation((R->GetActorLocation()-Character->GetPawnViewLocation()).Rotation());
		return true;
	}
	if(Action==TEXT("RemoveReserve")) {for(TActorIterator<ABotanicusWaterReserveActor> It(W);It;++It) if(It->GetFName()==ReserveName) return It->Destroy();return false;}
	if(Action==TEXT("PhysicalPickup"))
	{
		UClass* Class=LoadClass<ABotanicusWateringCanActor>(nullptr,TEXT("/Game/Botanicus/blueprints/BP_Item_WateringCan.BP_Item_WateringCan_C"));
		auto* Can=W->SpawnActor<ABotanicusWateringCanActor>(Class);if(!Can) return false;Can->InitializePlacedItem(TEXT("WateringCan"),1);
		Can->SetActorLocation(P->GetPawnViewLocation()+P->GetActorForwardVector()*120-FVector(0,0,30));
		FVector Centre,Extent;Can->GetActorBounds(true,Centre,Extent);PC->SetControlRotation((Centre-P->GetPawnViewLocation()).Rotation());
		if(PC->PlayerCameraManager) PC->PlayerCameraManager->UpdateCamera(0);
		PC->ServerToggleWateringCan(Can);return Can->GetCarrier()==P;
	}
	if(Action==TEXT("PhysicalDrop")) {auto* Can=P->GetHeldWateringCan();if(!IsValid(Can)) return false;PC->ServerToggleWateringCan(Can);return Can->GetCarrier()==nullptr;}
	if(Action==TEXT("Pickup") || Action==TEXT("PickupFull"))
	{
		auto* Item=W->SpawnActor<ABotanicusPlaceableItemActor>();if(!Item) return false;Item->InitializePlacedItem(TEXT("GardenTrowel"),1);
		Item->SetActorLocation(P->GetPawnViewLocation()+P->GetActorForwardVector()*150-FVector(0,0,70));PC->SetControlRotation((Item->GetActorLocation()-P->GetPawnViewLocation()).Rotation());
		PC->ServerBeginPlaceableItemMove(Item);PC->ServerCollectStorageItem(Item,1);
		return Action==TEXT("PickupFull") ? IsValid(Item) : !IsValid(Item);
	}
	if(Action==TEXT("DropSelected"))
	{
		PC->CancelQuickBarItemPlacement();const auto S=Bar->GetSelectedSlot();
		for(float Forward:{180.f,240.f,300.f}) for(float Right:{0.f,100.f,-100.f,180.f})
		{
			FTransform Placement;const FVector Candidate=P->GetActorLocation()+P->GetActorForwardVector()*Forward+P->GetActorRightVector()*Right-FVector(0,0,80);
			if(PC->ResolveQuickBarItemPlacement(S.ItemKey,Candidate,P->GetActorRotation().Yaw,Placement,nullptr,1,true))
			{
				PC->ServerPlaceQuickBarItem(Bar->GetSelectedSlotIndex(),S.InstanceId,S.ItemKey,Placement.GetLocation(),Placement.Rotator().Yaw,1,true);
				return Bar->GetSelectedSlot().Quantity<S.Quantity;
			}
		}
		return false;
	}
	if(Action==TEXT("ChangeDefinition") || Action==TEXT("RestoreDefinition"))
	{
		auto* C=W->GetGameInstance()->GetSubsystem<UBotanicusItemCatalogSubsystem>();auto* D=const_cast<FBotanicusItemDefinition*>(C->FindItem(TEXT("GardenTrowel")));if(!D) return false;
		if(Action==TEXT("ChangeDefinition"))
		{
			if(bDefinitionChanged) return false;OriginalDefinition=*D;bDefinitionChanged=true;
			D->WorldActorClass=TSoftClassPtr<AActor>(D->WorldActorClass.Get()==ABotanicusPlaceableItemActor::StaticClass() ? ABotanicusWateringCanActor::StaticClass() : ABotanicusPlaceableItemActor::StaticClass());D->WorldScale*=1.1f;
		}
		else {if(!bDefinitionChanged) return false;*D=OriginalDefinition;bDefinitionChanged=false;}
		return Step(Context,TEXT("UnrelatedAll"));
	}
#endif
	return false;
}
