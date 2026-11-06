#include "Sales/Checkout/BotanicusCheckoutSettings.h"
UBotanicusCheckoutSettings::UBotanicusCheckoutSettings()
{
    ScannerClass=TSoftClassPtr<ABotanicusCheckoutScanner>(FSoftObjectPath(TEXT("/Game/Botanicus/blueprints/Checkout/BP_BotanicusScanner.BP_BotanicusScanner_C")));
    PaymentCoinsClass=TSoftClassPtr<ABotanicusPaymentCoins>(FSoftObjectPath(TEXT("/Game/Botanicus/blueprints/Checkout/BP_BotanicusPaymentCoins.BP_BotanicusPaymentCoins_C")));
    BodyMaterial=TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Botanicus/Materials/Juice/Checkout/M_CheckoutBody.M_CheckoutBody")));
    BeamMaterial=TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Botanicus/Materials/Juice/Checkout/M_CheckoutBeam.M_CheckoutBeam")));
}
