#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlimeEngulfVenomComponent.generated.h"

class AActor;
class UMaterialInterface;
class UProceduralMeshComponent;

/**
 * Venom-style local engulf presentation (alt B′):
 * 1–3 organic ribbons wrap prey in place. No soft-volume climb, no COM yank.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class RAISINGSLIME_API USlimeEngulfVenomComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USlimeEngulfVenomComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void BeginEngulf(AActor* Prey, UMaterialInterface* SlimeMaterial);
	void SetEngulfProgress01(float Engulf01);
	void BeginFadeOut();
	void Clear();

	bool IsActive() const { return bActive; }
	float GetVisual01() const { return Visual01; }

	UPROPERTY(EditAnywhere, Category = "Engulf|Venom", meta = (ClampMin = "1", ClampMax = "3"))
	int32 RibbonCount = 3;

	/** Overdamped follow rate for visual wrap height (1/s). */
	UPROPERTY(EditAnywhere, Category = "Engulf|Venom", meta = (ClampMin = "1", ClampMax = "20"))
	float VisualFollowRate = 6.5f;

	UPROPERTY(EditAnywhere, Category = "Engulf|Venom", meta = (ClampMin = "0.5", ClampMax = "12"))
	float FadeOutSeconds = 0.35f;

	UPROPERTY(EditAnywhere, Category = "Engulf|Venom")
	float RibbonRadiusCm = 9.f;

	UPROPERTY(EditAnywhere, Category = "Engulf|Venom")
	float CoatExtraCm = 7.f;

protected:
	struct FRibbonMesh
	{
		TObjectPtr<UProceduralMeshComponent> Mesh;
	};

	void EnsureMeshes();
	void RefreshPreyBounds();
	void RebuildAllRibbons();
	void BuildRibbonSection(int32 RibbonIndex, float Wrap01, float Opacity);
	FVector GetBodyAttachWorld() const;

	TArray<FRibbonMesh> Ribbons;
	TWeakObjectPtr<AActor> Prey;
	TObjectPtr<UMaterialInterface> Material;

	FVector PreyCenter = FVector::ZeroVector;
	float PreyRadiusXY = 30.f;
	float PreyBottomZ = 0.f;
	float PreyTopZ = 60.f;

	float Target01 = 0.f;
	float Visual01 = 0.f;
	float Fade01 = 0.f;
	bool bActive = false;
	bool bFading = false;
};
