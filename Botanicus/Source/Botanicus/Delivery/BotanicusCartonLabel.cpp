#include "Delivery/BotanicusCartonLabel.h"

#include "Catalog/BotanicusItemCatalogSubsystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/App.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RenderingThread.h"
#include "Slate/WidgetRenderer.h"
#include "Styling/CoreStyle.h"
#include "UObject/ConstructorHelpers.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Text/STextBlock.h"
#if WITH_EDITOR
#include "TextureCompiler.h"
#endif

namespace
{
	const FVector2D LabelPixels(1200, 1800);
	const FLinearColor Ink = FLinearColor::FromSRGBColor(FColor(44, 30, 18));
	const FLinearColor Paper = FLinearColor::FromSRGBColor(FColor(246, 236, 220));
	const FText Missing = NSLOCTEXT("CartonLabel", "Missing", "—");
	const FName LabelTexture(TEXT("LabelTexture"));
	const FName LabelOpacity(TEXT("LabelOpacity"));

	// ISO/IEC 15417 symbol widths in codeword order, starting with a black bar.
	constexpr const char* Code128Widths[] = {
		"212222","222122","222221","121223","121322","131222","122213","122312","132212","221213",
		"221312","231212","112232","122132","122231","113222","123122","123221","223211","221132",
		"221231","213212","223112","312131","311222","321122","321221","312212","322112","322211",
		"212123","212321","232121","111323","131123","131321","112313","132113","132311","211313",
		"231113","231311","112133","112331","132131","113123","113321","133121","313121","211331",
		"231131","213113","213311","213131","311123","311321","331121","312113","312311","332111",
		"314111","221411","431111","111224","111422","121124","121421","141122","141221","112214",
		"112412","122114","122411","142112","142211","241211","221114","413111","241112","134111",
		"111242","121142","121241","114212","124112","124211","411212","421112","421211","212141",
		"214121","412121","111143","111341","131141","114113","114311","411113","411311","113141",
		"114131","311141","411131","211412","211214","211232","2331112"
	};

	TArray<int32> EncodeCode128B(const FString& Value)
	{
		if (Value.IsEmpty() || Value.Len() > 39)
			return {};

		constexpr int32 StartB = 104;
		int32 Checksum = StartB;
		TArray<int32> Symbols{StartB};
		for (int32 Index = 0; Index < Value.Len(); ++Index)
		{
			const int32 Character = Value[Index];
			if (Character < 32 || Character > 126)
				return {};
			const int32 Codeword = Character - 32;
			Symbols.Add(Codeword);
			Checksum += (Index + 1) * Codeword;
		}
		Symbols.Add(Checksum % 103);
		Symbols.Add(106);
		return Symbols;
	}

	// Own the brushes for as long as Slate uses them during the offscreen draw.
	struct FLabelContent
	{
		UTexture2D* Icon;
		const FText& Category;
		const FText& ItemName;
		const FString& Reference;
		const FString& NCCode;
		const FString& Barcode;
		const FText& Instructions;
		int32 Quantity;
	};

