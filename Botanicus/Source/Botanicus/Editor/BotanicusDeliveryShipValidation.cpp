#include "Editor/BotanicusDeliveryShipValidation.h"
#if WITH_EDITOR
#include "BotanicusPlayerController.h"
#include "BotanicusGameMode.h"
#include "BotanicusGameState.h"
#include "Delivery/BotanicusDeliveryShip.h"
#include "Delivery/BotanicusDeliveryZoneActor.h"
#include "Delivery/BotanicusDeliveryParcelActor.h"
#include "Save/BotanicusWorldSaveGame.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraActor.h"
#include "EngineUtils.h"
#include "Engine/NetConnection.h"
#include "Editor.h"
#include "Json.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "TimerManager.h"
namespace
{
UWorld* World(UObject* C) { auto* W=C?C->GetWorld():nullptr;return W && W->WorldType==EWorldType::PIE && FPaths::ProjectSavedDir().Contains(TEXT("isolated_user")) && FParse::Param(FCommandLine::Get(),TEXT("BotanicusShipValidation"))?W:nullptr; }
TSharedPtr<FJsonValue> Vec(const FVector& P) { return MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueNumber>(P.X),MakeShared<FJsonValueNumber>(P.Y),MakeShared<FJsonValueNumber>(P.Z)}); }
ABotanicusPlayerController* PC(UWorld* W,int32 I) { TArray<ABotanicusPlayerController*> P;for(auto It=W->GetPlayerControllerIterator();It;++It)if(auto* C=Cast<ABotanicusPlayerController>(It->Get()))P.Add(C);P.Sort([](auto& A,auto& B){return A.IsLocalController() && !B.IsLocalController();});return P.IsValidIndex(I)?P[I]:nullptr; }
}
#endif
bool UBotanicusDeliveryShipValidation::Step(UObject* Context,FName Action,int32 Index)
{
#if WITH_EDITOR
 auto* W=World(Context);if(!W)return false;
 static bool bExecuting=false;
 if(!bExecuting){TWeakObjectPtr<UWorld> Weak=W;W->GetTimerManager().SetTimerForNextTick([Weak,Action,Index](){if(Weak.IsValid()){TGuardValue<bool> Guard(bExecuting,true);Step(Weak.Get(),Action,Index);}});return true;}
 auto* C=PC(W,Index);auto* Mode=W->GetAuthGameMode<ABotanicusGameMode>();
 if(Action=="Setup")
 {
  if(auto* State=W->GetGameState<ABotanicusGameState>())State->AddSharedFunds(100000);
  ABotanicusDeliveryZoneActor* Zone=nullptr;for(TActorIterator<ABotanicusDeliveryZoneActor> I(W);I;++I){Zone=*I;break;}
  if(!Zone)return false;auto Frame=Zone->GetDeliveryFrame();
  for(int32 i=0;i<2;++i)if(auto* Player=PC(W,i))
  {
   if(Player->GetPawn())Player->GetPawn()->SetActorLocation(Frame.GetLocation()+FVector(-1200,1000,100));
   auto* Camera=W->SpawnActor<ACameraActor>(Frame.GetLocation()+FVector(-2900,-2700,2100),FRotator::ZeroRotator);
   Camera->SetReplicates(true);Camera->SetActorRotation((Frame.GetLocation()+FVector(0,0,1150)-Camera->GetActorLocation()).Rotation());
   Player->SetViewTarget(Camera);Player->ClientSetViewTarget(Camera);
  }
  return true;
 }
 if(Action=="Order1" || Action=="Order3" || Action=="Order6")
 {if(!C)return false;const int32 N=Action=="Order6"?6:Action=="Order3"?3:1;for(int32 i=0;i<N;++i)C->ServerPlaceCatalogOrder(TEXT("SeedPacket_AureliaSweet"));return true;}
 if(Action=="Requeue")
 {if(!C)return false;for(TActorIterator<ABotanicusDeliveryZoneActor>I(W);I;++I)for(auto& O:C->GetPendingOrders())if(O.DeliveryServerTime<=W->GetGameState()->GetServerWorldTimeSeconds())I->EnqueueOrder(O);return true;}
 if(Action=="LateJoin"){GEditor->RequestLateJoin();return true;}
 if(Action=="Disconnect")
 {if(!C || !Mode || C->IsLocalController())return false;Mode->Logout(C);C->Destroy();return true;}
 if(Action=="Save"){return Mode && Mode->BotanicusSaveNow();}
 if(Action=="ClearDelivered")
 {if(!Mode)return false;for(TActorIterator<ABotanicusDeliveryParcelActor>I(W);I;++I)I->Destroy();return true;}
 if(Action=="ReadSave")return true;
#endif
 return false;
}
FString UBotanicusDeliveryShipValidation::Snapshot(UObject* Context)
{
#if WITH_EDITOR
 auto* W=World(Context);if(!W)return TEXT("{}");auto R=MakeShared<FJsonObject>();
 R->SetNumberField("time",W->GetTimeSeconds());R->SetNumberField("serverTime",W->GetGameState()?W->GetGameState()->GetServerWorldTimeSeconds():0);R->SetBoolField("authority",W->GetAuthGameMode()!=nullptr);
 TArray<TSharedPtr<FJsonValue>> Ships,Parcels,Orders;
 for(TActorIterator<ABotanicusDeliveryShip>I(W);I;++I)
 {auto O=MakeShared<FJsonObject>();O->SetStringField("name",I->GetName());O->SetNumberField("phase",int32(I->GetPhase()));O->SetField("location",Vec(I->GetActorLocation()));O->SetField("cargo",Vec(I->GetCargoLocation()));O->SetBoolField("tick",I->IsActorTickEnabled());O->SetBoolField("collision",I->GetActorEnableCollision());O->SetStringField("mesh",GetPathNameSafe(I->ShipMesh->GetStaticMesh()));Ships.Add(MakeShared<FJsonValueObject>(O));}
 for(TActorIterator<ABotanicusDeliveryParcelActor>I(W);I;++I)
 {auto O=MakeShared<FJsonObject>();O->SetStringField("name",I->GetName());O->SetStringField("key",I->GetItemKey().ToString());O->SetNumberField("qty",I->GetQuantity());O->SetField("location",Vec(I->GetActorLocation()));O->SetField("velocity",Vec(I->GetVelocity()));O->SetField("rotation",Vec(FVector(I->GetActorRotation().Pitch,I->GetActorRotation().Yaw,I->GetActorRotation().Roll)));auto* Body=Cast<UPrimitiveComponent>(I->GetRootComponent());O->SetBoolField("physics",Body && Body->IsSimulatingPhysics());O->SetBoolField("gravity",Body && Body->IsGravityEnabled());O->SetBoolField("collision",I->GetActorEnableCollision());Parcels.Add(MakeShared<FJsonValueObject>(O));}
 int32 Players=0;
 for(auto It=W->GetPlayerControllerIterator();It;++It)if(auto* C=Cast<ABotanicusPlayerController>(It->Get()))
 {++Players;for(auto& P:C->GetPendingOrders()){auto O=MakeShared<FJsonObject>();O->SetStringField("id",P.OrderId.ToString());O->SetStringField("key",P.ItemKey.ToString());O->SetNumberField("qty",P.Quantity);O->SetNumberField("due",P.DeliveryServerTime);O->SetStringField("owner",C->GetName());Orders.Add(MakeShared<FJsonValueObject>(O));}}
 R->SetNumberField("players",Players);
 for(TActorIterator<ABotanicusDeliveryZoneActor>I(W);I;++I){R->SetField("center",Vec(I->GetDeliveryFrame().GetLocation()));R->SetNumberField("yaw",I->GetDeliveryFrame().Rotator().Yaw);R->SetBoolField("queued",I->HasQueuedOrders());break;}
 R->SetArrayField("ships",Ships);R->SetArrayField("parcels",Parcels);R->SetArrayField("orders",Orders);
 FString Out;auto Writer=TJsonWriterFactory<>::Create(&Out);FJsonSerializer::Serialize(R,Writer);return Out;
#else
 return TEXT("{}");
#endif
}


