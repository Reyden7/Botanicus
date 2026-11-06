#include "Visuals/BotanicusJuiceWorldSubsystem.h"
#include "Visuals/BotanicusJuiceSettings.h"
#include "Visuals/BotanicusJuiceRelay.h"
#include "Visuals/BotanicusPotSoilVisualActor.h"
#include "Audio/BotanicusAudioRelay.h"
#include "BotanicusCharacter.h"
#include "BotanicusPlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/ChildActorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Growing/BotanicusWateringCanActor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemInstanceController.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraEmitterInstance.h"
#include "TimerManager.h"
#include "Audio/BotanicusAudioWorldSubsystem.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Scalability.h"
#include "Visuals/BotanicusNoctifloreSettings.h"

bool UBotanicusJuiceWorldSubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{
	return Type==EWorldType::Game || Type==EWorldType::PIE;
}
void UBotanicusJuiceWorldSubsystem::OnWorldBeginPlay(UWorld& World)
{
	Super::OnWorldBeginPlay(World);
	if(World.GetNetMode()!=NM_Client) GetRelay();
	if(World.GetNetMode()==NM_DedicatedServer) return;
	const auto* Settings=GetDefault<UBotanicusJuiceSettings>();
	MotionCatalog=Settings->MotionCatalog.LoadSynchronous();
	NoctifloreCatalog=GetDefault<UBotanicusNoctifloreSettings>()->GlowCatalog.LoadSynchronous();
	SporeSystem=GetDefault<UBotanicusNoctifloreSettings>()->SporeSystem.LoadSynchronous();
	UpdateNoctiflores(); // Apply disabled/custom settings before the first rendered frame.
	UpdateMotionParameters();
	WaterSystem=Settings->WaterSystem.LoadSynchronous();BurstSystem=Settings->BurstSystem.LoadSynchronous();
	World.GetTimerManager().SetTimer(ReconcileTimer,this,&UBotanicusJuiceWorldSubsystem::ReconcileWater,.1f,true);
}
ABotanicusJuiceRelay* UBotanicusJuiceWorldSubsystem::GetRelay()
{
	if(Relay.IsValid()) return Relay.Get();
	for(TActorIterator<ABotanicusJuiceRelay> It(GetWorld());It;++It) {Relay=*It;return *It;}
	if(GetWorld()->GetNetMode()!=NM_Client) Relay=GetWorld()->SpawnActor<ABotanicusJuiceRelay>();
	return Relay.Get();
}
void UBotanicusJuiceWorldSubsystem::PlantSucceeded(AActor* Pot,const FVector& Position)
{
	if(IsValid(Pot) && Pot->HasAuthority())
		if(auto* Juice=Pot->GetWorld()->GetSubsystem<UBotanicusJuiceWorldSubsystem>())
			if(auto* Events=Juice->GetRelay()) Events->PlantSucceeded(Pot,Position);
}
FVector UBotanicusJuiceWorldSubsystem::SoilSurface(AActor* Pot,const FVector& Hint)
{
	if(!IsValid(Pot)) return Hint;
	TArray<UStaticMeshComponent*> Meshes;Pot->GetComponents(Meshes);
	for(auto* Mesh:Meshes) if(Mesh->GetFName()==TEXT("MultiSoil")) return FVector(Hint.X,Hint.Y,Mesh->Bounds.Origin.Z+Mesh->Bounds.BoxExtent.Z);
	TArray<UChildActorComponent*> Children;Pot->GetComponents(Children);
	for(auto* Child:Children) if(auto* Soil=Cast<ABotanicusPotSoilVisualActor>(Child->GetChildActor())) return Soil->GetSurfaceCentre();
	return Hint;
}
FVector UBotanicusJuiceWorldSubsystem::ListenerPosition() const
{
	const APlayerController* PC=GetWorld()->GetFirstPlayerController();
	return PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
}
UNiagaraComponent* UBotanicusJuiceWorldSubsystem::CreateEffect(UNiagaraSystem* System)
{
	if(!System || GetWorld()->GetNetMode()==NM_DedicatedServer) return nullptr;
	auto* Component=NewObject<UNiagaraComponent>(GetWorld());
	Component->SetAutoActivate(false);Component->SetAutoDestroy(false);Component->SetAsset(System);
	Component->SetCastShadow(false);Component->RegisterComponentWithWorld(GetWorld());
	return Component;
}
void UBotanicusJuiceWorldSubsystem::ReceivePlant(uint32 Sequence,AActor* Pot,const FVector& Position)
{
	if(Sequence==0 || static_cast<int32>(Sequence-LastPlantSequence)<=0) return;
	LastPlantSequence=Sequence;
	const auto* S=GetDefault<UBotanicusJuiceSettings>();
	if(!S->bEnabled || !S->bPlantingBurst || S->PlantingIntensity<=0 || !IsValid(Pot) || FVector::DistSquared(ListenerPosition(),Position)>FMath::Square(S->PlantingDisplayDistance)) return;
	SpawnBurst(Pot,Position+FVector(0,0,1.5f),S->SoilTint,FMath::Clamp(FMath::RoundToInt(S->PlantingCount*S->PlantingIntensity),1,24),2.1f*S->PlantingIntensity,S->PlantingLifetime,60.f);
	++PlantBursts;
}
void UBotanicusJuiceWorldSubsystem::SpawnBurst(AActor* Surface,const FVector& Position,const FLinearColor& Tint,int32 Count,float Size,float Lifetime,float Speed,AActor* Source)
{
	if(!IsValid(Surface) || Bursts.Num()>=GetDefault<UBotanicusJuiceSettings>()->MaximumBurstComponents) return;
	auto* Component=CreateEffect(BurstSystem);if(!Component) return;
	Component->SetWorldLocation(Position);Component->SetVariableInt(TEXT("User.BurstCount"),Count);
	Component->SetVariableFloat(TEXT("User.Lifetime"),FMath::Clamp(Lifetime,.1f,.8f));
	Component->SetVariableFloat(TEXT("User.Size"),FMath::Clamp(Size,.2f,4.f));
	Component->SetVariableLinearColor(TEXT("User.Tint"),Tint);Component->SetVariableVec3(TEXT("User.Velocity"),FVector(0,0,Speed));
	Component->SetVariableVec3(TEXT("User.Gravity"),FVector(0,0,-180));Component->Activate(true);
	auto& Burst=Bursts.AddDefaulted_GetRef();Burst.Component=Component;Burst.Surface=Surface;Burst.Source=Source ? Source : Surface;Burst.ExpireAt=GetWorld()->GetTimeSeconds()+Lifetime+.15;
}
void UBotanicusJuiceWorldSubsystem::SetSoilWetness(UObject* Owner,UMaterialInstanceDynamic* Material,float Wetness,float Exponent)
{
	if(!IsValid(Owner) || !Material || !Owner->GetWorld() || Owner->GetWorld()->GetNetMode()==NM_DedicatedServer) return;
	const auto* S=GetDefault<UBotanicusJuiceSettings>();
	const float Target=FMath::Clamp(FMath::Pow(FMath::Clamp(Wetness,0.f,1.f),Exponent)*(S->bEnabled ? S->WetnessIntensity : 1.f),0.f,1.f);
	auto* Juice=Owner->GetWorld()->GetSubsystem<UBotanicusJuiceWorldSubsystem>();
	if(!Juice || !Owner->GetWorld()->HasBegunPlay() || !S->bEnabled) {if(Juice) Juice->WetnessTransitions.Remove(Material);Material->SetScalarParameterValue(TEXT("Wetness"),Target);return;}
	auto* Existing=Juice->WetnessTransitions.Find(Material);
	const float Current=Existing ? Existing->Current : Material->K2_GetScalarParameterValue(TEXT("Wetness"));
	if(FMath::IsNearlyEqual(Current,Target,.001f)) {Material->SetScalarParameterValue(TEXT("Wetness"),Target);Juice->WetnessTransitions.Remove(Material);return;}
	auto& Blend=Juice->WetnessTransitions.FindOrAdd(Material);Blend.Owner=Owner;Blend.Material=Material;Blend.Current=Current;Blend.Target=Target;
}
bool UBotanicusJuiceWorldSubsystem::IsTickable() const
{
	return Super::IsTickable() && GetWorld() && GetWorld()->GetNetMode()!=NM_DedicatedServer && (!Water.IsEmpty() || !Bursts.IsEmpty() || !WetnessTransitions.IsEmpty());
}
void UBotanicusJuiceWorldSubsystem::ReconcileWater()
{
	UpdateMotionParameters();
	UpdateNoctiflores();
	const auto* S=GetDefault<UBotanicusJuiceSettings>();
	for(auto& Pair:Water) Pair.Value.bDesired=false;
	if(!AudioRelay.IsValid()) for(TActorIterator<ABotanicusAudioRelay> It(GetWorld());It;++It) {AudioRelay=*It;break;}
	if(!S->bEnabled || !S->bWaterDrops || !AudioRelay.IsValid()) return;
	for(const auto& State:AudioRelay->GetWaterStates())
	{
		APawn* Pawn=Cast<APawn>(State.Source);if(!IsValid(Pawn)) continue;
		if(FVector::DistSquared(ListenerPosition(),Pawn->GetActorLocation())>FMath::Square(S->WaterDisplayDistance)) continue;
		if(Pawn->IsLocallyControlled()) if(auto* PC=Cast<ABotanicusPlayerController>(Pawn->GetController());PC && !PC->IsJuiceWaterInputHeld()) continue;
		Water.FindOrAdd(Pawn).bDesired=true;
	}
}
void UBotanicusJuiceWorldSubsystem::UpdateWater(float DeltaTime)
{
	const auto* S=GetDefault<UBotanicusJuiceSettings>();const double Now=GetWorld()->GetTimeSeconds();
	for(auto It=Water.CreateIterator();It;++It)
	{
		auto& Visual=It.Value();APawn* Pawn=Cast<APawn>(It.Key().Get());
		bool bImmediate=!IsValid(Pawn);
		ABotanicusWateringCanActor* Can=nullptr;
		if(auto* Character=Cast<ABotanicusCharacter>(Pawn)) Can=Character->GetHeldWateringCan();
		const bool bLocal=Pawn && Pawn->IsLocallyControlled();
		if(bLocal)
		{
			if(auto* PC=Cast<ABotanicusPlayerController>(Pawn->GetController())) Can=PC->GetJuiceWateringCan();
			if(!IsValid(Can)) {bImmediate=true;Visual.bDesired=false;}
		}
		else if(Pawn && Visual.bDesired && !IsValid(Can) && S->bRemoteHeldCan)
		{
			if(!IsValid(Visual.RemoteCan))
			{
				UClass* Class=S->WateringCanClass.LoadSynchronous();
				if(Class) {Visual.RemoteCan=GetWorld()->SpawnActor<ABotanicusWateringCanActor>(Class);if(Visual.RemoteCan) {Visual.RemoteCan->Tags.Add(TEXT("BotanicusJuiceRemotePreview"));Visual.RemoteCan->Tags.Add(TEXT("BotanicusPlacementPreview"));Visual.RemoteCan->InitializePlacedItem(TEXT("WateringCan"),1);Visual.RemoteCan->ConfigureAsLocalPreview(true);Visual.RemoteCan->ConfigureAsLocalInspection();}}
			}
			Can=Visual.RemoteCan;
			if(Can)
			{
				const FRotator View=Pawn->GetBaseAimRotation();const FRotationMatrix Basis(View);
				const float Forward=FMath::Clamp(70.f+Can->GetPlacementBoxExtent().GetAbs().GetMax()*.35f,75.f,130.f);
				Can->SetActorLocationAndRotation(Pawn->GetPawnViewLocation()+View.Vector()*Forward+Basis.GetUnitAxis(EAxis::Y)*24.f-Basis.GetUnitAxis(EAxis::Z)*24.f,FRotator(0,View.Yaw+180,0));
			}
		}
		if(!S->bEnabled || !S->bWaterDrops) Visual.bDesired=false;
		if(Visual.bDesired && Can && !bImmediate)
		{
			const FVector Nozzle=Can->GetJuiceNozzleLocation();
			const FVector View=Pawn->GetPawnViewLocation();const FVector Aim=Pawn->GetBaseAimRotation().Vector();
			FCollisionQueryParams Query(SCENE_QUERY_STAT(BotanicusJuiceWater),false,Pawn);Query.AddIgnoredActor(Can);
			FHitResult Hit;const bool bHit=GetWorld()->LineTraceSingleByChannel(Hit,View,View+Aim*450.f,ECC_Visibility,Query);
			FVector Target=bHit ? Hit.ImpactPoint : Nozzle+Aim*220.f-FVector(0,0,90);
			if(bHit) {const FVector Surface=SoilSurface(Hit.GetActor(),Target);Target.Z=Surface.Z+1.f;}
			const float Flight=FMath::Clamp(FVector::Distance(Nozzle,Target)/FMath::Max(100.f,S->DropletSpeed),.08f,.55f);
			const FVector Gravity(0,0,-S->Gravity);const FVector Velocity=(Target-Nozzle-.5f*Gravity*Flight*Flight)/Flight;
			if(!Visual.Drops) Visual.Drops=CreateEffect(WaterSystem);
			if(Visual.Drops)
			{
				Visual.Drops->SetWorldLocation(Nozzle);Visual.Drops->SetVariableVec3(TEXT("User.Velocity"),Velocity);Visual.Drops->SetVariableVec3(TEXT("User.Gravity"),Gravity);
				Visual.Drops->SetVariableFloat(TEXT("User.Lifetime"),Flight);Visual.Drops->SetVariableFloat(TEXT("User.Size"),FMath::Clamp(S->DropletSize,.2f,4.f));
				Visual.Drops->SetVariableFloat(TEXT("User.SpawnRate"),FMath::Clamp(S->DropsPerSecond,0.f,160.f));Visual.Drops->SetVariableLinearColor(TEXT("User.Tint"),S->WaterTint);
				if(Visual.StopAt>0) {Visual.Drops->Activate(false);Visual.StopAt=0;}
				if(!Visual.Drops->IsActive()) {Visual.Drops->Activate(true);++WaterStarts;}
			}
			if(bHit && IsValid(Hit.GetActor()) && S->bSplashes && Now>=Visual.NextSplash)
			{
				SpawnBurst(Hit.GetActor(),Target+Hit.ImpactNormal*1.5f,S->WaterTint,FMath::Clamp(S->SplashCount,1,8),S->DropletSize*.8f,S->SplashLifetime,35.f,Can);
				Visual.NextSplash=Now+FMath::Max(.08f,S->SplashInterval);
			}
			Visual.LastHit=Hit.GetActor();
		}
		else
		{
			if(Visual.StopAt==0) {Visual.StopAt=Now+.65;if(Visual.Drops) {Visual.Drops->SetVariableFloat(TEXT("User.SpawnRate"),0);Visual.Drops->Deactivate();}}
			if(bImmediate || Now>=Visual.StopAt)
			{
				if(Visual.Drops) {Visual.Drops->DeactivateImmediate();Visual.Drops->DestroyComponent();}
				if(IsValid(Visual.RemoteCan)) Visual.RemoteCan->Destroy();It.RemoveCurrent();
			}
		}
	}
}
void UBotanicusJuiceWorldSubsystem::Tick(float DeltaTime)
{
	UpdateWater(DeltaTime);const double Now=GetWorld()->GetTimeSeconds();
	for(int32 I=Bursts.Num()-1;I>=0;--I) if(!Bursts[I].Surface.IsValid() || !Bursts[I].Source.IsValid() || Now>=Bursts[I].ExpireAt) {if(Bursts[I].Component) {Bursts[I].Component->DeactivateImmediate();Bursts[I].Component->DestroyComponent();}Bursts.RemoveAtSwap(I);}
	for(auto It=WetnessTransitions.CreateIterator();It;++It)
	{
		auto& Blend=It.Value();if(!Blend.Owner.IsValid() || !Blend.Material.IsValid()) {It.RemoveCurrent();continue;}
		Blend.Current=FMath::FInterpTo(Blend.Current,Blend.Target,DeltaTime,FMath::Max(.1f,GetDefault<UBotanicusJuiceSettings>()->WetnessInterpolationSpeed));
		if(FMath::IsNearlyEqual(Blend.Current,Blend.Target,.001f)) {Blend.Current=Blend.Target;Blend.Material->SetScalarParameterValue(TEXT("Wetness"),Blend.Current);It.RemoveCurrent();}
		else Blend.Material->SetScalarParameterValue(TEXT("Wetness"),Blend.Current);
	}
}
int32 UBotanicusJuiceWorldSubsystem::GetActiveWaterCount() const
{
	int32 Count=0;for(const auto& Pair:Water) if(Pair.Value.bDesired && Pair.Value.Drops && Pair.Value.Drops->IsActive()) ++Count;return Count;
}
int32 UBotanicusJuiceWorldSubsystem::GetParticleCount() const
{
	int32 Count=0;auto Add=[&Count](UNiagaraComponent* Component)
	{
		if(!Component) return;auto Controller=Component->GetSystemInstanceController();if(!Controller.IsValid()) return;
		Controller->WaitForConcurrentTickAndFinalize();
		if(auto* Instance=Controller->GetSystemInstance_Unsafe()) for(const auto& Emitter:Instance->GetEmitters()) Count+=Emitter->GetNumParticles();
	};
	for(const auto& Pair:Water) Add(Pair.Value.Drops);for(const auto& Burst:Bursts) Add(Burst.Component);return Count;
}
void UBotanicusJuiceWorldSubsystem::Deinitialize()
{
	GetWorld()->GetTimerManager().ClearTimer(ReconcileTimer);
	for(auto& Pair:Water) {if(Pair.Value.Drops) {Pair.Value.Drops->DeactivateImmediate();Pair.Value.Drops->DestroyComponent();}if(IsValid(Pair.Value.RemoteCan)) Pair.Value.RemoteCan->Destroy();}
	for(auto& Burst:Bursts) if(Burst.Component) {Burst.Component->DeactivateImmediate();Burst.Component->DestroyComponent();}
	ClearNoctifloreSpores();NoctifloreCandidates.Reset();
	Water.Reset();Bursts.Reset();WetnessTransitions.Reset();Super::Deinitialize();
}

