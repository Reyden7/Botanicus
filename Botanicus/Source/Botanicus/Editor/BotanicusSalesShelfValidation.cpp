#include "Editor/BotanicusSalesShelfValidation.h"
#if WITH_EDITOR
#include "BotanicusPlayerController.h"
#include "BotanicusCharacter.h"
#include "BotanicusGameMode.h"
#include "BotanicusGameState.h"
#include "Sales/BotanicusSalesShelfActor.h"
#include "Sales/BotanicusSalesDisplayActor.h"
#include "Sales/BotanicusSalePotActor.h"
#include "Sales/BotanicusCashRegisterActor.h"
#include "Sales/BotanicusSelfCheckoutActor.h"
#include "Visitors/BotanicusVisitorCharacter.h"
#include "Visitors/BotanicusVisitorManager.h"
#include "Visitors/BotanicusVisitorZoneActor.h"
#include "QuickBar/BotanicusQuickBarComponent.h"
#include "Catalog/BotanicusItemCatalogSubsystem.h"
#include "Save/BotanicusWorldSaveGame.h"
#include "Visuals/BotanicusJuiceWorldSubsystem.h"
#include "Audio/BotanicusAudioWorldSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "NavigationSystem.h"
#include "InputKeyEventArgs.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
namespace
{
FVector Origin;FVector WallPoint;FVector RealWallNormal;TSet<TWeakObjectPtr<AActor>> RealExistingShelves;FName NPCName,HeldName;FTransform CachedShelfPose;FGuid CachedShelfId;FName CachedShelfKey;int32 CachedShelfSlot=INDEX_NONE;bool bExecuting=false;
UWorld* SalesShelfTestWorld(UObject* Context)
{
    auto* W=GEngine ? GEngine->GetWorldFromContextObject(Context,EGetWorldErrorMode::ReturnNull):nullptr;
    FString Saved=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());FPaths::NormalizeDirectoryName(Saved);
    return W && W->WorldType==EWorldType::PIE && Saved.Contains(TEXT("/isolated_user/")) && FParse::Param(FCommandLine::Get(),TEXT("BotanicusSalesShelfValidation")) ? W:nullptr;
}
ABotanicusSalesShelfActor* ShelfFor(UWorld* W,int32 Capacity=6)
{for(TActorIterator<ABotanicusSalesShelfActor> It(W);It;++It)if(It->GetIsReplicated() && It->GetCapacity()==Capacity && !It->ActorHasTag(TEXT("BotanicusPlacementPreview")) && !It->ActorHasTag(TEXT("BotanicusEquippedPreview")))return *It;return nullptr;}
ABotanicusSalesDisplayActor* FloorDisplay(UWorld* W)
{for(TActorIterator<ABotanicusSalesDisplayActor> It(W);It;++It)if(It->GetName().Contains(TEXT("ShelfValidationFloor")) || (!It->IsShelfSlot() && FVector::DistSquared2D(It->GetActorLocation(),Origin+FVector(-25,-250,0))<25))return *It;return nullptr;}
AStaticMeshActor* Box(UWorld* W,FName Name,FVector At,FVector Size)
{
    FActorSpawnParameters P;P.Name=Name;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* A=W->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),At,FRotator::ZeroRotator,P);
    auto* M=A->GetStaticMeshComponent();M->SetMobility(EComponentMobility::Movable);M->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));M->SetWorldScale3D(Size/100.f);M->SetCollisionProfileName(TEXT("BlockAll"));A->SetReplicates(true);M->SetIsReplicated(true);return A;
}
void Aim(APlayerController* PC,const FVector& At){FVector Eye;FRotator Rot;PC->GetPlayerViewPoint(Eye,Rot);PC->SetControlRotation((At-Eye).Rotation());}
}
#endif
bool UBotanicusSalesShelfValidation::Step(UObject* Context,FName Action,int32 Index)
{
#if WITH_EDITOR
    auto* W=SalesShelfTestWorld(Context);if(!W)return false;
    if(!bExecuting){TWeakObjectPtr<UWorld> Weak=W;W->GetTimerManager().SetTimerForNextTick([Weak,Action,Index](){if(Weak.IsValid()){TGuardValue<bool> Guard(bExecuting,true);Step(Weak.Get(),Action,Index);}});return true;}
    auto* PC=Cast<ABotanicusPlayerController>(W->GetFirstPlayerController());if(!PC || !PC->GetPawn())return false;
    auto* Pawn=Cast<ABotanicusCharacter>(PC->GetPawn());auto* Bar=Pawn ? Pawn->GetQuickBarComponent():nullptr;if(!Bar)return false;
    auto* Shelf=ShelfFor(W);auto* Display=Index<0 ? FloorDisplay(W):(Shelf ? Shelf->GetSlot(Index):nullptr);
    const bool Server=W->GetNetMode()!=NM_Client;
    if(Action==TEXT("RealWall"))
    {
        if(!Server)return false;
        for(TActorIterator<ABotanicusVisitorManager> It(W);It;++It)It->SetActorTickEnabled(false);
        for(TActorIterator<ABotanicusVisitorCharacter> It(W);It;++It)It->Destroy();
        RealExistingShelves.Reset();for(TActorIterator<ABotanicusSalesShelfActor> It(W);It;++It)RealExistingShelves.Add(*It);
        UStaticMeshComponent* Target=nullptr;
        const FString Name=FString::Printf(TEXT("StaticMeshActor_%d"),Index);
        for(TActorIterator<AStaticMeshActor> It(W);It;++It)if(It->GetName()==Name){Target=It->GetStaticMeshComponent();break;}
        if(!Target)return false;RealWallNormal=-Target->GetRightVector();RealWallNormal.Z=0;RealWallNormal.Normalize();
        FCollisionQueryParams Q(SCENE_QUERY_STAT(RealShelfWall),true,Pawn);FHitResult Hit;
        const FVector Center=Target->Bounds.Origin;
        if(!W->LineTraceSingleByChannel(Hit,Center+RealWallNormal*100,Center-RealWallNormal*100,ECC_Visibility,Q) || Hit.GetComponent()!=Target)return false;
        WallPoint=Hit.ImpactPoint;RealWallNormal=FVector(Hit.ImpactNormal.X,Hit.ImpactNormal.Y,0).GetSafeNormal();
        FHitResult Floor;const FVector At=WallPoint+RealWallNormal*150;
        if(!W->LineTraceSingleByChannel(Floor,At+FVector(0,0,100),At-FVector(0,0,1000),ECC_Visibility,Q))return false;
        Origin=WallPoint;Origin.Z=Floor.ImpactPoint.Z;
        Pawn->SetActorLocation(Origin+RealWallNormal*150+FVector(0,0,Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+3));
        Bar->ClearSlot(9);return true;
    }
    if(Action==TEXT("RealDistance")){Pawn->SetActorLocation(Origin+RealWallNormal*Index+FVector(0,0,Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+3));return true;}
    if(Action==TEXT("ClearRealShelves")){if(!Server)return false;for(TActorIterator<ABotanicusSalesShelfActor> It(W);It;++It)if(It->GetIsReplicated() && !RealExistingShelves.Contains(*It))It->Destroy();return true;}
    if(Action==TEXT("MoveRemote") || Action==TEXT("GiveRemoteShelf"))
    {
        if(!Server)return false;
        for(FConstPlayerControllerIterator It=W->GetPlayerControllerIterator();It;++It)if(!It->Get()->IsLocalController())if(auto* P=Cast<ABotanicusCharacter>(It->Get()->GetPawn()))
        {
            if(Action==TEXT("MoveRemote"))
            {
                Pawn->SetActorLocation(Origin+RealWallNormal*150+FVector(0,200,Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+3));
                P->SetActorLocation(Origin+RealWallNormal*Index+FVector(0,0,P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+3));P->ForceNetUpdate();return true;
            }
            int32 Added;auto* B=P->GetQuickBarComponent();if(!B->AddItem(FName(*FString::Printf(TEXT("SalesShelfS%d"),Index)),1,Added))return false;B->SelectSlotAuthoritative(Added);return true;
        }
        return false;
    }
    if(Action==TEXT("CacheShelfClick"))
    {
        if(!PC->LocalQuickBarItemPreview || !PC->bLocalQuickBarItemPlacementValid)return false;
        CachedShelfPose=PC->LocalQuickBarItemPreview->GetActorTransform();CachedShelfSlot=PC->LocalQuickBarItemSlotIndex;CachedShelfId=PC->LocalQuickBarItemInstanceId;CachedShelfKey=PC->LocalQuickBarItemKey;
        PC->QuickBarItemPreviewUpdateAccumulator=-10.f;auto Rotation=PC->GetControlRotation();Rotation.Pitch+=25;Rotation.Yaw+=30;PC->SetControlRotation(Rotation);return true;
    }
    if(Action==TEXT("ConfirmCachedShelf"))
    {
        PC->ServerPlaceQuickBarItem(CachedShelfSlot,CachedShelfId,CachedShelfKey,CachedShelfPose.GetLocation(),CachedShelfPose.Rotator().Yaw,1,true);PC->CancelQuickBarItemPlacement();return true;
    }
    if(Action==TEXT("RealPitch")){auto R=PC->GetControlRotation();R.Pitch=Index;PC->SetControlRotation(R);return true;}
    if(Action==TEXT("Setup"))
    {
        if(!Server)return false;
        for(TActorIterator<ABotanicusVisitorManager> It(W);It;++It)It->SetActorTickEnabled(false);
        for(TActorIterator<ABotanicusVisitorCharacter> It(W);It;++It)It->Destroy();
        Origin=Pawn->GetActorLocation();
        for(TActorIterator<ABotanicusCashRegisterActor> It(W);It;++It){Origin=It->GetActorLocation()+FVector(-300,-500,0);break;}
        FNavLocation Floor;auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(W);
        if(!Nav || !Nav->ProjectPointToNavigation(Origin,Floor,FVector(400,400,500)))return false;
        Origin=Floor.Location;WallPoint=Origin+FVector(90,0,100);
        auto* Wall=Box(W,TEXT("ShelfValidationWall"),Origin+FVector(100,0,175),FVector(20,900,350));Wall->Tags.Add(TEXT("BotanicusSalesShelfWall"));
        auto* Zone=W->SpawnActor<ABotanicusVisitorZoneActor>();Zone->SetActorLocation(Origin);Zone->InitializeZone(EBotanicusVisitorZoneType::SalesArea,FVector(500,500,10));
        FActorSpawnParameters FloorParams;FloorParams.Name=TEXT("ShelfValidationFloor");FloorParams.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* FloorStand=W->SpawnActor<ABotanicusSalesDisplayActor>(ABotanicusSalesDisplayActor::StaticClass(),Origin+FVector(-25,-250,32.5f),FRotator::ZeroRotator,FloorParams);FloorStand->InitializePlacedItem(TEXT("SalesDisplay"));
        for(int32 Count=1;Count<=5;++Count){auto* S=W->SpawnActor<ABotanicusSalesShelfActor>(ABotanicusSalesShelfActor::StaticClass(),Origin+FVector(3000,Count*400,100),FRotator(0,180,0));S->InitializePlacedItem(FName(*FString::Printf(TEXT("SalesShelfS%d"),Count)));}
        int32 PlayerIndex=0;for(FConstPlayerControllerIterator It=W->GetPlayerControllerIterator();It;++It)if(auto* P=Cast<ABotanicusCharacter>(It->Get()->GetPawn())){P->SetActorLocation(Origin+FVector(-150,(PlayerIndex++ ? 40:-40),P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+3));P->ForceNetUpdate();auto* B=P->GetQuickBarComponent();for(int32 I=0;I<10;++I)B->ClearSlot(I);}
        return true;
    }
    if(Action==TEXT("ReturnPlayers")){if(!Server)return false;int32 N=0;for(FConstPlayerControllerIterator It=W->GetPlayerControllerIterator();It;++It)if(auto* P=Cast<ABotanicusCharacter>(It->Get()->GetPawn()))P->SetActorLocation(Origin+FVector(-150,(N++ ? 40:-40),P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+3));return true;}
    if(Action==TEXT("SeedOccupied")){return Server && Display && Display->TryMountSalePotState(TEXT("PottingSoil"),TEXT("Harvest_AureliaSweet_Beautiful"),PC,TEXT("SalePot"),FGuid(),true);}
    if(Action==TEXT("CreateHeld"))
    {
        if(!Server)return false;FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Pot=W->SpawnActor<ABotanicusSalePotActor>(ABotanicusSalePotActor::StaticClass(),Origin+FVector(-90,0,1),FRotator::ZeroRotator,Params);Pot->InitializePlacedItem(TEXT("SalePot"));Pot->RestoreSalePotState(TEXT("PottingSoil"),TEXT("Harvest_AureliaSweet_Beautiful"));Pot->SetActorLocation(Origin+FVector(-90,0,Pot->GetPlacementPivotToBottomOffset()+1));HeldName=Pot->GetFName();return true;
    }
    if(Action==TEXT("AimHeld") || Action==TEXT("Hold"))
    {for(TActorIterator<ABotanicusSalePotActor> It(W);It;++It)if(It->GetFName()==HeldName){if(Action==TEXT("AimHeld"))Aim(PC,It->GetActorLocation()+FVector(0,0,12));else PC->BeginWorldItemMove(*It,false);return true;}return false;}
    if(Action==TEXT("GiveShelf") || Action==TEXT("GivePlain"))
    {if(!Server)return false;int32 Added;const FName Key=Action==TEXT("GiveShelf") ? FName(*FString::Printf(TEXT("SalesShelfS%d"),Index>=1 && Index<=6 ? Index:6)):FName(TEXT("PottingSoil"));if(!Bar->AddItem(Key,1,Added))return false;Bar->SelectSlotAuthoritative(Added);return true;}
    if(Action==TEXT("GivePlant") || Action==TEXT("GiveRemote"))
    {
        if(!Server)return false;auto* Ch=Pawn;
        if(Action==TEXT("GiveRemote"))for(FConstPlayerControllerIterator It=W->GetPlayerControllerIterator();It;++It)if(!It->Get()->IsLocalController())Ch=Cast<ABotanicusCharacter>(It->Get()->GetPawn());
        if(!Ch)return false;FBotanicusCarriedItemState State;State.bHasSalePotState=true;State.SaleSoilItemKey=TEXT("PottingSoil");State.SalePlantItemKey=TEXT("Harvest_AureliaSweet_Beautiful");
        if(!W->GetGameInstance()->GetSubsystem<UBotanicusItemCatalogSubsystem>()->FindItem(State.SalePlantItemKey))return false;int32 Added;
        if(!Ch->GetQuickBarComponent()->AddUniqueItem(TEXT("SalePot"),State,Added))return false;Ch->GetQuickBarComponent()->SelectSlotAuthoritative(Added);return true;
    }
    if(Action==TEXT("AimHeight")){Aim(PC,FVector(WallPoint.X,WallPoint.Y,Origin.Z+Index));return true;}
    if(Action==TEXT("TurnWall"))
    {
        if(!Server)return false;
        const FRotator Rotation(0,Index,0);
        for(TActorIterator<AStaticMeshActor> It(W);It;++It)if(It->GetFName()==TEXT("ShelfValidationWall")){It->SetActorLocationAndRotation(Origin+Rotation.RotateVector(FVector(100,0,175)),Rotation);break;}
        WallPoint=Origin+Rotation.RotateVector(FVector(90,0,100));
        int32 N=0;for(FConstPlayerControllerIterator It=W->GetPlayerControllerIterator();It;++It)if(auto* P=Cast<ABotanicusCharacter>(It->Get()->GetPawn()))P->SetActorLocation(Origin+Rotation.RotateVector(FVector(-150,(N++ ? 40:-40),P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+3)));return true;
    }
    if(Action==TEXT("AimWall") || Action==TEXT("AimLow") || Action==TEXT("AimHigh") || Action==TEXT("AimFloor"))
    {FVector At=WallPoint;if(Action==TEXT("AimLow"))At.Z=Origin.Z+35;if(Action==TEXT("AimHigh"))At.Z=Origin.Z+220;if(Action==TEXT("AimFloor"))At=Origin;Aim(PC,At);return true;}
    if(Action==TEXT("AimSlot") || Action==TEXT("AimPot"))
    {if(!Display)return false;Aim(PC,Display->GetSalePotPlacementTransform().GetLocation()+FVector(0,0,Action==TEXT("AimPot") ? 12:2));return true;}
    if(Action==TEXT("A") || Action==TEXT("Click") || Action==TEXT("Cancel"))
    {FInputKeyEventArgs Args;Args.Key=Action==TEXT("A") ? EKeys::A:Action==TEXT("Cancel") ? EKeys::RightMouseButton:EKeys::LeftMouseButton;Args.Event=IE_Pressed;const bool Handled=PC->InputKey(Args);if(Action==TEXT("Click")){Args.Event=IE_Released;PC->InputKey(Args);}return Handled;}
    if(Action==TEXT("Retrieve")){if(!Display)return false;PC->ServerRetrieveDisplayedSalePot(Display);return true;}
    if(Action==TEXT("SideDoor") || Action==TEXT("SideGlass")){if(!Server)return false;Box(W,Action==TEXT("SideDoor") ? FName(TEXT("ShelfValidationDoorSide")):FName(TEXT("ShelfValidationGlassSide")),Origin+FVector(91.7f,100,110),FVector(4,30,180));return true;}
    if(Action==TEXT("Door") || Action==TEXT("Glass")){if(!Server)return false;Box(W,Action==TEXT("Door") ? FName(TEXT("ShelfValidationDoor")):FName(TEXT("ShelfValidationGlass")),Origin+FVector(75,0,110),FVector(4,100,180));return true;}
    if(Action==TEXT("ClearSurface")){if(!Server)return false;for(TActorIterator<AStaticMeshActor> It(W);It;++It)if(It->GetName().StartsWith(TEXT("ShelfValidationDoor")) || It->GetName().StartsWith(TEXT("ShelfValidationGlass")))It->Destroy();return true;}
    if(Action==TEXT("VisualObstacle"))
    {if(!Server)return false;auto* A=W->SpawnActor<ABotanicusPlaceableItemActor>(ABotanicusPlaceableItemActor::StaticClass(),Origin+FVector(63,100,120),FRotator::ZeroRotator);A->Tags.Add(TEXT("SalesShelfVisualObstacle"));A->Mesh->SetRelativeScale3D(FVector(.15f,.2f,.8f));A->Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);return true;}
    if(Action==TEXT("Obstacle")){if(!Server)return false;Box(W,TEXT("ShelfValidationObstacle"),Origin+FVector(63,100,120),FVector(15,20,80));return true;}
    if(Action==TEXT("RemoveObstacle")){if(!Server)return false;for(TActorIterator<ABotanicusPlaceableItemActor> It(W);It;++It)if(It->ActorHasTag(TEXT("SalesShelfVisualObstacle")))It->Destroy();for(TActorIterator<AStaticMeshActor> It(W);It;++It)if(It->GetName().StartsWith(TEXT("ShelfValidationObstacle")))It->Destroy();return true;}
    if(Action==TEXT("SaveReload"))
    {if(!Server)return false;auto* GM=W->GetAuthGameMode<ABotanicusGameMode>();if(!GM || !GM->BotanicusSaveNow())return false;GM->LoadAutosave();GM->RestoreWorldState();return true;}
    if(Action==TEXT("StartNPC"))
    {
        if(!Server || !Display || Display->IsEmpty())return false;
        // Only the selected shelf product competes in this isolated deterministic shopping fixture.
        for(TActorIterator<ABotanicusSalesDisplayActor> It(W);It;++It)if(*It!=Display)It->RestoreDisplayedPlant(NAME_None);
        FActorSpawnParameters P;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* V=W->SpawnActor<ABotanicusVisitorCharacter>(ABotanicusVisitorCharacter::StaticClass(),Origin+FVector(-200,0,96),FRotator::ZeroRotator,P);
        const TArray<FVector> Route{Origin+FVector(-200,0,0),Origin+FVector(-150,0,0),Origin+FVector(-200,150,0),Origin+FVector(-250,200,0)};
        V->InitializeQueuedCircuit(Route,2,Route[0],TArray<FVector>{Route[0],Route[1]},TArray<FVector>{Route[1],Route[3]});V->AdmitFromQueue();
        V->ShoppingBudget=10000;V->MinimumAcceptableScore=-1000;V->DesiredInspectionCount=1;V->InspectionDuration=.1f;V->DisplaySearchRemaining=0;
        NPCName=V->GetFName();return V->TryFindAvailableDisplay();
    }
    if(Action==TEXT("FinishNPC"))
    {
        if(!Server)return false;for(TActorIterator<ABotanicusVisitorCharacter> It(W);It;++It)if(It->GetFName()==NPCName && It->bCarryingPlant && It->TargetDisplay)return It->TargetDisplay->CompleteVisitorPurchase(*It);return false;
    }
    if(Action==TEXT("MoveAway"))
    {if(!Server)return false;for(FConstPlayerControllerIterator It=W->GetPlayerControllerIterator();It;++It)if(auto* P=It->Get()->GetPawn().Get())P->SetActorLocation(Origin+FVector(-350,300,100));return true;}
    return false;
