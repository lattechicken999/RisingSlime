#include "RaisingSlimePlayerController.h"
#include "SlimePawn.h"
#include "ExpeditionDirector.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/GameplayStatics.h"

ARaisingSlimePlayerController::ARaisingSlimePlayerController()
{
	bShowMouseCursor = false;
	// Actions/IMC must exist before pawn SetupPlayerInputComponent (runs during Possess,
	// typically before this controller's BeginPlay).
	BuildRuntimeInput();
}

void ARaisingSlimePlayerController::BeginPlay()
{
	Super::BeginPlay();
	EnsureRuntimeInput();
	ApplyCursorState();
	NotifyPossessedPawnToBindInput();
	// Hangul IME can swallow WASD; force game viewport focus and English-friendly input mode.
	FInputModeGameOnly Mode;
	SetInputMode(Mode);
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport(EFocusCause::SetDirectly);
	}
}

void ARaisingSlimePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	EnsureRuntimeInput();
}

void ARaisingSlimePlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	EnsureRuntimeInput();
	NotifyPossessedPawnToBindInput();
}

void ARaisingSlimePlayerController::AcknowledgePossession(APawn* P)
{
	Super::AcknowledgePossession(P);
	EnsureRuntimeInput();
	NotifyPossessedPawnToBindInput();
}

void ARaisingSlimePlayerController::EnsureRuntimeInput()
{
	BuildRuntimeInput();
	ApplyMappingContext();
}

void ARaisingSlimePlayerController::ApplyMappingContext()
{
	if (!MappingContext)
	{
		return;
	}

	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			if (!Subsystem->HasMappingContext(MappingContext))
			{
				Subsystem->AddMappingContext(MappingContext, 0);
				UE_LOG(LogTemp, Display, TEXT("RaisingSlimePC: mapping context added (%s)"), *MappingContext->GetName());
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("RaisingSlimePC: EnhancedInputLocalPlayerSubsystem missing"));
		}
	}
}

void ARaisingSlimePlayerController::NotifyPossessedPawnToBindInput()
{
	if (ASlimePawn* SoftSlime = Cast<ASlimePawn>(GetPawn()))
	{
		SoftSlime->BindEnhancedInput();
	}
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

void ARaisingSlimePlayerController::GiveTraitPurgeItem()
{
	if (AExpeditionDirector* Director = Cast<AExpeditionDirector>(
			UGameplayStatics::GetActorOfClass(GetWorld(), AExpeditionDirector::StaticClass())))
	{
		Director->SetHasTraitPurgeItem(true);
		UE_LOG(LogTemp, Display, TEXT("GiveTraitPurgeItem: purge unlocked"));
	}
}

bool ARaisingSlimePlayerController::TryLoadAssetInput()
{
	if (MappingContext && MoveAction)
	{
		return true;
	}

	UInputMappingContext* LoadedImc = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Input/IMC_Slime.IMC_Slime"));
	UInputAction* LoadedMove = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/IA_Move.IA_Move"));
	UInputAction* LoadedLook = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/IA_Look.IA_Look"));
	UInputAction* LoadedJump = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/IA_Jump.IA_Jump"));
	UInputAction* LoadedAttack = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/IA_Attack.IA_Attack"));
	UInputAction* LoadedTrait = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/IA_Trait.IA_Trait"));
	UInputAction* LoadedCycle = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/IA_CycleTrait.IA_CycleTrait"));
	UInputAction* LoadedTraitPanel = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/IA_TraitPanel.IA_TraitPanel"));
	UInputAction* LoadedSpit = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/IA_Spit.IA_Spit"));
	UInputAction* LoadedCursor = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/IA_Cursor.IA_Cursor"));
	UInputAction* LoadedFocus = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/IA_Focus.IA_Focus"));

	if (!LoadedImc || !LoadedMove || !LoadedLook || !LoadedJump || !LoadedAttack
		|| !LoadedTrait || !LoadedCycle || !LoadedCursor || !LoadedFocus)
	{
		return false;
	}

	MappingContext = LoadedImc;
	MoveAction = LoadedMove;
	LookAction = LoadedLook;
	JumpAction = LoadedJump;
	AttackAction = LoadedAttack;
	TraitAction = LoadedTrait;
	CycleTraitAction = LoadedCycle;
	CursorAction = LoadedCursor;
	FocusAction = LoadedFocus;

	// Tab panel is runtime-bound so legacy IMC E→IA_Digest does not gate digests.
	if (LoadedTraitPanel)
	{
		TraitPanelAction = LoadedTraitPanel;
	}
	else
	{
		TraitPanelAction = NewObject<UInputAction>(this, TEXT("IA_TraitPanel"));
		TraitPanelAction->ValueType = EInputActionValueType::Boolean;
	}
	MappingContext->MapKey(TraitPanelAction, EKeys::Tab);

	// Spit on Q; move CycleTrait off Q → R so they don't fight (legacy IMC had Cycle on Q).
	if (LoadedSpit)
	{
		SpitAction = LoadedSpit;
	}
	else
	{
		SpitAction = NewObject<UInputAction>(this, TEXT("IA_Spit"));
		SpitAction->ValueType = EInputActionValueType::Boolean;
	}
	MappingContext->UnmapKey(CycleTraitAction, EKeys::Q);
	MappingContext->MapKey(CycleTraitAction, EKeys::R);
	MappingContext->MapKey(SpitAction, EKeys::Q);

	UE_LOG(LogTemp, Display, TEXT("RaisingSlimePC: loaded IMC_Slime (TraitPanel=Tab Spit=Q Cycle=R)"));
	return true;
}

void ARaisingSlimePlayerController::BuildRuntimeInput()
{
	if (MappingContext && MoveAction)
	{
		return;
	}

	// Prefer Content assets on real instances (not CDO).
	if (!HasAnyFlags(RF_ClassDefaultObject) && TryLoadAssetInput())
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
	TraitPanelAction = MakeAction(TEXT("IA_TraitPanel"), EInputActionValueType::Boolean);
	SpitAction = MakeAction(TEXT("IA_Spit"), EInputActionValueType::Boolean);
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
	MappingContext->MapKey(CycleTraitAction, EKeys::R);
	MappingContext->MapKey(SpitAction, EKeys::Q);
	MappingContext->MapKey(TraitPanelAction, EKeys::Tab);
	MappingContext->MapKey(CursorAction, EKeys::Escape);
	MappingContext->MapKey(FocusAction, EKeys::LeftMouseButton);

	UE_LOG(LogTemp, Display, TEXT("RaisingSlimePC: runtime IMC/actions built (Spit=Q Cycle=R)"));
}
