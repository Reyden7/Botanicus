#include "Visuals/BotanicusJuiceWorldSubsystem.h"
#include "Engine/World.h"
#include "Sales/Checkout/BotanicusCheckoutSettings.h"
#include "Sales/Checkout/BotanicusCheckoutScanner.h"
#include "Sales/BotanicusCashRegisterActor.h"
#include "Visitors/BotanicusVisitorCharacter.h"
#include "Visuals/BotanicusPlacementJuiceSettings.h"
#include "Components/StaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Scalability.h"
#include "Audio/BotanicusAudioWorldSubsystem.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"

void UBotanicusJuiceWorldSubsystem::StartCheckoutScan(ABotanicusCashRegisterActor* Register,const FBotanicusCheckoutScan& Scan)
{
    const auto* Settings=GetDefault<UBotanicusCheckoutSettings>();
    FBox Bounds;
    if(!IsValid(Register) || !IsValid(Scan.Customer) || !IsValid(Register->GetDockScanner()) || !Scan.Customer->GetCheckoutPlantVisualBounds(Bounds) ||
        GetWorld()->GetNetMode()==NM_DedicatedServer || !Settings->bScanBeamEnabled || !Settings->bAnimationsEnabled || CheckoutBeams.Num()>=4 ||
        Register->CheckoutServerTime()-Scan.StartedAt>=Scan.TotalSeconds() ||
        (Settings->bDisableMotionOnLowQuality && Scalability::GetQualityLevels().EffectsQuality==0) || FVector::DistSquared(ListenerPosition(),Bounds.GetCenter())>FMath::Square(Settings->MaximumEffectDistance) ||
        CheckoutBeams.ContainsByPredicate([&Scan](const auto& Existing){return Existing.Id==Scan.Id;}))return;
    // Exactly the same material and parameter vocabulary as the placement hologram.
    auto* Hologram=Settings->ScanHologramMaterial.LoadSynchronous();if(!Hologram)return;
    FBotanicusCheckoutBeam Effect;Effect.Register=Register;Effect.Scanner=Register->GetDockScanner();Effect.Customer=Scan.Customer;
    Effect.Id=Scan.Id;Effect.StartedAt=Scan.StartedAt;Effect.Up=Scan.UpSeconds;Effect.Down=Scan.DownSeconds;Effect.Fade=Scan.FadeSeconds;
    Effect.Material=UMaterialInstanceDynamic::Create(Hologram,this);Effect.ContactMaterial=UMaterialInstanceDynamic::Create(Hologram,this);
    for(auto* Material:{Effect.Material.Get(),Effect.ContactMaterial.Get()})
    {
        Material->SetVectorParameterValue(TEXT("BodyTint"),Settings->ScanBeamColor);
        Material->SetVectorParameterValue(TEXT("RimTint"),GetDefault<UBotanicusPlacementJuiceSettings>()->ValidRim);
        Material->SetScalarParameterValue(TEXT("FresnelIntensity"),GetDefault<UBotanicusPlacementJuiceSettings>()->FresnelIntensity);
        Material->SetScalarParameterValue(TEXT("Emission"),FMath::Clamp(Settings->ScanGlowIntensity,0.f,3.f));
    }
    auto Configure=[this](UPrimitiveComponent* Component)
    {
        Component->SetCanEverAffectNavigation(false);Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->SetGenerateOverlapEvents(false);Component->SetCastShadow(false);Component->SetMobility(EComponentMobility::Movable);
        Component->RegisterComponentWithWorld(GetWorld());
    };
    Effect.Beam=NewObject<UProceduralMeshComponent>(GetWorld(),NAME_None,RF_Transient);Configure(Effect.Beam);Effect.Beam->SetMaterial(0,Effect.Material);
    Effect.Vertices.Init(FVector::ZeroVector,3);Effect.Normals.Init(FVector::UpVector,3);Effect.UV={FVector2D(.5,0),FVector2D(0,1),FVector2D(1,1)};
    Effect.Beam->CreateMeshSection(0,Effect.Vertices,TArray<int32>{0,1,2},Effect.Normals,Effect.UV,TArray<FColor>(),TArray<FProcMeshTangent>(),false);
    // A hollow horizontal ribbon outlines the scanned volume without filling over the foliage.
    Effect.Impact=NewObject<UProceduralMeshComponent>(GetWorld(),NAME_None,RF_Transient);Configure(Effect.Impact);Effect.Impact->SetMaterial(0,Effect.ContactMaterial);
    constexpr int32 Segments=20;TArray<int32> Triangles;Effect.RingVertices.Init(FVector::ZeroVector,Segments*2);Effect.RingNormals.Init(FVector::UpVector,Segments*2);
    for(int32 I=0;I<Segments;++I)
    {
        const float Angle=2*PI*I/Segments;Effect.Circle.Add(FVector2D(FMath::Cos(Angle),FMath::Sin(Angle)));
        Effect.RingUV.Add(FVector2D(float(I)/Segments,0));Effect.RingUV.Add(FVector2D(float(I)/Segments,1));
        const int32 A=I*2,B=((I+1)%Segments)*2;Triangles.Append({A,B,A+1,B,B+1,A+1});
    }
    Effect.Impact->CreateMeshSection(0,Effect.RingVertices,Triangles,Effect.RingNormals,Effect.RingUV,TArray<FColor>(),TArray<FProcMeshTangent>(),false);
    // The replicated scan ID already deduplicates this local presentation on every peer.
    const float AudioAge=FMath::Max(0.f,float(Register->CheckoutServerTime()-Scan.StartedAt));
    if(AudioAge<Effect.Up+Effect.Down && Settings->ScanSoundVolume>0)
        if(auto* Audio=GetWorld()->GetSubsystem<UBotanicusAudioWorldSubsystem>())
            Effect.ScanVoice=Audio->PlaySpatialTransition(Effect.Scanner.Get(),Settings->ScanSound.LoadSynchronous(),Effect.Scanner->GetMuzzleLocation(),Settings->ScanSoundVolume,AudioAge);
    CheckoutBeams.Add(MoveTemp(Effect));++CheckoutScansStarted;UpdateCheckoutBeams();
}

