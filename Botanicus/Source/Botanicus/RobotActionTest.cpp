#include "RobotActionTest.h"
#include "BotanicusGameState.h"
#include "BotanicusPlayerController.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Delivery/BotanicusBotanistBookActor.h"
#include "Engine/World.h"
#include "Gardening/BotanicusGardeningRobot.h"
#include "Growing/BotanicusPlantPotActor.h"
#include "Presentation/BotanicusHeldItemComponent.h"
#include "QuickBar/BotanicusQuickBarComponent.h"
#include "Sales/BotanicusCashRegisterActor.h"
#include "Tools/BotanicusMultiToolComponent.h"
#include "Visitors/BotanicusVisitorZoneActor.h"
#include "EngineUtils.h"
#include "Visuals/BotanicusJuiceWorldSubsystem.h"
#if WITH_EDITOR
#include "AnimGraphNode_Slot.h"
#include "AnimGraphNode_SaveCachedPose.h"
#include "AnimGraphNode_UseCachedPose.h"
#include "Animation/Skeleton.h"
#endif
#include "BotanicusCharacter.h"
#include "Delivery/BotanicusDeliveryParcelActor.h"
#include "GameFramework/PlayerController.h"
#include "InputKeyEventArgs.h"
#include "TimerManager.h"

#if WITH_EDITOR
#include "Animation/AnimBlueprint.h"
#include "AnimationStateMachineGraph.h"
#include "AnimGraphNode_LayeredBoneBlend.h"
#include "AnimGraphNode_ModifyBone.h"
#include "AnimGraphNode_SequenceEvaluator.h"
#include "AnimGraphNode_StateMachine.h"
#include "AnimGraphNode_TwoBoneIK.h"
#include "EdGraph/EdGraph.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

namespace
{
	const TCHAR* AnimPath = TEXT("/Game/Characters/Robot/Animation/Blueprints/ABP_Robot.ABP_Robot");
	const TCHAR* CarryPath = TEXT("/Game/Characters/Robot/Animation/Mixamo/locomotion/Running_With_Box.Running_With_Box");
	const TCHAR* SquarePath = TEXT("/Game/Botanicus/Items/itemsMesh/carton/carton2/Labels/BP_CartonSquare.BP_CartonSquare_C");
	const TCHAR* RectangularPath = TEXT("/Game/Botanicus/Items/itemsMesh/carton/carton2/Labels/BP_CartonRectangular.BP_CartonRectangular_C");

	bool Fail(const TCHAR* Step)
	{
		UE_LOG(LogTemp, Error, TEXT("PARCEL_IK failed: %s"), Step);
		return false;
	}

	UEdGraphPin* FirstPin(const UEdGraphNode& Node, EEdGraphPinDirection Direction)
	{
		for (UEdGraphPin* Pin : Node.Pins)
		{
			if (Pin->Direction == Direction)
			{
				return Pin;
			}
		}
		return nullptr;
	}

	UEdGraphPin* FindOrShowPin(UAnimGraphNode_Base& Node, FName Name)
	{
		if (UEdGraphPin* Pin = Node.FindPin(Name))
		{
			return Pin;
		}
		for (FOptionalPinFromProperty& Property : Node.ShowPinForProperties)
		{
			if (Property.PropertyName == Name)
			{
				Property.bShowPin = true;
				Node.ReconstructNode();
				return Node.FindPin(Name);
			}
		}
		return nullptr;
	}

	bool Link(UEdGraphPin* A, UEdGraphPin* B)
	{
		return A && B && A->GetOwningNode()->GetSchema()->TryCreateConnection(A, B);
	}

	bool LinkPose(const UEdGraphNode& A, const UEdGraphNode& B)
	{
		return Link(FirstPin(A, EGPD_Output), FirstPin(B, EGPD_Input));
	}

	bool LinkVariable(UAnimGraphNode_Base& Node, const TCHAR* PinName, const TCHAR* Variable, int32 Y)
	{
		UEdGraphPin* Pin = FindOrShowPin(Node, PinName);
		if (!Pin)
		{
			return Fail(PinName);
		}
		FGraphNodeCreator<UK2Node_VariableGet> Creator(*Node.GetGraph());
		UK2Node_VariableGet* Getter = Creator.CreateNode(false);
		Getter->VariableReference.SetSelfMember(Variable);
		Getter->NodePosX = Node.NodePosX - 155;
		Getter->NodePosY = Y;
		Creator.Finalize();
		return Link(FirstPin(*Getter, EGPD_Output), Pin);
	}

