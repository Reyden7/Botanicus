#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BotanicusPlantContextSubsystem.generated.h"

class UWidgetComponent;
class UTexture2D;
class APlayerController;
struct FBotanicusPlantEnvironmentState;
struct FBotanicusPlantDiseaseState;
struct FBotanicusPlantDefinition;

/** Coordinates existing world widgets for this world's local player. No Tick or replicated UI state. */
UCLASS()
class BOTANICUS_API UBotanicusPlantContextSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    void RegisterPlant(AActor* Plant);
    void ObserveAim(APlayerController& Player,const FHitResult& Hit);
    void Update(APlayerController& Player,float DeltaSeconds);
    virtual void Deinitialize() override;
private:
    struct FTarget
    {
        TWeakObjectPtr<AActor> Plant;
        int32 Slot=INDEX_NONE;
        bool operator==(const FTarget& Other) const { return Plant==Other.Plant && Slot==Other.Slot; }
        bool operator!=(const FTarget& Other) const { return !(*this==Other); }
    };
    struct FSlotWidgets
    {
        TWeakObjectPtr<UWidgetComponent> Growth,Environment;
    };
    struct FEntry
    {
        TWeakObjectPtr<AActor> Plant;
        TArray<FSlotWidgets> Slots;
        TWeakObjectPtr<UWidgetComponent> Inspection,Alert;
    };
    struct FAlert
    {
        float Priority=0;
        UTexture2D* Texture=nullptr;
        FLinearColor Tint=FLinearColor::White;
    };
    FEntry* FindEntry(const FTarget& Target);
    bool IsPlanted(const FTarget& Target) const;
    FVector PlantPoint(const FTarget& Target) const;
    bool CanSee(APlayerController& Player,const FVector& Point,AActor* Plant,float Distance) const;
    void ShowTarget(const FTarget& Target,float Opacity);
    void RefreshAlerts(APlayerController& Player);
    UTexture2D* Icon(const TCHAR* Path);
    FAlert PlantAlert(const FBotanicusPlantDefinition* Definition,float Water,bool bDead,
        const FBotanicusPlantDiseaseState& Disease,const FBotanicusPlantEnvironmentState& Environment);
    static void ShowWidget(UWidgetComponent* Widget,float Opacity);
    TArray<FEntry> Entries;
    FTarget Candidate,Active;
    float CandidateSince=0,Opacity=0,AlertAccumulator=0,SafetyAccumulator=0;
    UPROPERTY(Transient) TMap<FName,TObjectPtr<UTexture2D>> Icons;
};