void UBotanicusJuiceWorldSubsystem::UpdateCheckoutBeams()
{
    const auto* Settings=GetDefault<UBotanicusCheckoutSettings>();
    for(int32 Index=CheckoutBeams.Num()-1;Index>=0;--Index)
    {
        auto& Effect=CheckoutBeams[Index];
        const float Age=Effect.Register.IsValid() ? float(Effect.Register->CheckoutServerTime()-Effect.StartedAt):FLT_MAX;
        FBox Bounds;
        if(!Effect.Register.IsValid() || Effect.Register->GetCheckoutScan().Id!=Effect.Id || !Effect.Scanner.IsValid() || !Effect.Customer.IsValid() ||
           !Effect.Customer->GetCheckoutPlantVisualBounds(Bounds) || Age>=Effect.Up+Effect.Down+Effect.Fade || !Settings->bAnimationsEnabled || !Settings->bScanBeamEnabled)
        {
            if(IsValid(Effect.ScanVoice))Effect.ScanVoice->FadeOut(.03f,0.f);
            if(Effect.Beam)Effect.Beam->DestroyComponent();if(Effect.Impact)Effect.Impact->DestroyComponent();CheckoutBeams.RemoveAtSwap(Index);continue;
        }
        const float Travel=Effect.Up+Effect.Down;
        // Two strictly linear passes, driven by one server timestamp. No transaction callbacks here.
        const float Progress=Age<Effect.Up ? FMath::Clamp(Age/Effect.Up,0.f,1.f):FMath::Clamp(1-(Age-Effect.Up)/Effect.Down,0.f,1.f);
        const FVector Muzzle=Effect.Scanner->GetMuzzleLocation();
        if(IsValid(Effect.ScanVoice))
        {
            Effect.ScanVoice->SetWorldLocation(Muzzle);
            if(!Effect.bAudioFading && Age>=Travel)
            {
                Effect.bAudioFading=true;
                Effect.ScanVoice->FadeOut(FMath::Min(Effect.Fade,FMath::Clamp(Settings->ScanSoundFadeOut,.01f,.15f)),0.f);
            }
        }
        // A shared carrier position orients the slice identically in first/third person.
        const FVector Operator=Effect.Scanner->GetCarrier() ? Effect.Scanner->GetCarrier()->GetActorLocation():Muzzle;
        FVector Facing=Operator-Bounds.GetCenter();Facing.Z=0;Facing=Facing.GetSafeNormal();if(Facing.IsNearlyZero())Facing=FVector::ForwardVector;
        const FVector Right=FVector::CrossProduct(Facing,FVector::UpVector).GetSafeNormal();
        // Project the original oriented mesh bounds once, avoiding a second AABB expansion.
        const float HalfWidth=Effect.Customer->GetCheckoutPlantProjectedExtent(Right)+1;
        const float HalfDepth=Effect.Customer->GetCheckoutPlantProjectedExtent(Facing)+1;
        FVector Contact=Bounds.GetCenter()+Facing*(HalfDepth*.70710678f);
        Contact.Z=FMath::Lerp(Bounds.Min.Z,Bounds.Max.Z,Progress);
        FVector Left=Contact-Right*(HalfWidth*.70710678f),RightEdge=Contact+Right*(HalfWidth*.70710678f);
        FVector Normal=FVector::CrossProduct(Left-Muzzle,RightEdge-Muzzle).GetSafeNormal();
        if(FVector::DotProduct(Normal,ListenerPosition()-(Muzzle+Contact)*.5)<0){Swap(Left,RightEdge);Normal=-Normal;}
        Effect.Vertices[0]=Muzzle;Effect.Vertices[1]=Left;Effect.Vertices[2]=RightEdge;for(auto& N:Effect.Normals)N=Normal;
        Effect.Beam->UpdateMeshSection(0,Effect.Vertices,Effect.Normals,Effect.UV,TArray<FColor>(),TArray<FProcMeshTangent>());
        const float RibbonWidth=FMath::Clamp(Settings->ScanBeamWidth*.5f,.2f,FMath::Max(.2f,FMath::Min(HalfWidth,HalfDepth)*.15f));
        FVector SliceCentre=Bounds.GetCenter();SliceCentre.Z=Contact.Z;
        for(int32 I=0;I<Effect.Circle.Num();++I)
        {
            const auto Unit=Effect.Circle[I];
            Effect.RingVertices[I*2]=SliceCentre+Right*(Unit.X*HalfWidth)+Facing*(Unit.Y*HalfDepth);
            Effect.RingVertices[I*2+1]=SliceCentre+Right*(Unit.X*(HalfWidth-RibbonWidth))+Facing*(Unit.Y*(HalfDepth-RibbonWidth));
        }
        Effect.Impact->UpdateMeshSection(0,Effect.RingVertices,Effect.RingNormals,Effect.RingUV,TArray<FColor>(),TArray<FProcMeshTangent>());
        const float Fade=FMath::Clamp(Age/.04f,0.f,1.f)*(Age<Travel ? 1.f:FMath::Clamp(1-(Age-Travel)/Effect.Fade,0.f,1.f));
        Effect.Material->SetScalarParameterValue(TEXT("Opacity"),FMath::Clamp(Settings->ScanBeamOpacity,.05f,.7f)*.28f*Fade);
        Effect.ContactMaterial->SetScalarParameterValue(TEXT("Opacity"),FMath::Clamp(Settings->ScanBeamOpacity,.05f,.7f)*.6f*Fade);
        Effect.TargetBounds=Bounds;Effect.Contact=Contact;Effect.Progress=Progress;Effect.BandWidth=HalfWidth*2;
    }
}

void UBotanicusJuiceWorldSubsystem::StopCheckoutScan(const FGuid& Id)
{
    if(!Id.IsValid())return;
    for(int32 Index=CheckoutBeams.Num()-1;Index>=0;--Index)if(CheckoutBeams[Index].Id==Id)
    {
        auto& Effect=CheckoutBeams[Index];if(IsValid(Effect.ScanVoice))Effect.ScanVoice->FadeOut(.03f,0.f);if(Effect.Beam)Effect.Beam->DestroyComponent();if(Effect.Impact)Effect.Impact->DestroyComponent();CheckoutBeams.RemoveAtSwap(Index);
    }
}
void UBotanicusJuiceWorldSubsystem::ClearCheckoutBeams()
{
    for(auto& Effect:CheckoutBeams){if(IsValid(Effect.ScanVoice)){Effect.ScanVoice->Stop();Effect.ScanVoice->DestroyComponent();}if(Effect.Beam)Effect.Beam->DestroyComponent();if(Effect.Impact)Effect.Impact->DestroyComponent();}CheckoutBeams.Reset();
}