namespace
{
FVector LifecyclePosition(AActor* Pot,UStaticMeshComponent* Foliage)
{
	return IsValid(Foliage) ? Foliage->Bounds.Origin+FVector(0,0,Foliage->Bounds.BoxExtent.Z*.35f) : Pot->GetActorLocation()+FVector(0,0,35);
}
}
void UBotanicusJuiceWorldSubsystem::Growth(AActor* Pot,float Before,float After,UStaticMeshComponent* Foliage)
{
	if(!IsValid(Pot) || !Pot->HasAuthority()) return;
	// Share the established audio thresholds without emitting another sound.
	const FName Event=UBotanicusAudioWorldSubsystem::GrowthEvent(Before,After);
	const uint8 Id=Event==TEXT("Mature") ? 3 : Event==TEXT("GrowthLarge") ? 2 : Event==TEXT("GrowthYoung") ? 1 : 0;
	if(Id) if(auto* Juice=Pot->GetWorld()->GetSubsystem<UBotanicusJuiceWorldSubsystem>())
		if(auto* Events=Juice->GetRelay()) Events->Lifecycle(Pot,Id,LifecyclePosition(Pot,Foliage));
}
void UBotanicusJuiceWorldSubsystem::HarvestSucceeded(AActor* Pot,UStaticMeshComponent* Foliage)
{
	if(IsValid(Pot) && Pot->HasAuthority()) if(auto* Juice=Pot->GetWorld()->GetSubsystem<UBotanicusJuiceWorldSubsystem>())
		if(auto* Events=Juice->GetRelay()) Events->Lifecycle(Pot,4,LifecyclePosition(Pot,Foliage));
}
void UBotanicusJuiceWorldSubsystem::ReceiveLifecycle(uint32 Sequence,AActor* Pot,uint8 Event,const FVector& Position)
{
	if(Sequence==0 || static_cast<int32>(Sequence-LastLifecycleSequence)<=0) return;
	LastLifecycleSequence=Sequence;
	const auto* S=GetDefault<UBotanicusJuiceSettings>();
	if(!S->bEnabled || !S->bLifecycleBursts || S->TransitionIntensity<=0 || !IsValid(Pot) || Event<1 || Event>4 || FVector::DistSquared(ListenerPosition(),Position)>FMath::Square(S->GrowthDisplayDistance)) return;
	const int32 Count=FMath::Clamp(FMath::RoundToInt((Event==1 ? S->YoungCount : Event==2 ? S->DevelopedCount : Event==3 ? S->MatureCount : S->HarvestCount)*S->TransitionIntensity),0,16);
	if(Count==0) return;
	const int32 Previous=Bursts.Num();
	SpawnBurst(Pot,Position,Event==3 ? S->MatureTint : Event==4 ? S->HarvestTint : S->GrowthTint,Count,1.3f*S->TransitionIntensity,Event==3 ? S->MatureLifetime : Event==4 ? S->HarvestLifetime : S->TransitionLifetime,Event==3 ? 32.f : 24.f);
	if(Bursts.Num()>Previous) {++LifecycleBursts;LastLifecycleEvent=Event;}
}
void UBotanicusJuiceWorldSubsystem::UpdateMotionParameters()
{
	if(!MotionCatalog || !MotionCatalog->Parameters) return;
	const auto* S=GetDefault<UBotanicusJuiceSettings>();
	const bool bEnabled=S->bEnabled && S->bOrganicMotion && (!S->bDisableMotionOnLowEffectsQuality || Scalability::GetQualityLevels().EffectsQuality>0);
	const FVector Values(bEnabled ? FMath::Clamp(S->MotionAmplitude,0.f,.75f) : 0.f,FMath::Clamp(S->MotionSpeed,.1f,2.f),FMath::Max(100.f,S->GrowthDisplayDistance));
	if(Values.Equals(CachedMotionParameters)) return;
	if(auto* Instance=GetWorld()->GetParameterCollectionInstance(MotionCatalog->Parameters))
	{
		Instance->SetScalarParameterValue(TEXT("BotanicusMotionAmplitude"),Values.X);
		Instance->SetScalarParameterValue(TEXT("BotanicusMotionSpeed"),Values.Y);
		Instance->SetScalarParameterValue(TEXT("BotanicusMotionDistance"),Values.Z);
		CachedMotionParameters=Values;
	}
}
void UBotanicusJuiceWorldSubsystem::BindFoliage(UStaticMeshComponent* Foliage,bool bAlive,bool bNoctiflore,float Growth)
{
	if(!IsValid(Foliage) || !Foliage->GetWorld() || Foliage->GetWorld()->GetNetMode()==NM_DedicatedServer || !Foliage->GetStaticMesh()) return;
	auto* Juice=Foliage->GetWorld()->GetSubsystem<UBotanicusJuiceWorldSubsystem>();
	if(!Juice || !Juice->MotionCatalog) return;
	if(bNoctiflore && bAlive) Juice->NoctifloreCandidates.FindOrAdd(Foliage);
	else Juice->NoctifloreCandidates.Remove(Foliage);
	const auto* Catalog=bNoctiflore && Juice->NoctifloreCatalog ? Juice->NoctifloreCatalog.Get() : Juice->MotionCatalog.Get();
	for(int32 Index=0;Index<Foliage->GetNumMaterials();++Index)
	{
		UMaterialInterface* Original=Foliage->GetStaticMesh()->GetMaterial(Index);
		const auto* Variant=Catalog->Variants.Find(Original);if(!Variant || !*Variant) continue;
		auto* MID=Cast<UMaterialInstanceDynamic>(Foliage->GetMaterial(Index));
		if(!MID || MID->Parent!=*Variant) {MID=UMaterialInstanceDynamic::Create(*Variant,Foliage);Foliage->SetMaterial(Index,MID);}
		if(bNoctiflore) MID->SetScalarParameterValue(TEXT("NoctGrowthStrength"),FMath::Lerp(.65f,1.f,FMath::Clamp(Growth,0.f,1.f)));
		const float Alive=bAlive ? 1.f : 0.f;
		if(MID->K2_GetScalarParameterValue(TEXT("BotanicusMotionAlive"))!=Alive) MID->SetScalarParameterValue(TEXT("BotanicusMotionAlive"),Alive);
	}
}
