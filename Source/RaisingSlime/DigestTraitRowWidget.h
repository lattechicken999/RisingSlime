#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RaisingSlimeTypes.h"
#include "DigestTraitRowWidget.generated.h"

class UDigestMenuWidget;

/** Legacy digest row — unused after field absorb. */
UCLASS()
class RAISINGSLIME_API UDigestTraitRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(UDigestMenuWidget* /*OwnerMenu*/, ETraitId /*InTrait*/, bool /*bInConfirm*/) {}
};
