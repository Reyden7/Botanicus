#pragma once

#include "Blueprint/UserWidget.h"
#include "Rendering/RenderingCommon.h"
#include "BotanicusRadialEmoteMenu.generated.h"

class UBotanicusEmoteCatalog;
class USkeleton;

UCLASS()
class BOTANICUS_API UBotanicusRadialEmoteMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	bool Open(const UBotanicusEmoteCatalog& Catalog, const USkeleton* Skeleton);
	void Close();
	bool IsMenuOpen() const { return bMenuOpen; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaSeconds) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
		FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual FReply NativeOnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event) override;

	UPROPERTY(EditDefaultsOnly, Category="Radial|Layout", meta=(ClampMin="100"))
	float Radius = 282.f;

	UPROPERTY(EditDefaultsOnly, Category="Radial|Layout", meta=(ClampMin="24"))
	float DeadZoneRadius = 93.f;

	UPROPERTY(EditDefaultsOnly, Category="Radial|Layout", meta=(ClampMin="1", ClampMax="8"))
	int32 MaxSectorsPerPage = 8;

	UPROPERTY(EditDefaultsOnly, Category="Radial|Layout", meta=(ClampMin="10"))
	float IconSize = 40.f;

	UPROPERTY(EditDefaultsOnly, Category="Radial|Layout", meta=(ClampMin="8", ClampMax="24"))
	int32 LabelSize = 14;

	UPROPERTY(EditDefaultsOnly, Category="Radial|Style")
	FLinearColor SectorColor = FLinearColor(.040f, .090f, .054f, .94f);

	UPROPERTY(EditDefaultsOnly, Category="Radial|Style")
	FLinearColor AlternateSectorColor = FLinearColor(.066f, .108f, .057f, .94f);

	UPROPERTY(EditDefaultsOnly, Category="Radial|Style")
	FLinearColor SelectedColor = FLinearColor(.274f, .305f, .090f, .99f);

	UPROPERTY(EditDefaultsOnly, Category="Radial|Style")
	FLinearColor BorderColor = FLinearColor(.56f, .438f, .22f, .80f);

	UPROPERTY(EditDefaultsOnly, Category="Radial|Style")
	FLinearColor TextColor = FLinearColor(.94f, .87f, .70f);

	UPROPERTY(EditDefaultsOnly, Category="Radial|Animation", meta=(ClampMin="0", Units="s"))
	float OpenDuration = .12f;

	UPROPERTY(EditDefaultsOnly, Category="Radial|Animation", meta=(ClampMin="0", Units="s"))
	float CloseDuration = .08f;

	UPROPERTY(EditDefaultsOnly, Category="Radial|Animation", meta=(ClampMin="0", Units="s"))
	float HoverDuration = .08f;

	UPROPERTY(EditDefaultsOnly, Category="Radial|Animation", meta=(ClampMin="0", Units="s"))
	float PageDuration = .10f;

	UPROPERTY(EditDefaultsOnly, Category="Radial|Animation", meta=(ClampMin="1", ClampMax="1.15"))
	float HoverScale = 1.05f;

private:
	struct FPage
	{
		FText Category;
		TArray<int32> Entries;
	};
	struct FSector
	{
		TArray<FVector2f> Outline;
		TArray<SlateIndex> Indices;
		FVector2f Direction;
	};

	void BuildPages(const USkeleton* Skeleton);
	void ChangePage(int32 Direction);
	void RebuildSectors();
	void SetHovered(int32 Index);
	int32 HitSector(FVector2f Offset, float Scale) const;
	float GetLayoutScale(const FGeometry& Geometry) const;

	UPROPERTY()
	TObjectPtr<const UBotanicusEmoteCatalog> MenuCatalog;

	TArray<FPage> Pages;
	TArray<FSector> Sectors;
	TArray<float> HoverWeights;
	int32 PageIndex = 0;
	int32 Hovered = INDEX_NONE;
	float Fade = 0.f;
	float PageFade = 1.f;
	bool bMenuOpen = false;
};
