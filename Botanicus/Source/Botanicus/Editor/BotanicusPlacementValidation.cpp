#include "Editor/BotanicusPlacementValidation.h"
#if WITH_EDITOR
#include "BotanicusPlayerController.h"
#include "BotanicusCharacter.h"
#include "BotanicusGameState.h"
#include "Delivery/BotanicusPlaceableItemActor.h"
#include "Delivery/BotanicusLargeEquipmentActor.h"
#include "Growing/BotanicusPlantPotActor.h"
#include "Growing/BotanicusWaterReserveActor.h"
#include "Growing/BotanicusPlantSubsystem.h"
#include "Catalog/BotanicusItemCatalogSubsystem.h"
#include "Visuals/BotanicusJuiceWorldSubsystem.h"
#include "Visuals/BotanicusPlacementJuiceSettings.h"
#include "Audio/BotanicusAudioWorldSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "UnrealClient.h"
#include "HAL/FileManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "UObject/UnrealType.h"
#include "Engine/OverlapResult.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
namespace
{
FName PlacementTestItem=TEXT("PlantPot"),PlacementTestLargeName,PlacementTestBlockerName;
FVector PlacementTestTarget(-8000,-8200,150);
float PlacementTestYaw=37.f;
UWorld* PlacementValidationWorld(UObject* Context)
{
    UWorld* W=GEngine ? GEngine->GetWorldFromContextObject(Context,EGetWorldErrorMode::ReturnNull) : nullptr;
    FString Saved=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());FPaths::NormalizeDirectoryName(Saved);
    return W && W->WorldType==EWorldType::PIE && FParse::Param(FCommandLine::Get(),TEXT("BotanicusPlacementValidation")) && Saved.Contains(TEXT("/isolated_user/")) ? W : nullptr;
}
bool InPlacementTestArea(const AActor* Actor)
{
    return IsValid(Actor) && FVector::DistSquared2D(Actor->GetActorLocation(),FVector(-8000,-8200,0))<FMath::Square(700.f);
}
ABotanicusPlaceableItemActor* PlacementTestLatestItem(UWorld* W)
{
    ABotanicusPlaceableItemActor* Latest=nullptr;float Age=FLT_MAX;
    for(TActorIterator<ABotanicusPlaceableItemActor> It(W);It;++It)
        if(It->GetIsReplicated() && InPlacementTestArea(*It) && It->GetFName()!=PlacementTestBlockerName && !It->ActorHasTag(TEXT("BotanicusPlacementPreview")) && It->GetGameTimeSinceCreation()<Age)
        {Latest=*It;Age=It->GetGameTimeSinceCreation();}
    return Latest;
}
}
#endif
bool UBotanicusPlacementValidation::Prepare(UObject* Context,FName ItemKey)
{
#if WITH_EDITOR
    UWorld* W=PlacementValidationWorld(Context);if(!W) return false;
    auto* PC=Cast<ABotanicusPlayerController>(W->GetFirstPlayerController());auto* Pawn=PC ? Cast<ABotanicusCharacter>(PC->GetPawn()) : nullptr;if(!Pawn) return false;
    if(W->GetNetMode()==NM_Client) return false;
    TGuardValue<bool> Routing(GAllowActorScriptExecutionInEditor,false);
    PC->CancelQuickBarItemPlacement();
    for(TActorIterator<ABotanicusPlaceableItemActor> It(W);It;++It) if(InPlacementTestArea(*It)) It->Destroy();
    for(TActorIterator<ABotanicusLargeEquipmentActor> It(W);It;++It) if(InPlacementTestArea(*It)) It->Destroy();
    auto* Catalog=W->GetGameInstance()->GetSubsystem<UBotanicusItemCatalogSubsystem>();const auto* D=Catalog->FindItem(ItemKey);if(!D) return false;
    PlacementTestItem=ItemKey;PlacementTestTarget=FVector(-8000,-8200,150);PlacementTestYaw=37;
    int32 I=0;
    for(FConstPlayerControllerIterator It=W->GetPlayerControllerIterator();It;++It) if(auto* Other=Cast<ABotanicusCharacter>(It->Get()->GetPawn()))
    {
        Other->SetActorLocation(FVector(-8000+(I%2 ? 100.f : -100.f)*(I/2),-8500-I*100.f,235.4f));
        if(auto* Controller=Cast<APlayerController>(Other->GetController())) Controller->SetControlRotation(FRotator(-15,90,0));++I;
        auto* Bar=Other->GetQuickBarComponent();for(int32 Slot=0;Slot<10;++Slot) {const auto S=Bar->GetSlot(Slot);if(!S.IsEmpty()) Bar->RemoveQuantity(Slot,S.Quantity);}
        if(D->WeightClass==EBotanicusItemWeightClass::Hotbar) {int32 Slot;Bar->AddItem(ItemKey,6,Slot);Bar->SelectSlotAuthoritative(Slot);}
    }
    if(D->WeightClass!=EBotanicusItemWeightClass::Hotbar)
    {
        UClass* Class=D->WorldActorClass.LoadSynchronous();if(!Class || !Class->IsChildOf(ABotanicusLargeEquipmentActor::StaticClass())) return false;
        auto* Large=W->SpawnActor<ABotanicusLargeEquipmentActor>(Class);Large->InitializeEquipment(ItemKey);Large->SetActorLocation(PlacementTestTarget);
        if(Large->RequiresTwoPlayers())
        {
            Large->BeginCooperativeHold(Pawn);
            for(FConstPlayerControllerIterator It=W->GetPlayerControllerIterator();It;++It) if(auto* Helper=Cast<ABotanicusCharacter>(It->Get()->GetPawn());Helper && Helper!=Pawn) {Large->BeginCooperativeHold(Helper);break;}
        }
        else {Large->BeginFurnitureMove(Pawn);PC->ServerBeginLargeEquipmentPlacement(Large);}
        PlacementTestLargeName=Large->GetFName();FTransform T;
        const bool bValid=PC->ResolveLargeEquipmentPlacement(Large,PlacementTestTarget,PlacementTestYaw,T);Large->UpdatePlacement(T,bValid);return bValid;
    }
    PlacementTestLargeName=NAME_None;return true;
#else
    return false;
#endif
}
bool UBotanicusPlacementValidation::Step(UObject* Context,FName Action)
{
#if WITH_EDITOR
    UWorld* W=PlacementValidationWorld(Context);if(!W) return false;auto* Settings=GetMutableDefault<UBotanicusPlacementJuiceSettings>();
    if(Action==TEXT("Off") || Action==TEXT("On")) {Settings->bPlacementJuiceEnabled=Action==TEXT("On");return true;}
    if(Action==TEXT("NoFly") || Action==TEXT("Fly")) {Settings->bFlyFromHand=Action==TEXT("Fly");return true;}
    if(Action==TEXT("BudgetOne") || Action==TEXT("BudgetDefault")) {Settings->MaximumSimultaneousAnimations=Action==TEXT("BudgetOne") ? 1 : 4;return true;}
    auto* PC=Cast<ABotanicusPlayerController>(W->GetFirstPlayerController());auto* Pawn=PC ? Cast<ABotanicusCharacter>(PC->GetPawn()) : nullptr;if(!Pawn) return false;
    TGuardValue<bool> Routing(GAllowActorScriptExecutionInEditor,false);auto* Bar=Pawn->GetQuickBarComponent();
    if(Action==TEXT("PrepareRefillRange"))
    {
        if(W->GetNetMode()==NM_Client) return false;
        ABotanicusWaterReserveActor* Reserve=nullptr;float Age=FLT_MAX;
        for(TActorIterator<ABotanicusWaterReserveActor> It(W);It;++It) if(It->GetGameTimeSinceCreation()<Age) {Reserve=*It;Age=It->GetGameTimeSinceCreation();}
        if(!Reserve) return false;FVector Centre,Extent;Reserve->GetActorBounds(true,Centre,Extent);int32 I=0;
        const FVector Forward=Pawn->GetActorForwardVector(),Right=Pawn->GetActorRightVector();
        for(FConstPlayerControllerIterator It=W->GetPlayerControllerIterator();It;++It) if(auto* Other=Cast<ABotanicusCharacter>(It->Get()->GetPawn()))
        {
            FVector Position=Reserve->GetActorLocation()-Forward*150+Right*(I<2 ? I*105 : 700+I*120);Position.Z=Other->GetActorLocation().Z;
            Other->SetActorLocation(Position);Other->ForceNetUpdate();It->Get()->SetControlRotation((Centre-Other->GetPawnViewLocation()).Rotation());++I;
        }
        return I==4;
    }
    if(Action==TEXT("PrepareWave"))
    {
        if(W->GetNetMode()==NM_Client || !Prepare(Context,TEXT("PlantPot"))) return false;
        int32 I=0;
        for(FConstPlayerControllerIterator It=W->GetPlayerControllerIterator();It;++It) if(auto* Other=Cast<ABotanicusCharacter>(It->Get()->GetPawn()))
        {
            Other->SetActorLocation(FVector(-8170+I*115,-8500,235.4));++I;auto* B=Other->GetQuickBarComponent();
            for(int32 K=0;K<10;++K) {auto Slot=B->GetSlot(K);if(!Slot.IsEmpty()) B->RemoveQuantity(K,Slot.Quantity);}
            int32 K;B->AddItem(TEXT("WateringCan"),1,K);B->SelectSlotAuthoritative(K);
        }
        return I==4;
    }
    if(Action==TEXT("Wave"))
    {
        if(W->GetNetMode()==NM_Client) return false;
        for(TActorIterator<ABotanicusPlaceableItemActor> It(W);It;++It) if(InPlacementTestArea(*It) && It->GetIsReplicated() && It->GetItemKey()==TEXT("PlantPot")) It->Destroy();
        int32 I=0,Success=0;
        for(FConstPlayerControllerIterator It=W->GetPlayerControllerIterator();It;++It) if(auto* OtherPC=Cast<ABotanicusPlayerController>(It->Get())) if(auto* Other=Cast<ABotanicusCharacter>(OtherPC->GetPawn()))
        {
            auto* B=Other->GetQuickBarComponent();int32 K;B->AddItem(TEXT("PlantPot"),1,K);auto Slot=B->GetSlot(K);FTransform T;
            const FVector Request(-8170+I*115,-8200,150);++I;
            if(OtherPC->ResolveQuickBarItemPlacement(Slot.ItemKey,Request,37,T,nullptr,1,true))
            {OtherPC->ServerPlaceQuickBarItem(K,Slot.InstanceId,Slot.ItemKey,T.GetLocation(),37,1,true);++Success;}
        }
        return Success==4;
    }
    if(Action==TEXT("UseSoil") || Action==TEXT("EndUse"))
    {
        if(W->GetNetMode()==NM_Client) return false;
        auto* Pot=Cast<ABotanicusPlantPotActor>(PlacementTestLatestItem(W));if(!Pot) return false;
        if(Action==TEXT("UseSoil")) Pot->BeginPrimaryUse(Pawn);else Pot->EndPrimaryUse(Pawn);return true;
    }
    if(Action==TEXT("Preview")) {PC->BeginQuickBarItemPlacement();return IsValid(PC->LocalQuickBarItemPreview);}
    if(Action==TEXT("Cancel")) {PC->CancelQuickBarItemPlacement();return !IsValid(PC->LocalQuickBarItemPreview);}
    if(Action==TEXT("Rotate")) {PC->RotateQuickBarItemPlacement(1);return true;}
    if(Action==TEXT("BlockPreview"))
    {
        if(W->GetNetMode()==NM_Client || !IsValid(PC->LocalQuickBarItemPreview)) return false;
        auto* Blocker=W->SpawnActor<ABotanicusPlaceableItemActor>();Blocker->Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
        Blocker->Mesh->SetRelativeScale3D(FVector(.8f));Blocker->SetActorLocation(PC->LocalQuickBarItemPreview->GetActorLocation());PlacementTestBlockerName=Blocker->GetFName();return true;
    }
    if(Action==TEXT("Unblock")) {for(TActorIterator<AActor> It(W);It;++It) if(It->GetFName()==PlacementTestBlockerName) {It->Destroy();return true;}return false;}
    if(Action==TEXT("AimInvalid")) {PC->SetControlRotation(FRotator(65,90,0));return true;}
    if(Action==TEXT("AimValid")) {PC->SetControlRotation(FRotator(-15,90,0));return true;}
    if(Action==TEXT("ConfirmPreview")) {PC->ConfirmQuickBarItemPlacement();return true;}
    if(Action==TEXT("Place") || Action==TEXT("Reject") || Action==TEXT("PlaceNext"))
    {
        PC->CancelQuickBarItemPlacement();const auto Slot=Bar->GetSelectedSlot();if(Slot.IsEmpty()) return false;
        FVector Requested=PlacementTestTarget;
        if(Action==TEXT("Reject")) Requested=Pawn->GetActorLocation()+FVector(10000,0,0);
        if(Action==TEXT("PlaceNext")) {PlacementTestTarget.X+=125;Requested=PlacementTestTarget;}
        FTransform T;const bool bValid=PC->ResolveQuickBarItemPlacement(Slot.ItemKey,Requested,PlacementTestYaw,T,nullptr,1,true);
        PC->ServerPlaceQuickBarItem(Bar->GetSelectedSlotIndex(),Slot.InstanceId,Slot.ItemKey,Action==TEXT("Reject") ? Requested : T.GetLocation(),PlacementTestYaw,1,true);
        return Action==TEXT("Reject") ? !bValid : bValid;
    }
    if(Action==TEXT("LargeConfirm"))
    {
        for(TActorIterator<ABotanicusLargeEquipmentActor> It(W);It;++It) if(It->GetFName()==PlacementTestLargeName)
        {PC->ServerConfirmLargeEquipmentPlacement(*It,It->GetActorLocation(),PlacementTestYaw);return true;}return false;
    }
    if(Action==TEXT("FillPlant"))
    {
        if(W->GetNetMode()==NM_Client) return false;
        if(auto* Pot=Cast<ABotanicusPlantPotActor>(PlacementTestLatestItem(W)))
        {
            const auto Plants=W->GetGameInstance()->GetSubsystem<UBotanicusPlantSubsystem>()->GetAllPlants();if(Plants.IsEmpty()) return false;
            Pot->RestoreGrowingState(true,Plants[0].PlantKey,.6f,.7f,.2f);Pot->SetActorTickEnabled(false);return true;
        }return false;
    }
    if(Action==TEXT("AimMove") || Action==TEXT("ReserveMove") || Action==TEXT("Move") || Action==TEXT("Destroy"))
    {
        auto* Item=PlacementTestLatestItem(W);if(!Item) return false;
        if(Action==TEXT("Destroy")) {if(W->GetNetMode()==NM_Client) return false;Item->Destroy();return true;}
        if(Action==TEXT("AimMove"))
        {
            FVector Centre,Extent;Item->GetActorBounds(true,Centre,Extent);Pawn->SetActorLocation(FVector(Centre.X,Centre.Y-180,Pawn->GetActorLocation().Z));const FVector Eye=Pawn->GetPawnViewLocation();
            PC->SetControlRotation((Centre-Eye).Rotation());return true;
        }
        if(Action==TEXT("ReserveMove")) {PC->ServerBeginPlaceableItemMove(Item);return PC->ServerMovedPlaceableItem==Item;}
        const FVector Requested=Item->GetActorLocation()+FVector(150,0,0);FTransform T;
        const bool bValid=PC->ResolveQuickBarItemPlacement(Item->GetItemKey(),Requested,71,T,Item,1,false);
        if(bValid) PC->ServerConfirmPlaceableItemMove(Item,T.GetLocation(),71);return bValid;
    }
    if(Action==TEXT("SelectSoil"))
    {
        if(W->GetNetMode()==NM_Client) return false;int32 Index;Bar->AddItem(TEXT("PottingSoil"),5,Index);Bar->SelectSlotAuthoritative(Index);return true;
    }
    return false;
#else
    return false;
#endif
}
FString UBotanicusPlacementValidation::Snapshot(UObject* Context)
{
#if WITH_EDITOR
    UWorld* W=PlacementValidationWorld(Context);if(!W) return TEXT("{}");auto Root=MakeShared<FJsonObject>();
    auto* PC=Cast<ABotanicusPlayerController>(W->GetFirstPlayerController());auto* Pawn=PC ? Cast<ABotanicusCharacter>(PC->GetPawn()) : nullptr;
    if(auto* J=W->GetSubsystem<UBotanicusJuiceWorldSubsystem>())
    {
        Root->SetNumberField(TEXT("active"),J->GetPlacementAnimationCount());Root->SetNumberField(TEXT("holograms"),J->GetPlacementHologramCount());
        Root->SetNumberField(TEXT("copies"),J->GetPlacementCopyCount());Root->SetNumberField(TEXT("created"),J->GetPlacementCreatedComponents());Root->SetNumberField(TEXT("destroyed"),J->GetPlacementDestroyedComponents());
        Root->SetNumberField(TEXT("started"),J->GetPlacementStartedCount());Root->SetNumberField(TEXT("finished"),J->GetPlacementFinishedCount());
        const FVector Phase=J->GetPlacementPhaseSample();Root->SetNumberField(TEXT("elapsed"),Phase.X);Root->SetNumberField(TEXT("scale"),Phase.Y);Root->SetNumberField(TEXT("height"),Phase.Z);
    }
    if(auto* Audio=W->GetSubsystem<UBotanicusAudioWorldSubsystem>())
    {Root->SetNumberField(TEXT("potSounds"),Audio->GetEventPlayCount(TEXT("PotPlace")));Root->SetNumberField(TEXT("equipmentSounds"),Audio->GetEventPlayCount(TEXT("EquipmentPlace")));}
    if(Pawn) {const auto Slot=Pawn->GetQuickBarComponent()->GetSelectedSlot();Root->SetNumberField(TEXT("quantity"),Slot.Quantity);}
    auto* Preview=PC ? PC->LocalQuickBarItemPreview.Get() : nullptr;
    Root->SetBoolField(TEXT("preview"),IsValid(Preview));Root->SetBoolField(TEXT("valid"),PC && PC->bLocalQuickBarItemPlacementValid);
    if(Preview)
    {
        Root->SetStringField(TEXT("previewName"),Preview->GetName());Root->SetBoolField(TEXT("previewNaniteFallback"),Preview->Mesh->bForceDisableNanite);
        Root->SetNumberField(TEXT("previewYaw"),Preview->GetActorRotation().Yaw);
        if(auto* MID=Cast<UMaterialInstanceDynamic>(Preview->Mesh->GetMaterial(0)))
        {const auto Color=MID->K2_GetVectorParameterValue(TEXT("BodyTint"));Root->SetNumberField(TEXT("blue"),Color.B);Root->SetNumberField(TEXT("red"),Color.R);Root->SetStringField(TEXT("previewParent"),MID->Parent->GetPathName());}
    }
    TArray<TSharedPtr<FJsonValue>> Items;
    for(TActorIterator<AActor> It(W);It;++It) if(It->GetIsReplicated() && InPlacementTestArea(*It) && It->GetFName()!=PlacementTestBlockerName && !It->ActorHasTag(TEXT("BotanicusPlacementPreview")) &&
        (It->IsA<ABotanicusPlaceableItemActor>() || It->IsA<ABotanicusLargeEquipmentActor>()))
    {
        auto Item=MakeShared<FJsonObject>();Item->SetStringField(TEXT("name"),It->GetName());Item->SetStringField(TEXT("transform"),It->GetActorTransform().ToString());
        if(auto* Pot=Cast<ABotanicusPlantPotActor>(*It)) if(auto* Property=FindFProperty<FFloatProperty>(Pot->GetClass(),TEXT("SoilFillProgress"))) Item->SetNumberField(TEXT("soil"),Property->GetPropertyValue_InContainer(Pot));
        Item->SetBoolField(TEXT("actorCollision"),It->GetActorEnableCollision());Item->SetBoolField(TEXT("replicated"),It->GetIsReplicated());Item->SetBoolField(TEXT("hidden"),It->IsHidden());
        if(auto* C=It->FindComponentByClass<UStaticMeshComponent>()) {Item->SetNumberField(TEXT("collision"),static_cast<int32>(C->GetCollisionEnabled()));Item->SetBoolField(TEXT("mainPass"),C->bRenderInMainPass);Item->SetBoolField(TEXT("visible"),C->IsVisible());}
        FVector Centre,Extent;It->GetActorBounds(true,Centre,Extent);Item->SetStringField(TEXT("bounds"),Centre.ToString()+Extent.ToString());
        FHitResult Hit;FCollisionQueryParams Query(SCENE_QUERY_STAT(PlacementValidation),false);const bool bHit=W->LineTraceSingleByChannel(Hit,Centre+FVector(0,0,300),Centre-FVector(0,0,300),ECC_Visibility,Query);
        Item->SetBoolField(TEXT("collisionTrace"),bHit && Hit.GetActor()==*It);
        Items.Add(MakeShared<FJsonValueObject>(Item));
    }
    Root->SetArrayField(TEXT("items"),Items);
    bool bSafe=true,bStable=true;int32 CopyParts=0;
    if(auto* J=W->GetSubsystem<UBotanicusJuiceWorldSubsystem>()) for(const auto& A:J->PlacementAnimations)
    {
        bStable &= A.Source.IsValid() && A.Source->GetActorTransform().Equals(A.FinalTransform,.001f);
        for(const auto& M:A.Meshes) {++CopyParts;bSafe &= M.Copy && !M.Copy->GetIsReplicated() && M.Copy->GetCollisionEnabled()==ECollisionEnabled::NoCollision && !M.Copy->GetGenerateOverlapEvents();}
    }
    Root->SetBoolField(TEXT("copiesSafe"),bSafe);Root->SetBoolField(TEXT("rootsStable"),bStable);Root->SetNumberField(TEXT("copyParts"),CopyParts);FString Out;auto Writer=TJsonWriterFactory<>::Create(&Out);FJsonSerializer::Serialize(Root,Writer);return Out;
#else
    return TEXT("{}");
#endif
}

bool UBotanicusPlacementValidation::CaptureRawFrame(UObject* Context,const FString& Filename)
{
#if WITH_EDITOR
    UWorld* W=PlacementValidationWorld(Context);if(!W) return false;
    const FString Destination=FPaths::ConvertRelativePathToFull(Filename);
    const FString Allowed=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("../../captures/placement_frames/"));
    // Diagnostic capture may only write inside this isolated run's captures directory.
    if(!FPaths::IsUnderDirectory(Destination,Allowed)) return false;
    if(auto* PC=W->GetFirstPlayerController()) if(auto* Local=PC->GetLocalPlayer()) if(UGameViewportClient* View=Local->ViewportClient) if(View->Viewport)
    {
        TArray<FColor> Pixels;if(!View->Viewport->ReadPixels(Pixels)) return false;
        const FIntPoint Size=View->Viewport->GetSizeXY();TUniquePtr<FArchive> File(IFileManager::Get().CreateFileWriter(*Destination));if(!File) return false;
        int32 X=Size.X,Y=Size.Y;*File<<X;*File<<Y;File->Serialize(Pixels.GetData(),Pixels.Num()*sizeof(FColor));File->Close();return !File->IsError();
    }
#endif
    return false;
}
