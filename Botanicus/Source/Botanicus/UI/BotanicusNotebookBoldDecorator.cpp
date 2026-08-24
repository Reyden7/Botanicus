// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BotanicusNotebookBoldDecorator.h"

#include "Components/RichTextBlock.h"

namespace
{
class FBotanicusNotebookBoldDecorator final : public FRichTextDecorator
{
public:
	explicit FBotanicusNotebookBoldDecorator(URichTextBlock* InOwner)
		: FRichTextDecorator(InOwner)
	{
	}

	virtual bool Supports(
		const FTextRunParseResults& RunParseResult,
		const FString& Text) const override
	{
		return RunParseResult.Name.Equals(TEXT("b"), ESearchCase::IgnoreCase);
	}

protected:
	virtual void CreateDecoratorText(
		const FTextRunInfo& RunInfo,
		FTextBlockStyle& InOutTextStyle,
		FString& InOutString) const override
	{
		// The supplied handwritten notebook font has no separate bold face.
		// A one-pixel same-colour outline gives important words a genuine,
		// readable bold weight while retaining that font's visual identity.
		InOutTextStyle.Font.OutlineSettings.OutlineSize = 1;
		InOutTextStyle.Font.OutlineSettings.OutlineColor =
			FLinearColor(0.24f, 0.16f, 0.08f, 1.0f);
	}
};
}

TSharedPtr<ITextDecorator>
UBotanicusNotebookBoldDecorator::CreateDecorator(URichTextBlock* InOwner)
{
	return MakeShared<FBotanicusNotebookBoldDecorator>(InOwner);
}
