#include "Editor/BotanicusPlantContextValidation.h"
#if WITH_EDITOR
#include "BotanicusPlayerController.h"
#include "BotanicusCharacter.h"
#include "BotanicusGameState.h"
#include "Growing/BotanicusPlantPotActor.h"
#include "Growing/BotanicusMultiPlantPotActor.h"
#include "Growing/BotanicusPlantSubsystem.h"
#include "IllegalTrade/BotanicusIllegalPlanterActor.h"
#include "QuickBar/BotanicusQuickBarComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "Serialization/JsonSerializer.h"
namespace
{
bool bExecutingContextTest=false;
FVector ContextTestOrigin;
TMap<FString,FVector> ContextTestPositions;
FString ContextTestLabel(AActor* A)
{
    for(const auto& Pair:ContextTestPositions) if(FVector::DistSquared(A->GetActorLocation(),Pair.Value)<25.f) return TEXT("PlantContextFixture_")+Pair.Key;
    return A->GetName().StartsWith(TEXT("PlantContextFixture_")) ? A->GetName() : FString();
}
UWorld* ContextTestWorld(UObject* Context)
{
    UWorld* W=GEngine ? GEngine->GetWorldFromContextObject(Context,EGetWorldErrorMode::ReturnNull) : nullptr;
    FString Saved=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());FPaths::NormalizeDirectoryName(Saved);
    return W && W->WorldType==EWorldType::PIE && Saved.Contains(TEXT("/isolated_user/")) && FParse::Param(FCommandLine::Get(),TEXT("BotanicusPlantContextValidation")) ? W : nullptr;
}
AActor* ContextTestActor(UWorld* W,const FString& Suffix)
{
    for(TActorIterator<AActor> It(W);It;++It) if(ContextTestLabel(*It)==TEXT("PlantContextFixture_")+Suffix) return *It;
    return nullptr;
}
}
#endif

