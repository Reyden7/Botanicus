#pragma once
#include "CoreMinimal.h"
#include "QuickBar/BotanicusQuickBarComponent.h"
#include "Components/ChildActorComponent.h"
#include "Visuals/BotanicusPotSoilVisualActor.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

/** Existing saved entries are bag contributions, with no horizontal assignment.
 * Keeping every unfinished contribution avoids rounding several unpaid half-bags
 * into a paid bag during migration. Each contribution still costs exactly one bag. */
namespace BotanicusPlanterSoil
{
inline FVector2D ClassicExtent(int32 Capacity)
{
    // Two-centimetre blockout walls plus 2.5 mm of clearance against their inner faces.
    return FVector2D((.45f+Capacity*.3f)*50.f-2.25f,18.75f);
}
inline bool TrySurfaceAim(const AActor* Container,const APawn* Source,FVector2D Extent,float Height,FVector& WorldPoint)
{
    if(!Source || !Container->GetWorld())return false;
    FVector Eye=Source->GetPawnViewLocation();FRotator View=Source->GetBaseAimRotation();
    if(const auto* PC=Cast<APlayerController>(Source->GetController()))PC->GetPlayerViewPoint(Eye,View);
    const auto& T=Container->GetActorTransform();
    const FVector Start=T.InverseTransformPosition(Eye),Direction=T.InverseTransformVector(View.Vector());
    if(FMath::Abs(Direction.Z)<=SMALL_NUMBER)return false;
    const float Distance=(Height-Start.Z)/Direction.Z;
    if(Distance<0 || Distance>500)return false;
    const FVector Local=Start+Direction*Distance;
    if(FMath::Abs(Local.X)>Extent.X || FMath::Abs(Local.Y)>Extent.Y)return false;
    WorldPoint=T.TransformPosition(Local);
    FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(BotanicusPlanterSoilSurface),false,Source);
    if(Container->GetWorld()->LineTraceSingleByChannel(Hit,Eye,WorldPoint,ECC_Visibility,Q) && Hit.GetActor()!=Container)
        return Hit.GetActor() && Hit.GetActor()->GetAttachParentActor()==Container;
    return true;
}
inline float Total(const TArray<float>& Contributions,int32 Required)
{
    float Sum=0;
    for(int32 I=0;I<FMath::Min(Required,Contributions.Num());++I) Sum+=FMath::Clamp(Contributions[I],0.f,1.f);
    return Sum;
}
struct FTransfer
{
    bool bChanged=false;
    bool bPaidBag=false;
    bool bStopped=false;
};
inline FTransfer Transfer(TArray<float>& Contributions,int32 Required,int32& PaidBags,UBotanicusQuickBarComponent* Bar,float AddedBags)
{
    FTransfer Result;
    if(!Bar)return Result;
    Contributions.SetNum(FMath::Max(Required,Contributions.Num()));
    for(int32 I=0;I<Required && AddedBags>0;++I)
    {
        const float Previous=Contributions[I];
        if(Previous>=1.f)continue;
        if(Bar->GetSelectedSlot().IsEmpty() || Bar->GetSelectedSlot().ItemKey!=TEXT("PottingSoil"))
        {Result.bStopped=true;break;}
        const float Amount=FMath::Min(AddedBags,1.f-Previous);
        const float Next=FMath::Min(1.f,Previous+Amount);
        if(Next>=1.f)
        {
            // Commit the whole-bag debit before making this contribution complete.
            if(!Bar->ConsumeSelectedItem(1)){Result.bStopped=true;break;}
            ++PaidBags;Result.bPaidBag=true;
        }
        Contributions[I]=Next;AddedBags-=Amount;Result.bChanged|=Next>Previous;
    }
    return Result;
}
inline void Hide(UChildActorComponent* Visual)
{
    if(!Visual)return;
    if(auto* Soil=Cast<ABotanicusPotSoilVisualActor>(Visual->GetChildActor()))
    {Soil->ConfigureSoilShape(FVector2D(1),FVector2D(1),1,0,false,true);Soil->SetActorHiddenInGame(true);}
}
inline void Surface(UChildActorComponent* Visual,FVector2D Extent,float Minimum,float Maximum,float Ratio,float Wetness)
{
    if(!Visual)return;
    const float Height=FMath::Lerp(Minimum,FMath::Max(Minimum,Maximum),Ratio);
    const float Depth=FMath::Max(.5f,Height-Minimum);
    Visual->SetRelativeLocation(FVector(0,0,Height-Depth));
    if(auto* Soil=Cast<ABotanicusPotSoilVisualActor>(Visual->GetChildActor()))
    {
        Soil->SetActorHiddenInGame(false);
        // FillAlpha=1 deliberately bypasses the reference pot's minimum 2% cap.
        // Moving the base keeps the surface exactly at Lerp(Min,Max,Ratio).
        Soil->ConfigureSoilShape(Extent,Extent,Depth,1,Ratio>0,true,Wetness);
    }
}
inline FVector AimedSurface(const AActor* Container,const APawn* Source,FVector2D Extent,float Height)
{
    FVector Local(0,0,Height);
    if(Source)
    {
        FVector Eye=Source->GetPawnViewLocation();FRotator View=Source->GetBaseAimRotation();
        if(const auto* PC=Cast<APlayerController>(Source->GetController()))PC->GetPlayerViewPoint(Eye,View);
        const auto& T=Container->GetActorTransform();
        const FVector Start=T.InverseTransformPosition(Eye),Direction=T.InverseTransformVector(View.Vector());
        if(FMath::Abs(Direction.Z)>SMALL_NUMBER)
        {
            const float Distance=(Height-Start.Z)/Direction.Z;
            if(Distance>=0 && Distance<=700)Local=Start+Direction*Distance;
        }
    }
    Local.X=FMath::Clamp(Local.X,-Extent.X,Extent.X);Local.Y=FMath::Clamp(Local.Y,-Extent.Y,Extent.Y);Local.Z=Height;
    return Container->GetActorTransform().TransformPosition(Local);
}
}
