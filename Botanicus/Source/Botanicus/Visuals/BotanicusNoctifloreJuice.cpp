#include "Visuals/BotanicusJuiceWorldSubsystem.h"
#include "Visuals/BotanicusJuiceSettings.h"
#include "Visuals/BotanicusNoctifloreSettings.h"
#include "BotanicusGameState.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "NiagaraComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "NiagaraSystemInstanceController.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraEmitterInstance.h"
#include "Scalability.h"

int32 UBotanicusJuiceWorldSubsystem::GetSporeReservedParticles() const
{
    int32 Count=0;for(const auto& Spore:NoctifloreSpores) Count+=Spore.ReservedParticles;return Count;
}
void UBotanicusJuiceWorldSubsystem::ClearNoctifloreSpores()
{
    for(auto& Spore:NoctifloreSpores) if(Spore.Component) {Spore.Component->DeactivateImmediate();Spore.Component->DestroyComponent();}
    NoctifloreSpores.Reset();
}
void UBotanicusJuiceWorldSubsystem::UpdateNoctiflores()
{
    // Reuse the 3A reconciliation timer. No per-plant Tick, timer or world scan.
    const auto* S=GetDefault<UBotanicusNoctifloreSettings>();
    const bool bEnabled=S->bEnabled && GetDefault<UBotanicusJuiceSettings>()->bEnabled &&
        (!S->bDisableOnLowEffectsQuality || Scalability::GetQualityLevels().EffectsQuality>0);
    if(NoctifloreCatalog && NoctifloreCatalog->Parameters)
    {
        if(auto* MPC=GetWorld()->GetParameterCollectionInstance(NoctifloreCatalog->Parameters))
        {
            MPC->SetScalarParameterValue(TEXT("NoctGlowIntensity"),bEnabled && S->bBioluminescence ? FMath::Clamp(S->GlowIntensity,0.f,3.f) : 0.f);
            MPC->SetScalarParameterValue(TEXT("NoctPulseSpeed"),FMath::Clamp(S->PulseSpeed,0.f,2.f));
            MPC->SetScalarParameterValue(TEXT("NoctGlowDistance"),FMath::Clamp(S->GlowDisplayDistance,100.f,5000.f));
            MPC->SetVectorParameterValue(TEXT("NoctGlowTint"),S->GlowTint);
        }
    }
    const double Now=GetWorld()->GetTimeSeconds();
    const auto* State=GetWorld()->GetGameState<ABotanicusGameState>();
    const bool bSpores=bEnabled && S->bSpores && (!S->bSporesOnlyDuringTrade || (State && State->IsIllegalTradeActive()));
    const int32 MaxComponents=FMath::Clamp(S->MaximumSporeComponents,0,4);
    const int32 MaxParticles=FMath::Clamp(S->MaximumSporeParticles,0,24);
    if(!bSpores) ClearNoctifloreSpores();
    const FVector Listener=ListenerPosition();
    const float MaxDistance2=FMath::Square(FMath::Clamp(S->SporeDisplayDistance,100.f,2500.f));
    for(int32 I=NoctifloreSpores.Num()-1;I>=0;--I)
    {
        const auto& Spore=NoctifloreSpores[I];auto* Foliage=Spore.Foliage.Get();
        if(!IsValid(Foliage) || !IsValid(Foliage->GetOwner()) || !Foliage->IsVisible() || Foliage->GetOwner()->IsHidden() || !NoctifloreCandidates.Contains(Foliage) || Foliage->GetStaticMesh()!=Spore.Mesh.Get() ||
            FVector::DistSquared(Listener,Foliage->Bounds.Origin)>MaxDistance2 || Now>=Spore.ExpireAt ||
            NoctifloreSpores.Num()>MaxComponents || GetSporeReservedParticles()>MaxParticles)
        {
            if(Spore.Component) {Spore.Component->DeactivateImmediate();Spore.Component->DestroyComponent();}
            NoctifloreSpores.RemoveAtSwap(I);
        }
    }
    UStaticMeshComponent* Candidate=nullptr;double Oldest=DBL_MAX;
    for(auto It=NoctifloreCandidates.CreateIterator();It;++It)
    {
        auto* Foliage=It.Key().Get();
        if(!IsValid(Foliage) || !IsValid(Foliage->GetOwner()) || !Foliage->IsVisible()) {It.RemoveCurrent();continue;}
        if(Foliage->GetOwner()->IsHidden()) continue;
        if(FVector::DistSquared(Listener,Foliage->Bounds.Origin)<=MaxDistance2 && It.Value()<Oldest) {Oldest=It.Value();Candidate=Foliage;}
    }
    if(!bSpores || !SporeSystem || !Candidate || Now<NextSporeAt || NoctifloreSpores.Num()>=MaxComponents) return;
    const int32 Count=FMath::Min(FMath::Clamp(S->SporeCount,0,6),MaxParticles-GetSporeReservedParticles());if(Count<=0) return;
    auto* Component=CreateEffect(SporeSystem);if(!Component) return;
    const float Life=FMath::Clamp(S->SporeLifetime,.5f,5.f);
    const float Phase=FMath::Fmod(FMath::Abs(Candidate->Bounds.Origin.X*.031f+Candidate->Bounds.Origin.Y*.047f),6.283185f);
    Component->SetWorldLocation(Candidate->Bounds.Origin);
    Component->SetVariableInt(TEXT("User.BurstCount"),Count);
    Component->SetVariableFloat(TEXT("User.Lifetime"),Life);
    Component->SetVariableFloat(TEXT("User.Size"),FMath::Clamp(S->SporeSize,.2f,3.f));
    Component->SetVariableFloat(TEXT("User.Radius"),FMath::Clamp(Candidate->Bounds.BoxExtent.Z*.4f,5.f,18.f));
    Component->SetVariableVec3(TEXT("User.Velocity"),FVector(FMath::Sin(Phase)*.45f,FMath::Cos(Phase)*.45f,1.f)*FMath::Clamp(S->SporeSpeed,0.f,15.f));
    Component->SetVariableVec3(TEXT("User.Gravity"),FVector::ZeroVector);
    Component->SetVariableLinearColor(TEXT("User.Tint"),S->SporeTint);Component->Activate(true);
    auto& Spore=NoctifloreSpores.AddDefaulted_GetRef();Spore.Component=Component;Spore.Foliage=Candidate;
    Spore.Mesh=Candidate->GetStaticMesh();Spore.ExpireAt=Now+Life+.15;Spore.ReservedParticles=Count;
    NoctifloreCandidates[Candidate]=Now;
    NextSporeAt=Now+FMath::Clamp(S->SporeInterval,.5f,15.f)*(1.f+.12f*FMath::Sin(Phase));
}

int32 UBotanicusJuiceWorldSubsystem::GetSporeParticleCount() const
{
    int32 Count=0;
    for(const auto& Spore:NoctifloreSpores) if(Spore.Component)
    {
        auto Controller=Spore.Component->GetSystemInstanceController();if(!Controller.IsValid()) continue;
        Controller->WaitForConcurrentTickAndFinalize();
        if(auto* Instance=Controller->GetSystemInstance_Unsafe()) for(const auto& Emitter:Instance->GetEmitters()) Count+=Emitter->GetNumParticles();
    }
    return Count;
}