FString UBotanicusDeliveryShipValidation::SavedSnapshot()
{
#if WITH_EDITOR
 if(!FPaths::ProjectSavedDir().Contains(TEXT("isolated_user")))return TEXT("{}");
 auto* Save=Cast<UBotanicusWorldSaveGame>(UGameplayStatics::LoadGameFromSlot(TEXT("Botanicus_Autosave_Lvl_BuildingTest"),0));
 if(!Save)return TEXT("{}");
 auto R=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Parcels,Orders;
 for(const auto& I:Save->WorldItems)if(I.ActorClass.ToString().Contains(TEXT("DeliveryParcel")))
 {auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("key"),I.ItemKey.ToString());O->SetNumberField(TEXT("qty"),I.Quantity);Parcels.Add(MakeShared<FJsonValueObject>(O));}
 for(const auto& E:Save->PlayerEconomies)for(const auto& I:E.PendingOrders)
 {auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("id"),I.OrderId.ToString());O->SetStringField(TEXT("key"),I.ItemKey.ToString());O->SetNumberField(TEXT("qty"),I.Quantity);Orders.Add(MakeShared<FJsonValueObject>(O));}
 R->SetArrayField(TEXT("saved_parcels"),Parcels);R->SetArrayField(TEXT("saved_pending"),Orders);
 FString Out;auto Writer=TJsonWriterFactory<>::Create(&Out);FJsonSerializer::Serialize(R,Writer);return Out;
#else
 return TEXT("{}");
#endif
}