	// Own the brushes for as long as Slate uses them during the offscreen draw.
	class SCartonLabel : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SCartonLabel) {}
		SLATE_END_ARGS()

		void Construct(const FArguments&, const UBotanicusCartonLabelCatalog& Catalog,
			const FLabelContent& Content)
		{
			Canvas = SNew(SConstraintCanvas);
			ChildSlot[Canvas.ToSharedRef()];
			AddImage(BackgroundBrush, Catalog.Background, 0, 0, LabelPixels.X, LabelPixels.Y);
			AddImage(LogoBrush, Catalog.Logo, 105, 117, 388, 230);
			if (!Catalog.Logo)
				AddText(FText::AsCultureInvariant(TEXT("Botanicus")), 106, 149, 445, 112, 488, 65, Ink);
			AddImage(IconBrush, Content.Icon, 121, 490, 260, 234);
			AddText(Content.Category, 491, 533, 615, 67, 550, 48, Paper);
			AddText(Content.ItemName, 491, 620, 615, 65, 540, 39, Paper);
			AddText(Content.Reference.IsEmpty() ? Missing : FText::AsCultureInvariant(Content.Reference), 360, 862, 226, 68, 220, 30, Ink);
			AddText(Content.NCCode.IsEmpty() ? Missing : FText::AsCultureInvariant(Content.NCCode), 360, 994, 226, 68, 220, 32, Ink);
			AddBarcode(Content.Barcode);
			if (Content.Quantity > 0)
				AddText(FText::Format(NSLOCTEXT("CartonLabel", "Quantity", "Quantité : {0}"), Content.Quantity),
					770, 1217, 342, 45, 335, 29, Ink);
			AddText(Content.Instructions.IsEmpty() ? Missing : Content.Instructions, 115, 1280, 970, 326, 940, 42, Ink, true);
		}

	private:
		void AddImage(FSlateBrush& Brush, UTexture2D* Texture, float X, float Y, float W, float H)
		{
			if (!Texture)
				return;
			Brush.SetResourceObject(Texture);
			const double Ratio = static_cast<double>(Texture->GetSizeX()) / FMath::Max(1, Texture->GetSizeY());
			const double Width = FMath::Min<double>(W, H * Ratio);
			const double Height = FMath::Min<double>(H, W / FMath::Max(Ratio, UE_SMALL_NUMBER));
			Brush.ImageSize = FVector2D(Width, Height);
			Brush.DrawAs = ESlateBrushDrawType::Image;
			Canvas->AddSlot().Alignment(FVector2D::ZeroVector)
				.Offset(FMargin(X + (W - Width) * .5, Y + (H - Height) * .5, Width, Height))
				[SNew(SImage).Image(&Brush)];
		}

		void AddText(const FText& Value, float X, float Y, float W, float H, float Wrap, int32 FontSize,
			const FLinearColor& Tint, bool bInstructions = false)
		{
			Canvas->AddSlot().Alignment(FVector2D::ZeroVector).Offset(FMargin(X, Y, W, H))
			[
				SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
				.HAlign(bInstructions ? HAlign_Left : HAlign_Center).VAlign(bInstructions ? VAlign_Top : VAlign_Center)
				[
					SNew(STextBlock).Text(Value).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), FontSize))
					.ColorAndOpacity(Tint).WrapTextAt(bInstructions ? Wrap : 0.f)
				]
			];
		}

		void AddBarcode(const FString& Value)
		{
			const TArray<int32> Symbols = EncodeCode128B(Value);
			if (Symbols.IsEmpty())
				return;
			constexpr int32 Width = 490;
			// Start, data, checksum, 13 modules of stop, and 10 white modules on either side.
			const int32 Modules = (Symbols.Num() - 1) * 11 + 13 + 20;
			const int32 ModulePixels = Width / Modules;
			const int32 Left = 644 + (Width - Modules * ModulePixels) / 2;
			const auto* WhiteBrush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
			Canvas->AddSlot().ZOrder(1).Alignment(FVector2D::ZeroVector).Offset(FMargin(644, 829, Width, 187))
			[
				SNew(SImage).Image(WhiteBrush).ColorAndOpacity(Paper)
			];
			int32 X = Left + 10 * ModulePixels;
			for (const int32 Symbol : Symbols)
			{
				bool bBlack = true;
				for (const char* Run = Code128Widths[Symbol]; *Run; ++Run)
				{
					const int32 Pixels = ModulePixels * (*Run - '0');
					if (bBlack)
					{
						Canvas->AddSlot().ZOrder(2).Alignment(FVector2D::ZeroVector).Offset(FMargin(X, 859, Pixels, 133))
						[
							SNew(SImage).Image(WhiteBrush).ColorAndOpacity(FLinearColor::Black)
						];
					}
					X += Pixels;
					bBlack = !bBlack;
				}
			}
			AddText(FText::AsCultureInvariant(Value), 676, 1044, 435, 41, 427, 23, Paper);
		}

		TSharedPtr<SConstraintCanvas> Canvas;
		FSlateBrush BackgroundBrush;
		FSlateBrush LogoBrush;
		FSlateBrush IconBrush;
	};

	// Length prefixes avoid ambiguity when user-authored values contain separators.
	FString MakeCacheKey(std::initializer_list<FString> Values)
	{
		FString Key;
		for (const FString& Value : Values)
			Key += FString::FromInt(Value.Len()) + TEXT(":") + Value;
		return Key;
	}
}

UBotanicusCartonLabelCatalog* UBotanicusCartonLabelCatalog::Get()
{
	static const TSoftObjectPtr<UBotanicusCartonLabelCatalog> Asset(FSoftObjectPath(
		TEXT("/Game/Botanicus/Items/itemsMesh/carton/carton2/Labels/DA_BotanicusCartonLabelCatalog.DA_BotanicusCartonLabelCatalog")));
	return Asset.LoadSynchronous();
}

bool UBotanicusCartonLabelSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE ||
		WorldType == EWorldType::Editor || WorldType == EWorldType::EditorPreview;
}

void UBotanicusCartonLabelSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (FApp::CanEverRender() && InWorld.GetNetMode() != NM_DedicatedServer)
		PreloadedCatalog = UBotanicusCartonLabelCatalog::Get();
}

void UBotanicusCartonLabelSubsystem::ConfigureSurface(UStaticMeshComponent* Surface)
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
	Surface->SetStaticMesh(Plane.Object);
	Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Surface->SetGenerateOverlapEvents(false);
	Surface->SetCanEverAffectNavigation(false);
	Surface->SetCastShadow(false);
	Surface->SetVisibility(false);
}

void UBotanicusCartonLabelSubsystem::SuppressImportedLabel(USkeletalMeshComponent* Carton)
{
	const int32 Slot = Carton->GetMaterialIndex(TEXT("etiquette"));
	const auto* Catalog = Slot != INDEX_NONE ? UBotanicusCartonLabelCatalog::Get() : nullptr;
	if (Catalog && Catalog->SurfaceMaterial)
		Carton->SetMaterial(Slot, Catalog->SurfaceMaterial);
}

