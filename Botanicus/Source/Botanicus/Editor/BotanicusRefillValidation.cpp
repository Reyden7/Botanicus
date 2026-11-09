#include "Editor/BotanicusRefillValidation.h"
#if WITH_EDITOR
#include "BotanicusPlayerController.h"
#include "BotanicusCharacter.h"
#include "BotanicusGameMode.h"
#include "Growing/BotanicusWaterReserveActor.h"
#include "Growing/BotanicusWateringCanActor.h"
#include "Growing/BotanicusPlantPotActor.h"
#include "Growing/BotanicusPlantSubsystem.h"
#include "QuickBar/BotanicusQuickBarComponent.h"
#include "Audio/BotanicusAudioWorldSubsystem.h"
#include "Visuals/BotanicusJuiceWorldSubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "NiagaraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "TimerManager.h"
#include "Editor.h"
namespace
{
UWorld* RefillValidationWorld(UObject* C)
{
    auto* W=C ? C->GetWorld():nullptr;
    FString Saved=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());FPaths::NormalizeDirectoryName(Saved);
    return W && W->WorldType==EWorldType::PIE && FParse::Param(FCommandLine::Get(),TEXT("BotanicusRefillValidation")) && Saved.Contains(TEXT("/isolated_user")) ? W:nullptr;
}
TSharedPtr<FJsonObject> GroundCanDiagnostic(ABotanicusWateringCanActor* Can)
{
    auto J=MakeShared<FJsonObject>();if(!Can || !Can->Mesh)return J;
    J->SetStringField(TEXT("meshLocation"),Can->Mesh->GetRelativeLocation().ToString());
    J->SetStringField(TEXT("meshRotation"),Can->Mesh->GetRelativeRotation().ToString());
    J->SetNumberField(TEXT("up"),FVector::DotProduct(Can->Mesh->GetUpVector(),FVector::UpVector));
    J->SetNumberField(TEXT("yaw"),Can->GetActorRotation().Yaw);
    J->SetNumberField(TEXT("water"),Can->GetWaterLevel());
    J->SetStringField(TEXT("collision"),Can->Mesh->GetCollisionProfileName().ToString());
    J->SetNumberField(TEXT("collisionMode"),int32(Can->Mesh->GetCollisionEnabled()));
    const FVector Base=Can->Mesh->GetComponentTransform().TransformPosition(FVector(0,0,-32.23605));
    FHitResult Floor;FCollisionQueryParams Q(SCENE_QUERY_STAT(GroundCanDiagnostic),false,Can);
    if(Can->GetWorld()->LineTraceSingleByChannel(Floor,Base+FVector(0,0,4),Base-FVector(0,0,150),ECC_Visibility,Q))
        J->SetNumberField(TEXT("baseGap"),Base.Z-Floor.ImpactPoint.Z);
    return J;
}
ABotanicusWaterReserveActor* RefillFixture(UWorld* W)
{for(TActorIterator<ABotanicusWaterReserveActor> It(W);It;++It)if(It->IsRealStation() && (It->ActorHasTag(TEXT("RefillFixture")) || FVector::DistSquared2D(It->GetActorLocation(),FVector(-8200,-8200,0))<100))return *It;return nullptr;}
ABotanicusWateringCanActor* RefillFixtureCan(UWorld* W)
{
    auto* R=RefillFixture(W);if(R && R->GetDockedCan())return R->GetDockedCan();
    for(TActorIterator<ABotanicusWateringCanActor> It(W);It;++It)if(It->GetIsReplicated() && !It->ActorHasTag(TEXT("BotanicusPlacementPreview")) && !It->ActorHasTag(TEXT("BotanicusEquippedPreview")) && R && FVector::DistSquared2D(It->GetActorLocation(),R->GetActorLocation())<FMath::Square(650.f))return *It;
    return nullptr;
}
}
#endif
bool UBotanicusRefillValidation::Action(UObject* Context,FName Command,float Value)
{
#if WITH_EDITOR
    UWorld* W=RefillValidationWorld(Context);if(!W)return false;
    W->GetTimerManager().SetTimerForNextTick([Weak=TWeakObjectPtr<UWorld>(W),Command,Value]()
    {
        UWorld* World=Weak.Get();if(!World)return;
        auto* PC=Cast<ABotanicusPlayerController>(World->GetFirstPlayerController());
        auto* Pawn=PC ? Cast<ABotanicusCharacter>(PC->GetPawn()):nullptr;if(!Pawn)return;
        auto* R=RefillFixture(World);auto* Can=RefillFixtureCan(World);auto* Bar=Pawn->GetQuickBarComponent();
        if(Command==TEXT("Prepare") || Command==TEXT("PrepareNative"))
        {
            if(World->GetNetMode()==NM_Client)return;
            if(R)R->Destroy();
            UClass* C=LoadClass<ABotanicusWaterReserveActor>(nullptr,TEXT("/Game/Botanicus/blueprints/BP_Item_WaterReserve.BP_Item_WaterReserve_C"));
            if(Command==TEXT("PrepareNative"))C=ABotanicusWaterReserveActor::StaticClass();
            R=World->SpawnActor<ABotanicusWaterReserveActor>(C);if(!R)return;
            R->Tags.Add(TEXT("RefillFixture"));R->InitializePlacedItem(TEXT("WaterReserve"),1);
            FHitResult Floor;World->LineTraceSingleByChannel(Floor,FVector(-8200,-8200,350),FVector(-8200,-8200,0),ECC_Visibility);
            R->SetActorLocation(FVector(-8200,-8200,Floor.ImpactPoint.Z+R->GetPlacementPivotToBottomOffset()));R->ForceNetUpdate();
            int32 I=0;
            for(FConstPlayerControllerIterator It=World->GetPlayerControllerIterator();It;++It)if(auto* P=Cast<ABotanicusCharacter>(It->Get()->GetPawn()))
            {
                auto* B=P->GetQuickBarComponent();for(int32 K=0;K<10;++K){auto S=B->GetSlot(K);if(!S.IsEmpty())B->RemoveQuantity(K,S.Quantity);}
                FBotanicusCarriedItemState State;State.bHasWateringCanState=true;State.WateringCanWaterLevel=.2f;int32 K;
                B->AddUniqueItem(TEXT("WateringCan"),State,K);B->SelectSlotAuthoritative(K);
                P->GetCharacterMovement()->StopMovementImmediately();
                P->SetActorLocation(FVector(-8105+I*100,-8390,Floor.ImpactPoint.Z+P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2));P->ForceNetUpdate();++I;
            }
            return;
        }
        if(Command==TEXT("Near") && World->GetNetMode()!=NM_Client && R)
        {
            int32 I=0;for(FConstPlayerControllerIterator It=World->GetPlayerControllerIterator();It;++It)if(auto* P=Cast<ABotanicusCharacter>(It->Get()->GetPawn()))
            {P->SetActorLocation(FVector(-8105+I*100,-8390,R->GetDockGroundLocation().Z+P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2));P->ForceNetUpdate();++I;}return;
        }
        if(Command==TEXT("LateJoin")){if(GEditor && World->GetNetMode()!=NM_Client)GEditor->RequestLateJoin();return;}
        if(Command==TEXT("Save")){if(auto* GM=World->GetAuthGameMode<ABotanicusGameMode>())GM->BotanicusSaveNow();return;}
        if(!R)return;
        if(Command==TEXT("AimStation") || Command==TEXT("AimPlace") || Command==TEXT("AimOutside") || Command==TEXT("AimCan") || Command==TEXT("AimPlant"))
        {
            FVector Target=R->FaucetOutlet->GetComponentLocation();
            if(Command==TEXT("AimPlace"))Target=R->GetDockGroundLocation()+FVector(0,0,2);
            if(Command==TEXT("AimOutside"))Target=R->GetDockGroundLocation()+FVector(180,0,2);
            if(Command==TEXT("AimCan") && Can){FVector Extent;Can->GetActorBounds(true,Target,Extent);}
            if(Command==TEXT("AimPlant"))for(TActorIterator<ABotanicusPlantPotActor> It(World);It;++It)if(It->ActorHasTag(TEXT("RefillPlant"))){FVector Extent;It->GetActorBounds(true,Target,Extent);break;}
            PC->SetControlRotation((Target-Pawn->GetPawnViewLocation()).Rotation());return;
        }
        if(Command==TEXT("E") || Command==TEXT("ERelease") || Command==TEXT("A") || Command==TEXT("LMB") || Command==TEXT("LMBRelease") || Command==TEXT("Cancel"))
        {
            FKey Key=Command==TEXT("A") ? EKeys::A:Command==TEXT("E") || Command==TEXT("ERelease") ? EKeys::E:Command==TEXT("Cancel") ? EKeys::RightMouseButton:EKeys::LeftMouseButton;
            FInputKeyEventArgs Args;Args.Key=Key;Args.Event=Command==TEXT("ERelease") || Command==TEXT("LMBRelease") ? IE_Released:IE_Pressed;Args.AmountDepressed=1;PC->InputKey(Args);return;
        }
        if(Command==TEXT("RetryOpen")){PC->ServerSetRefillFaucet(R,R->GetDockedCan(),true);return;}
        if(Command==TEXT("Yaw")){PC->QuickBarItemPlacementYaw=Value;PC->UpdateQuickBarItemPlacement(1);return;}
        if(Command==TEXT("OldRefill")){PC->ServerRefillWateringCan(R);return;}
        if(Command==TEXT("Collect"))
        {
            if(Can){PC->ServerBeginPlaceableItemMove(Can);PC->ServerCollectStorageItem(Can,1);}return;
        }
        if(World->GetNetMode()==NM_Client)return;
        if(Command==TEXT("GiveCan"))
        {
            for(FConstPlayerControllerIterator It=World->GetPlayerControllerIterator();It;++It)
                if(auto* P=Cast<ABotanicusCharacter>(It->Get()->GetPawn()))
                {
                    auto* B=P->GetQuickBarComponent();FBotanicusCarriedItemState State;State.bHasWateringCanState=true;State.WateringCanWaterLevel=Value;int32 K;
                    if(B->AddUniqueItem(TEXT("WateringCan"),State,K))B->SelectSlotAuthoritative(K);
                }
            return;
        }
        if(Command==TEXT("ClearCans"))
        {
            for(TActorIterator<ABotanicusWateringCanActor> It(World);It;++It)
                if(It->GetIsReplicated() && !It->ActorHasTag(TEXT("BotanicusEquippedPreview")) && !It->ActorHasTag(TEXT("BotanicusPlacementPreview")))It->Destroy();
            return;
        }
        if(Command==TEXT("ReservePose"))
        {
            R->SetActorLocation(R->GetActorLocation()+FVector(180,70,0));
            R->SetActorRotation(FRotator(0,Value,0));R->ForceNetUpdate();return;
        }
        if(Command==TEXT("Level") && Can){Can->RestoreWaterLevel(Value);return;}
        if(Command==TEXT("MoveOut") && Can){Can->SetActorLocation(Can->GetActorLocation()+FVector(400,0,0));return;}
        if(Command==TEXT("DestroyCan") && Can){Can->Destroy();return;}
        if(Command==TEXT("DockFixture") || Command==TEXT("ExtraCan") || Command==TEXT("OutsideCan") || Command==TEXT("LegacySavedCan"))
        {
            UClass* C=LoadClass<ABotanicusWateringCanActor>(nullptr,TEXT("/Game/Botanicus/blueprints/BP_Item_WateringCan.BP_Item_WateringCan_C"));
            auto* NewCan=World->SpawnActor<ABotanicusWateringCanActor>(C);if(!NewCan)return;NewCan->InitializePlacedItem(TEXT("WateringCan"),1);NewCan->RestoreWaterLevel(Value);
            FVector P=R->GetDockGroundLocation();P.Z+=NewCan->GetPlacementPivotToBottomOffset()+1;
            if(Command==TEXT("OutsideCan") || Command==TEXT("LegacySavedCan"))P.X+=180;
            NewCan->SetActorLocation(P);ABotanicusWaterReserveActor::AcceptPlacement(NewCan);NewCan->ForceNetUpdate();return;
        }
        if(Command==TEXT("Plant"))
        {
            auto* Pot=World->SpawnActor<ABotanicusPlantPotActor>();Pot->InitializePlacedItem(TEXT("PlantPot"),1);
            auto Plants=World->GetGameInstance()->GetSubsystem<UBotanicusPlantSubsystem>()->GetAllPlants();if(Plants.IsEmpty())return;
            Pot->RestoreGrowingState(true,Plants[0].PlantKey,.1f,.2f,1.f);Pot->Tags.Add(TEXT("RefillPlant"));
            FVector P=R->GetDockGroundLocation()+FVector(100,0,0);P.Z+=Pot->GetPlacementPivotToBottomOffset();Pot->SetActorLocation(P);
            FBotanicusCarriedItemState State;State.bHasWateringCanState=true;State.WateringCanWaterLevel=.8f;int32 K;if(Bar->AddUniqueItem(TEXT("WateringCan"),State,K))Bar->SelectSlotAuthoritative(K);return;
        }
    });return true;
#else
    return false;
#endif
}
FString UBotanicusRefillValidation::Snapshot(UObject* Context)
{
#if WITH_EDITOR
    auto* W=RefillValidationWorld(Context);if(!W)return TEXT("{}");
    auto J=MakeShared<FJsonObject>();auto* R=RefillFixture(W);
    J->SetNumberField(TEXT("time"),W->GetTimeSeconds());J->SetBoolField(TEXT("authority"),W->GetNetMode()!=NM_Client);
    auto* PC=Cast<ABotanicusPlayerController>(W->GetFirstPlayerController());auto* P=PC ? Cast<ABotanicusCharacter>(PC->GetPawn()):nullptr;
    if(P){auto S=P->GetQuickBarComponent()->GetSelectedSlot();J->SetStringField(TEXT("selected"),S.ItemKey.ToString());J->SetNumberField(TEXT("heldWater"),P->GetQuickBarComponent()->GetSelectedWateringCanWaterLevel());}
    if(PC){J->SetBoolField(TEXT("preview"),IsValid(PC->LocalQuickBarItemPreview));if(PC->LocalQuickBarItemPreview)J->SetStringField(TEXT("previewLocation"),PC->LocalQuickBarItemPreview->GetActorLocation().ToString());J->SetBoolField(TEXT("validPreview"),PC->bLocalQuickBarItemPlacementValid);FHitResult H;PC->TraceRefillStationAim(H);J->SetStringField(TEXT("aim"),GetNameSafe(H.GetActor()));}
    if(PC)
    {
        if(auto* Preview=Cast<ABotanicusWateringCanActor>(PC->LocalQuickBarItemPreview))J->SetObjectField(TEXT("groundPreview"),GroundCanDiagnostic(Preview));
        if(auto* Held=Cast<ABotanicusWateringCanActor>(PC->LocalEquippedQuickBarItem))J->SetObjectField(TEXT("heldCan"),GroundCanDiagnostic(Held));
        if(auto* Held=Cast<ABotanicusWateringCanActor>(PC->LocalInspectedQuickBarItem))J->SetObjectField(TEXT("inspectionCan"),GroundCanDiagnostic(Held));
    }
    if(R)
    {
        J->SetNumberField(TEXT("lastTransferTime"),R->LastTransferTime);J->SetBoolField(TEXT("open"),R->IsOpen());J->SetNumberField(TEXT("opens"),R->GetOpenCount());J->SetStringField(TEXT("can"),GetNameSafe(R->GetDockedCan()));
        if(auto* Can=R->GetDockedCan()){J->SetNumberField(TEXT("water"),Can->GetWaterLevel());J->SetStringField(TEXT("target"),Can->GetFillTargetLocation().ToString());}
        J->SetStringField(TEXT("zoneLocal"),R->FillZone->GetRelativeLocation().ToString());J->SetStringField(TEXT("zoneSize"),R->FillZone->GetUnscaledBoxExtent().ToString());J->SetStringField(TEXT("reservePosition"),R->GetActorLocation().ToString());J->SetNumberField(TEXT("reserveYaw"),R->GetActorRotation().Yaw);J->SetStringField(TEXT("outlet"),R->FaucetOutlet->GetComponentLocation().ToString());J->SetStringField(TEXT("ground"),R->GetDockGroundLocation().ToString());
        TArray<UNiagaraComponent*> FX;R->GetComponents(FX);int32 Active=0;for(auto* F:FX)Active+=F->IsActive();J->SetNumberField(TEXT("activeFX"),Active);
        TArray<UStaticMeshComponent*> Parts;R->GetComponents(Parts);for(auto* M:Parts)if(M->GetName()==TEXT("Refill Area"))J->SetBoolField(TEXT("zoneVisible"),M->IsVisible());
    }
    TArray<TSharedPtr<FJsonValue>> Cans;
    for(TActorIterator<ABotanicusWateringCanActor> It(W);It;++It)if(It->GetIsReplicated() && !It->ActorHasTag(TEXT("BotanicusPlacementPreview")) && !It->ActorHasTag(TEXT("BotanicusEquippedPreview")))
    {
        auto C=GroundCanDiagnostic(*It);C->SetStringField(TEXT("name"),It->GetName());C->SetNumberField(TEXT("water"),It->GetWaterLevel());C->SetStringField(TEXT("location"),It->GetActorLocation().ToString());Cans.Add(MakeShared<FJsonValueObject>(C));
    }
    J->SetArrayField(TEXT("cans"),Cans);
    if(auto* A=W->GetSubsystem<UBotanicusAudioWorldSubsystem>())J->SetNumberField(TEXT("openingSounds"),A->GetEventPlayCount(TEXT("FaucetOpen")));
    int32 Loops=0;for(TObjectIterator<UAudioComponent> It;It;++It)if(It->GetWorld()==W && It->IsPlaying() && It->Sound && It->Sound->GetName()==TEXT("SW_ReserveRefillLoop"))++Loops;J->SetNumberField(TEXT("flowVoices"),Loops);
    if(auto* Juice=W->GetSubsystem<UBotanicusJuiceWorldSubsystem>())J->SetNumberField(TEXT("placementPops"),Juice->GetPlacementStartedCount());
    for(TActorIterator<ABotanicusPlantPotActor> It(W);It;++It)if(It->ActorHasTag(TEXT("RefillPlant")))J->SetNumberField(TEXT("plantWater"),It->GetWaterLevel());
    FString Result;auto Writer=TJsonWriterFactory<>::Create(&Result);FJsonSerializer::Serialize(J,Writer);return Result;
#else
    return TEXT("{}");
#endif
}
