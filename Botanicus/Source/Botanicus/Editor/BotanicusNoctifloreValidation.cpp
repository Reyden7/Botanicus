#include "Editor/BotanicusNoctifloreValidation.h"
#if WITH_EDITOR
#include "Visuals/BotanicusNoctifloreSettings.h"
#include "Visuals/BotanicusJuiceWorldSubsystem.h"
#include "Visuals/BotanicusJuiceSettings.h"
#include "Audio/BotanicusAudioWorldSubsystem.h"
#include "BotanicusGameState.h"
#include "BotanicusCharacter.h"
#include "IllegalTrade/BotanicusIllegalPlanterActor.h"
#include "Environment/BotanicusAmbienceDirector.h"
#include "Components/StaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "Engine/Light.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
namespace {
UWorld* NoctifloreValidationWorld(UObject* Context)
{
    UWorld* W=GEngine ? GEngine->GetWorldFromContextObject(Context,EGetWorldErrorMode::ReturnNull) : nullptr;
    FString Saved=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());FPaths::NormalizeDirectoryName(Saved);
    return W && W->WorldType==EWorldType::PIE && FParse::Param(FCommandLine::Get(),TEXT("BotanicusNoctifloreValidation")) && Saved.Contains(TEXT("/isolated_user/")) ? W : nullptr;
}
}
#endif
bool UBotanicusNoctifloreValidation::Step(UObject* Context,FName Action)
{
#if WITH_EDITOR
    UWorld* W=NoctifloreValidationWorld(Context);if(!W) return false;
    auto* Settings=GetMutableDefault<UBotanicusNoctifloreSettings>();
    if(Action==TEXT("Off") || Action==TEXT("On")) {Settings->bEnabled=Action==TEXT("On");return true;}
    if(Action==TEXT("StressBudget")) {Settings->SporeInterval=.5f;Settings->SporeLifetime=5;Settings->SporeCount=6;return true;}
    if(Action==TEXT("DefaultBudget")) {Settings->SporeInterval=2.4f;Settings->SporeLifetime=3.2f;Settings->SporeCount=3;return true;}
    if(Action==TEXT("SporesOff") || Action==TEXT("SporesOn")) {Settings->bSpores=Action==TEXT("SporesOn");return true;}
    if(W->GetNetMode()==NM_Client) return false;
    TGuardValue<bool> Routing(GAllowActorScriptExecutionInEditor,false);
    if(Action==TEXT("Setup"))
    {
        for(TActorIterator<ABotanicusIllegalPlanterActor> It(W);It;++It) It->Destroy(); // Test copy only: isolate exactly four planters, without saved crops.
        for(int32 I=0;I<4;++I)
        {
            auto* Pot=W->SpawnActor<ABotanicusIllegalPlanterActor>();if(!Pot) return false;
            Pot->Tags.Add(TEXT("NoctifloreTest"));Pot->InitializePlacedItem(TEXT("IllegalPlanter"),1);
            Pot->SetActorLocation(FVector(-7090.f+(I%2)*260.f,-7340.f+(I/2)*220.f,147.36f));Pot->SetActorTickEnabled(false);
            TArray<FBotanicusIllegalPlantSlotState> Slots;Slots.SetNum(4);
            const float Growth[]={.1f,.4f,.7f,1.f};
            for(int32 J=0;J<4;++J) {Slots[J].PlantId=TEXT("Noctiflore");Slots[J].GrowthProgress=Growth[J];Slots[J].WaterAmount=1;}
            Pot->RestoreIllegalPlanterState(1,4,0,Slots);Pot->ForceNetUpdate();
        }
        int32 I=0;for(TActorIterator<ABotanicusCharacter> It(W);It;++It)
        {
            It->SetActorLocation(FVector(-6980.f-(I++)*100.f,-7710.f,235.36f));
            if(auto* PC=Cast<APlayerController>(It->GetController())) PC->SetControlRotation(FRotator(-9.f,84.f,0));
        }
        return true;
    }
    bool bFound=false;
    for(TActorIterator<ABotanicusIllegalPlanterActor> It(W);It;++It) if(It->ActorHasTag(TEXT("NoctifloreTest")) ||
        (It->GetActorLocation().X>=-7091 && It->GetActorLocation().X<=-6829 && It->GetActorLocation().Y>=-7341 && It->GetActorLocation().Y<=-7119))
    {
        bFound=true;It->SetActorTickEnabled(false);
        if(Action==TEXT("Destroy")) {It->Destroy();continue;}
        if(Action==TEXT("Freeze")) continue;
        if(Action==TEXT("Hide") || Action==TEXT("Show")) {It->SetActorHiddenInGame(Action==TEXT("Hide"));continue;}
        if(Action==TEXT("Clear")) {It->RestoreIllegalPlanterState(1,4,0,{});continue;}
        float Growth=Action==TEXT("Young") ? .1f : Action==TEXT("Developed") ? .4f : Action==TEXT("Large") ? .7f : Action==TEXT("Mature") ? 1.f : -1;
        if(Growth>=0) {It->SetAllGrowthForDevelopment(Growth);It->ForceNetUpdate();}
    }
    return bFound;
#else
    return false;
#endif
}
FString UBotanicusNoctifloreValidation::Snapshot(UObject* Context)
{
#if WITH_EDITOR
    UWorld* W=NoctifloreValidationWorld(Context);if(!W) return TEXT("{}");
    auto Root=MakeShared<FJsonObject>();
    if(auto* J=W->GetSubsystem<UBotanicusJuiceWorldSubsystem>())
    {
        Root->SetNumberField(TEXT("components"),J->GetSporeComponentCount());Root->SetNumberField(TEXT("particles"),J->GetSporeParticleCount());
        Root->SetNumberField(TEXT("reserved"),J->GetSporeReservedParticles());Root->SetNumberField(TEXT("candidates"),J->GetNoctifloreCandidateCount());
    }
    if(auto* GS=W->GetGameState<ABotanicusGameState>()) {Root->SetNumberField(TEXT("minutes"),GS->GetDayTimeMinutes());Root->SetBoolField(TEXT("trade"),GS->IsIllegalTradeActive());}
    if(auto* Audio=W->GetSubsystem<UBotanicusAudioWorldSubsystem>()) {Root->SetNumberField(TEXT("audioStarts"),Audio->GetAmbienceStartCount());Root->SetNumberField(TEXT("nightVolume"),Audio->GetAmbienceMixVolume(true));}
    TArray<TSharedPtr<FJsonValue>> Lights;
    for(TActorIterator<ABotanicusAmbienceDirector> It(W);It;++It) for(ALight* Light:It->ClandestineLights)
        if(Light) Lights.Add(MakeShared<FJsonValueNumber>(Light->GetLightComponent()->Intensity));
    Root->SetArrayField(TEXT("lights"),Lights);
    TArray<TSharedPtr<FJsonValue>> Plants;
    for(TActorIterator<ABotanicusIllegalPlanterActor> It(W);It;++It)
    {
        auto Slots=It->GetPlantSlots();TArray<UStaticMeshComponent*> Components;It->GetComponents(Components);
        for(auto* C:Components) if(C->GetName().StartsWith(TEXT("IllegalPlantVisual")) && C->IsVisible())
        {
            auto P=MakeShared<FJsonObject>();P->SetStringField(TEXT("name"),C->GetPathName());
            const int32 SlotIndex=FCString::Atoi(*C->GetName().Right(2));
            if(Slots.IsValidIndex(SlotIndex)) {P->SetNumberField(TEXT("growth"),Slots[SlotIndex].GrowthProgress);P->SetNumberField(TEXT("water"),Slots[SlotIndex].WaterAmount);}
P->SetStringField(TEXT("mesh"),C->GetStaticMesh()->GetPathName());
            P->SetStringField(TEXT("transform"),C->GetRelativeTransform().ToString());P->SetNumberField(TEXT("collision"),static_cast<int32>(C->GetCollisionEnabled()));
            if(auto* MID=Cast<UMaterialInstanceDynamic>(C->GetMaterial(0))) {P->SetStringField(TEXT("parent"),MID->Parent->GetPathName());P->SetNumberField(TEXT("strength"),MID->K2_GetScalarParameterValue(TEXT("NoctGrowthStrength")));}
            Plants.Add(MakeShared<FJsonValueObject>(P));
        }
    }
    Root->SetArrayField(TEXT("plants"),Plants);FString Out;auto Writer=TJsonWriterFactory<>::Create(&Out);FJsonSerializer::Serialize(Root,Writer);return Out;
#else
    return TEXT("{}");
#endif
}
