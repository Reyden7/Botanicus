#include "UI/BotanicusRadialEmoteMenu.h"

#include "Blueprint/WidgetTree.h"
#include "BotanicusPlayerController.h"
#include "Components/CanvasPanel.h"
#include "Engine/Texture2D.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Presentation/BotanicusEmoteCatalog.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "BotanicusRadialEmoteMenu"

namespace
{
	constexpr float NavDistance = 34.f;
	constexpr float NavOffset = 79.f;
	constexpr float NavHalfWidth = 21.f;
	constexpr float NavHalfHeight = 17.f;
	constexpr ESlateDrawEffect NoEffects = ESlateDrawEffect::None;

	FVector2f Polar(float Angle, float Length)
	{
		return FVector2f(FMath::Sin(Angle), -FMath::Cos(Angle)) * Length;
	}

	FLinearColor WithOpacity(FLinearColor Color, float Opacity)
	{
		Color.A *= Opacity;
		return Color;
	}
}

void UBotanicusRadialEmoteMenu::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree && !WidgetTree->RootWidget)
		WidgetTree->RootWidget = WidgetTree->ConstructWidget<UCanvasPanel>();
	SetIsFocusable(true);
	SetVisibility(ESlateVisibility::Collapsed);
}

bool UBotanicusRadialEmoteMenu::Open(const UBotanicusEmoteCatalog& Catalog, const USkeleton* Skeleton)
{
	if (MenuCatalog != &Catalog || Pages.IsEmpty())
	{
		MenuCatalog = &Catalog;
		BuildPages(Skeleton);
	}
	if (Pages.IsEmpty())
		return false;
	bMenuOpen = true;
	SetHovered(INDEX_NONE);
	SetVisibility(ESlateVisibility::Visible);
	return true;
}

