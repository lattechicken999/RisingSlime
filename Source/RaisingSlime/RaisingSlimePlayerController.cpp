#include "RaisingSlimePlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

ARaisingSlimePlayerController::ARaisingSlimePlayerController()
{
	bShowMouseCursor = false;
}

void ARaisingSlimePlayerController::BeginPlay()
{
	Super::BeginPlay();
	BuildRuntimeInput();

	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			if (MappingContext)
			{
				Subsystem->AddMappingContext(MappingContext, 0);
			}
		}
	}

	ApplyCursorState();
}

void ARaisingSlimePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
}

void ARaisingSlimePlayerController::SetMenuOpen(bool bOpen)
{
	bMenuOpen = bOpen;
	if (bOpen)
	{
		bCursorUnlocked = true;
	}
	ApplyCursorState();
}

void ARaisingSlimePlayerController::SetCursorUnlocked(bool bUnlocked)
{
	bCursorUnlocked = bUnlocked;
	ApplyCursorState();
}

void ARaisingSlimePlayerController::ApplyCursorState()
{
	const bool bShow = bMenuOpen || bCursorUnlocked;
	bShowMouseCursor = bShow;
	if (bShow)
	{
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		SetInputMode(Mode);
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
	}
}

void ARaisingSlimePlayerController::BuildRuntimeInput()
{
	if (MappingContext)
	{
		return;
	}

	MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Slime_Runtime"));

	auto MakeAction = [this](const TCHAR* Name, EInputActionValueType Type) -> UInputAction*
	{
		UInputAction* Action = NewObject<UInputAction>(this, Name);
		Action->ValueType = Type;
		return Action;
	};

	MoveAction = MakeAction(TEXT("IA_Move"), EInputActionValueType::Axis2D);
	LookAction = MakeAction(TEXT("IA_Look"), EInputActionValueType::Axis2D);
	JumpAction = MakeAction(TEXT("IA_Jump"), EInputActionValueType::Boolean);
	AttackAction = MakeAction(TEXT("IA_Attack"), EInputActionValueType::Boolean);
	TraitAction = MakeAction(TEXT("IA_Trait"), EInputActionValueType::Boolean);
	CycleTraitAction = MakeAction(TEXT("IA_CycleTrait"), EInputActionValueType::Boolean);
	DigestAction = MakeAction(TEXT("IA_Digest"), EInputActionValueType::Boolean);
	CursorAction = MakeAction(TEXT("IA_Cursor"), EInputActionValueType::Boolean);
	FocusAction = MakeAction(TEXT("IA_Focus"), EInputActionValueType::Boolean);

	// WASD → Axis2D (X right, Y forward)
	{
		FEnhancedActionKeyMapping& W = MappingContext->MapKey(MoveAction, EKeys::W);
		UInputModifierSwizzleAxis* SwizzleW = NewObject<UInputModifierSwizzleAxis>(MappingContext);
		SwizzleW->Order = EInputAxisSwizzle::YXZ;
		W.Modifiers.Add(SwizzleW);

		FEnhancedActionKeyMapping& S = MappingContext->MapKey(MoveAction, EKeys::S);
		UInputModifierNegate* NegS = NewObject<UInputModifierNegate>(MappingContext);
		S.Modifiers.Add(NegS);
		UInputModifierSwizzleAxis* SwizzleS = NewObject<UInputModifierSwizzleAxis>(MappingContext);
		SwizzleS->Order = EInputAxisSwizzle::YXZ;
		S.Modifiers.Add(SwizzleS);

		MappingContext->MapKey(MoveAction, EKeys::D);

		FEnhancedActionKeyMapping& A = MappingContext->MapKey(MoveAction, EKeys::A);
		UInputModifierNegate* NegA = NewObject<UInputModifierNegate>(MappingContext);
		A.Modifiers.Add(NegA);
	}

	MappingContext->MapKey(LookAction, EKeys::Mouse2D);
	MappingContext->MapKey(JumpAction, EKeys::SpaceBar);
	MappingContext->MapKey(AttackAction, EKeys::LeftMouseButton);
	MappingContext->MapKey(TraitAction, EKeys::RightMouseButton);
	MappingContext->MapKey(CycleTraitAction, EKeys::Q);
	MappingContext->MapKey(DigestAction, EKeys::E);
	MappingContext->MapKey(CursorAction, EKeys::Escape);
	MappingContext->MapKey(FocusAction, EKeys::LeftMouseButton);
}
