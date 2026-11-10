#include "Sales/BotanicusSalesShelfActor.h"
#include "Sales/BotanicusSalesDisplayActor.h"
#include "Delivery/BotanicusLargeEquipmentActor.h"
#include "StaticMeshResources.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
bool IsSalesShelfWall(const FHitResult& Surface)
{
    auto* Actor=Surface.GetActor();auto* Component=Surface.GetComponent();if(!IsValid(Actor) || !IsValid(Component))return false;
    if(Actor->IsA<ABotanicusPlaceableItemActor>() || Actor->IsA<ABotanicusLargeEquipmentActor>())return false;
    const auto* Mesh=Cast<UStaticMeshComponent>(Component);
    const FString Name=(Actor->GetClass()->GetName()+Actor->GetName()+Component->GetName()+(Mesh && Mesh->GetStaticMesh() ? Mesh->GetStaticMesh()->GetName():TEXT(""))).ToLower();
    const bool bWall=Actor->ActorHasTag(TEXT("BotanicusSalesShelfWall")) || Component->ComponentHasTag(TEXT("BotanicusSalesShelfWall")) || Name.Contains(TEXT("wall")) || Name.Contains(TEXT("mur"));
    return bWall && !Name.Contains(TEXT("door")) && !Name.Contains(TEXT("porte")) && !Name.Contains(TEXT("glass")) && !Name.Contains(TEXT("window")) && !Name.Contains(TEXT("vitre"));
}
}

