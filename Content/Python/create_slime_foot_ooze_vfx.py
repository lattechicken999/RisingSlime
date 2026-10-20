# DEPRECATED (2026-10-20): NS_FootOoze_Fluids / Grid2D_SW_Drop is NO LONGER used for gameplay.
# Foot item effect is C++ ASlimeTrailMark (short-lived slime-colored plane) via SlimeVfx::TickFootSlimeTrail.
# This script is kept only so the unused Fluids asset is not recreated by mistake.
#
# Do not re-enable SW_Drop for foot marks — it spreads sideways like water.
import unreal

unreal.log_warning(
    "create_slime_foot_ooze_vfx.py DEPRECATED — foot trail is ASlimeTrailMark (no Fluids). Skipping."
)
try:
    unreal.SystemLibrary.quit_editor()
except Exception:
    pass