#else
    return false;
#endif
}
FString UBotanicusSalesShelfValidation::Snapshot(UObject* Context)
{
#if WITH_EDITOR
    auto* W=SalesShelfTestWorld(Context);if(!W)return TEXT("{}");auto R=MakeShared<FJsonObject>();auto* PC=Cast<ABotanicusPlayerController>(W->GetFirstPlayerController());if(!PC)return TEXT("{}");
    auto* Pawn=Cast<ABotanicusCharacter>(PC->GetPawn());if(!Pawn)return TEXT("{}");auto* Bar=Pawn->GetQuickBarComponent();const auto Selected=Bar->GetSelectedSlot();
    FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);R->SetStringField(TEXT("eye"),Eye.ToString());R->SetStringField(TEXT("view"),View.ToString());
    FHitResult Trace;FCollisionQueryParams Params(SCENE_QUERY_STAT(ShelfDiagnostic),true,Pawn);if(PC->LocalQuickBarItemPreview)Params.AddIgnoredActor(PC->LocalQuickBarItemPreview);if(PC->LocalInspectedQuickBarItem)Params.AddIgnoredActor(PC->LocalInspectedQuickBarItem);if(PC->LocalEquippedQuickBarItem)Params.AddIgnoredActor(PC->LocalEquippedQuickBarItem);
    W->LineTraceSingleByChannel(Trace,Eye,Eye+View.Vector()*450,ECC_Visibility,Params);R->SetStringField(TEXT("rawHit"),Trace.ImpactPoint.ToString());R->SetStringField(TEXT("rawNormal"),Trace.ImpactNormal.ToString());R->SetStringField(TEXT("hitActor"),GetNameSafe(Trace.GetActor()));R->SetStringField(TEXT("hitComponent"),GetNameSafe(Trace.GetComponent()));
    TArray<TSharedPtr<FJsonValue>> Walls;
    for(TActorIterator<AActor> It(W);It;++It)
    {
        TInlineComponentArray<UStaticMeshComponent*> Parts(*It);
        for(auto* M:Parts)if(M->GetStaticMesh())
        {
            const FString Name=It->GetName()+M->GetName()+M->GetStaticMesh()->GetName();
            if(!Name.Contains(TEXT("wall"),ESearchCase::IgnoreCase) && !Name.Contains(TEXT("mur"),ESearchCase::IgnoreCase))continue;
            auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("actor"),It->GetName());J->SetStringField(TEXT("component"),M->GetName());J->SetStringField(TEXT("mesh"),M->GetStaticMesh()->GetPathName());J->SetStringField(TEXT("location"),M->GetComponentLocation().ToString());J->SetStringField(TEXT("rotation"),M->GetComponentRotation().ToString());J->SetStringField(TEXT("boundsOrigin"),M->Bounds.Origin.ToString());J->SetStringField(TEXT("boundsExtent"),M->Bounds.BoxExtent.ToString());Walls.Add(MakeShared<FJsonValueObject>(J));
        }
    }
    R->SetArrayField(TEXT("wallCandidates"),Walls);
    R->SetNumberField(TEXT("realFloorZ"),Origin.Z);R->SetStringField(TEXT("realWallPoint"),WallPoint.ToString());
    R->SetStringField(TEXT("selectedId"),Selected.InstanceId.ToString());R->SetStringField(TEXT("selectedKey"),Selected.ItemKey.ToString());R->SetStringField(TEXT("selectedPlant"),Selected.CarriedState.SalePlantItemKey.ToString());
    int32 Pots=0;for(int32 I=0;I<10;++I){const auto S=Bar->GetSlot(I);if(S.CarriedState.bHasSalePotState)Pots+=S.Quantity;}R->SetNumberField(TEXT("inventoryPots"),Pots);
    R->SetBoolField(TEXT("heldWorld"),IsValid(PC->LocalMovedPlaceableItem));R->SetBoolField(TEXT("heldModel"),IsValid(PC->LocalInspectedQuickBarItem));
    int32 PhysicalPots=0;for(TActorIterator<ABotanicusSalePotActor> It(W);It;++It)if(It->GetIsReplicated())++PhysicalPots;R->SetNumberField(TEXT("physicalPots"),PhysicalPots);
    R->SetBoolField(TEXT("preview"),IsValid(PC->LocalQuickBarItemPreview));R->SetBoolField(TEXT("placementValid"),PC->bLocalQuickBarItemPlacementValid);
    if(PC->LocalQuickBarItemPreview){R->SetStringField(TEXT("previewPose"),PC->LocalQuickBarItemPreview->GetActorTransform().ToHumanReadableString());}
    ABotanicusSalesDisplayActor* AimDisplay=nullptr;PC->FindAimedSalesDisplay(AimDisplay);R->SetStringField(TEXT("aimed"),AimDisplay ? AimDisplay->GetName():TEXT(""));
    TArray<TSharedPtr<FJsonValue>> Shelves;
    for(int32 Cap=1;Cap<=6;++Cap)if(auto* Shelf=ShelfFor(W,Cap))
    {
        auto S=MakeShared<FJsonObject>();S->SetNumberField(TEXT("capacity"),Shelf->GetCapacity());S->SetNumberField(TEXT("width"),Shelf->Mesh->Bounds.BoxExtent.Y*2);S->SetStringField(TEXT("item"),Shelf->GetItemKey().ToString());S->SetStringField(TEXT("pose"),Shelf->GetActorTransform().ToHumanReadableString());
        TArray<TSharedPtr<FJsonValue>> Slots;for(int32 I=0;I<Cap;++I)if(auto* D=Shelf->GetSlot(I)){auto Dto=MakeShared<FJsonObject>();Dto->SetNumberField(TEXT("index"),I);Dto->SetStringField(TEXT("plant"),D->GetDisplayedPlantItemKey().ToString());Dto->SetStringField(TEXT("potId"),D->GetDisplayedPotInstanceId().ToString());Dto->SetStringField(TEXT("lot"),D->GetSaleLotId().ToString());Dto->SetStringField(TEXT("pose"),D->GetSalePotPlacementTransform().ToHumanReadableString());TArray<UMeshComponent*> Parts;D->GetSalePresentationMeshes(Parts);int32 Visible=0;for(auto* M:Parts)if(M->IsVisible() && M->bRenderInMainPass)++Visible;Dto->SetNumberField(TEXT("visibleParts"),Visible);if(!Parts.IsEmpty())Dto->SetNumberField(TEXT("potWidth"),Parts[0]->Bounds.BoxExtent.Y*2);int32 Markers=0;TInlineComponentArray<UInstancedStaticMeshComponent*> Components(D);for(auto* C:Components)if(C->IsVisible())++Markers;Dto->SetNumberField(TEXT("markers"),Markers);Slots.Add(MakeShared<FJsonValueObject>(Dto));}S->SetArrayField(TEXT("slots"),Slots);Shelves.Add(MakeShared<FJsonValueObject>(S));
    }
    R->SetArrayField(TEXT("shelves"),Shelves);
    if(auto* D=FloorDisplay(W)){R->SetStringField(TEXT("floorPlant"),D->GetDisplayedPlantItemKey().ToString());R->SetStringField(TEXT("floorPotId"),D->GetDisplayedPotInstanceId().ToString());}
    if(auto* J=W->GetSubsystem<UBotanicusJuiceWorldSubsystem>()){R->SetNumberField(TEXT("copies"),J->GetPlacementCopyCount());R->SetStringField(TEXT("phase"),J->GetPlacementPhaseSample().ToString());}
    if(auto* A=W->GetSubsystem<UBotanicusAudioWorldSubsystem>()){R->SetNumberField(TEXT("PotPlace"),A->GetEventPlayCount(TEXT("PotPlace")));R->SetNumberField(TEXT("Listed"),A->GetEventPlayCount(TEXT("Listed")));}
    if(auto* GS=W->GetGameState<ABotanicusGameState>())R->SetNumberField(TEXT("funds"),GS->GetSharedFunds());
    for(TActorIterator<ABotanicusVisitorCharacter> It(W);It;++It)if(It->GetFName()==NPCName){R->SetNumberField(TEXT("npcState"),static_cast<int32>(It->VisitorState));R->SetBoolField(TEXT("npcCarries"),It->bCarryingPlant);R->SetBoolField(TEXT("npcTargetsShelf"),It->TargetDisplay && It->TargetDisplay->IsShelfSlot());R->SetNumberField(TEXT("npcZ"),It->GetActorLocation().Z);R->SetStringField(TEXT("npcLocation"),It->GetActorLocation().ToString());}
    FString Out;auto Writer=TJsonWriterFactory<>::Create(&Out);FJsonSerializer::Serialize(R,Writer);return Out;
#else
    return TEXT("{}");
#endif
}