void UBotanicusRadialEmoteMenu::Close()
{
	bMenuOpen = false;
	SetHovered(INDEX_NONE);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UBotanicusRadialEmoteMenu::BuildPages(const USkeleton* Skeleton)
{
	Pages.Reset();
	PageIndex = 0;
	TSet<FName> UniqueEmotes;
	TArray<FName> Categories;
	TMap<FName, FText> Labels;
	TMap<FName, TArray<int32>> Groups;
	for (const auto& Category : MenuCatalog->Categories)
	{
		Categories.AddUnique(Category.Id);
		Labels.Add(Category.Id, Category.DisplayName);
	}
	for (int32 Index = 0; Index < MenuCatalog->Emotes.Num(); ++Index)
	{
		const auto& Entry = MenuCatalog->Emotes[Index];
		if (UniqueEmotes.Contains(Entry.EmoteId))
			continue;
		UniqueEmotes.Add(Entry.EmoteId);
		if (!Entry.IsPlayable(Skeleton))
			continue;
		Categories.AddUnique(Entry.Category);
		Groups.FindOrAdd(Entry.Category).Add(Index);
	}
	const int32 Limit = FMath::Clamp(MaxSectorsPerPage, 1, 8);
	for (const FName Category : Categories)
	{
		TArray<int32>* Group = Groups.Find(Category);
		if (!Group)
			continue;
		Group->StableSort([this](int32 A, int32 B)
		{
			return MenuCatalog->Emotes[A].SortOrder < MenuCatalog->Emotes[B].SortOrder;
		});
		const FText* Label = Labels.Find(Category);
		for (int32 First = 0; First < Group->Num(); First += Limit)
		{
			auto& Page = Pages.AddDefaulted_GetRef();
			Page.Category = Label && !Label->IsEmpty() ? *Label :
				(Category.IsNone() ? LOCTEXT("OtherEmotes", "Emotes") : FText::FromName(Category));
			Page.Entries.Append(Group->GetData() + First, FMath::Min(Limit, Group->Num() - First));
		}
	}
	RebuildSectors();
}

void UBotanicusRadialEmoteMenu::RebuildSectors()
{
	Sectors.Reset();
	const int32 Count = Pages.IsValidIndex(PageIndex) ? Pages[PageIndex].Entries.Num() : 0;
	HoverWeights.Init(0.f, Count);
	if (Count == 0)
		return;

	const float Step = 2.f * PI / Count;
	const float Inner = FMath::Min(DeadZoneRadius, Radius - 44.f);
	const int32 Slices = FMath::Max(12, 80 / Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FSector& Sector = Sectors.AddDefaulted_GetRef();
		const float Angle = Step * Index;
		Sector.Direction = Polar(Angle, 1.f);
		const float Start = Angle - Step * .5f + .017f;
		const float End = Angle + Step * .5f - .017f;
		for (int32 Point = 0; Point <= Slices; ++Point)
		{
			const float T = static_cast<float>(Point) / Slices;
			Sector.Outline.Add(Polar(FMath::Lerp(Start, End, T), Radius));
		}
		for (int32 Point = Slices; Point >= 0; --Point)
			Sector.Outline.Add(Polar(FMath::Lerp(Start, End, static_cast<float>(Point) / Slices), Inner));
		for (int32 Point = 0; Point < Slices; ++Point)
		{
			const SlateIndex O = static_cast<SlateIndex>(Point);
			const SlateIndex I = static_cast<SlateIndex>(2 * Slices + 1 - Point);
			Sector.Indices.Append({O, static_cast<SlateIndex>(O + 1), I,
				I, static_cast<SlateIndex>(O + 1), static_cast<SlateIndex>(I - 1)});
		}
	}
}

void UBotanicusRadialEmoteMenu::ChangePage(int32 Direction)
{
	if (Pages.Num() > 1)
	{
		PageIndex = (PageIndex + Direction + Pages.Num()) % Pages.Num();
		SetHovered(INDEX_NONE);
		PageFade = 0.f;
		RebuildSectors();
	}
}

void UBotanicusRadialEmoteMenu::SetHovered(int32 Index)
{
	Hovered = Index;
}

float UBotanicusRadialEmoteMenu::GetLayoutScale(const FGeometry& Geometry) const
{
	const FVector2D Size = Geometry.GetLocalSize();
	return FMath::Clamp(static_cast<float>(FMath::Min(Size.X, Size.Y) / (2.f * (Radius + 66.f))), .1f, 1.f);
}

int32 UBotanicusRadialEmoteMenu::HitSector(FVector2f Offset, float Scale) const
{
	const float Length = Offset.Size() / Scale;
	const float Inner = FMath::Min(DeadZoneRadius, Radius - 44.f);
	if (Length <= Inner || Length > Radius || Sectors.IsEmpty())
		return INDEX_NONE;

	const float Angle = FMath::Atan2(Offset.X, -Offset.Y);
	const float Step = 2.f * PI / Sectors.Num();
	const int32 Nearest = FMath::FloorToInt((Angle + 2.f * PI + Step * .5f) / Step) % Sectors.Num();
	const float Relative = FMath::UnwindRadians(Angle - Nearest * Step);
	return FMath::Abs(Relative) <= Step * .5f - .017f ? Nearest : INDEX_NONE;
}

void UBotanicusRadialEmoteMenu::NativeTick(const FGeometry& Geometry, float DeltaSeconds)
{
	Super::NativeTick(Geometry, DeltaSeconds);
	const float Duration = bMenuOpen ? OpenDuration : CloseDuration;
	Fade = FMath::FInterpConstantTo(Fade, bMenuOpen ? 1.f : 0.f, DeltaSeconds, 1.f / FMath::Max(.001f, Duration));
	if (!bMenuOpen && Fade <= 0.f)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	PageFade = FMath::FInterpConstantTo(PageFade, 1.f, DeltaSeconds, 1.f / FMath::Max(.001f, PageDuration));
	for (int32 Index = 0; Index < HoverWeights.Num(); ++Index)
		HoverWeights[Index] = FMath::FInterpConstantTo(HoverWeights[Index], Index == Hovered ? 1.f : 0.f,
			DeltaSeconds, 1.f / FMath::Max(.001f, HoverDuration));
	if (const TSharedPtr<SWidget> Widget = GetCachedWidget())
		Widget->Invalidate(EInvalidateWidgetReason::Paint);
}

FReply UBotanicusRadialEmoteMenu::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (bMenuOpen && Event.GetKey() == EKeys::Escape)
	{
		if (auto* PC = GetOwningPlayer<ABotanicusPlayerController>())
			PC->CloseRadialEmoteMenu();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(Geometry, Event);
}

FReply UBotanicusRadialEmoteMenu::NativeOnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (bMenuOpen)
		SetHovered(HitSector(FVector2f(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()) -
			Geometry.GetLocalSize() * .5), GetLayoutScale(Geometry)));
	return bMenuOpen ? FReply::Handled() : FReply::Unhandled();
}

