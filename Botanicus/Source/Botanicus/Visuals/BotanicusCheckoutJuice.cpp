#include "Visuals/BotanicusJuiceWorldSubsystem.h"
#include "Engine/World.h"
#include "Sales/Checkout/BotanicusCheckoutSettings.h"
#include "Sales/Checkout/BotanicusCheckoutScanner.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Scalability.h"
void UBotanicusJuiceWorldSubsystem::StartCheckoutScan(ABotanicusCheckoutScanner* Scanner,const FVector& Start,const FVector& End,float Age)
{
    const auto* S=GetDefault<UBotanicusCheckoutSettings>();const float Duration=FMath::Clamp(S->ScanSeconds,.15f,.25f);
    if(GetWorld()->GetNetMode()==NM_DedicatedServer || !S->bScanBeamEnabled || !S->bAnimationsEnabled || CheckoutBeams.Num()>=4 || Age>=Duration ||
        (S->bDisableMotionOnLowQuality && Scalability::GetQualityLevels().EffectsQuality==0) || FVector::DistSquared(ListenerPosition(),End)>FMath::Square(S->MaximumEffectDistance)) return;
    if(!CheckoutBeamMaterial) CheckoutBeamMaterial=S->BeamMaterial.LoadSynchronous();if(!CheckoutBeamMaterial) return;
    FBotanicusCheckoutBeam B;B.Scanner=Scanner;B.Start=Start;B.End=End;B.Duration=Duration;B.StartedAt=GetWorld()->GetTimeSeconds()-FMath::Max(0.f,Age);
    B.Material=UMaterialInstanceDynamic::Create(CheckoutBeamMaterial,this);
    auto Create=[this,&B](const TCHAR* Path)
    {
        auto* C=NewObject<UStaticMeshComponent>(GetWorld(),NAME_None,RF_Transient);C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,Path));C->SetCanEverAffectNavigation(false);
        C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetGenerateOverlapEvents(false);C->SetCastShadow(false);C->SetMaterial(0,B.Material);C->SetMobility(EComponentMobility::Movable);C->RegisterComponentWithWorld(GetWorld());return C;
    };
    B.Beam=Create(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));B.Impact=Create(TEXT("/Engine/BasicShapes/Sphere.Sphere"));CheckoutBeams.Add(MoveTemp(B));++CheckoutScansStarted;UpdateCheckoutBeams();
}
void UBotanicusJuiceWorldSubsystem::UpdateCheckoutBeams()
{
    const auto* S=GetDefault<UBotanicusCheckoutSettings>();const double Now=GetWorld()->GetTimeSeconds();
    for(int32 I=CheckoutBeams.Num()-1;I>=0;--I)
    {
        auto& B=CheckoutBeams[I];const float U=(Now-B.StartedAt)/B.Duration;
        if(U>=1 || !S->bAnimationsEnabled || !S->bScanBeamEnabled)
        {if(B.Beam) B.Beam->DestroyComponent();if(B.Impact) B.Impact->DestroyComponent();CheckoutBeams.RemoveAtSwap(I);continue;}
        const FVector Start=B.Scanner.IsValid() ? B.Scanner->GetMuzzleLocation() : B.Start;const FVector Delta=B.End-Start;
        B.Beam->SetWorldTransform(FTransform(FRotationMatrix::MakeFromZ(Delta).ToQuat(),(Start+B.End)*.5,FVector(S->BeamWidth/100.f,S->BeamWidth/100.f,Delta.Size()/100.f)));
        B.Impact->SetWorldTransform(FTransform(FQuat::Identity,B.End,FVector(.035f*(1-.25f*U))));B.Material->SetScalarParameterValue(TEXT("Fade"),FMath::Square(1-U));
    }
}
void UBotanicusJuiceWorldSubsystem::ClearCheckoutBeams()
{
    for(auto& B:CheckoutBeams) {if(B.Beam) B.Beam->DestroyComponent();if(B.Impact) B.Impact->DestroyComponent();}CheckoutBeams.Reset();
}