	UAnimGraphNode_StateMachine* FindRobotMachine(UAnimBlueprint& Anim)
	{
		TArray<UEdGraph*> Graphs;
		Anim.GetAllGraphs(Graphs);
		for (UEdGraph* Graph : Graphs)
		{
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				auto* Machine = Cast<UAnimGraphNode_StateMachine>(Node);
				if (Machine && Machine->GetStateMachineName() == TEXT("SM_Robot_Locomotion"))
				{
					return Machine;
				}
			}
		}
		return nullptr;
	}

	UAnimGraphNode_ModifyBone* FindBoneControl(UEdGraph& Graph, FName Bone)
	{
		for (UEdGraphNode* Node : Graph.Nodes)
		{
			auto* Modify = Cast<UAnimGraphNode_ModifyBone>(Node);
			if (Modify && Modify->Node.BoneToModify.BoneName == Bone)
			{
				return Modify;
			}
		}
		return nullptr;
	}

	struct FArmPins
	{
		const TCHAR* Bone;
		const TCHAR* Hand;
		const TCHAR* Elbow;
		const TCHAR* Wrist;
		const TCHAR* Weight;
	};

	UAnimGraphNode_ModifyBone* AddArm(UEdGraphNode& Previous, const FArmPins& Pins, int32 X)
	{
		UEdGraph& Graph = *Previous.GetGraph();
		FGraphNodeCreator<UAnimGraphNode_TwoBoneIK> IKCreator(Graph);
		auto* IK = IKCreator.CreateNode(false);
		IK->NodePosX = X;
		IK->Node.IKBone.BoneName = Pins.Bone;
		IK->Node.EffectorLocationSpace = BCS_ComponentSpace;
		IK->Node.JointTargetLocationSpace = BCS_ComponentSpace;
		IK->Node.bAllowStretching = false;
		IK->Node.bMaintainEffectorRelRot = true;
		IKCreator.Finalize();

		FGraphNodeCreator<UAnimGraphNode_ModifyBone> WristCreator(Graph);
		auto* Wrist = WristCreator.CreateNode(false);
		Wrist->NodePosX = X + 305;
		Wrist->Node.BoneToModify.BoneName = Pins.Bone;
		Wrist->Node.RotationMode = BMM_Replace;
		Wrist->Node.RotationSpace = BCS_ParentBoneSpace;
		WristCreator.Finalize();
		if (!LinkVariable(*IK, TEXT("EffectorLocation"), Pins.Hand, 190)
			|| !LinkVariable(*IK, TEXT("JointTargetLocation"), Pins.Elbow, 255)
			|| !LinkVariable(*IK, TEXT("Alpha"), Pins.Weight, 320)
			|| !LinkVariable(*Wrist, TEXT("Rotation"), Pins.Wrist, 190)
			|| !LinkVariable(*Wrist, TEXT("Alpha"), Pins.Weight, 255)
			|| !LinkPose(Previous, *IK) || !LinkPose(*IK, *Wrist))
		{
			return nullptr;
		}
		return Wrist;
	}

	bool SaveAsset(UObject& Asset)
	{
		UPackage* Package = Asset.GetOutermost();
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Asset.MarkPackageDirty();
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		return UPackage::SavePackage(Package, &Asset, *Filename, Args);
	}
}
#endif

