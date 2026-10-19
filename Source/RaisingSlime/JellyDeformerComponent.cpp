#include "JellyDeformerComponent.h"

UJellyDeformerComponent::UJellyDeformerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UJellyDeformerComponent::Step(float /*Dt*/)
{
}

void UJellyDeformerComponent::ResetMotion()
{
}

void UJellyDeformerComponent::AddImpulse(const FVector& /*LocalPoint*/, const FVector& /*LocalDirection*/, float /*Magnitude*/, float /*Radius*/, float /*Squash*/, float /*Stretch*/)
{
}
