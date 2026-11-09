#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Growing/BotanicusPlantCatalog.h"
#include "BotanicusPlantContextSubsystem.generated.h"

class UWidgetComponent;
class UUserWidget;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
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
    void UpdateCameraLayout(APlayerController& Player);
    virtual void Deinitialize() override;
private:
    friend class UBotanicusPlantContextValidation;
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
        TWeakObjectPtr<USceneComponent> Root,TopAnchor;
        TWeakObjectPtr<UStaticMesh> CachedMeshes[3];
        FTransform CachedTransforms[3];
        bool CachedVisible[3]={false,false,false};
        FBox PlantBounds=FBox(ForceInit),PotBounds=FBox(ForceInit);
        int32 LayoutSlot=INDEX_NONE;
        FVector LastEye=FVector::ZeroVector;
        FVector2D PanelSize=FVector2D::ZeroVector;
        FVector CachedSideIconsLocalOffset=FVector::ZeroVector;
        float CachedSideIconsScale=0;
        FVector2D CachedPanelOffsets=FVector2D::ZeroVector;
        FRotator LastView=FRotator::ZeroRotator;
        bool bLayoutValid=false;
    };
    struct FAlert
    {
        float Priority=0;
        UTexture2D* Texture=nullptr;
        FLinearColor Tint=FLinearColor::White;
    };
    struct FReadout
    {
        FText Name,Quality=FText::FromString(TEXT("—"));
        float Water=0,Growth=0,Age=-1;
        int32 Price=0;
        const FBotanicusPlantDefinition* Definition=nullptr;
        FBotanicusPlantEnvironmentState Environment;
    };
    void RegisterReadyPlant(AActor* Plant);
    FReadout Readout(const FTarget& Target) const;
    void RefreshReadout(const FTarget& Target);
    void UpdateLayout(APlayerController& Player,const FTarget& Target);
    void VisualComponents(const FTarget& Target,UStaticMeshComponent*& Plant,UStaticMeshComponent*& Stem,UStaticMeshComponent*& Pot) const;
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
    TSet<TWeakObjectPtr<AActor>> Pending;
    TArray<int32> Nearby;
    FVector CandidateSight=FVector::ZeroVector,ActiveSight=FVector::ZeroVector;
    TArray<FEntry> Entries;
    FTarget Candidate,Active;
    float CandidateSince=0,Opacity=0,AlertAccumulator=0,SafetyAccumulator=0;
    UPROPERTY(Transient) TMap<FName,TObjectPtr<UTexture2D>> Icons;
};