bool URobotActionTest::VerifyActionSlot()
{
#if WITH_EDITOR
	auto* Anim = LoadObject<UAnimBlueprint>(nullptr, AnimPath);
	auto* Machine = Anim ? FindRobotMachine(*Anim) : nullptr;
	if (!Machine || Anim->Status == BS_Error)
		return false;
	int32 Slots = 0, Arms = 0, ExistingBox = 0, IK = 0;
	const UAnimGraphNode_LayeredBoneBlend* ArmLayer = nullptr;
	const UAnimGraphNode_Slot* ActionSlot = nullptr;
	for (const UEdGraphNode* Node : Machine->GetGraph()->Nodes)
	{
		if (auto* Slot = Cast<UAnimGraphNode_Slot>(Node); Slot && Slot->Node.SlotName == TEXT("DefaultSlot"))
		{
			ActionSlot = Slot;
			++Slots;
		}
		if (auto* Layer = Cast<UAnimGraphNode_LayeredBoneBlend>(Node))
		{
			Arms += Layer->Node.LayerSetup.Num() == 2;
			if (Layer->Node.LayerSetup.Num() == 2)
				ArmLayer = Layer;
			ExistingBox += Layer->Node.LayerSetup.Num() == 1;
		}
		if (auto* Hand = Cast<UAnimGraphNode_TwoBoneIK>(Node); Hand && !Hand->Node.bAllowStretching)
			++IK;
	}
	if (Slots != 1 || Arms != 1 || ExistingBox != 1 || IK != 2 || !ArmLayer || !ActionSlot)
		return false;
	for (const TCHAR* PinName : {TEXT("BlendPoses_0"), TEXT("BlendPoses_1")})
	{
		const auto* Pin = ArmLayer->FindPin(PinName);
		if (!Pin || Pin->LinkedTo.Num() != 1)
			return false;
		const auto* Use = Cast<UAnimGraphNode_UseCachedPose>(Pin->LinkedTo[0]->GetOwningNode());
		const auto* Save = Use ? Use->SaveCachedPoseNode.Get() : nullptr;
		const auto* In = Save ? FirstPin(*Save, EGPD_Input) : nullptr;
		if (!In || In->LinkedTo.Num() != 1 || In->LinkedTo[0]->GetOwningNode() != ActionSlot)
			return false;
	}
	return true;
#else
	return false;
#endif
}

