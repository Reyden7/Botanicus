#include "Sales/BotanicusSalesShelfActor.h"
#include "Sales/BotanicusSalesDisplayActor.h"
#include "Delivery/BotanicusLargeEquipmentActor.h"
#include "StaticMeshResources.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "HAL/IConsoleManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
TAutoConsoleVariable<int32> SalesShelfPlacementDebug(TEXT("botanicus.SalesShelfPlacementDebug"),0,TEXT("Log wall placement hit, final location and exact refusal reason."));
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

bool ABotanicusSalesShelfActor::ResolveWallPlacement(APlayerController* PC,FName Key,FTransform& Out,const AActor* Ignored,const FTransform* RequestedPose)
{
    const int32 Count=CapacityForKey(Key);if(!PC || !PC->GetPawn() || !Count)return false;
    UWorld* World=PC->GetWorld();FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(SalesShelfWall),true,PC->GetPawn());if(Ignored)Query.AddIgnoredActor(Ignored);
    // Exclude cosmetic representations even if a Blueprint gave one an unexpected collision preset.
    for(TActorIterator<ABotanicusPlaceableItemActor> It(World);It;++It)
        if(It->ActorHasTag(TEXT("BotanicusPlacementPreview")) || It->ActorHasTag(TEXT("BotanicusEquippedPreview")) || It->ActorHasTag(TEXT("BotanicusInspectionPreview")))Query.AddIgnoredActor(*It);
    FHitResult Hit;Out=RequestedPose ? *RequestedPose:FTransform(FRotator(0,View.Yaw,0),Eye+View.Vector()*450);
    FVector Desired=Out.GetLocation();
    auto Result=[&](bool Valid,const TCHAR* Reason)
    {
        if(SalesShelfPlacementDebug.GetValueOnGameThread())UE_LOG(LogTemp,Display,TEXT("ShelfPlacement PC=%s Key=%s Mode=%s RawHitLocation=%s RawHitNormal=%s HitActor=%s HitComponent=%s Eye=%s View=%s DesiredShelfLocation=%s FinalShelfLocation=%s PlacementValid=%d Reason=%s"),*GetNameSafe(PC),*Key.ToString(),RequestedPose ? TEXT("ValidateRequested"):TEXT("CameraPreview"),*Hit.ImpactPoint.ToString(),*Hit.ImpactNormal.ToString(),*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()),*Eye.ToString(),*View.ToString(),*Desired.ToString(),*Out.GetLocation().ToString(),Valid,Reason);
        return Valid;
    };
    if(RequestedPose)
    {
        // Validate the clicked pose, not a later camera orientation received with the RPC.
        if(RequestedPose->ContainsNaN() || FVector::DistSquared(Eye,RequestedPose->GetLocation())>FMath::Square(450.f))return Result(false,TEXT("RequestedPoseOutOfRange"));
        const FRotator Rotation=RequestedPose->Rotator();
        if(FMath::Abs(Rotation.Pitch)>.01f || FMath::Abs(Rotation.Roll)>.01f)return Result(false,TEXT("RequestedPoseTilted"));
        const FVector Facing=Rotation.Vector();const FVector At=RequestedPose->GetLocation();
        if(!World->LineTraceSingleByChannel(Hit,At+Facing*8,At-Facing*8,ECC_Visibility,Query))return Result(false,TEXT("NoWallAtRequestedPose"));
    }
    else
    {
        if(!World->LineTraceSingleByChannel(Hit,Eye,Eye+View.Vector()*450,ECC_Visibility,Query))return Result(false,TEXT("NoHit"));
        // Even an incompatible hit keeps a red preview at the actual aimed position.
        Out.SetLocation(Hit.ImpactPoint);Desired=Hit.ImpactPoint;
    }
    if(FMath::Abs(Hit.ImpactNormal.Z)>.15f)return Result(false,TEXT("NonVerticalSurface"));
    if(!IsSalesShelfWall(Hit))return Result(false,TEXT("IncompatibleWall"));
    const FVector Normal=FVector(Hit.ImpactNormal.X,Hit.ImpactNormal.Y,0).GetSafeNormal();
    FRotator Rot(0,Normal.Rotation().Yaw,0);
    if(RequestedPose)
    {
        if(FMath::Abs(FMath::FindDeltaAngleDegrees(Rot.Yaw,RequestedPose->Rotator().Yaw))>1.f)return Result(false,TEXT("RequestedPoseNotOnWall"));
        FHitResult Visible;
        if(World->LineTraceSingleByChannel(Visible,Eye,RequestedPose->GetLocation(),ECC_Visibility,Query))return Result(false,TEXT("RequestedPoseOccluded"));
        Rot=RequestedPose->Rotator(); // Keep the same horizontal pose, including its quantized network location.
    }
    else
    {
        Desired=Hit.ImpactPoint+Normal*.5f;Out=FTransform(Rot,Desired);
    }
    // Physical support bounds the available height. No floor reference or fixed Z limit.
    const FTransform Wall(Rot,Hit.ImpactPoint);const float Half=LengthForCapacity(Count)*.5f;float SurfaceDepth=0.f;
    const int32 Probes=FMath::CeilToInt((Half*2-4)/24.f);
    for(int32 Probe=0;Probe<=Probes;++Probe)
    {
        const float Y=FMath::Lerp(-Half+2,Half-2,float(Probe)/Probes);
        for(const float Z : {-30.f,2.f})
        {
            const FVector Point=Wall.TransformPosition(FVector(0,Y,Z));FHitResult Back;
            if(!World->LineTraceSingleByChannel(Back,Point+Normal*5,Point-Normal*5,ECC_Visibility,Query) || !IsSalesShelfWall(Back) || FVector::DotProduct(FVector(Back.ImpactNormal.X,Back.ImpactNormal.Y,0).GetSafeNormal(),Normal)<.98f)return Result(false,TEXT("MissingWallSupport"));
            SurfaceDepth=FMath::Max(SurfaceDepth,float(FVector::DotProduct(Back.ImpactPoint-Hit.ImpactPoint,Normal)));
        }
    }
    FCollisionObjectQueryParams Objects;Objects.AddObjectTypesToQuery(ECC_WorldStatic);Objects.AddObjectTypesToQuery(ECC_WorldDynamic);Objects.AddObjectTypesToQuery(ECC_Pawn);
    const float BracketY=Half-9.f;
    const FBox Clearance[] = {
        FBox(FVector(.5f,-Half+.5f,-3.5f),FVector(43.5f,Half-.5f,-.5f)),
        FBox(FVector(.5f,-BracketY-1.25f,-27.5f),FVector(27.5f,-BracketY+1.25f,-2.5f)),
        FBox(FVector(.5f,BracketY-1.25f,-27.5f),FVector(27.5f,BracketY+1.25f,-2.5f))};
    // Fit the flat back against the actual wall relief over each real part.
    // Only the horizontal normal offset changes: the aimed Z is never corrected.
    for(const FBox& Part:Clearance)
    {
        FHitResult Contact;const FVector Center=Wall.TransformPosition(Part.GetCenter());
        if(World->SweepSingleByObjectType(Contact,Center+Normal*10,Center-Normal*5,Rot.Quaternion(),Objects,FCollisionShape::MakeBox(Part.GetExtent()),Query))
        {
            if(Contact.bStartPenetrating || !IsSalesShelfWall(Contact) || FVector::DotProduct(FVector(Contact.ImpactNormal.X,Contact.ImpactNormal.Y,0).GetSafeNormal(),Normal)<.98f)return Result(false,TEXT("ObstructedWallGeometry"));
            SurfaceDepth=FMath::Max(SurfaceDepth,float(FVector::DotProduct(Contact.ImpactPoint-Hit.ImpactPoint,Normal)));
        }
    }
    const FVector FittedLocation=Hit.ImpactPoint+Normal*(SurfaceDepth+.5f);
    if(RequestedPose)
    {
        if(FMath::Abs(FVector::DotProduct(RequestedPose->GetLocation()-FittedLocation,Normal))>.15f)return Result(false,TEXT("RequestedPoseNotOnWall"));
    }
    else Out.SetLocation(FittedLocation);
    FCollisionQueryParams Overlap(Query);Overlap.bTraceComplex=false;
    for(const FBox& Part:Clearance)
    {
        const FVector Center=Out.TransformPosition(Part.GetCenter());const auto Shape=FCollisionShape::MakeBox(Part.GetExtent());
        TArray<FOverlapResult> SimpleHits,PreciseHits;
        World->OverlapMultiByObjectType(SimpleHits,Center,Rot.Quaternion(),Objects,Shape,Overlap);
        bool bPreciseReady=false;
        for(const auto& Contact:SimpleHits)
        {
            const FHitResult Surface(Contact.GetActor(),Contact.GetComponent(),Center,Normal);
            if(IsSalesShelfWall(Surface))
            {
                // Imported walls may have a convex/simple proxy outside the visible surface.
                // Refine walls only; keep the original blocking volumes of other objects and pawns.
                if(!bPreciseReady){auto Precise=Overlap;Precise.bTraceComplex=true;World->OverlapMultiByObjectType(PreciseHits,Center,Rot.Quaternion(),Objects,Shape,Precise);bPreciseReady=true;}
                if(!PreciseHits.ContainsByPredicate([&](const FOverlapResult& H){return H.GetComponent()==Contact.GetComponent();}))
                {
                    if(SalesShelfPlacementDebug.GetValueOnGameThread())UE_LOG(LogTemp,Display,TEXT("ShelfPlacementOverlap Actor=%s Component=%s SimpleBlocked=1 ComplexBlocked=0 IgnoredOversizedWallProxy=1"),*GetNameSafe(Contact.GetActor()),*GetNameSafe(Contact.GetComponent()));
                    continue;
                }
            }
            if(SalesShelfPlacementDebug.GetValueOnGameThread())UE_LOG(LogTemp,Display,TEXT("ShelfPlacementOverlap Actor=%s Component=%s"),*GetNameSafe(Contact.GetActor()),*GetNameSafe(Contact.GetComponent()));
            return Result(false,TEXT("PhysicalOverlap"));
        }
    }
    auto VisuallyBlocked=[&](const AActor* Furniture)
    {
        if(Furniture==Ignored || Furniture->IsHidden() || !Furniture->GetIsReplicated() || (Ignored && Furniture->IsAttachedTo(Ignored)))return false;
        TInlineComponentArray<UStaticMeshComponent*> Parts(Furniture);
        for(auto* Visual:Parts)
        {
            const UStaticMesh* Asset=Visual->GetStaticMesh();
            if(!Asset || !Visual->IsVisible() || Visual->bHiddenInGame || Visual->GetCollisionEnabled()!=ECollisionEnabled::NoCollision)continue;
            const FBox Local=Asset->GetRenderData() ? Asset->GetRenderData()->Bounds.GetBox():Asset->GetBoundingBox();
            const FBox VisualBounds=Local.TransformBy(Visual->GetComponentTransform()*Out.Inverse());
            for(const FBox& Part:Clearance)if(VisualBounds.Intersect(Part))return true;
        }
        return false;
    };
    for(TActorIterator<ABotanicusLargeEquipmentActor> It(World);It;++It)if(VisuallyBlocked(*It))return Result(false,TEXT("VisualFurnitureOverlap"));
    for(TActorIterator<ABotanicusPlaceableItemActor> It(World);It;++It)if(VisuallyBlocked(*It))return Result(false,TEXT("VisualFurnitureOverlap"));
    return Result(true,TEXT("Accepted"));
}
