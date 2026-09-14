#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "BotanicusSpecialOrderTestCommandlet.generated.h"

/** Runs the synchronous order tests without opening an editor viewport or a saved game. */
UCLASS()
class UBotanicusSpecialOrderTestCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	UBotanicusSpecialOrderTestCommandlet();
	virtual int32 Main(const FString& Params) override;
};
