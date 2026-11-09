#include "UI/BotanicusPlantContextSubsystem.h"
#include "UI/BotanicusPlantContextSettings.h"
#include "UI/BotanicusPlantGrowthWidget.h"
#include "UI/BotanicusPlantInspectionWidget.h"
#include "Sales/BotanicusSalePotActor.h"
#include "IllegalTrade/BotanicusIllegalTradeSettings.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "UI/BotanicusPlantEnvironmentAlertWidget.h"
#include "Growing/BotanicusPlantPotActor.h"
#include "Growing/BotanicusMultiPlantPotActor.h"
#include "Growing/BotanicusPlantSubsystem.h"
#include "IllegalTrade/BotanicusIllegalPlanterActor.h"
#include "Catalog/BotanicusItemCatalogSubsystem.h"
#include "Components/WidgetComponent.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "TimerManager.h"

namespace
{
FBox ContextMeshBounds(UStaticMeshComponent* Component);
const TCHAR* WaterIcon=TEXT("/Game/Botanicus/UI/Plant/Textures/T_PlantUI_Water.T_PlantUI_Water");
const TCHAR* GrowthIcon=TEXT("/Game/Botanicus/UI/Plant/Textures/T_PlantUI_Growth.T_PlantUI_Growth");
AActor* PlantOwner(AActor* Actor)
{
    for(int32 I=0;IsValid(Actor) && I<5;++I)
    {
        if(Actor->IsA<ABotanicusPlantPotActor>() || Actor->IsA<ABotanicusIllegalPlanterActor>() || Actor->IsA<ABotanicusSalePotActor>()) return Actor;
        Actor=Actor->GetAttachParentActor() ? Actor->GetAttachParentActor() : Actor->GetOwner();
    }
    return nullptr;
}
}

void UBotanicusPlantContextSubsystem::RegisterPlant(AActor* Plant)
{
    if(!IsValid(Plant) || !GetWorld()->IsGameWorld() || GetWorld()->GetNetMode()==NM_DedicatedServer || Pending.Contains(Plant) ||
       Entries.ContainsByPredicate([Plant](const FEntry& E){return E.Plant==Plant;}))return;
    Pending.Add(Plant);
    GetWorld()->GetTimerManager().SetTimerForNextTick([Self=TWeakObjectPtr<UBotanicusPlantContextSubsystem>(this),Weak=TWeakObjectPtr<AActor>(Plant)]()
    {if(Self.IsValid()){Self->Pending.Remove(Weak);if(Weak.IsValid())Self->RegisterReadyPlant(Weak.Get());}});
}
void UBotanicusPlantContextSubsystem::RegisterReadyPlant(AActor* Plant)
{
    if(!IsValid(Plant) || !Plant->GetIsReplicated() || Plant->ActorHasTag(TEXT("BotanicusPlacementPreview")) || Plant->ActorHasTag(TEXT("BotanicusEquippedPreview")) || Plant->ActorHasTag(TEXT("BotanicusInspectionPreview")))return;
    if(Entries.ContainsByPredicate([Plant](const FEntry& E){return E.Plant==Plant;})) return;
    FEntry Entry;Entry.Plant=Plant;
    if(auto* Multi=Cast<ABotanicusMultiPlantPotActor>(Plant))
    {
        for(int32 I=0;I<Multi->SlotGrowthWidgets.Num();++I) Entry.Slots.Add({Multi->SlotGrowthWidgets[I].Get(),Multi->EnvironmentDebugWidgets.IsValidIndex(I) ? Multi->EnvironmentDebugWidgets[I].Get() : nullptr});
        if(Multi->EnvironmentAlertWidgets.Num()) Entry.Alert=Multi->EnvironmentAlertWidgets[0];
        Entry.Inspection=Multi->PlantInspectionWidget;
    }
    else if(auto* Pot=Cast<ABotanicusPlantPotActor>(Plant))
    {
        Entry.Slots.Add({Pot->PlantGrowthWidget.Get(),Pot->EnvironmentDebugWidget.Get()});Entry.Inspection=Pot->PlantInspectionWidget;Entry.Alert=Pot->EnvironmentAlertWidget;
    }
    else if(auto* Illegal=Cast<ABotanicusIllegalPlanterActor>(Plant))
    {
        for(UWidgetComponent* Widget:Illegal->PlantGrowthWidgets) Entry.Slots.Add({Widget,nullptr});
        // One lightweight local alert per planter, created once and using the existing alert widget/icons.
        auto* Alert=NewObject<UWidgetComponent>(Plant,TEXT("ContextPlantAlert"));Plant->AddInstanceComponent(Alert);
        Alert->SetupAttachment(Plant->GetRootComponent());Alert->SetWidgetSpace(EWidgetSpace::World);
        Alert->SetDrawSize(FVector2D(240,72));Alert->SetPivot(FVector2D(.5,.5));Alert->SetTwoSided(true);
        Alert->SetCollisionEnabled(ECollisionEnabled::NoCollision);Alert->SetWidgetClass(UBotanicusPlantEnvironmentAlertWidget::StaticClass());
        Alert->SetVisibility(false);Alert->RegisterComponent();Alert->InitWidget();Entry.Alert=Alert;
    }
    else if(auto* Sale=Cast<ABotanicusSalePotActor>(Plant))
    {
        auto* Growth=NewObject<UWidgetComponent>(Plant,TEXT("SalePlantGrowth"));Plant->AddInstanceComponent(Growth);
        Growth->SetupAttachment(Plant->GetRootComponent());Growth->SetWidgetClass(UBotanicusPlantGrowthWidget::StaticClass());
        Growth->SetCollisionEnabled(ECollisionEnabled::NoCollision);Growth->SetVisibility(false);Growth->RegisterComponent();Growth->InitWidget();
        Entry.Slots.Add({Growth,nullptr});
    }
    else return;
    if(!Entry.Inspection.IsValid())
    {
        auto* Panel=NewObject<UWidgetComponent>(Plant,TEXT("ContextPlantPanel"));Plant->AddInstanceComponent(Panel);
        Panel->SetupAttachment(Plant->GetRootComponent());Panel->SetWidgetClass(UBotanicusPlantInspectionWidget::StaticClass());
        Panel->SetCollisionEnabled(ECollisionEnabled::NoCollision);Panel->SetVisibility(false);Panel->RegisterComponent();Entry.Inspection=Panel;
    }
    Entry.Inspection->InitWidget();Entry.Inspection->SetWidgetSpace(EWidgetSpace::World);
    Entry.Inspection->SetDrawSize(FVector2D(420,360));Entry.Inspection->SetPivot(FVector2D(.5,.5));
    for(auto& Slot:Entry.Slots)if(auto* Growth=Slot.Growth.Get())
    {
        Growth->SetWidgetSpace(EWidgetSpace::World);Growth->SetDrawSize(FVector2D(360,128));Growth->SetPivot(FVector2D(.5,1));
    }
    TInlineComponentArray<UWidgetComponent*> Widgets;Plant->GetComponents(Widgets);
    for(auto* Widget:Widgets)
    {
        // Depth-tested world widgets remain local. Tagged components are positioned only by this coordinator.
        Widget->SetBlendMode(EWidgetBlendMode::Transparent);Widget->SetTintColorAndOpacity(FLinearColor::White);Widget->SetTwoSided(true);Widget->SetCollisionEnabled(ECollisionEnabled::NoCollision);ShowWidget(Widget,0);
        Widget->ComponentTags.AddUnique(TEXT("BotanicusPlantContextUI"));
    }
    if(Entry.Alert.IsValid()) Entry.Alert->SetRelativeScale3D(FVector(.14f));
    Entries.Add(MoveTemp(Entry));
}

