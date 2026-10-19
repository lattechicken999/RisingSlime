#include "RaisingSlimeGameMode.h"
#include "RaisingSlimePlayerController.h"
#include "RaisingSlimeHud.h"
#include "SlimePawn.h"

ARaisingSlimeGameMode::ARaisingSlimeGameMode()
{
	DefaultPawnClass = ASlimePawn::StaticClass();
	PlayerControllerClass = ARaisingSlimePlayerController::StaticClass();
	HUDClass = ARaisingSlimeHUD::StaticClass();
}