bool URobotActionTest::InstallActionSlot()
{
#if WITH_EDITOR
	if (VerifyActionSlot())
		return true;
	auto* Anim = LoadObject<UAnimBlueprint>(nullptr, AnimPath);
	auto* Machine = Anim ? FindRobotMachine(*Anim) : nullptr;
	const FName SlotName(TEXT("DefaultSlot"));
	if (!Machine || !Anim->TargetSkeleton || !Anim->TargetSkeleton->ContainsSlotName(SlotName))
		return Fail(TEXT("find an existing slot without editing the skeleton"));

	UEdGraph& Graph = *Machine->GetGraph();
	UAnimGraphNode_LayeredBoneBlend* Arms = nullptr;
	UAnimGraphNode_LayeredBoneBlend* Box = nullptr;
	UAnimGraphNode_Slot* ExistingSlot = nullptr;
	for (UEdGraphNode* Node : Graph.Nodes)
	{
		if (auto* Layer = Cast<UAnimGraphNode_LayeredBoneBlend>(Node); Layer && Layer->Node.LayerSetup.Num() == 2)
			Arms = Layer;
		if (auto* Layer = Cast<UAnimGraphNode_LayeredBoneBlend>(Node); Layer && Layer->Node.LayerSetup.Num() == 1)
			Box = Layer;
		if (auto* Slot = Cast<UAnimGraphNode_Slot>(Node); Slot && Slot->Node.SlotName == SlotName)
			ExistingSlot = Slot;
	}
	auto* Left = Arms ? Arms->FindPin(TEXT("BlendPoses_0")) : nullptr;
	auto* Right = Arms ? Arms->FindPin(TEXT("BlendPoses_1")) : nullptr;
	auto* BoxPose = Box ? Box->FindPin(TEXT("BlendPoses_0")) : nullptr;
	if (!Left || !Right || !BoxPose)
		return Fail(TEXT("find carry and two arm inputs"));
	UEdGraphPin* Reference = nullptr;
	for (UEdGraphPin* Pin : {Left, Right, BoxPose})
		if (Pin->LinkedTo.Num() == 1 && Cast<UAnimGraphNode_SequenceEvaluator>(Pin->LinkedTo[0]->GetOwningNode()))
			Reference = Pin->LinkedTo[0];
	if (!Reference)
		return Fail(TEXT("find the original carry reference clip"));
	Anim->Modify();
	Graph.Modify();
	auto* Slot = ExistingSlot;
	if (Slot)
	{
		Slot->Modify();
		UEdGraphPin* Input = FirstPin(*Slot, EGPD_Input);
		UEdGraphPin* Output = FirstPin(*Slot, EGPD_Output);
		if (Input->LinkedTo.Num() == 1 && Output->LinkedTo.Num() > 0)
		{
			UEdGraphPin* Previous = Input->LinkedTo[0];
			const TArray<UEdGraphPin*> Next = Output->LinkedTo;
			Input->BreakLinkTo(Previous);
			Output->BreakAllPinLinks();
			for (UEdGraphPin* Downstream : Next)
				if (!Link(Previous, Downstream))
					return Fail(TEXT("preserve the existing base-pose route"));
		}
		else if (!Input->LinkedTo.IsEmpty() || !Output->LinkedTo.IsEmpty())
			return Fail(TEXT("unexpected existing default slot wiring"));
	}
	else
	{
		FGraphNodeCreator<UAnimGraphNode_Slot> Creator(Graph);
		Slot = Creator.CreateNode(false);
		Slot->Node.SlotName = SlotName;
		Creator.Finalize();
	}
	Slot->NodePosX = Arms->NodePosX - 265;
	Slot->NodePosY = Arms->NodePosY - 185;
	Slot->NodeComment = TEXT("Interactions use the existing DefaultSlot and the selected arm masks.");
	Left->BreakAllPinLinks();
	Right->BreakAllPinLinks();
	BoxPose->BreakAllPinLinks();
	auto SavePose = [&](const TCHAR* Name, int32 X, int32 Y, UEdGraphPin* Input)
	{
		FGraphNodeCreator<UAnimGraphNode_SaveCachedPose> Creator(Graph);
		auto* Save = Creator.CreateNode(false);
		Save->CacheName = Name;
		Save->NodePosX = X;
		Save->NodePosY = Y;
		Creator.Finalize();
		return Link(Input, FirstPin(*Save, EGPD_Input)) ? Save : nullptr;
	};
	auto UsePose = [&](UAnimGraphNode_SaveCachedPose* Save, int32 X, int32 Y, UEdGraphPin* Output)
	{
		if (!Save)
			return false;
		FGraphNodeCreator<UAnimGraphNode_UseCachedPose> Creator(Graph);
		auto* Use = Creator.CreateNode(false);
		Use->SaveCachedPoseNode = Save;
		Use->NodePosX = X;
		Use->NodePosY = Y;
		Creator.Finalize();
		return Link(FirstPin(*Use, EGPD_Output), Output);
	};
	const int32 X = Arms->NodePosX;
	const int32 Y = Arms->NodePosY;
	auto* ReferenceCache = SavePose(TEXT("Carry Reference"), X - 410, Y - 380, Reference);
	if (!UsePose(ReferenceCache, Box->NodePosX - 220, Box->NodePosY - 70, BoxPose) ||
		!UsePose(ReferenceCache, X - 475, Y - 185, FirstPin(*Slot, EGPD_Input)))
		return Fail(TEXT("share the carry sample without fan-out pose wires"));
	auto* ActionCache = SavePose(TEXT("Interaction Arms"), X, Y - 185, FirstPin(*Slot, EGPD_Output));
	if (!UsePose(ActionCache, X - 240, Y + 45, Left) || !UsePose(ActionCache, X - 240, Y + 135, Right))
		return Fail(TEXT("feed the cached interaction pose to both arm masks"));

	FCompilerResultsLog Report;
	FKismetEditorUtilities::CompileBlueprint(Anim, EBlueprintCompileOptions::None, &Report);
	return Report.NumErrors == 0 && VerifyActionSlot() && SaveAsset(*Anim);
#else
	return false;
#endif
}