UBotanicusPlantContextSubsystem::FEntry* UBotanicusPlantContextSubsystem::FindEntry(const FTarget& Target)
{
    return Entries.FindByPredicate([&Target](const FEntry& E){return E.Plant==Target.Plant;});
}

bool UBotanicusPlantContextSubsystem::IsPlanted(const FTarget& Target) const
{
    AActor* Actor=Target.Plant.Get();if(!Actor || !Actor->GetIsReplicated() || Actor->IsHidden() || Actor->ActorHasTag(TEXT("BotanicusPlacementPreview")) || Actor->ActorHasTag(TEXT("BotanicusEquippedPreview")) || Actor->ActorHasTag(TEXT("BotanicusInspectionPreview"))) return false;
    if(const auto* Multi=Cast<ABotanicusMultiPlantPotActor>(Actor)) return Multi->PlantSlots.IsValidIndex(Target.Slot) && Target.Slot<Multi->GetPlantCapacity() && !Multi->PlantSlots[Target.Slot].PlantKey.IsNone();
    if(const auto* Pot=Cast<ABotanicusPlantPotActor>(Actor)) return Target.Slot==0 && !Pot->PlantKey.IsNone();
    if(const auto* Illegal=Cast<ABotanicusIllegalPlanterActor>(Actor)) return Illegal->PlantSlots.IsValidIndex(Target.Slot) && !Illegal->PlantSlots[Target.Slot].PlantId.IsNone();
    if(const auto* Sale=Cast<ABotanicusSalePotActor>(Actor))return Target.Slot==0 && Sale->IsReadyForSale();
    return false;
}

FVector UBotanicusPlantContextSubsystem::PlantPoint(const FTarget& Target) const
{
    AActor* Actor=Target.Plant.Get();if(!Actor) return FVector::ZeroVector;
    FVector Local(0,0,25);
    if(auto* Multi=Cast<ABotanicusMultiPlantPotActor>(Actor)) Local=Multi->GetSlotLocalLocation(Target.Slot);
    else if(auto* Pot=Cast<ABotanicusPlantPotActor>(Actor)) Local.Z=Pot->GetSoilMaximumHeight();
    else if(auto* Sale=Cast<ABotanicusSalePotActor>(Actor)) Local.Z=Sale->GetConfiguredSoilMaximumHeight();
    else if(auto* Illegal=Cast<ABotanicusIllegalPlanterActor>(Actor)) Local=FVector((Target.Slot-(Illegal->PlantSlots.Num()-1)*.5f)*Illegal->GetPlantSlotSpacing(),0,Illegal->SoilBottomHeight+Illegal->SoilVolumeHeight);
    return Actor->GetActorTransform().TransformPosition(Local);
}

