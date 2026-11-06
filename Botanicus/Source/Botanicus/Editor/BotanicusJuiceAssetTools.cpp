#include "Editor/BotanicusJuiceAssetTools.h"
#if WITH_EDITOR
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraMeshRendererProperties.h"
#include "EdGraph/EdGraphPin.h"
#include "NiagaraParameterStore.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraSystemFactoryNew.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraNodeAssignment.h"
#include "NiagaraSpriteRendererProperties.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"
#include "UObject/SavePackage.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#endif
FString UBotanicusJuiceAssetTools::InspectNiagara(const FString& AssetPath)
{
	FString Out;
#if WITH_EDITOR
	UObject* Object=LoadObject<UObject>(nullptr,*AssetPath);
	if (!Object) return TEXT("NOT_FOUND");
	TArray<FVersionedNiagaraEmitter> Emitters;
	if(auto* System=Cast<UNiagaraSystem>(Object))
	{
		TArray<FNiagaraVariable> Parameters; System->GetExposedParameters().GetParameters(Parameters);
		for(const auto& P:Parameters) Out+=FString::Printf(TEXT("USER %s %s\n"),*P.GetType().GetName(),*P.GetName().ToString());
		for(auto& Handle:System->GetEmitterHandles()) Emitters.Add(Handle.GetInstance());
	}
	if(auto* Emitter=Cast<UNiagaraEmitter>(Object)) Emitters.Add(FVersionedNiagaraEmitter(Emitter,Emitter->GetExposedVersion().VersionGuid));
	for(auto& Emitter:Emitters)
	{
		auto* Data=Emitter.GetEmitterData(); if(!Data) continue;
		Out+=FString::Printf(TEXT("EMITTER %s Sim=%d Local=%d\n"),*Emitter.Emitter->GetName(),static_cast<int32>(Data->SimTarget),Data->bLocalSpace);
		for(auto* Renderer:Data->GetRenderers())
		{
			Out+=TEXT("RENDERER ")+Renderer->GetClass()->GetName()+TEXT("\n");
			if(auto* Mesh=Cast<UNiagaraMeshRendererProperties>(Renderer)) for(auto& M:Mesh->Meshes) if(M.Mesh) Out+=TEXT("MESH ")+M.Mesh->GetPathName()+TEXT("\n");
		}
		if(auto* Source=Cast<UNiagaraScriptSource>(Data->GraphSource))
			for(UEdGraphNode* Node:Source->NodeGraph->Nodes) if(auto* Function=Cast<UNiagaraNodeFunctionCall>(Node))
			{
				Out+=FString::Printf(TEXT("FUNCTION %s %s\n"),*Function->GetFunctionName(),Function->FunctionScript ? *Function->FunctionScript->GetPathName() : TEXT("none"));
				for(auto* Pin:Function->Pins) Out+=FString::Printf(TEXT(" PIN %s = %s\n"),*Pin->PinName.ToString(),*Pin->DefaultValue);
			}
		for(auto* Script:{Data->EmitterSpawnScriptProps.Script.Get(),Data->EmitterUpdateScriptProps.Script.Get(),Data->SpawnScriptProps.Script.Get(),Data->UpdateScriptProps.Script.Get()})
		{
			if(!Script) continue;
			Out+=TEXT("SCRIPT ")+Script->GetName()+TEXT("\n");
			TArray<FNiagaraVariable> Parameters;Script->RapidIterationParameters.GetParameters(Parameters);
			for(auto& P:Parameters)
			{
				Out+=FString::Printf(TEXT(" RI %s %s = "),*P.GetType().GetName(),*P.GetName().ToString());
				if(auto* Bytes=Script->RapidIterationParameters.GetParameterData(P)) {for(int32 I=0;I<P.GetSizeInBytes()/4;++I) Out+=FString::SanitizeFloat(reinterpret_cast<const float*>(Bytes)[I])+TEXT(",");}
				Out+=TEXT("\n");
			}
		}
	}
#endif
	return Out;
}

