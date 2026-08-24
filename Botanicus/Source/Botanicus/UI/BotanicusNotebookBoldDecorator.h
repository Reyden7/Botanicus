// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/RichTextBlockDecorator.h"
#include "BotanicusNotebookBoldDecorator.generated.h"

/** Applies the notebook's bold emphasis to text enclosed in <b>...</>. */
UCLASS()
class BOTANICUS_API UBotanicusNotebookBoldDecorator
	: public URichTextBlockDecorator
{
	GENERATED_BODY()

public:
	virtual TSharedPtr<ITextDecorator> CreateDecorator(
		URichTextBlock* InOwner) override;
};