bool URobotActionTest::PrepareHotbar(ABotanicusCharacter* Pawn, bool bBook)
{
	if (!Pawn || !Pawn->HasAuthority() || !Pawn->GetWorld()->IsPlayInEditor())
		return false;
	UBotanicusQuickBarComponent& Bar = *Pawn->GetQuickBarComponent();
	Bar.ClearSlot(6);
	Bar.SetSlotItem(0, TEXT("SeedPacket_AureliaSweet"));
	const bool bItems = Bar.SetSlotItem(1, TEXT("SeedPacket_AureliaSweet"))
		&& Bar.SetSlotItem(2, TEXT("PottingSoil")) && Bar.SetSlotItem(3, TEXT("BoxCutter"))
		&& Bar.SetSlotItem(4, TEXT("MultiTool_L1")) && Bar.SetSlotItem(5, TEXT("WateringCan"))
		&& Bar.SetSlotItem(7, TEXT("GardenTrowel"));
	int32 Slot = INDEX_NONE;
	const bool bBookOk = !bBook || (Bar.AddUniqueItem(ABotanicusBotanistBookActor::BookKey(),
		FBotanicusCarriedItemState(), Slot, ABotanicusBotanistBookActor::BookId()) && Slot == 6);
	Bar.ClearSlot(0);
	return bItems && bBookOk;
}

void URobotActionTest::Select(ABotanicusCharacter* Pawn, int32 Slot)
{
	if (!Pawn || !Pawn->IsLocallyControlled())
		return;
	const TWeakObjectPtr<UBotanicusQuickBarComponent> Bar = Pawn->GetQuickBarComponent();
	Pawn->GetWorldTimerManager().SetTimerForNextTick([Bar, Slot]()
	{
		if (Bar.IsValid())
			Bar->SelectSlot(Slot);
	});
}

AActor* URobotActionTest::GetActiveItem(ABotanicusCharacter* Pawn)
{
	auto* Grip = Pawn ? Pawn->GetRobotHeldGrip() : nullptr;
	return Grip ? Grip->GetOwner() : nullptr;
}

int32 URobotActionTest::GetToolMode(ABotanicusCharacter* Pawn)
{
	const auto* Tool = Pawn ? Pawn->FindComponentByClass<UBotanicusMultiToolComponent>() : nullptr;
	return Tool ? static_cast<int32>(Tool->GetMode()) : -1;
}

FVector URobotActionTest::GetActionTarget(ABotanicusCharacter* Pawn, int32 Index)
{
	if (!Pawn)
		return FVector::ZeroVector;
	const auto Cue = Index == 0 ? Pawn->GetPendingRobotAction() : Pawn->GetRobotActionState(static_cast<EBotanicusRobotAction>(Index));
	return Cue.bHasTarget ? Cue.Target : FVector::ZeroVector;
}

ABotanicusPlantPotActor* URobotActionTest::SpawnPot(APlayerController* PC, FVector Position)
{
	if (!PC || !PC->HasAuthority() || !PC->GetWorld()->IsPlayInEditor())
		return nullptr;
	UClass* Class = LoadClass<ABotanicusPlantPotActor>(nullptr, TEXT("/Game/Botanicus/blueprints/BP_Item_PlantPot.BP_Item_PlantPot_C"));
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Pot = PC->GetWorld()->SpawnActor<ABotanicusPlantPotActor>(Class, Position, FRotator::ZeroRotator, Params);
	if (Pot)
	{
		Pot->InitializePlacedItem(TEXT("PlantPot"));
		Pot->RestoreGrowingState(false, NAME_None, 0, 0, 0);
	}
	return Pot;
}

ABotanicusGardeningRobot* URobotActionTest::SpawnGardener(APlayerController* PC, FVector Position)
{
	if (!PC || !PC->HasAuthority() || !PC->GetWorld()->IsPlayInEditor())
		return nullptr;
	if (auto* State = PC->GetWorld()->GetGameState<ABotanicusGameState>())
		State->UnlockElectricWhip();
	UClass* Class = LoadClass<ABotanicusGardeningRobot>(nullptr,
		TEXT("/Game/Botanicus/blueprints/BP_Item_GardeningRobot_L1.BP_Item_GardeningRobot_L1_C"));
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return PC->GetWorld()->SpawnActor<ABotanicusGardeningRobot>(Class, Position, FRotator::ZeroRotator, Params);
}

void URobotActionTest::Mature(ABotanicusPlantPotActor* Pot)
{
	if (Pot && Pot->HasAuthority() && Pot->GetWorld()->IsPlayInEditor())
		Pot->RestoreGrowingState(true, Pot->GetPlantKey().IsNone() ? FName(TEXT("AureliaSweet")) : Pot->GetPlantKey(), .4f, 1.f, 1.f);
}