#if WITH_EDITOR
namespace
{
void BindJuiceInput(UNiagaraNodeFunctionCall* Node,const TCHAR* Input,const FNiagaraVariable& User,const TSet<FNiagaraVariableBase>& Known)
{
	const auto Handle=FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(FNiagaraParameterHandle(FName(*FString::Printf(TEXT("Module.%s"),Input))),Node);
	auto& Pin=FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(*Node,Handle,User.GetType(),FGuid(),FGuid());
	FNiagaraStackGraphUtilities::SetLinkedParameterValueForFunctionInput(Pin,User,Known);
}
void JuiceLiteral(UNiagaraNodeFunctionCall* Node,const TCHAR* PinName,const TCHAR* Value)
{
	for(auto* Pin:Node->Pins) if(Pin->Direction==EGPD_Input && Pin->PinName==PinName) {Pin->DefaultValue=Value;Node->MarkNodeRequiresSynchronization(TEXT("Juice static switch"),true);return;}
}
bool BuildJuiceSprite(const FString& Name,bool bStream)
{
	const FString PackageName=TEXT("/Game/Botanicus/VFX/Juice/")+Name;
	// Re-running the tool must never overwrite an artist's work.
	if(FPackageName::DoesPackageExist(PackageName)) return LoadObject<UNiagaraSystem>(nullptr,*(PackageName+TEXT(".")+Name))!=nullptr;
	UPackage* Package=CreatePackage(*PackageName);
	auto* System=NewObject<UNiagaraSystem>(Package,*Name,RF_Public|RF_Standalone|RF_Transactional);
	UNiagaraSystemFactoryNew::InitializeSystem(System,true);
	auto* Template=LoadObject<UNiagaraEmitter>(nullptr,TEXT("/Niagara/DefaultAssets/Templates/Emitters/SimpleSpriteBurst.SimpleSpriteBurst"));
	if(!Template) return false;
	FNiagaraEditorUtilities::AddEmitterToSystem(*System,*Template,Template->GetExposedVersion().VersionGuid,true);
	auto Instance=System->GetEmitterHandles()[0].GetInstance();auto* Data=Instance.GetEmitterData();
	Data->SimTarget=ENiagaraSimTarget::CPUSim;Data->bLocalSpace=false;
	UNiagaraGraph* Graph=CastChecked<UNiagaraScriptSource>(Data->GraphSource)->NodeGraph;
	UNiagaraNodeOutput *Spawn=nullptr,*Update=nullptr,*EmitterUpdate=nullptr;
	for(UEdGraphNode* N:Graph->Nodes) if(auto* Output=Cast<UNiagaraNodeOutput>(N))
	{
		if(Output->GetUsage()==ENiagaraScriptUsage::ParticleSpawnScript) Spawn=Output;
		if(Output->GetUsage()==ENiagaraScriptUsage::ParticleUpdateScript) Update=Output;
		if(Output->GetUsage()==ENiagaraScriptUsage::EmitterUpdateScript) EmitterUpdate=Output;
	}
	if(!Spawn || !Update || !EmitterUpdate) return false;
	FNiagaraVariable Life(FNiagaraTypeDefinition::GetFloatDef(),TEXT("User.Lifetime"));
	FNiagaraVariable Size(FNiagaraTypeDefinition::GetFloatDef(),TEXT("User.Size"));
	FNiagaraVariable Color(FNiagaraTypeDefinition::GetColorDef(),TEXT("User.Tint"));
	FNiagaraVariable Velocity(FNiagaraTypeDefinition::GetVec3Def(),TEXT("User.Velocity"));
	FNiagaraVariable Gravity(FNiagaraTypeDefinition::GetVec3Def(),TEXT("User.Gravity"));
	FNiagaraVariable Count(FNiagaraTypeDefinition::GetIntDef(),TEXT("User.BurstCount"));
	FNiagaraVariable Rate(FNiagaraTypeDefinition::GetFloatDef(),TEXT("User.SpawnRate"));
	auto& Store=System->GetExposedParameters();
	Store.SetParameterValue(.4f,Life,true);Store.SetParameterValue(1.6f,Size,true);
	Store.SetParameterValue(FLinearColor(.15f,.65f,.85f,.8f),Color,true);
	Store.SetParameterValue(FVector3f(0,0,45),Velocity,true);Store.SetParameterValue(FVector3f(0,0,-180),Gravity,true);
	Store.SetParameterValue(bStream ? 0 : 8,Count,true);Store.SetParameterValue(bStream ? 65.f : 0.f,Rate,true);
	TSet<FNiagaraVariableBase> Known;for(const auto& Var:{Life,Size,Color,Velocity,Gravity,Count,Rate}) Known.Add(Var);
	TArray<UNiagaraNodeFunctionCall*> Modules;for(UEdGraphNode* N:Graph->Nodes) if(auto* F=Cast<UNiagaraNodeFunctionCall>(N)) Modules.Add(F);
	for(auto* Node:Modules)
	{
		const FString Function=Node->GetFunctionName();
		if(Function==TEXT("InitializeParticle")) {BindJuiceInput(Node,TEXT("Lifetime"),Life,Known);BindJuiceInput(Node,TEXT("Uniform Sprite Size"),Size,Known);BindJuiceInput(Node,TEXT("Color"),Color,Known);}
		if(Function==TEXT("SpawnBurst_Instantaneous")) BindJuiceInput(Node,TEXT("Spawn Count"),Count,Known);
		if(Function==TEXT("EmitterState")) JuiceLiteral(Node,TEXT("Loop Behavior"),bStream ? TEXT("NewEnumerator0") : TEXT("NewEnumerator1"));
	}
	// Set the initial velocity once; integration and fading remain in standard Niagara modules.
	auto* Assign=FNiagaraStackGraphUtilities::AddParameterModuleToStack({FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(),TEXT("Particles.Velocity"))},*Spawn,INDEX_NONE,{TEXT("0,0,45")});
	BindJuiceInput(Assign,TEXT("Particles.Velocity"),Velocity,Known);
	auto* GravityScript=LoadObject<UNiagaraScript>(nullptr,TEXT("/Niagara/Modules/Update/Forces/GravityForce.GravityForce"));
	if(!GravityScript) return false;
	auto* Force=FNiagaraStackGraphUtilities::AddScriptModuleToStack(GravityScript,*Update,0);
	BindJuiceInput(Force,TEXT("Gravity"),Gravity,Known);
	if(bStream)
	{
		auto* Script=LoadObject<UNiagaraScript>(nullptr,TEXT("/Niagara/Modules/Emitter/SpawnRate.SpawnRate"));
		auto* Node=FNiagaraStackGraphUtilities::AddScriptModuleToStack(Script,*EmitterUpdate,INDEX_NONE);
		BindJuiceInput(Node,TEXT("SpawnRate"),Rate,Known);
	}
	// A tiny spherical spawn region adds variation without collision, mesh renderers or a fluid grid.
	auto* ShapeScript=LoadObject<UNiagaraScript>(nullptr,TEXT("/Niagara/Modules/Spawn/Location/V2/ShapeLocation.ShapeLocation"));
	if(ShapeScript)
	{
		auto* Node=FNiagaraStackGraphUtilities::AddScriptModuleToStack(ShapeScript,*Spawn,INDEX_NONE);
		const FNiagaraVariable Radius(FNiagaraTypeDefinition::GetFloatDef(),TEXT("User.Radius"));
		Store.SetParameterValue(bStream ? .7f : 4.f,Radius,true);Known.Add(Radius);
		BindJuiceInput(Node,TEXT("Sphere Radius"),Radius,Known);
	}
	System->SetWarmupTime(0);System->SetFixedBounds(FBox(FVector(-800),FVector(800)));
	static_cast<UEdGraph*>(Graph)->NotifyGraphChanged();System->RequestCompile(true);System->WaitForCompilationComplete(false,false);
	if(!System->IsReadyToRun()) return false;
	FAssetRegistryModule::AssetCreated(System);Package->MarkPackageDirty();
	FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
	return UPackage::SavePackage(Package,System,*FPackageName::LongPackageNameToFilename(PackageName,FPackageName::GetAssetPackageExtension()),Args);
}
}
#endif
bool UBotanicusJuiceAssetTools::BuildSpriteSystems()
{
#if WITH_EDITOR
	return BuildJuiceSprite(TEXT("NS_BotanicusWaterDrops"),true) && BuildJuiceSprite(TEXT("NS_BotanicusJuiceBurst"),false);
#else
	return false;
#endif
}