void UBotanicusPlantContextSubsystem::ObserveAim(APlayerController& Player,const FHitResult& Hit)
{
    if(!Player.IsLocalPlayerController()) return;
    FTarget Next;FVector Sight=Hit.ImpactPoint;AActor* Plant=PlantOwner(Hit.GetActor());
    FVector Eye;FRotator Rotation;Player.GetPlayerViewPoint(Eye,Rotation);
    if(Plant && FVector::DistSquared2D(Player.GetPawn() ? Player.GetPawn()->GetActorLocation() : Eye,Hit.ImpactPoint)<=FMath::Square(FMath::Clamp(GetDefault<UBotanicusPlantContextSettings>()->PanelDistance,25.f,500.f)))
    {
        Next.Plant=Plant;Next.Slot=0;
        // Use the exact slot resolvers already used by planting, watering and harvesting.
        if(auto* Multi=Cast<ABotanicusMultiPlantPotActor>(Plant)) Next.Slot=Multi->ResolveAimedSlot(Player.GetPawn());
        else if(auto* Illegal=Cast<ABotanicusIllegalPlanterActor>(Plant)) Next.Slot=Illegal->ResolveAimedSlot(Player.GetPawn());
        if(!IsPlanted(Next)) Next=FTarget();
    }
    if(!Next.Plant.IsValid())
    {
        // Presentation-only fallback for a visible single plant's non-colliding foliage.
        // Multi/illegal slots keep their existing interaction resolver unchanged.
        float Best=Hit.bBlockingHit ? FVector::Dist(Eye,Hit.ImpactPoint):1000.f;
        const FVector Delta=Rotation.Vector()*1000.f;
        for(int32 Index:Nearby)
        {
            if(!Entries.IsValidIndex(Index))continue;
            auto* Actor=Entries[Index].Plant.Get();
            if(!Actor || Actor->IsA<ABotanicusMultiPlantPotActor>() || Actor->IsA<ABotanicusIllegalPlanterActor>())continue;
            const FTarget Target{Actor,0};if(!IsPlanted(Target))continue;
            UStaticMeshComponent *Leaf,*Stem,*Container;VisualComponents(Target,Leaf,Stem,Container);
            FBox Bounds=Leaf && Leaf->IsVisible() && Leaf->GetStaticMesh() ? Leaf->Bounds.GetBox():FBox(ForceInit);if(Stem && Stem->IsVisible() && Stem->GetStaticMesh())Bounds+=Stem->Bounds.GetBox();if(!Bounds.IsValid)continue;
            float Near=0,Far=1;bool Intersects=true;
            for(int32 Axis=0;Axis<3;++Axis)
            {
                if(FMath::Abs(Delta[Axis])<KINDA_SMALL_NUMBER){if(Eye[Axis]<Bounds.Min[Axis] || Eye[Axis]>Bounds.Max[Axis])Intersects=false;continue;}
                float A=(Bounds.Min[Axis]-Eye[Axis])/Delta[Axis],B=(Bounds.Max[Axis]-Eye[Axis])/Delta[Axis];if(A>B)Swap(A,B);
                Near=FMath::Max(Near,A);Far=FMath::Min(Far,B);if(Near>Far)Intersects=false;
            }
            const FVector Point=Eye+Delta*Near;
            if(Intersects && Near>=0 && Near*1000<Best && FVector::DistSquared2D(Player.GetPawn() ? Player.GetPawn()->GetActorLocation():Eye,Point)<=FMath::Square(GetDefault<UBotanicusPlantContextSettings>()->PanelDistance))
            {Next=Target;Sight=Point;Best=Near*1000;}
        }
    }
    CandidateSight=Sight;if(Next==Active)ActiveSight=Sight;
    if(Next!=Candidate) {Candidate=Next;CandidateSince=GetWorld()->GetTimeSeconds();}
}

bool UBotanicusPlantContextSubsystem::CanSee(APlayerController& Player,const FVector& Point,AActor* Plant,float Distance) const
{
    FVector Eye;FRotator Rotation;Player.GetPlayerViewPoint(Eye,Rotation);
    if(FVector::DistSquared2D(Player.GetPawn() ? Player.GetPawn()->GetActorLocation() : Eye,Point)>FMath::Square(Distance) || FVector::DotProduct(Rotation.Vector(),Point-Eye)<=0) return false;
    FVector2D Screen;int32 Width,Height;Player.GetViewportSize(Width,Height);
    if(!Player.ProjectWorldLocationToScreen(Point,Screen,true) || Screen.X<0 || Screen.Y<0 || Screen.X>Width || Screen.Y>Height) return false;
    FHitResult Hit;FCollisionQueryParams Query(SCENE_QUERY_STAT(BotanicusPlantContextVisibility),false,Player.GetPawn());
    return !GetWorld()->LineTraceSingleByChannel(Hit,Eye,Point,ECC_Visibility,Query) || PlantOwner(Hit.GetActor())==Plant;
}

void UBotanicusPlantContextSubsystem::ShowWidget(UWidgetComponent* Component,float Alpha)
{
    if(!Component) return;
    const bool bShow=Alpha>0;
    if(Component->IsVisible()!=bShow) Component->SetVisibility(bShow);
    if(auto* Widget=Component->GetWidget()) if(!FMath::IsNearlyEqual(Widget->GetRenderOpacity(),Alpha)) Widget->SetRenderOpacity(Alpha);
}