void UBotanicusCartonLabelSubsystem::Apply(UStaticMeshComponent* Surface, FName ItemId, int32 Quantity)
{
	auto* World = Surface->GetWorld();
	auto* Renderer = World && FSlateApplication::IsInitialized() && FApp::CanEverRender() && !ItemId.IsNone() &&
		World->GetNetMode() != NM_DedicatedServer ? World->GetSubsystem<UBotanicusCartonLabelSubsystem>() : nullptr;
	auto* Material = Renderer ? Renderer->Resolve(ItemId, Quantity) : nullptr;
	Surface->SetMaterial(0, Material);
	Surface->SetVisibility(Material != nullptr);
}

UMaterialInstanceDynamic* UBotanicusCartonLabelSubsystem::Resolve(FName ItemId, int32 Quantity)
{
	const auto* Catalog = PreloadedCatalog ? PreloadedCatalog.Get() : UBotanicusCartonLabelCatalog::Get();
	if (!Catalog || !Catalog->Background || !Catalog->SurfaceMaterial)
		return nullptr;

	const auto* ItemCatalog = GetWorld()->GetGameInstance() ?
		GetWorld()->GetGameInstance()->GetSubsystem<UBotanicusItemCatalogSubsystem>() : nullptr;
	const FBotanicusItemDefinition* Item = ItemCatalog ? ItemCatalog->FindItem(ItemId) : nullptr;
	if (!ItemCatalog)
	{
		const auto* BaseCatalog = GetDefault<UBotanicusItemCatalogSettings>()->Catalog.LoadSynchronous();
		Item = BaseCatalog ? BaseCatalog->FindItem(ItemId) : nullptr;
	}
	const auto* Entry = Catalog->Items.Find(ItemId);
	FName CategoryId = Entry ? Entry->Category : NAME_None;
	if (CategoryId.IsNone())
	{
		const FString Key = ItemId.ToString();
		for (const auto& Rule : Catalog->CategoryRules)
		{
			if (!Rule.ItemIdPrefix.IsEmpty() && Key.StartsWith(Rule.ItemIdPrefix) &&
				(Rule.PlantTypeTag.IsNone() || (Item && Item->PlantTypeTag == Rule.PlantTypeTag)))
			{
				CategoryId = Rule.Category;
				break;
			}
		}
	}
	const auto* Category = Catalog->Categories.Find(CategoryId);
	UTexture2D* Icon = Entry && Entry->CategoryIcon ? Entry->CategoryIcon.Get() : (Category ? Category->CategoryIcon.Get() : nullptr);
	if (!Icon && Item)
		Icon = Item->Icon.LoadSynchronous();
	const FText CategoryName = Category ? Category->DisplayName : Missing;
	const FText ItemName = Item && !Item->DisplayName.IsEmpty() ? Item->DisplayName : FText::FromName(ItemId);
	const FString Reference = Entry && !Entry->ItemReference.IsEmpty() ? Entry->ItemReference : ItemId.ToString();
	const FString NCCode = Entry ? Entry->NCCode : FString();
	const FString Barcode = Entry && !Entry->BarcodeValue.IsEmpty() ? Entry->BarcodeValue : Reference;
	const FText Instructions = Entry ? Entry->HandlingInstructions : FText();
	const FString Key = MakeCacheKey({CategoryName.ToString(), ItemName.ToString(), Reference, NCCode, Barcode,
		Instructions.ToString(), GetPathNameSafe(Icon), GetPathNameSafe(Catalog->Background),
		GetPathNameSafe(Catalog->Logo), GetPathNameSafe(Catalog->SurfaceMaterial), FString::FromInt(Quantity)});
	if (auto* Existing = Materials.Find(Key))
		if (auto* Material = Existing->Get())
			return Material;

#if WITH_EDITOR
	// PIE and the Blueprint viewport can run while newly imported textures are still compiling.
	TArray<UTexture*, TInlineAllocator<3>> PendingTextures;
	for (UTexture2D* Texture : {Catalog->Background.Get(), Catalog->Logo.Get(), Icon})
		if (Texture && Texture->IsCompiling())
			PendingTextures.AddUnique(Texture);
	if (!PendingTextures.IsEmpty())
		FTextureCompilingManager::Get().FinishCompilation(PendingTextures);
#endif
	UTextureRenderTarget2D* Target = FWidgetRenderer::CreateTargetFor(LabelPixels, TF_Bilinear, false);
	auto* Renderer = new FWidgetRenderer(false);
	const FLabelContent Content{Icon, CategoryName, ItemName, Reference, NCCode, Barcode, Instructions, Quantity};
	Renderer->DrawWidget(Target, SNew(SCartonLabel, *Catalog, Content), LabelPixels, 0.f);
	BeginCleanup(Renderer);
	auto* Material = UMaterialInstanceDynamic::Create(Catalog->SurfaceMaterial, this);
	Material->SetTextureParameterValue(LabelTexture, Target);
	Material->SetScalarParameterValue(LabelOpacity, 1.f);
	for (auto It = Materials.CreateIterator(); It; ++It)
		if (!It.Value().IsValid())
			It.RemoveCurrent();
	Materials.Add(Key, Material);
	return Material;
}
