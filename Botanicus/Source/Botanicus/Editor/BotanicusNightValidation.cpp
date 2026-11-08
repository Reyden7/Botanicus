#include "Editor/BotanicusNightValidation.h"
#if WITH_EDITOR
#include "Environment/BotanicusClandestineDoor.h"
#include "Environment/BotanicusAmbienceDirector.h"
#include "Audio/BotanicusAudioWorldSubsystem.h"
#include "BotanicusCharacter.h"
#include "BotanicusGameState.h"
#include "BotanicusGameMode.h"
#include "Visitors/BotanicusVisitorCharacter.h"
#include "UI/BotanicusClockWidget.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "Components/TextBlock.h"
#include "Engine/Light.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "NavigationSystem.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Serialization/JsonSerializer.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "Editor.h"
namespace
{
UWorld* TestWorld(UObject* C)
{
    UWorld* W=C ? C->GetWorld():nullptr;
    return W && W->WorldType==EWorldType::PIE && FParse::Param(FCommandLine::Get(),TEXT("BotanicusNightValidation")) && FPaths::ProjectSavedDir().Contains(TEXT("/isolated_user/")) ? W:nullptr;
}
ABotanicusClandestineDoor* Door(UWorld* W){for(TActorIterator<ABotanicusClandestineDoor> It(W);It;++It)return *It;return nullptr;}
TMap<TWeakObjectPtr<UWorld>,bool> PlayerBlocked,NPCBlocked;
}
#endif
bool UBotanicusNightValidation::Action(UObject* Context,FName Command,float Value)
{
#if WITH_EDITOR
    UWorld* W=TestWorld(Context);if(!W || W->GetNetMode()==NM_Client)return false;
    W->GetTimerManager().SetTimerForNextTick([Weak=TWeakObjectPtr<UWorld>(W),Command,Value]()
    {
        UWorld* World=Weak.Get();if(!World)return;auto* D=Door(World);auto* GS=World->GetGameState<ABotanicusGameState>();if(!D || !GS)return;
        if(Command==TEXT("Time")){GS->SetDayTimeMinutesForDevelopment(Value*60.f);D->RefreshAuthority();return;}
        if(Command==TEXT("Save")){if(auto* GM=World->GetAuthGameMode<ABotanicusGameMode>())GM->BotanicusSaveNow();return;}
        if(Command==TEXT("LateJoin")){if(GEditor)GEditor->RequestLateJoin();return;}
        if(Command==TEXT("SweepNPC"))
        {
            auto* NPC=World->SpawnActor<ABotanicusVisitorCharacter>();if(!NPC)return;
            const FVector A=D->GetActorTransform().TransformPosition(FVector(250,0,100)),B=D->GetActorTransform().TransformPosition(FVector(-250,0,100));
            NPC->SetActorLocation(A,false,nullptr,ETeleportType::TeleportPhysics);FHitResult Hit;NPC->SetActorLocation(B,true,&Hit);NPCBlocked.Add(World,Hit.bBlockingHit && Hit.GetComponent()==D->GetBlocker());NPC->Destroy();return;
        }
        int32 I=0;
        for(TActorIterator<ABotanicusCharacter> It(World);It;++It)
        {
            FVector Local(650,I++*180,It->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+3);
            if(Command==TEXT("Portal"))Local=FVector(0,0,Local.Z);
            if(Command==TEXT("Inside"))Local=FVector(-220,0,Local.Z);
            if(Command==TEXT("SweepPlayer"))Local=FVector(250,0,Local.Z);
            It->GetCharacterMovement()->StopMovementImmediately();It->SetActorLocation(D->GetActorTransform().TransformPosition(Local),false,nullptr,ETeleportType::TeleportPhysics);
            if(AController* PC=It->GetController())PC->SetControlRotation((D->GetActorTransform().TransformPosition(FVector(0,0,D->Height*.53f))-It->GetPawnViewLocation()).Rotation());
            if(Command==TEXT("SweepPlayer"))
            {
                FHitResult Hit;It->SetActorLocation(D->GetActorTransform().TransformPosition(FVector(-250,0,Local.Z)),true,&Hit);PlayerBlocked.Add(World,Hit.bBlockingHit && Hit.GetComponent()==D->GetBlocker());break;
            }
        }
    });return true;
#else
    return false;
#endif
}
FString UBotanicusNightValidation::Snapshot(UObject* Context)
{
#if WITH_EDITOR
    UWorld* W=TestWorld(Context);if(!W)return TEXT("{}");auto* D=Door(W);auto* GS=W->GetGameState<ABotanicusGameState>();if(!D || !GS)return TEXT("{}");
    auto R=MakeShared<FJsonObject>();R->SetNumberField(TEXT("hour"),GS->GetDayTimeMinutes()/60);R->SetBoolField(TEXT("shop"),GS->IsMainShopOpen());R->SetBoolField(TEXT("trade"),GS->IsIllegalTradeActive());
    R->SetNumberField(TEXT("open"),D->GetOpenFraction());R->SetNumberField(TEXT("neon"),D->GetNeonWeight());R->SetNumberField(TEXT("publicWeight"),D->GetPublicLightWeight());R->SetBoolField(TEXT("blocked"),D->IsPassageBlocked());R->SetBoolField(TEXT("safetyHold"),D->IsWaitingForExit());
    R->SetNumberField(TEXT("collision"),int32(D->GetBlocker()->GetCollisionEnabled()));R->SetBoolField(TEXT("playerSweepBlocked"),PlayerBlocked.FindRef(W));R->SetBoolField(TEXT("npcSweepBlocked"),NPCBlocked.FindRef(W));
    R->SetNumberField(TEXT("motionDuration"),D->GetPresentation().MotionDuration);R->SetNumberField(TEXT("neonStart"),D->GetPresentation().NeonStart);R->SetNumberField(TEXT("neonOff"),D->GetPresentation().NeonOff);
    auto* Audio=W->GetSubsystem<UBotanicusAudioWorldSubsystem>();
    R->SetNumberField(TEXT("openAudio"),Audio && D->OpenSound ? Audio->GetEventPlayCount(D->OpenSound->GetFName()) : 0);R->SetNumberField(TEXT("closeAudio"),Audio && D->CloseSound ? Audio->GetEventPlayCount(D->CloseSound->GetFName()) : 0);R->SetNumberField(TEXT("neonAudio"),Audio && D->NeonSound ? Audio->GetEventPlayCount(D->NeonSound->GetFName()) : 0);
    TArray<TSharedPtr<FJsonValue>> Lights;
    for(TActorIterator<ABotanicusAmbienceDirector> It(W);It;++It)
    {for(ALight* L:It->InteriorLights)if(L)Lights.Add(MakeShared<FJsonValueNumber>(L->GetLightComponent()->Intensity));for(ALight* L:It->ClandestineLights)if(L)R->SetNumberField(TEXT("accentIntensity"),L->GetLightComponent()->Intensity);}
    R->SetArrayField(TEXT("interiorIntensities"),Lights);
    const auto& Parts=D->GetSections();if(!Parts.IsEmpty())
    {R->SetNumberField(TEXT("firstPanelPitch"),Parts[0]->GetRelativeRotation().Pitch);R->SetNumberField(TEXT("firstPanelZ"),Parts[0]->GetRelativeLocation().Z);R->SetNumberField(TEXT("lastPanelX"),Parts[D->SectionCount-1]->GetRelativeLocation().X);}
    TArray<UUserWidget*> Widgets;UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,Widgets,UBotanicusClockWidget::StaticClass(),false);
    for(UUserWidget* Widget:Widgets)if(auto* Label=Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("ScheduleLabel")))){R->SetStringField(TEXT("hud"),Label->GetText().ToString());break;}
    if(W->GetNetMode()!=NM_Client)
    {
        auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(W);FNavLocation A,B;
        const FVector P=D->GetActorTransform().TransformPosition(FVector(230,0,70)),Q=D->GetActorTransform().TransformPosition(FVector(-230,0,70));
        const bool Projected=Nav && Nav->ProjectPointToNavigation(P,A,FVector(70,70,180)) && Nav->ProjectPointToNavigation(Q,B,FVector(70,70,180));R->SetBoolField(TEXT("navProjected"),Projected);
        FVector Hit;R->SetBoolField(TEXT("navRayBlocked"),Projected && UNavigationSystemV1::NavigationRaycast(W,A.Location,B.Location,Hit));
    }
    FString Text;auto Writer=TJsonWriterFactory<>::Create(&Text);FJsonSerializer::Serialize(R,Writer);return Text;
#else
    return TEXT("{}");
#endif
}
