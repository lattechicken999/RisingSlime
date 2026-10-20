#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NestZone.generated.h"

class ASlimePawn;
class UBoxComponent;

UCLASS()
class RAISINGSLIME_API ANestZone : public AActor
{
	GENERATED_BODY()

public:
	ANestZone();

	virtual void BeginPlay() override;

	bool IsPlayerInside() const { return bPlayerInside; }
	void Refresh();

protected:
	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* Overlapped, AActor* Other, UPrimitiveComponent* OtherComp, int32 BodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* Overlapped, AActor* Other, UPrimitiveComponent* OtherComp, int32 BodyIndex);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Nest")
	TObjectPtr<UBoxComponent> Trigger;

	bool bPlayerInside = false;
};
