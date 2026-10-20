#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RaisingSlimeTypes.h"
#include "DigestMenuWidget.generated.h"

/**
 * Legacy nest digest confirm UI — disabled.
 * Field absorb + Tab TraitPanelWidget replace this flow.
 */
UCLASS()
class RAISINGSLIME_API UDigestMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Refresh() {}
	void HandleConfirm(ETraitId /*Trait*/) {}
	void HandleSpit(ETraitId /*Trait*/) {}
};
