#include "UI/BotanicusPlantContextSubsystem.h"
#include "UI/BotanicusPlantContextSettings.h"
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

namespace
{
const TCHAR* WaterIcon=TEXT("/Game/Botanicus/UI/Plant/Textures/T_PlantUI_Water.T_PlantUI_Water");
const TCHAR* GrowthIcon=TEXT("/Game/Botanicus/UI/Plant/Textures/T_PlantUI_Growth.T_PlantUI_Growth");
AActor* PlantOwner(AActor* Actor)
{
    for(int32 I=0;IsValid(Actor) && I<5;++I)
    {
        if(Actor->IsA<ABotanicusPlantPotActor>() || Actor->IsA<ABotanicusIllegalPlanterActor>()) return Actor;
        Actor=Actor->GetAttachParentActor() ? Actor->GetAttachParentActor() : Actor->GetOwner();
    }
    return nullptr;
}
}

void UBotanicusPlantContextSubsystem::RegisterPlant(AActor* Plant)
{
    if(!IsValid(Plant) || !GetWorld()->IsGameWorld() || GetWorld()->GetNetMode()==NM_DedicatedServer) return;
    if(Entries.ContainsByPredicate([Plant](const FEntry& E){return E.Plant==Plant;})) return;
    FEntry Entry;Entry.Plant=Plant;
    if(auto* Multi=Cast<ABotanicusMultiPlantPotActor>(Plant))
    {
        for(int32 I=0;I<Multi->SlotGrowthWidgets.Num();++I) Entry.Slots.Add({Multi->SlotGrowthWidgets[I].Get(),Multi->EnvironmentDebugWidgets.IsValidIndex(I) ? Multi->EnvironmentDebugWidgets[I].Get() : nullptr});
        if(Multi->EnvironmentAlertWidgets.Num()) Entry.Alert=Multi->EnvironmentAlertWidgets[0];
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
    else return;
    TInlineComponentArray<UWidgetComponent*> Widgets;Plant->GetComponents(Widgets);
    for(auto* Widget:Widgets)
    {
        // Transparent world widgets retain depth testing and support genuine opacity fades.
        Widget->SetBlendMode(EWidgetBlendMode::Transparent);ShowWidget(Widget,0);
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
    AActor* Actor=Target.Plant.Get();if(!Actor || Actor->ActorHasTag(TEXT("BotanicusPlacementPreview"))) return false;
    if(const auto* Multi=Cast<ABotanicusMultiPlantPotActor>(Actor)) return Multi->PlantSlots.IsValidIndex(Target.Slot) && Target.Slot<Multi->GetPlantCapacity() && !Multi->PlantSlots[Target.Slot].PlantKey.IsNone();
    if(const auto* Pot=Cast<ABotanicusPlantPotActor>(Actor)) return Target.Slot==0 && !Pot->PlantKey.IsNone();
    if(const auto* Illegal=Cast<ABotanicusIllegalPlanterActor>(Actor)) return Illegal->PlantSlots.IsValidIndex(Target.Slot) && !Illegal->PlantSlots[Target.Slot].PlantId.IsNone();
    return false;
}

FVector UBotanicusPlantContextSubsystem::PlantPoint(const FTarget& Target) const
{
    AActor* Actor=Target.Plant.Get();if(!Actor) return FVector::ZeroVector;
    FVector Local(0,0,25);
    if(auto* Multi=Cast<ABotanicusMultiPlantPotActor>(Actor)) Local=Multi->GetSlotLocalLocation(Target.Slot);
    else if(auto* Pot=Cast<ABotanicusPlantPotActor>(Actor)) Local.Z=Pot->GetSoilMaximumHeight();
    else if(auto* Illegal=Cast<ABotanicusIllegalPlanterActor>(Actor)) Local=FVector((Target.Slot-(Illegal->PlantSlots.Num()-1)*.5f)*Illegal->GetPlantSlotSpacing(),0,Illegal->SoilBottomHeight+Illegal->SoilVolumeHeight);
    return Actor->GetActorTransform().TransformPosition(Local);
}

void UBotanicusPlantContextSubsystem::ObserveAim(APlayerController& Player,const FHitResult& Hit)
{
    if(!Player.IsLocalPlayerController()) return;
    FTarget Next;AActor* Plant=PlantOwner(Hit.GetActor());
    FVector Eye;FRotator Rotation;Player.GetPlayerViewPoint(Eye,Rotation);
    if(Plant && FVector::DistSquared2D(Player.GetPawn() ? Player.GetPawn()->GetActorLocation() : Eye,Hit.ImpactPoint)<=FMath::Square(FMath::Clamp(GetDefault<UBotanicusPlantContextSettings>()->PanelDistance,25.f,500.f)))
    {
        Next.Plant=Plant;Next.Slot=0;
        // Use the exact slot resolvers already used by planting, watering and harvesting.
        if(auto* Multi=Cast<ABotanicusMultiPlantPotActor>(Plant)) Next.Slot=Multi->ResolveAimedSlot(Player.GetPawn());
        else if(auto* Illegal=Cast<ABotanicusIllegalPlanterActor>(Plant)) Next.Slot=Illegal->ResolveAimedSlot(Player.GetPawn());
        if(!IsPlanted(Next)) Next=FTarget();
    }
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
    ShowWidget(Entry->Slots[Target.Slot].Growth.Get(),Alpha);ShowWidget(Entry->Slots[Target.Slot].Environment.Get(),Alpha);ShowWidget(Entry->Inspection.Get(),Alpha);
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
        if(Active.Plant.IsValid() && (!IsPlanted(Active) || !CanSee(Player,PlantPoint(Active),Active.Plant.Get(),Settings->PanelDistance)))
        {ShowTarget(Active,0);Active=FTarget();Opacity=0;}
    }
    if(Active!=Desired)
    {
        Opacity=Settings->FadeOutSeconds>0 ? FMath::Max(0.f,Opacity-DeltaSeconds/Settings->FadeOutSeconds) : 0;
        ShowTarget(Active,Opacity);
        // Finish the outgoing fade before opening the next target: never two detailed plants.
        if(Opacity<=0) {Active=Desired;ShowTarget(Active,0);}
    }
    else if(Active.Plant.IsValid())
    {
        Opacity=Settings->FadeInSeconds>0 ? FMath::Min(1.f,Opacity+DeltaSeconds/Settings->FadeInSeconds) : 1;
        ShowTarget(Active,Opacity);
    }
    AlertAccumulator+=DeltaSeconds;
    if(AlertAccumulator>=.2f)
    {
        AlertAccumulator=0;Entries.RemoveAll([](const FEntry& E){return !E.Plant.IsValid();});RefreshAlerts(Player);
        if(auto* Pot=Cast<ABotanicusPlantPotActor>(Active.Plant.Get())) if(!Pot->IsA<ABotanicusMultiPlantPotActor>()) Pot->RefreshInspectionWidget();
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
    for(auto& Entry:Entries)
    {
        auto* Alert=Entry.Alert.Get();if(!Alert) continue;
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
            auto* Growth=Entry.Slots[BestSlot].Growth.Get();FVector Position=PlantPoint({Entry.Plant,BestSlot})+FVector(0,0,30);
            if(Growth) Position=Growth->GetComponentLocation()-FVector(0,0,Growth->GetDrawSize().Y*Growth->GetComponentScale().Z*Growth->GetPivot().Y)+FVector(0,0,-8);
            Alert->SetWorldLocation(Position);FVector Eye;FRotator Rotation;Player.GetPlayerViewPoint(Eye,Rotation);Alert->SetWorldRotation((Eye-Position).Rotation());
            if(auto* Widget=Cast<UBotanicusPlantEnvironmentAlertWidget>(Alert->GetWidget())) Widget->SetContextAlert(Best.Texture,Best.Tint);
            ShowWidget(Alert,1);
        }
        else ShowWidget(Alert,0);
    }
}

void UBotanicusPlantContextSubsystem::Deinitialize()
{
    Entries.Reset();Icons.Reset();Super::Deinitialize();
}