bool UBotanicusPlantContextValidation::Step(UObject* Context,FName Action)
{
#if WITH_EDITOR
    UWorld* W=ContextTestWorld(Context);if(!W) return false;
    if(!bExecutingContextTest)
    {
        TWeakObjectPtr<UWorld> Weak=W;
        W->GetTimerManager().SetTimerForNextTick([Weak,Action](){if(Weak.IsValid()){TGuardValue<bool> Guard(bExecutingContextTest,true);Step(Weak.Get(),Action);}});return true;
    }
    auto* PC=Cast<ABotanicusPlayerController>(W->GetFirstPlayerController());auto* Pawn=PC ? Cast<ABotanicusCharacter>(PC->GetPawn()) : nullptr;if(!Pawn) return false;
    const FString Text=Action.ToString();const FString Suffix=Text.Contains(TEXT("_")) ? Text.RightChop(Text.Find(TEXT("_"))+1) : TEXT("A");
    AActor* Target=ContextTestActor(W,Suffix.Left(1));const int32 Slot=Suffix.Len()>1 ? FCString::Atoi(*Suffix.RightChop(1)) : 0;
    if(Text.StartsWith(TEXT("Position_")) && Target)
    {
        FVector P=ContextTestOrigin;P.Y=Target->GetActorLocation().Y;Pawn->SetActorLocation(P,false,nullptr,ETeleportType::TeleportPhysics);Pawn->ForceNetUpdate();return true;
    }
    if(Text.StartsWith(TEXT("Aim_")) && Target)
    {
        FVector Point=Target->GetActorLocation()+FVector(0,0,18);
        if(auto* Multi=Cast<ABotanicusMultiPlantPotActor>(Target)) Point+=Target->GetActorForwardVector()*((Slot-(Multi->GetPlantCapacity()-1)*.5f)*55.f);
        else if(auto* Pot=Cast<ABotanicusPlantPotActor>(Target)) Point.Z=Pot->GetActorLocation().Z+Pot->GetSoilMaximumHeight();
        else if(Cast<ABotanicusIllegalPlanterActor>(Target)) Point=Target->GetActorTransform().TransformPosition(FVector((Slot-1.5f)*49.333f,0,80));
        FVector Eye;FRotator Rot;PC->GetPlayerViewPoint(Eye,Rot);PC->SetControlRotation((Point-Eye).Rotation());return true;
    }
    if(Action==TEXT("Away")) {PC->SetControlRotation(FRotator(0,180,0));return true;}
    if(Action==TEXT("MouseDown") || Action==TEXT("MouseUp")) {FInputKeyEventArgs Args;Args.Key=EKeys::LeftMouseButton;Args.Event=Action==TEXT("MouseDown") ? IE_Pressed : IE_Released;return PC->InputKey(Args);}
    if(W->GetNetMode()==NM_Client) return false;
    auto* Bar=Pawn->GetQuickBarComponent();const auto Definitions=W->GetGameInstance()->GetSubsystem<UBotanicusPlantSubsystem>()->GetAllPlants();
    const auto* Def=Definitions.FindByPredicate([](const auto& D){return D.Element==EBotanicusPlantElement::Normal;});if(!Def || !Bar) return false;
    auto Clear=[Bar](){for(int32 I=0;I<Bar->SlotCount;++I) {const auto S=Bar->GetSlot(I);if(!S.IsEmpty()) Bar->RemoveQuantity(I,S.Quantity);}};
    if(Action==TEXT("Prepare"))
    {
        for(TActorIterator<AActor> It(W);It;++It)
        {
            if(It->GetName().StartsWith(TEXT("PlantContextFixture_"))) It->Destroy();
            if(It->GetClass()->GetName().Contains(TEXT("VisitorManager")) || It->GetClass()->GetName().Contains(TEXT("IllegalTradeManager"))) It->SetActorTickEnabled(false);
        }
        if(auto* GS=W->GetGameState<ABotanicusGameState>()) GS->SetActorTickEnabled(false);
        ContextTestOrigin=Pawn->GetActorLocation();PC->SetControlRotation(FRotator::ZeroRotator);Clear();
        for(FConstPlayerControllerIterator It=W->GetPlayerControllerIterator();It;++It) if(It->Get()!=PC && It->Get()->GetPawn()) {APawn* Other=It->Get()->GetPawn();Other->SetActorLocation(ContextTestOrigin+FVector(0,120,0),false,nullptr,ETeleportType::TeleportPhysics);Other->ForceNetUpdate();}
        for(int32 I=0;I<5;++I)
        {
            FActorSpawnParameters Params;Params.Name=FName(*FString::Printf(TEXT("PlantContextFixture_%c"),I<3 ? 'A'+I : I==3 ? 'M' : 'I'));Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            FVector P=ContextTestOrigin+FVector(185,(I-1)*55, -88);if(I==3) P.Y=ContextTestOrigin.Y+250;if(I==4) P.Y=ContextTestOrigin.Y-280;
            UClass* Class=I==3 ? ABotanicusMultiPlantPotActor::StaticClass() : I==4 ? ABotanicusIllegalPlanterActor::StaticClass() : LoadClass<ABotanicusPlantPotActor>(nullptr,TEXT("/Game/Botanicus/blueprints/BP_Item_PlantPot.BP_Item_PlantPot_C"));
            auto* A=W->SpawnActor<ABotanicusPlaceableItemActor>(Class,P,FRotator(0,90,0),Params);if(!A) return false;
            if(auto* Multi=Cast<ABotanicusMultiPlantPotActor>(A)) {Multi->InitializePlacedItem(TEXT("PreparationPlanter2"));TArray<FBotanicusMultiPlantSlotState> Slots;Slots.SetNum(2);for(auto& S:Slots) {S.PlantKey=Def->PlantKey;S.WaterLevel=0.5f;S.GrowthProgress=0.6f;S.CareScore=0.3f;}Multi->RestoreMultiPlantState(2,Slots);}
            else if(auto* Pot=Cast<ABotanicusPlantPotActor>(A)) {Pot->InitializePlacedItem(TEXT("PlantPot"));Pot->RestoreGrowingState(true,Def->PlantKey,0.5f,0.6f,0.3f);}
            else if(auto* Illegal=Cast<ABotanicusIllegalPlanterActor>(A)) {Illegal->InitializePlacedItem(TEXT("IllegalPlanter"));TArray<FBotanicusIllegalPlantSlotState> Slots;Slots.SetNum(4);for(auto& S:Slots) {S.PlantId=TEXT("Noctiflore");S.GrowthProgress=0.5f;S.WaterAmount=0.7f;}Illegal->RestoreIllegalPlanterState(1,4,0,Slots);}
            A->bAlwaysRelevant=true;ContextTestPositions.Add(FString::Chr(I<3 ? 'A'+I : I==3 ? 'M' : 'I'),A->GetActorLocation());A->ForceNetUpdate();
        }
        return true;
    }
    if(Action==TEXT("WaterTool") || Action==TEXT("HarvestTool"))
    {
        Clear();int32 Index;if(Action==TEXT("WaterTool")) {FBotanicusCarriedItemState State;State.bHasWateringCanState=true;State.WateringCanWaterLevel=1;if(!Bar->AddUniqueItem(TEXT("WateringCan"),State,Index)) return false;}
        else if(!Bar->AddItem(Def->HarvestToolItemKey,1,Index)) return false;
        Bar->SelectSlotAuthoritative(Index);return true;
    }
    if(Action==TEXT("Mature") || Action==TEXT("Dry"))
    {
        auto* Pot=Cast<ABotanicusPlantPotActor>(ContextTestActor(W,TEXT("A")));if(!Pot) return false;
        Pot->RestoreGrowingState(true,Pot->GetPlantKey(),Action==TEXT("Dry") ? 0 : 0.5f,Action==TEXT("Mature") ? 1 : 0.6f,0.3f);Pot->ForceNetUpdate();return true;
    }
    if(Action==TEXT("WallOn") || Action==TEXT("WallOff"))
    {
        if(auto* Wall=ContextTestActor(W,TEXT("Wall"))) Wall->Destroy();
        if(Action==TEXT("WallOn"))
        {
            FActorSpawnParameters Params;Params.Name=TEXT("PlantContextFixture_Wall");auto* Wall=W->SpawnActor<AActor>(AActor::StaticClass(),ContextTestOrigin+FVector(95,0,0),FRotator::ZeroRotator,Params);auto* Mesh=NewObject<UStaticMeshComponent>(Wall);Wall->SetRootComponent(Mesh);Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));Mesh->SetWorldLocation(ContextTestOrigin+FVector(95,0,0));Mesh->SetWorldScale3D(FVector(0.1f,3,4));Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Mesh->SetCollisionResponseToAllChannels(ECR_Block);Mesh->RegisterComponent();
        }
        return true;
    }
    if(Action==TEXT("Destroy")) {if(Target) Target->Destroy();return true;}