FReply UBotanicusRadialEmoteMenu::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (!bMenuOpen)
		return FReply::Unhandled();

	if (Event.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		const FVector2f Offset(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()) - Geometry.GetLocalSize() * .5);
		const float Scale = GetLayoutScale(Geometry);
		const FVector2f Point = Offset / Scale;
		if (FMath::Abs(Point.Y - (Radius + NavDistance)) <= NavHalfHeight &&
			FMath::Abs(FMath::Abs(Point.X) - NavOffset) <= NavHalfWidth)
			ChangePage(Point.X < 0.f ? -1 : 1);
		else if (const int32 Index = HitSector(Offset, Scale); Index != INDEX_NONE && Pages.IsValidIndex(PageIndex))
		{
			if (auto* PC = GetOwningPlayer<ABotanicusPlayerController>())
				PC->SelectRadialRobotEmote(MenuCatalog->Emotes[Pages[PageIndex].Entries[Index]].EmoteId);
		}
	}
	return FReply::Handled();
}

FReply UBotanicusRadialEmoteMenu::NativeOnMouseButtonUp(const FGeometry&, const FPointerEvent&)
{
	return bMenuOpen ? FReply::Handled() : FReply::Unhandled();
}

FReply UBotanicusRadialEmoteMenu::NativeOnMouseWheel(const FGeometry&, const FPointerEvent& Event)
{
	if (!bMenuOpen)
		return FReply::Unhandled();
	if (!FMath::IsNearlyZero(Event.GetWheelDelta()))
		ChangePage(Event.GetWheelDelta() < 0.f ? 1 : -1);
	return FReply::Handled();
}