void UBotanicusPlantContextSubsystem::ShowTarget(const FTarget& Target,float Alpha)
{
    auto* Entry=FindEntry(Target);if(!Entry || !Entry->Slots.IsValidIndex(Target.Slot)) return;
    ShowWidget(Entry->Slots[Target.Slot].Growth.Get(),Alpha);
    // The detailed climate display reuses the alert glyphs, not the numerical debug panel.
    ShowWidget(Entry->Slots[Target.Slot].Environment.Get(),0);ShowWidget(Entry->Inspection.Get(),Alpha);
    if(auto* Alert=Entry->Alert.Get())
    {
        if(Alpha>0)
        {
            Alert->SetWidgetSpace(EWidgetSpace::World);Alert->SetDrawSize(FVector2D(72,240));Alert->SetPivot(FVector2D(.5,.5));
            const auto* IconsWidget=Cast<UBotanicusPlantEnvironmentAlertWidget>(Alert->GetWidget());
            ShowWidget(Alert,IconsWidget && IconsWidget->HasAnyAlert() ? Alpha:0);
        }
        else {ShowWidget(Alert,0);Alert->SetWidgetSpace(EWidgetSpace::World);Alert->SetDrawSize(FVector2D(240,72));if(auto* W=Alert->GetWidget()){W->SetRenderScale(FVector2D(1));W->SetRenderTranslation(FVector2D::ZeroVector);}}
    }
    if(auto* Pot=Cast<ABotanicusPlantPotActor>(Target.Plant.Get())) Pot->bInspectionVisible=Alpha>0;
}

void UBotanicusPlantContextSubsystem::Update(APlayerController& Player,float DeltaSeconds)
{
    if(!Player.IsLocalPlayerController()) return;
    const auto* Settings=GetDefault<UBotanicusPlantContextSettings>();
    FTarget Desired=Candidate;
    if(GetWorld()->GetTimeSeconds()-CandidateSince<Settings->TargetHoldSeconds) Desired=Active;
    if(!Player.GetPawn() || !IsPlanted(Desired)) Desired=FTarget();
    SafetyAccumulator+=DeltaSeconds;
    if(SafetyAccumulator>=.05f)
    {
        SafetyAccumulator=0;
        if(Active.Plant.IsValid() && (!IsPlanted(Active) || !CanSee(Player,ActiveSight,Active.Plant.Get(),Settings->PanelDistance)))
        {ShowTarget(Active,0);Active=FTarget();Opacity=0;}
    }
    if(Active!=Desired)
    {
        Opacity=Settings->FadeOutSeconds>0 ? FMath::Max(0.f,Opacity-DeltaSeconds/Settings->FadeOutSeconds) : 0;
        ShowTarget(Active,Opacity);
        // Finish the outgoing fade before opening the next target: never two detailed plants.
        if(Opacity<=0) {Active=Desired;ActiveSight=CandidateSight;RefreshReadout(Active);if(auto* E=FindEntry(Active))E->bLayoutValid=false;ShowTarget(Active,0);}
    }
    else if(Active.Plant.IsValid())
    {
        Opacity=Settings->FadeInSeconds>0 ? FMath::Min(1.f,Opacity+DeltaSeconds/Settings->FadeInSeconds) : 1;
        ShowTarget(Active,Opacity);
    }
    // Camera layout runs after CameraManager, avoiding a one-frame projection lag.
    AlertAccumulator+=DeltaSeconds;
    if(AlertAccumulator>=.2f)
    {
        AlertAccumulator=0;Entries.RemoveAll([](const FEntry& E){return !E.Plant.IsValid();});RefreshAlerts(Player);
        RefreshReadout(Active);
    }
}

UTexture2D* UBotanicusPlantContextSubsystem::Icon(const TCHAR* Path)
{
    const FName Key(Path);if(const auto* Existing=Icons.Find(Key)) return *Existing;
    auto* Texture=LoadObject<UTexture2D>(nullptr,Path);Icons.Add(Key,Texture);return Texture;
}