#endif
    return false;
}

FString UBotanicusPlantContextValidation::Snapshot(UObject* Context)
{
#if WITH_EDITOR
    UWorld* W=ContextTestWorld(Context);if(!W) return TEXT("{}");auto Root=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Pots;
    for(TActorIterator<AActor> It(W);It;++It) if(!ContextTestLabel(*It).IsEmpty())
    {
        auto P=MakeShared<FJsonObject>();P->SetStringField(TEXT("name"),ContextTestLabel(*It));TArray<TSharedPtr<FJsonValue>> Widgets;TInlineComponentArray<UWidgetComponent*> Components;It->GetComponents(Components);
        for(auto* C:Components)
        {
            auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("name"),C->GetName());J->SetBoolField(TEXT("visible"),C->IsVisible());J->SetNumberField(TEXT("alpha"),C->GetWidget() ? C->GetWidget()->GetRenderOpacity() : 0.f);J->SetStringField(TEXT("position"),C->GetComponentLocation().ToString());J->SetStringField(TEXT("widget"),C->GetWidget() ? C->GetWidget()->GetPathName() : TEXT(""));Widgets.Add(MakeShared<FJsonValueObject>(J));
        }
        P->SetArrayField(TEXT("widgets"),Widgets);
        if(auto* Multi=Cast<ABotanicusMultiPlantPotActor>(*It)) {TArray<TSharedPtr<FJsonValue>> Slots;for(const auto& S:Multi->GetPlantSlots()) Slots.Add(MakeShared<FJsonValueString>(S.PlantKey.ToString()));P->SetArrayField(TEXT("slots"),Slots);}
        else if(auto* Pot=Cast<ABotanicusPlantPotActor>(*It)) {P->SetStringField(TEXT("plant"),Pot->GetPlantKey().ToString());P->SetNumberField(TEXT("water"),Pot->GetWaterLevel());P->SetNumberField(TEXT("growth"),Pot->GetGrowthProgress());P->SetNumberField(TEXT("wateringCount"),Pot->GetWateringCount());}
        Pots.Add(MakeShared<FJsonValueObject>(P));
    }
    Root->SetArrayField(TEXT("pots"),Pots);if(auto* PC=Cast<ABotanicusPlayerController>(W->GetFirstPlayerController())) {Root->SetStringField(TEXT("highlight"),PC->LocalInteractionHighlightActor.IsValid() ? ContextTestLabel(PC->LocalInteractionHighlightActor.Get()) : TEXT(""));FVector Eye;FRotator Rotation;PC->GetPlayerViewPoint(Eye,Rotation);Root->SetStringField(TEXT("view"),Eye.ToString()+Rotation.ToString());FHitResult Hit;FCollisionQueryParams Query(SCENE_QUERY_STAT(ContextTestRay),false,PC->GetPawn());if(W->LineTraceSingleByChannel(Hit,Eye,Eye+Rotation.Vector()*500,ECC_Visibility,Query)) {Root->SetStringField(TEXT("ray"),Hit.GetActor()->GetName());Root->SetStringField(TEXT("hit"),Hit.ImpactPoint.ToString());Root->SetNumberField(TEXT("hitDistance"),FVector::Dist(Eye,Hit.ImpactPoint));}Root->SetStringField(TEXT("pawn"),PC->GetPawn()->GetActorLocation().ToString());}if(auto* Pawn=Cast<ABotanicusCharacter>(W->GetFirstPlayerController()->GetPawn())) Root->SetNumberField(TEXT("toolWater"),Pawn->GetQuickBarComponent()->GetSelectedWateringCanWaterLevel());
    FString Out;auto Writer=TJsonWriterFactory<>::Create(&Out);FJsonSerializer::Serialize(Root,Writer);return Out;
#else
    return TEXT("{}");
#endif
}