ABotanicusSalesShelfActor::ABotanicusSalesShelfActor()
{
    bUseBlueprintAppearance=true;
    PrimaryActorTick.bStartWithTickEnabled=false;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if(Cube.Succeeded())Mesh->SetStaticMesh(Cube.Object);
    for(int32 I=0;I<2;++I)
    {
        auto* Part=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Wall Bracket %d"),I));
        Part->SetupAttachment(SceneRoot);if(Cube.Succeeded())Part->SetStaticMesh(Cube.Object);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);Part->SetCanEverAffectNavigation(false);Brackets.Add(Part);
    }
    RefreshLayout();
}
int32 ABotanicusSalesShelfActor::CapacityForKey(FName Key)
{
    const FString Name=Key.ToString();
    if(Name.Len()!=12 || !Name.StartsWith(TEXT("SalesShelfS")))return 0;
    const int32 Count=FCString::Atoi(*Name.Right(1));return Count>=1 && Count<=6 ? Count:0;
}
void ABotanicusSalesShelfActor::OnConstruction(const FTransform& Transform){Super::OnConstruction(Transform);RefreshLayout();}
void ABotanicusSalesShelfActor::ApplyItemDefinition()
{
    Super::ApplyItemDefinition();if(const int32 Count=CapacityForKey(GetItemKey()))Capacity=Count;
    RefreshLayout();if(HasActorBegunPlay())EnsureSlots();
}
void ABotanicusSalesShelfActor::RefreshLayout()
{
    Capacity=FMath::Clamp(Capacity,1,6);
    Mesh->SetRelativeLocation(FVector(22,0,-2));Mesh->SetRelativeScale3D(FVector(.44f,LengthForCapacity(Capacity)/100.f,.04f));
    for(int32 I=0;I<Brackets.Num();++I)
    {Brackets[I]->SetRelativeLocation(FVector(14,(I ? 1:-1)*(LengthForCapacity(Capacity)*.5f-9),-15));Brackets[I]->SetRelativeScale3D(FVector(.28f,.035f,.26f));}
}
void ABotanicusSalesShelfActor::BeginPlay()
{
    Super::BeginPlay();SetActorTickEnabled(false);RefreshLayout();
    if(GetNetMode()!=NM_DedicatedServer)
    {
        // Reuse the existing furniture body material, keeping meshes and materials replaceable in a Blueprint child.
        auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Botanicus/Materials/Floor/M_Floor_Terracotta_WorldWood_Inst.M_Floor_Terracotta_WorldWood_Inst"));
        if(!Base)Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
        if(Base){auto* Tint=UMaterialInstanceDynamic::Create(Base,this);Tint->SetVectorParameterValue(TEXT("Color"),FLinearColor(.62f,.40f,.18f));Mesh->SetMaterial(0,Tint);for(auto B:Brackets)B->SetMaterial(0,Tint);}
    }
    // Previews receive their tag/nonreplicated policy after SpawnActor: do not create commercial children during that window.
    if(HasAuthority())GetWorldTimerManager().SetTimerForNextTick(this,&ABotanicusSalesShelfActor::EnsureSlots);
}
FVector ABotanicusSalesShelfActor::GetSlotLocalLocation(int32 Index) const {return FVector(22,(Index-(Capacity-1)*.5f)*48.f,.25f);}
void ABotanicusSalesShelfActor::EnsureSlots()
{
    if(!HasAuthority() || !GetIsReplicated() || ActorHasTag(TEXT("BotanicusPlacementPreview")) || ActorHasTag(TEXT("BotanicusEquippedPreview")))return;
    if(GetItemKey().IsNone())InitializePlacedItem(FName(*FString::Printf(TEXT("SalesShelfS%d"),Capacity)));
    if(!CapacityForKey(GetItemKey()) || !Slots.IsEmpty())return;
    for(int32 I=0;I<Capacity;++I)
    {
        const FTransform Pose(GetActorQuat(),GetActorTransform().TransformPosition(GetSlotLocalLocation(I)),GetActorScale3D());
        auto* Slot=GetWorld()->SpawnActorDeferred<ABotanicusSalesDisplayActor>(ABotanicusSalesDisplayActor::StaticClass(),Pose,this,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if(!Slot)continue;Slot->InitializeShelfSlot(this,I);Slot->FinishSpawning(Pose);Slot->AttachToActor(this,FAttachmentTransformRules::KeepWorldTransform);Slots.Add(Slot);
    }
    ForceNetUpdate();
}
ABotanicusSalesDisplayActor* ABotanicusSalesShelfActor::GetSlot(int32 Index) const {return Slots.IsValidIndex(Index) && IsValid(Slots[Index]) ? Slots[Index].Get():nullptr;}
ABotanicusSalesDisplayActor* ABotanicusSalesShelfActor::FindSlotAtPoint(const FVector& WorldPoint) const
{
    const FVector Local=GetActorTransform().InverseTransformPosition(WorldPoint);
    if(Local.X<-.5 || Local.X>44.5 || FMath::Abs(Local.Z)>8)return nullptr;
    const int32 Index=FMath::RoundToInt(Local.Y/48.f+(Capacity-1)*.5f);
    return Index>=0 && Index<Capacity && FMath::Abs(Local.Y-GetSlotLocalLocation(Index).Y)<=20 ? GetSlot(Index):nullptr;
}
bool ABotanicusSalesShelfActor::HasContents() const {for(auto S:Slots)if(IsValid(S) && !S->IsEmpty())return true;return false;}
TArray<FBotanicusSavedSalesShelfSlot> ABotanicusSalesShelfActor::CaptureSlots() const
{
    TArray<FBotanicusSavedSalesShelfSlot> Result;
    for(int32 I=0;I<Capacity;++I){auto& Saved=Result.AddDefaulted_GetRef();if(auto* S=GetSlot(I)){Saved.PlantKey=S->GetDisplayedPlantItemKey();Saved.SoilKey=S->GetDisplayedSoilItemKey();Saved.PotKey=S->GetDisplayedPotItemKey();Saved.SaleLotId=S->GetSaleLotId();Saved.PotInstanceId=S->GetDisplayedPotInstanceId();}}
    return Result;
}
void ABotanicusSalesShelfActor::RestoreSlots(const TArray<FBotanicusSavedSalesShelfSlot>& Saved)
{
    if(!HasAuthority())return;EnsureSlots();
    for(int32 I=0;I<FMath::Min(Capacity,Saved.Num());++I)if(auto* S=GetSlot(I)){S->RestoreDisplayedPlant(Saved[I].PlantKey,Saved[I].SoilKey,Saved[I].PotKey);S->RestoreSaleLotId(Saved[I].SaleLotId);S->RestoreDisplayedPotInstanceId(Saved[I].PotInstanceId);}
}
void ABotanicusSalesShelfActor::OnRep_Capacity(){RefreshLayout();}
void ABotanicusSalesShelfActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const {Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(ABotanicusSalesShelfActor,Capacity);DOREPLIFETIME(ABotanicusSalesShelfActor,Slots);}
void ABotanicusSalesShelfActor::EndPlay(const EEndPlayReason::Type Reason)
{GetWorldTimerManager().ClearAllTimersForObject(this);if(HasAuthority())for(auto S:Slots)if(IsValid(S))S->Destroy();Super::EndPlay(Reason);}

bool ABotanicusSalesShelfActor::ResolveWallPlacement(APlayerController* PC,FName Key,FTransform& Out,const AActor* Ignored)
{
    const int32 Count=CapacityForKey(Key);if(!PC || !PC->GetPawn() || !Count)return false;
    UWorld* World=PC->GetWorld();FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(SalesShelfWall),true,PC->GetPawn());if(Ignored)Query.AddIgnoredActor(Ignored);
    FHitResult Hit;Out=FTransform(FRotator(0,View.Yaw,0),Eye+View.Vector()*150);
    if(!World->LineTraceSingleByChannel(Hit,Eye,Eye+View.Vector()*450,ECC_Visibility,Query) || FMath::Abs(Hit.ImpactNormal.Z)>.15f)return false;
    // Require a real wall, never a door, a pane, furniture, an NPC or another shelf.
    if(!IsSalesShelfWall(Hit))return false;
    const FVector Normal=FVector(Hit.ImpactNormal.X,Hit.ImpactNormal.Y,0).GetSafeNormal();const FRotator Rot(0,Normal.Rotation().Yaw,0);
    Out=FTransform(Rot,Hit.ImpactPoint+Normal*2.f);
    FHitResult Floor;if(!World->LineTraceSingleByChannel(Floor,Hit.ImpactPoint+Normal*65+FVector(0,0,5),Hit.ImpactPoint+Normal*65-FVector(0,0,220),ECC_Visibility,Query) || Floor.ImpactNormal.Z<.7f)return false;
    const float Height=Out.GetLocation().Z-Floor.ImpactPoint.Z;if(Height<65 || Height>140)return false;
    // Confirm support across the full width, not just at the central crosshair.
    const FTransform Wall(Rot,Hit.ImpactPoint);const float Half=LengthForCapacity(Count)*.5f;
    const int32 Probes=FMath::CeilToInt((Half*2-4)/24.f);
    for(int32 Probe=0;Probe<=Probes;++Probe)
    {
        const float Y=FMath::Lerp(-Half+2,Half-2,float(Probe)/Probes);
        const FVector Point=Wall.TransformPosition(FVector(0,Y,0));FHitResult Back;
        if(!World->LineTraceSingleByChannel(Back,Point+Normal*5,Point-Normal*5,ECC_Visibility,Query) || !IsSalesShelfWall(Back) || FVector::DotProduct(Back.ImpactNormal,Normal)<.98f || FMath::Abs(FVector::DotProduct(Back.ImpactPoint-Hit.ImpactPoint,Normal))>.5f)return false;
    }
    FCollisionObjectQueryParams Objects;Objects.AddObjectTypesToQuery(ECC_WorldStatic);Objects.AddObjectTypesToQuery(ECC_WorldDynamic);Objects.AddObjectTypesToQuery(ECC_Pawn);
    // Include the pot headroom, but never ignore the supporting wall itself.
    FCollisionQueryParams Overlap(Query);Overlap.bTraceComplex=false;
    if(World->OverlapAnyTestByObjectType(Out.TransformPosition(FVector(22,0,32)),Rot.Quaternion(),Objects,FCollisionShape::MakeBox(FVector(21,Half-.5f,59)),Overlap))return false;
    // Some authored furniture has decorative geometry without a collision body.
    // Check only real furniture while previewing/confirming; no permanent world scan.
    const FBox Clearance(FVector(1,-Half+.5f,-27),FVector(43,Half-.5f,110));
    auto VisuallyBlocked=[&](const AActor* Furniture)
    {
        if(Furniture==Ignored || Furniture->IsHidden() || !Furniture->GetIsReplicated() || (Ignored && Furniture->IsAttachedTo(Ignored)))return false;
        TInlineComponentArray<UStaticMeshComponent*> Parts(Furniture);
        for(auto* Visual:Parts)
        {
            const UStaticMesh* Asset=Visual->GetStaticMesh();
            if(!Asset || !Visual->IsVisible() || Visual->bHiddenInGame || Visual->GetCollisionEnabled()!=ECollisionEnabled::NoCollision)continue;
            const FBox Local=Asset->GetRenderData() ? Asset->GetRenderData()->Bounds.GetBox():Asset->GetBoundingBox();
            if(Local.TransformBy(Visual->GetComponentTransform()*Out.Inverse()).Intersect(Clearance))return true;
        }
        return false;
    };
    for(TActorIterator<ABotanicusLargeEquipmentActor> It(World);It;++It)if(VisuallyBlocked(*It))return false;
    for(TActorIterator<ABotanicusPlaceableItemActor> It(World);It;++It)if(VisuallyBlocked(*It))return false;
    return true;
}