UBotanicusPlantContextSubsystem::FAlert UBotanicusPlantContextSubsystem::PlantAlert(const FBotanicusPlantDefinition* Definition,float Water,bool bDead,const FBotanicusPlantDiseaseState& Disease,const FBotanicusPlantEnvironmentState& Environment)
{
    // Priority is presentation only. The conditions/thresholds all come from existing plant state.
    if(bDead) return {400,Icon(GrowthIcon),FLinearColor(.5f,.5f,.5f)};
    if(!Disease.ActiveDiseaseKeys.IsEmpty())
    {
        UTexture2D* Texture=nullptr;
        if(const auto* D=FindBotanicusPlantDisease(Disease.ActiveDiseaseKeys[0]))
            if(const auto* Item=GetWorld()->GetGameInstance()->GetSubsystem<UBotanicusItemCatalogSubsystem>()->FindItem(D->TreatmentItemKey)) Texture=Item->Icon.LoadSynchronous();
        return {300,Texture ? Texture : Icon(GrowthIcon),FLinearColor(1,.7f,.25f)};
    }
    if(Definition && (Water<Definition->MinimumHealthyWater || Water>Definition->MaximumHealthyWater)) return {200+FMath::Abs(Water-.5f),Icon(WaterIcon),Water>Definition->MaximumHealthyWater ? FLinearColor(1,.65f,.25f) : FLinearColor::White};
    FAlert Best;
    if(Environment.bEnvironmentAvailable)
    {
        auto Check=[this,&Best](EBotanicusPlantEnvironmentCondition Condition,float Comfort,const TCHAR* Low,const TCHAR* High)
        {
            if(Condition!=EBotanicusPlantEnvironmentCondition::TooLow && Condition!=EBotanicusPlantEnvironmentCondition::TooHigh) return;
            const float Priority=100+1-Comfort;if(Priority>Best.Priority) Best={Priority,Icon(Condition==EBotanicusPlantEnvironmentCondition::TooLow ? High : Low),FLinearColor::White};
        };
        Check(Environment.TemperatureCondition,Environment.TemperatureComfort,TEXT("/Game/Botanicus/UI/Plant/Environment/T_PlantEnvironment_TemperatureLow.T_PlantEnvironment_TemperatureLow"),TEXT("/Game/Botanicus/UI/Plant/Environment/T_PlantEnvironment_TemperatureHigh.T_PlantEnvironment_TemperatureHigh"));
        Check(Environment.AirHumidityCondition,Environment.AirHumidityComfort,TEXT("/Game/Botanicus/UI/Plant/Environment/T_PlantEnvironment_HumidityLow.T_PlantEnvironment_HumidityLow"),TEXT("/Game/Botanicus/UI/Plant/Environment/T_PlantEnvironment_HumidityHigh.T_PlantEnvironment_HumidityHigh"));
        Check(Environment.LuminosityCondition,Environment.LuminosityComfort,TEXT("/Game/Botanicus/UI/Plant/Environment/T_PlantEnvironment_LuminosityLow.T_PlantEnvironment_LuminosityLow"),TEXT("/Game/Botanicus/UI/Plant/Environment/T_PlantEnvironment_LuminosityHigh.T_PlantEnvironment_LuminosityHigh"));
    }
    return Best;
}

void UBotanicusPlantContextSubsystem::RefreshAlerts(APlayerController& Player)
{
    const auto* Settings=GetDefault<UBotanicusPlantContextSettings>();auto* Plants=GetWorld()->GetGameInstance()->GetSubsystem<UBotanicusPlantSubsystem>();
    Nearby.Reset();
    for(int32 EntryIndex=0;EntryIndex<Entries.Num();++EntryIndex)
    {
        auto& Entry=Entries[EntryIndex];
        if(Entry.Plant.IsValid() && Player.GetPawn() && FVector::DistSquared2D(Player.GetPawn()->GetActorLocation(),Entry.Plant->GetActorLocation())<=FMath::Square(Settings->PanelDistance+100))Nearby.Add(EntryIndex);
        auto* Alert=Entry.Alert.Get();if(!Alert) continue;
        if(Entry.Plant==Active.Plant && Opacity>0)continue;
        FAlert Best;int32 BestSlot=INDEX_NONE;
        if(Settings->bAlertsEnabled && Player.GetPawn())
        {
            for(int32 I=0;I<Entry.Slots.Num();++I)
            {
                const FTarget Target{Entry.Plant,I};if(!IsPlanted(Target) || !CanSee(Player,PlantPoint(Target),Entry.Plant.Get(),Settings->AlertDistance)) continue;
                FAlert Current;
                if(auto* Multi=Cast<ABotanicusMultiPlantPotActor>(Entry.Plant.Get())) {const auto& S=Multi->PlantSlots[I];Current=PlantAlert(Plants->FindPlant(S.PlantKey),S.WaterLevel,S.bElementalDead,S.DiseaseState,S.EnvironmentState);}
                else if(auto* Pot=Cast<ABotanicusPlantPotActor>(Entry.Plant.Get())) Current=PlantAlert(Plants->FindPlant(Pot->PlantKey),Pot->WaterLevel,Pot->bElementalDead,Pot->DiseaseState,Pot->EnvironmentState);
                else if(auto* Illegal=Cast<ABotanicusIllegalPlanterActor>(Entry.Plant.Get())) {const auto& S=Illegal->PlantSlots[I];if(S.GrowthProgress<1 && S.WaterAmount<=KINDA_SMALL_NUMBER) Current={200,Icon(WaterIcon),FLinearColor::White};}
                if(Current.Priority>Best.Priority) {Best=Current;BestSlot=I;}
            }
        }
        if(BestSlot!=INDEX_NONE && Best.Texture)
        {
            UStaticMeshComponent *Leaf,*Stem,*Container;VisualComponents({Entry.Plant,BestSlot},Leaf,Stem,Container);
            FBox Bounds=ContextMeshBounds(Leaf);const FBox StemBounds=ContextMeshBounds(Stem);if(StemBounds.IsValid)Bounds+=StemBounds;
            FVector Position=Bounds.IsValid ? Bounds.GetCenter():PlantPoint({Entry.Plant,BestSlot});Position.Z=Bounds.IsValid ? Bounds.Max.Z+15:Position.Z+30;
            Alert->SetWorldScale3D(FVector(.14f));Alert->SetWorldLocation(Position);FVector Eye;FRotator Rotation;Player.GetPlayerViewPoint(Eye,Rotation);Alert->SetWorldRotation((Eye-Position).Rotation());
            if(auto* Widget=Cast<UBotanicusPlantEnvironmentAlertWidget>(Alert->GetWidget())) Widget->SetContextAlert(Best.Texture,Best.Tint);
            ShowWidget(Alert,1);
        }
        else ShowWidget(Alert,0);
    }
}