FVector URobotActionTest::SoilPoint(ABotanicusPlantPotActor* Pot)
{
	return Pot ? Pot->GetSoilSurfaceForSlot(0) : FVector::ZeroVector;
}

float URobotActionTest::SoilLevel(ABotanicusPlantPotActor* Pot)
{
	const TArray<float> Levels = Pot ? Pot->GetSoilLevels() : TArray<float>();
	return Levels.IsEmpty() ? 0.f : Levels[0];
}

bool URobotActionTest::HasSoil(ABotanicusPlantPotActor* Pot)
{
	return Pot && Pot->HasSoil();
}

int32 URobotActionTest::Coverage(ABotanicusDeliveryParcelActor* Parcel)
{
	return Parcel ? Parcel->GetCutCoverageMask() : 0;
}

FString URobotActionTest::TapeTrace(APlayerController* PC, ABotanicusDeliveryParcelActor* Parcel)
{
	if (!PC || !Parcel)
		return FString();
	FVector Eye;
	FRotator View;
	FVector Ray;
	int32 W = 0, H = 0;
	PC->GetViewportSize(W, H);
	if (W <= 0 || H <= 0 || !PC->DeprojectScreenPositionToWorld(W * .5f, H * .5f, Eye, Ray))
	{
		PC->GetPlayerViewPoint(Eye, View);
		Ray = View.Vector();
	}
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(ActionTestTape), false, PC->GetPawn());
	PC->GetWorld()->LineTraceSingleByChannel(Hit, Eye, Eye + Ray.GetSafeNormal() * 700, ECC_Visibility, Query);
	return FString::Printf(TEXT("Aim=%s hit=%s local=%s tape=%d coverage=%d cut=%d"), *Ray.ToCompactString(),
		*GetNameSafe(Hit.GetActor()), *Parcel->GetActorTransform().InverseTransformPosition(Hit.ImpactPoint).ToCompactString(),
		Parcel->IsCuttingAreaHit(Hit), Parcel->GetCutCoverageMask(), Parcel->IsBeingCutBy(PC->GetPawn()));
}

bool URobotActionTest::IsCutting(ABotanicusDeliveryParcelActor* Parcel, APlayerController* PC)
{
	return Parcel && PC && Parcel->IsBeingCutBy(PC->GetPawn());
}

FName URobotActionTest::Planted(ABotanicusPlantPotActor* Pot)
{
	return Pot ? Pot->GetPlantKey() : NAME_None;
}

float URobotActionTest::WaterLevel(ABotanicusPlantPotActor* Pot)
{
	return Pot ? Pot->GetWaterLevel() : 0.f;
}

bool URobotActionTest::StageCheckout(APlayerController* PC, FVector Position)
{
	if (!PC || !PC->HasAuthority() || !PC->GetWorld()->IsPlayInEditor())
		return false;
	UWorld* World = PC->GetWorld();
	ABotanicusCashRegisterActor* Register = nullptr;
	for (TActorIterator<ABotanicusCashRegisterActor> It(World); It; ++It)
		if (!It->ActorHasTag(TEXT("BotanicusPlacementPreview")))
		{
			Register = *It;
			break;
		}
	if (!Register)
	{
		UClass* Class = LoadClass<ABotanicusCashRegisterActor>(nullptr,
			TEXT("/Game/Botanicus/blueprints/BP_Item_CashRegister.BP_Item_CashRegister_C"));
		Register = World->SpawnActor<ABotanicusCashRegisterActor>(Class, Position, FRotator::ZeroRotator);
	}
	if (!Register)
		return false;
	Register->SetActorLocationAndRotation(Position, FRotator::ZeroRotator);
	auto* Zone = World->SpawnActor<ABotanicusVisitorZoneActor>(Position, FRotator::ZeroRotator);
	if (!Zone)
		return false;
	Zone->InitializeZone(EBotanicusVisitorZoneType::Checkout, FVector(450, 350, 8));
	return Register->IsOperational();
}

