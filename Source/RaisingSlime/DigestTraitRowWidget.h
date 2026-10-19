#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RaisingSlimeTypes.h"
#include "DigestTraitRowWidget.generated.h"

class UButton;
class UTextBlock;
class UDigestMenuWidget;

UCLASS()
class RAISINGSLIME_API UDigestTraitRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(UDigestMenuWidget* OwnerMenu, ETraitId InTrait, bool bInConfirm);

protected:
	virtual void NativeConstruct() override;

	UFUNCTION()
	void OnActionClicked();

	UPROPERTY()
	TObjectPtr<UDigestMenuWidget> Menu;
	UPROPERTY()
	TObjectPtr<UButton> ActionButton;
	UPROPERTY()
	TObjectPtr<UTextBlock> NameText;

	ETraitId Trait = ETraitId::None;
	bool bConfirm = true;
};