void UBotanicusPlantContextSubsystem::Deinitialize()
{
    Entries.Reset();Pending.Reset();Nearby.Reset();Icons.Reset();Super::Deinitialize();
}

namespace
{
FText ContextQualityLabel(FName Tag)
{
    return FText::FromString(Tag==TEXT("Exceptional") ? TEXT("EXCEPTIONNELLE"):Tag==TEXT("Beautiful") ? TEXT("BELLE"):TEXT("STANDARD"));
}
FBox ContextMeshBounds(UStaticMeshComponent* Component)
{
    if(!Component || !Component->IsVisible() || Component->bHiddenInGame || !Component->GetStaticMesh())return FBox(ForceInit);
    const UStaticMesh* Mesh=Component->GetStaticMesh();const auto* RenderData=Mesh->GetRenderData();
    // Render bounds exclude optional culling extensions. Never include UI, effects or hidden meshes.
    const FBox Bounds=RenderData ? RenderData->Bounds.GetBox():Mesh->GetBoundingBox();
    return Bounds.TransformBy(Component->GetComponentTransform());
}
}
UBotanicusPlantContextSubsystem::FReadout UBotanicusPlantContextSubsystem::Readout(const FTarget& Target) const
{
    FReadout D;auto* Actor=Target.Plant.Get();if(!Actor || !GetWorld()->GetGameInstance())return D;
    auto* Plants=GetWorld()->GetGameInstance()->GetSubsystem<UBotanicusPlantSubsystem>();
    auto* Items=GetWorld()->GetGameInstance()->GetSubsystem<UBotanicusItemCatalogSubsystem>();
    FName Product;
    if(auto* Multi=Cast<ABotanicusMultiPlantPotActor>(Actor))
    {
        if(!Multi->PlantSlots.IsValidIndex(Target.Slot))return D;
        const auto& S=Multi->PlantSlots[Target.Slot];D.Definition=Plants->FindPlant(S.PlantKey);D.Water=S.WaterLevel;D.Growth=S.GrowthProgress;D.Environment=S.EnvironmentState;
        if(D.Definition)Product=Multi->GetHarvestItemKey(Target.Slot,*D.Definition);
    }
    else if(auto* Pot=Cast<ABotanicusPlantPotActor>(Actor))
    {
        D.Definition=Pot->GetPlantDefinition();D.Water=Pot->WaterLevel;D.Growth=Pot->GrowthProgress;D.Environment=Pot->EnvironmentState;
        D.Age=FMath::Max(0.f,float(GetWorld()->GetTimeSeconds()-Pot->PlantingStartServerTime));
        D.Quality=FText::FromString(Pot->GetPlantQualityLabel());Product=Pot->GetCurrentHarvestItemKey();
    }
    else if(auto* Sale=Cast<ABotanicusSalePotActor>(Actor))
    {
        Product=Sale->PlantItemKey;D.Definition=Plants->FindPlantByHarvestItem(Product);D.Growth=1;
    }
    else if(auto* Illegal=Cast<ABotanicusIllegalPlanterActor>(Actor))
    {
        if(!Illegal->PlantSlots.IsValidIndex(Target.Slot))return D;
        const auto& S=Illegal->PlantSlots[Target.Slot];const auto* Settings=GetDefault<UBotanicusIllegalTradeSettings>();
        D.Growth=S.GrowthProgress;D.Water=FMath::Clamp(S.WaterAmount/FMath::Max(.1f,Settings->GetPlanterLevel(Illegal->PlanterLevel).MaximumWater),0.f,1.f);
        if(const auto* Def=Settings->FindPlant(S.PlantId)){D.Name=Def->DisplayName;D.Price=Def->UnitSaleValue;}
    }
    if(D.Definition)D.Name=D.Definition->DisplayName;
    if(const auto* Item=Items->FindItem(Product))
    {
        D.Price=Item->SalePrice;
        if(Actor->IsA<ABotanicusSalePotActor>() || Actor->IsA<ABotanicusMultiPlantPotActor>())D.Quality=ContextQualityLabel(Item->PlantQualityTag);
    }
    return D;
}
void UBotanicusPlantContextSubsystem::RefreshReadout(const FTarget& Target)
{
    auto* E=FindEntry(Target);if(!E || !E->Slots.IsValidIndex(Target.Slot))return;
    const FReadout D=Readout(Target);
    if(auto* C=E->Slots[Target.Slot].Growth.Get())
        if(auto* Widget=Cast<UBotanicusPlantGrowthWidget>(C->GetWidget()))Widget->SetPlantState(D.Name,D.Water,D.Growth);
    if(auto* C=E->Inspection.Get())if(auto* Widget=Cast<UBotanicusPlantInspectionWidget>(C->GetWidget()))
    {
        const TCHAR* Element=TEXT("Normal");
        if(D.Definition)switch(D.Definition->Element)
        {
            case EBotanicusPlantElement::Fire:Element=TEXT("Fire");break;
            case EBotanicusPlantElement::Water:Element=TEXT("Water");break;
            case EBotanicusPlantElement::Ice:Element=TEXT("Ice");break;
            case EBotanicusPlantElement::Shadow:Element=TEXT("Shadow");break;
            default:break;
        }
        const FString Path=FString::Printf(TEXT("/Game/Botanicus/UI/Plant/Inspection/Textures/T_PlantElement_%s.T_PlantElement_%s"),Element,Element);
        Widget->SetPlantName(D.Name);Widget->SetInspectionData(D.Quality,D.Price,FText::GetEmpty(),Icon(*Path),0,D.Age);
    }
    if(auto* C=E->Alert.Get())
        if(auto* Widget=Cast<UBotanicusPlantEnvironmentAlertWidget>(C->GetWidget()))Widget->SetColumnState(D.Environment);
}
void UBotanicusPlantContextSubsystem::VisualComponents(const FTarget& Target,UStaticMeshComponent*& Plant,UStaticMeshComponent*& Stem,UStaticMeshComponent*& Pot) const
{
    Plant=Stem=Pot=nullptr;
    if(auto* Multi=Cast<ABotanicusMultiPlantPotActor>(Target.Plant.Get()))
    {
        if(Multi->MultiFlowerMeshes.IsValidIndex(Target.Slot))Plant=Multi->MultiFlowerMeshes[Target.Slot];
        if(Multi->MultiStemMeshes.IsValidIndex(Target.Slot))Stem=Multi->MultiStemMeshes[Target.Slot];Pot=Multi->Mesh;
    }
    else if(auto* P=Cast<ABotanicusPlantPotActor>(Target.Plant.Get())){Plant=P->FoliageMesh;Stem=P->StemMesh;Pot=P->Mesh;}
    else if(auto* SaleVisual=Cast<ABotanicusSalePotActor>(Target.Plant.Get())){Plant=SaleVisual->PlantVisual;Stem=SaleVisual->StemVisual;Pot=SaleVisual->Mesh;}
    else if(auto* IllegalVisual=Cast<ABotanicusIllegalPlanterActor>(Target.Plant.Get()))
    {if(IllegalVisual->PlantVisuals.IsValidIndex(Target.Slot))Plant=IllegalVisual->PlantVisuals[Target.Slot];Pot=IllegalVisual->Mesh;}
}
void UBotanicusPlantContextSubsystem::UpdateCameraLayout(APlayerController& Player)
{
    if(Player.IsLocalPlayerController() && Active.Plant.IsValid() && Opacity>0)UpdateLayout(Player,Active);
}

