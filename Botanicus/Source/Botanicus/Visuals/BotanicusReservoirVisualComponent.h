#pragma once
#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "BotanicusReservoirVisualComponent.generated.h"
class UStaticMesh;
class UStaticMeshComponent;
class UProceduralMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
/** Local presentation only. Never changes water quantities, navigation, collision or save data. */
UCLASS(ClassGroup=(Botanicus),meta=(BlueprintSpawnableComponent))
class BOTANICUS_API UBotanicusReservoirVisualComponent : public USceneComponent
{
    GENERATED_BODY()
public:
    UBotanicusReservoirVisualComponent();
    virtual void OnRegister() override;
    virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTick) override;
    void Configure(UStaticMeshComponent* Container,bool bGauge);
    void SetFillFraction(float Fraction);
    void SetReservoirVisible(bool bVisible);
    UPROPERTY(EditAnywhere,Category="Reservoir|Materials") TSoftObjectPtr<UMaterialInterface> RobotWaterMaterial;
    UPROPERTY(EditAnywhere,Category="Reservoir|Materials") TSoftObjectPtr<UMaterialInterface> ReserveWaterMaterial;
    UPROPERTY(EditAnywhere,Category="Reservoir|Materials") TSoftObjectPtr<UMaterialInterface> RobotGlassMaterial;
    UPROPERTY(EditAnywhere,Category="Reservoir|Materials") TSoftObjectPtr<UMaterialInterface> ReserveGlassMaterial;
    /** Measured in cuve_d_eau mesh coordinates, inside the existing metal frame. */
    UPROPERTY(EditAnywhere,Category="Reservoir|Gauge") FVector GaugeCenter=FVector(-4.2f,34.85f,5.5f);
    UPROPERTY(EditAnywhere,Category="Reservoir|Gauge") FVector GaugeHalfSize=FVector(2.65f,.1f,18.65f);
    UPROPERTY(EditAnywhere,Category="Reservoir|Gauge",meta=(ClampMin=".05",Units="cm")) float GlassGap=.2f;
    UPROPERTY(EditAnywhere,Category="Reservoir|Cylinder",meta=(ClampMin=".5",ClampMax=".99")) float InteriorScale=.94f;
    UPROPERTY(EditAnywhere,Category="Reservoir|Slosh") bool bSloshEnabled=true;
    UPROPERTY(EditAnywhere,Category="Reservoir|Slosh",meta=(ClampMin="0",ClampMax="15",Units="deg")) float MaxSloshAngle=12;
    UPROPERTY(EditAnywhere,Category="Reservoir|Slosh",meta=(ClampMin="0",ClampMax="4")) float AccelerationStrength=1.4f;
    UPROPERTY(EditAnywhere,Category="Reservoir|Slosh",meta=(ClampMin="1",ClampMax="20")) float SpringFrequency=8;
    UPROPERTY(EditAnywhere,Category="Reservoir|Slosh",meta=(ClampMin=".1",ClampMax="1.5")) float DampingRatio=.48f;
    UPROPERTY(EditAnywhere,Category="Reservoir|Slosh",meta=(ClampMin="500",Units="cm")) float MaxVisualDistance=4500;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Transient,Category="Reservoir") TObjectPtr<UMaterialInstanceDynamic> WaterMID;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Reservoir") TObjectPtr<UProceduralMeshComponent> Liquid;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Reservoir") TObjectPtr<UProceduralMeshComponent> GaugeGlass;
    float GetFillFraction() const{return FillFraction;}
    FVector2D GetSloshSlope() const{return Slosh;}
private:
    bool CanAnimate() const;
    void WriteSlosh();
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> ContainerMesh;
    UPROPERTY(Transient) TObjectPtr<UStaticMesh> BuiltMesh;
    FVector BuiltCenter=FVector::ZeroVector,BuiltHalfSize=FVector::ZeroVector;
    FVector2D LastWrittenSlosh=FVector2D(1000,1000);
    bool bLastSloshEnabled=false,bSloshWritten=false;
    float BuiltGlassGap=-1;
    float FillFraction=1;
    bool bIsGauge=false,bConfigured=false,bDisplay=true;
    FVector PreviousVelocity=FVector::ZeroVector;
    FVector2D Slosh=FVector2D::ZeroVector,SloshVelocity=FVector2D::ZeroVector,FilteredAcceleration=FVector2D::ZeroVector;
};