bool UBotanicusJuiceAssetTools::BuildNoctifloreSpores(bool bResetPreviewDefaults)
{
#if WITH_EDITOR
    const FString Path=TEXT("/Game/Botanicus/VFX/Noctiflore/NS_NoctifloreSpores");
    auto* Existing=LoadObject<UNiagaraSystem>(nullptr,*(Path+TEXT(".NS_NoctifloreSpores")));
    if(Existing && !bResetPreviewDefaults) return true;
    auto* Original=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/Botanicus/VFX/Juice/NS_BotanicusJuiceBurst.NS_BotanicusJuiceBurst"));
    auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Botanicus/Materials/Juice/Noctiflore/M_NoctifloreSpore.M_NoctifloreSpore"));
    if(!Original || !Material) return false;
    auto* Package=CreatePackage(*Path);
    auto* System=Existing ? Existing : DuplicateObject<UNiagaraSystem>(Original,Package,TEXT("NS_NoctifloreSpores"));
    System->SetFlags(RF_Public|RF_Standalone);System->ClearFlags(RF_Transient);
    for(auto& Handle:System->GetEmitterHandles()) if(auto* Data=Handle.GetInstance().GetEmitterData())
        for(auto* Renderer:Data->GetRenderers()) if(auto* Sprite=Cast<UNiagaraSpriteRendererProperties>(Renderer)) Sprite->Material=Material;
    // Match the calm in-game defaults when previewing this new asset in Niagara Editor.
    // Existing artist settings are preserved unless reset was explicitly requested.
    auto& Store=System->GetExposedParameters();
    Store.SetParameterValue(3,FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(),TEXT("User.BurstCount")),true);
    Store.SetParameterValue(3.2f,FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(),TEXT("User.Lifetime")),true);
    Store.SetParameterValue(1.f,FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(),TEXT("User.Size")),true);
    Store.SetParameterValue(12.f,FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(),TEXT("User.Radius")),true);
    Store.SetParameterValue(FLinearColor(.40f,.34f,.68f,.40f),FNiagaraVariable(FNiagaraTypeDefinition::GetColorDef(),TEXT("User.Tint")),true);
    Store.SetParameterValue(FVector3f(0,0,4),FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(),TEXT("User.Velocity")),true);
    Store.SetParameterValue(FVector3f::ZeroVector,FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(),TEXT("User.Gravity")),true);
    System->SetFixedBounds(FBox(FVector(-150),FVector(150)));System->SetWarmupTime(0);
    System->RequestCompile(true);System->WaitForCompilationComplete(false,false);
    if(!System->IsReadyToRun()) return false;
    if(!Existing) FAssetRegistryModule::AssetCreated(System);Package->MarkPackageDirty();
    FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
    return UPackage::SavePackage(Package,System,*FPackageName::LongPackageNameToFilename(Path,FPackageName::GetAssetPackageExtension()),Args);
#else
    return false;
#endif
}
