#include "SpecialOrders/BotanicusSpecialOrderTypes.h"

bool FBotanicusSpecialOrder::IsPending() const
{
	return Status == EBotanicusSpecialOrderStatus::Travelling ||
		Status == EBotanicusSpecialOrderStatus::Offered ||
		Status == EBotanicusSpecialOrderStatus::Active;
}

bool FBotanicusSpecialOrder::IsComplete() const
{
	return !Requirements.IsEmpty() && !Requirements.ContainsByPredicate(
		[](const FBotanicusSpecialOrderRequirement& Line)
		{ return Line.Quantity <= 0 || Line.Delivered < Line.Quantity; });
}

bool MatchesBotanicusSpecialOrder(const FBotanicusSpecialOrderRequirement& R,
	const FBotanicusSpecialOrderPlant& P)
{
	return !P.PlantKey.IsNone() && R.Quantity > 0 &&
		(R.PlantKey.IsNone() || R.PlantKey == P.PlantKey) &&
		(!R.bFilterElement || R.Element == P.Element) &&
		P.Quality >= R.MinimumQuality &&
		(R.RarityTag.IsNone() || R.RarityTag == P.RarityTag) &&
		(R.MutationTag.IsNone() || P.MutationTags.Contains(R.MutationTag)) &&
		(R.ColorTag.IsNone() || R.ColorTag == P.ColorTag) &&
		(!R.bRequirePreparedPot || P.bPreparedPot);
}

int32 FindBotanicusSpecialOrderDeliveryLine(
	const TArray<FBotanicusSpecialOrderRequirement>& Requirements,
	const FBotanicusSpecialOrderPlant& Plant)
{
	int32 BestIndex = INDEX_NONE;
	int32 BestSpecificity = -1;
	for (int32 Index = 0; Index < Requirements.Num(); ++Index)
	{
		const auto& R = Requirements[Index];
		if (R.Delivered >= R.Quantity || !MatchesBotanicusSpecialOrder(R, Plant)) continue;
		// Fill constrained lines first, so an exact specimen is not consumed by
		// an earlier generic elemental line. One item can only fill one line.
		const int32 Specificity = (!R.PlantKey.IsNone() ? 16 : 0) +
			(!R.MutationTag.IsNone() ? 8 : 0) + (!R.RarityTag.IsNone() ? 4 : 0) +
			R.MinimumQuality + (R.bFilterElement ? 1 : 0) +
			(!R.ColorTag.IsNone() ? 1 : 0) + (R.bRequirePreparedPot ? 1 : 0);
		if (Specificity > BestSpecificity) { BestIndex = Index; BestSpecificity = Specificity; }
	}
	return BestIndex;
}

FString GetBotanicusSpecialOrderElementLabel(EBotanicusPlantElement Element)
{
	switch (Element)
	{
	case EBotanicusPlantElement::Fire: return TEXT("feu");
	case EBotanicusPlantElement::Water: return TEXT("eau");
	case EBotanicusPlantElement::Ice: return TEXT("glace");
	case EBotanicusPlantElement::Shadow: return TEXT("ténèbres");
	default: return TEXT("normal");
	}
}