void URobotActionTest::InteractPot(ABotanicusPlantPotActor* Pot, ABotanicusCharacter* Pawn, bool bStart)
{
	if (!Pot || !Pawn || !Pot->HasAuthority() || !Pot->GetWorld()->IsPlayInEditor())
		return;
	const TWeakObjectPtr<ABotanicusPlantPotActor> P = Pot;
	const TWeakObjectPtr<ABotanicusCharacter> C = Pawn;
	Pot->GetWorldTimerManager().SetTimerForNextTick([P, C, bStart]()
	{
		if (P.IsValid() && C.IsValid())
		{
			if (bStart)
				P->BeginPrimaryUse(C.Get());
			else
				P->EndPrimaryUse(C.Get());
		}
	});
}

void URobotActionTest::Read(ABotanicusBotanistBookActor* Book, ABotanicusCharacter* Pawn, bool bOpen)
{
	if (!Book || !Pawn || !Pawn->HasAuthority() || !Pawn->GetWorld()->IsPlayInEditor())
		return;
	auto* PC = Cast<ABotanicusPlayerController>(Pawn->GetController());
	if (!PC)
		return;
	if (bOpen)
		Book->BeginReading(*PC);
	else
		Book->RequestClose(*PC);
}

ABotanicusDeliveryParcelActor* URobotActionTest::SpawnTestParcel(APlayerController* Controller, bool bRectangular,
	FVector HalfExtent, FVector Location, FName Tag)
{
#if WITH_EDITOR
	if (!Controller || !Controller->HasAuthority() || !Controller->GetWorld()->IsPlayInEditor())
	{
		return nullptr;
	}
	UClass* Class = LoadClass<ABotanicusDeliveryParcelActor>(nullptr, bRectangular ? RectangularPath : SquarePath);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Parcel = Controller->GetWorld()->SpawnActor<ABotanicusDeliveryParcelActor>(Class, Location, FRotator::ZeroRotator, Params);
	if (Parcel)
	{
		Parcel->Tags.Add(Tag);
		Parcel->RestoreParcelState(TEXT("GardenTrowel"), 1, 0, false, {
			bRectangular ? EBotanicusParcelShape::Rectangular : EBotanicusParcelShape::Square, HalfExtent, 1.0f});
	}
	return Parcel;
#else
	return nullptr;
#endif
}

void URobotActionTest::QueueInput(APlayerController* Controller, FName Action, float Amount)
{
	if (!Controller || !Controller->IsLocalController())
	{
		return;
	}
	const TWeakObjectPtr<APlayerController> WeakController(Controller);
	Controller->GetWorldTimerManager().SetTimerForNextTick([WeakController, Action, Amount]()
	{
		APlayerController* PC = WeakController.Get();
		if (!PC)
		{
			return;
		}
		FKey Key;
		EInputEvent Event = IE_Pressed;
		if (Action == TEXT("E")) Key = EKeys::E;
		else if (Action == TEXT("A")) Key = EKeys::A;
		else if (Action == TEXT("RMB")) Key = EKeys::RightMouseButton;
		else if (Action == TEXT("RMBUp")) { Key = EKeys::RightMouseButton; Event = IE_Released; }
		else if (Action == TEXT("LMB")) Key = EKeys::LeftMouseButton;
		else if (Action == TEXT("LMBUp")) { Key = EKeys::LeftMouseButton; Event = IE_Released; }
		else if (Action == TEXT("Shift")) Key = EKeys::LeftShift;
		else if (Action == TEXT("ShiftUp")) { Key = EKeys::LeftShift; Event = IE_Released; }
		else if (Action == TEXT("MouseX")) { Key = EKeys::MouseX; Event = IE_Axis; }
		else if (Action == TEXT("MouseY")) { Key = EKeys::MouseY; Event = IE_Axis; }
		if (Key.IsValid())
		{
			PC->InputKey(FInputKeyEventArgs(nullptr, FInputDeviceId::CreateFromInternalId(0), Key, Event,
				Event == IE_Axis ? Amount : 1.0f, false, FPlatformTime::Cycles64()));
		}
	});
}

ABotanicusDeliveryParcelActor* URobotActionTest::GetTestCarriedParcel(ABotanicusCharacter* Character)
{
	return Character ? Character->GetCarriedParcelForAnimation() : nullptr;
}

bool URobotActionTest::IsTestPlacementPreview(ABotanicusDeliveryParcelActor* Parcel)
{
	return Parcel && Parcel->IsCarriedPlacementPreview();
}