void UBotanicusPlantContextSubsystem::UpdateLayout(APlayerController& Player,const FTarget& Target)
{
    auto* Entry=FindEntry(Target);if(!Entry || !Entry->Slots.IsValidIndex(Target.Slot))return;
    UStaticMeshComponent *Leaf,*Stem,*Pot;VisualComponents(Target,Leaf,Stem,Pot);
    const auto* PanelWidget=Entry->Inspection.IsValid() ? Cast<UBotanicusPlantInspectionWidget>(Entry->Inspection->GetWidget()) : nullptr;
    const FVector2D PanelSize=PanelWidget ? PanelWidget->GetContextPanelSize():FVector2D(420,360);
    const auto* Settings=GetDefault<UBotanicusPlantContextSettings>();
    const FVector SideOffset=Settings->SideIconsLocalOffset;
    const float ClimateScale=FMath::Clamp(Settings->SideIconsScale,.05f,.30f);
    const FVector2D PanelOffsets(FMath::Clamp(Settings->InfoPanelVerticalOffset,-30.f,60.f),FMath::Clamp(Settings->InfoPanelPitchOffset,-30.f,30.f));
    const bool PresentationChanged=!Entry->CachedSideIconsLocalOffset.Equals(SideOffset,.001f) ||
        !FMath::IsNearlyEqual(Entry->CachedSideIconsScale,ClimateScale,.001f) || !Entry->CachedPanelOffsets.Equals(PanelOffsets,.001f);
    bool GeometryChanged=!Entry->bLayoutValid || PresentationChanged || Entry->LayoutSlot!=Target.Slot || !Entry->PanelSize.Equals(PanelSize,.01f);
    UStaticMeshComponent* Components[3]={Leaf,Stem,Pot};
    for(int32 I=0;I<3;++I)
    {
        auto* C=Components[I];const FTransform Transform=C ? C->GetComponentTransform():FTransform::Identity;
        UStaticMesh* Asset=C ? C->GetStaticMesh():nullptr;const bool Visible=C && C->IsVisible() && !C->bHiddenInGame;
        GeometryChanged|=Entry->CachedMeshes[I]!=Asset || !Entry->CachedTransforms[I].Equals(Transform,.001f) || Entry->CachedVisible[I]!=Visible;
        Entry->CachedMeshes[I]=Asset;Entry->CachedTransforms[I]=Transform;Entry->CachedVisible[I]=Visible;
    }
    if(!Entry->Root.IsValid())
    {
        auto* Root=NewObject<USceneComponent>(Target.Plant.Get(),TEXT("PlantContextRoot"));Target.Plant->AddInstanceComponent(Root);
        Root->SetupAttachment(Target.Plant->GetRootComponent());Root->SetAbsolute(false,true,true);Root->RegisterComponent();
        Root->SetWorldScale3D(FVector::OneVector);Entry->Root=Root;
        auto* Anchor=NewObject<USceneComponent>(Target.Plant.Get(),TEXT("PlantInfoTopAnchor"));Target.Plant->AddInstanceComponent(Anchor);
        Anchor->SetupAttachment(Root);Anchor->RegisterComponent();Entry->TopAnchor=Anchor;
        GeometryChanged=true;
    }
    if(GeometryChanged)
    {
        Entry->PlantBounds=ContextMeshBounds(Leaf);const FBox StemBounds=ContextMeshBounds(Stem);
        if(StemBounds.IsValid)Entry->PlantBounds+=StemBounds;
        if(!Entry->PlantBounds.IsValid)Entry->PlantBounds=FBox(PlantPoint(Target),PlantPoint(Target));
        Entry->PotBounds=ContextMeshBounds(Pot);
        if(!Entry->PotBounds.IsValid)Entry->PotBounds=FBox(PlantPoint(Target)-FVector(10),PlantPoint(Target)+FVector(10));
        const FVector Centre=Entry->PlantBounds.GetCenter();
        const FVector Origin(Centre.X,Centre.Y,Entry->PotBounds.Min.Z);
        Entry->Root->SetWorldLocation(Origin);Entry->TopAnchor->SetRelativeLocation(FVector(0,0,Entry->PlantBounds.Max.Z+10-Origin.Z));
        constexpr float GrowthScale=.16f,PanelScale=.12f;
        const FVector Extent=Entry->PotBounds.GetExtent();
        const bool Rectangular=Target.Plant->IsA<ABotanicusMultiPlantPotActor>() || Target.Plant->IsA<ABotanicusIllegalPlanterActor>();
        const float FrontRadius=Rectangular ? Extent.Size2D():FMath::Max(Extent.X,Extent.Y);
        const float PlantRadius=Entry->PlantBounds.GetExtent().Size2D();
        const float ColumnRight=FMath::Max3(PlantRadius+10,360*GrowthScale*.5f+8,float(PanelSize.X)*PanelScale*.5f+8);
        const float ColumnBottomZ=FMath::Max(Entry->PlantBounds.Max.Z+10-Origin.Z-3-240*.12f,Entry->PotBounds.GetSize().Z+2);
        const float ColumnCentreZ=ColumnBottomZ+240*ClimateScale*.5f;
        // Keep the tilted panel entirely in front of the pot; scaling icons grows the column upward.
        const float PanelTiltClearance=FMath::Abs(FMath::Sin(FMath::DegreesToRadians(float(PanelOffsets.Y))))*float(PanelSize.Y)*PanelScale*.5f;
        auto Place=[](UWidgetComponent* Widget,USceneComponent* Parent,const FVector& Offset,const FVector2D& Size,const FVector2D& Pivot,float Scale,const FRotator& Rotation=FRotator::ZeroRotator)
        {
            if(!Widget)return;
            if(Widget->GetAttachParent()!=Parent)Widget->AttachToComponent(Parent,FAttachmentTransformRules::KeepWorldTransform);
            Widget->SetWidgetSpace(EWidgetSpace::World);Widget->SetDrawAtDesiredSize(false);Widget->SetDrawSize(Size);Widget->SetPivot(Pivot);
            Widget->SetRelativeLocationAndRotation(Offset,Rotation);Widget->SetRelativeScale3D(FVector(Scale));
            if(auto* User=Widget->GetWidget()){User->SetRenderTransform(FWidgetTransform());User->SetRenderTransformPivot(Pivot);}
        };
        Place(Entry->Slots[Target.Slot].Growth.Get(),Entry->TopAnchor.Get(),FVector::ZeroVector,FVector2D(360,128),FVector2D(.5,1),GrowthScale);
        Place(Entry->Inspection.Get(),Entry->Root.Get(),FVector(FrontRadius+2+PanelTiltClearance,0,Extent.Z+PanelOffsets.X),PanelSize,FVector2D(.5,.5),PanelScale,FRotator(PanelOffsets.Y,0,0));
        // Widget faces +X, so -Y is the player's right. Every climate cell has a fixed slot.
        Place(Entry->Alert.Get(),Entry->Root.Get(),FVector(2,-ColumnRight,ColumnCentreZ)+SideOffset,FVector2D(72,240),FVector2D(.5,.5),ClimateScale);
        Entry->CachedSideIconsLocalOffset=SideOffset;Entry->CachedSideIconsScale=ClimateScale;Entry->CachedPanelOffsets=PanelOffsets;
        Entry->PanelSize=PanelSize;Entry->LayoutSlot=Target.Slot;Entry->bLayoutValid=true;
    }
    FVector Eye;FRotator View;Player.GetPlayerViewPoint(Eye,View);
    FVector Facing=Eye-Entry->Root->GetComponentLocation();Facing.Z=0;
    if(!Facing.IsNearlyZero())
    {
        const FRotator YawOnly(0,Facing.Rotation().Yaw,0);
        if(!Entry->Root->GetComponentRotation().Equals(YawOnly,.05f))Entry->Root->SetWorldRotation(YawOnly);
    }
}