int32 UBotanicusRadialEmoteMenu::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
	FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	Layer = Super::NativePaint(Args, Geometry, CullingRect, Elements, Layer, Style, bParentEnabled);
	if (Fade <= 0.f || Pages.IsEmpty())
		return Layer;

	const auto& Renderer = *FSlateApplication::Get().GetRenderer();
	const auto FontMeasure = Renderer.GetFontMeasureService();
	const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
	const auto Material = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*White);
	const FVector2f Center(Geometry.GetLocalSize() * .5);
	const float Alpha = FMath::SmoothStep(0.f, 1.f, Fade);
	const float Scale = GetLayoutScale(Geometry) * FMath::Lerp(.95f, 1.f, Alpha);
	const float SectorAlpha = Alpha * FMath::Lerp(.44f, 1.f, PageFade);
	const auto PaintGeometry = [&](FVector2f Position, FVector2f Size)
	{
		return Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Center + Position));
	};
	const auto Text = [&](const FText& Label, FVector2f Position, float Size, FLinearColor Color, bool bBold)
	{
		const auto Font = FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", FMath::Max(8, FMath::RoundToInt(Size * Scale)));
		const FVector2f Dimensions(FontMeasure->Measure(Label, Font));
		FSlateDrawElement::MakeText(Elements, Layer + 5, PaintGeometry(Position * Scale - Dimensions * .5f, Dimensions),
			Label, Font, NoEffects, WithOpacity(Color, Alpha));
	};
	const auto DrawShape = [&](const TArray<FVector2f>& Outline, const TArray<SlateIndex>& Indices, FLinearColor Fill,
		FLinearColor Edge, float Opacity, FVector2f Offset, float Size)
	{
		TArray<FSlateVertex> Vertices;
		TArray<FVector2f> Line;
		Vertices.Reserve(Outline.Num());
		Line.Reserve(Outline.Num() + 1);
		for (const FVector2f Point : Outline)
		{
			const FVector2f Position = Center + (Point * Size + Offset) * Scale;
			Line.Add(Position);
			Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Geometry.GetAccumulatedRenderTransform(),
				Position, FVector2f(.5f, .5f), WithOpacity(Fill, Opacity).ToFColorSRGB()));
		}
		Line.Add(FVector2f(Line[0]));
		FSlateDrawElement::MakeCustomVerts(Elements, Layer + 1, Material, Vertices, Indices, nullptr, 0, 0);
		FSlateDrawElement::MakeLines(Elements, Layer + 2, Geometry.ToPaintGeometry(), MoveTemp(Line), NoEffects,
			WithOpacity(Edge, Opacity), true, 1.4f);
	};

	FSlateDrawElement::MakeBox(Elements, Layer, Geometry.ToPaintGeometry(), White, NoEffects,
		FLinearColor(.006f, .013f, .009f, Alpha * .29f));
	const FPage& Page = Pages[PageIndex];
	for (int32 Index = 0; Index < Sectors.Num(); ++Index)
	{
		const FSector& Sector = Sectors[Index];
		const float Selection = FMath::SmoothStep(0.f, 1.f, HoverWeights[Index]);
		const float Enlargement = FMath::Lerp(1.f, HoverScale, Selection);
		const float Inset = FMath::Min(DeadZoneRadius, Radius - 44.f);
		const float Mid = (Radius + Inset) * .5f;
		const auto& Entry = MenuCatalog->Emotes[Page.Entries[Index]];
		DrawShape(Sector.Outline, Sector.Indices, FMath::Lerp(Index % 2 ? AlternateSectorColor : SectorColor,
			SelectedColor, Selection), FMath::Lerp(BorderColor, TextColor, Selection), SectorAlpha,
			Sector.Direction * Mid * (1.f - Enlargement), Enlargement);
		const FVector2f Position = Sector.Direction * Mid;
		if (Entry.Icon)
		{
			FSlateBrush Brush;
			Brush.SetResourceObject(Entry.Icon);
			const FVector2f Size(IconSize * Scale * Enlargement);
			FSlateDrawElement::MakeBox(Elements, Layer + 4,
				PaintGeometry((Position - FVector2f(0, 10)) * Scale - Size * .5f, Size), &Brush, NoEffects,
				FLinearColor(1, 1, 1, SectorAlpha));
		}
		else
		{
			const FText Icon = Entry.PlaceholderGlyph.IsEmpty() ? LOCTEXT("FallbackEmblem", "✦") : Entry.PlaceholderGlyph;
			Text(Icon, Position - FVector2f(0, 12), IconSize * .83f * Enlargement, WithOpacity(TextColor, PageFade), true);
		}
		Text(Entry.DisplayName, Position + FVector2f(0, 20), LabelSize, WithOpacity(TextColor, PageFade), Selection > .35f);
	}

	TArray<FVector2f> Disc;
	TArray<SlateIndex> Triangles;
	const float CenterRadius = FMath::Min(DeadZoneRadius, Radius - 44.f) - 6.f;
	for (int32 Index = 0; Index < 64; ++Index)
	{
		Disc.Add(Polar(Index * 2.f * PI / 64.f, CenterRadius));
		if (Index >= 2)
			Triangles.Append({0, static_cast<SlateIndex>(Index - 1), static_cast<SlateIndex>(Index)});
	}
	DrawShape(Disc, Triangles, FLinearColor(.020f, .055f, .035f, .96f), BorderColor, Alpha, FVector2f::ZeroVector, 1.f);
	Text(Page.Category, FVector2f(0, -19), 19, TextColor, true);
	if (Page.Entries.IsValidIndex(Hovered))
		Text(MenuCatalog->Emotes[Page.Entries[Hovered]].DisplayName, FVector2f(0, 7), LabelSize, TextColor, true);
	const FText Number = FText::Format(LOCTEXT("PageNumber", "{0} / {1}"), FText::AsNumber(PageIndex + 1), FText::AsNumber(Pages.Num()));
	Text(Number, FVector2f(0, 33), 12, TextColor, false);

	// Small paired leaves echo the notebook trim without adding a texture dependency.
	for (float Side : {-1.f, 1.f})
	{
		TArray<FVector2f> Leaf;
		const FVector2f Base = Center + FVector2f(Side * 19, -51) * Scale;
		Leaf.Append({Base, Base + FVector2f(Side * 9, -8) * Scale, Base + FVector2f(Side * 18, -6) * Scale,
			Base + FVector2f(Side * 11, 0) * Scale, Base});
		FSlateDrawElement::MakeLines(Elements, Layer + 4, Geometry.ToPaintGeometry(), MoveTemp(Leaf), NoEffects,
			WithOpacity(BorderColor, Alpha), true, 1.2f);
	}

	if (Pages.Num() > 1)
	{
		const float Y = Radius + NavDistance;
		for (float Side : {-1.f, 1.f})
		{
			const FVector2f Position(Side * NavOffset, Y);
			const TArray<FVector2f> Corners = {{-NavHalfWidth,-NavHalfHeight},{NavHalfWidth,-NavHalfHeight},
				{NavHalfWidth,NavHalfHeight},{-NavHalfWidth,NavHalfHeight}};
			DrawShape(Corners, {0,1,2,0,2,3}, SectorColor, BorderColor, Alpha, Position, 1.f);
			Text(Side < 0 ? LOCTEXT("PreviousPage", "‹") : LOCTEXT("NextPage", "›"), Position, 21, TextColor, true);
		}
		Text(LOCTEXT("WheelHint", "Molette"), FVector2f(0, Y), 12, TextColor, false);
	}
	Text(LOCTEXT("CloseHint", "G / Échap"), FVector2f(0, Radius + 63.f), 11, TextColor, false);
	return Layer + 5;
}

#undef LOCTEXT_NAMESPACE
