module;

#include <cstdint>

export module ct.addr;


#define P constexpr uintptr_t


export namespace ct::addr {
	P DRAW_EXCLAMATION_MARK = 0x0D8590;

	P CHRONO_CANVAS_INSTANCE = 0x41B4C4;

	P DETCHMAN_RESOURCE_LOAD_FILE_ENTRY = 0xB9DD0;

	P FIELD_IMPL_USER_SCROLL_DIAGONAL = 0x175DA0;
	// Field input dispatcher; this runs for cardinal and diagonal movement.
	P FIELD_IMPL_MOVEMENT_UPDATE = 0x175A40;
	// FieldScene setup routine that initializes an actor record from the
	// bookmark/map-entry cursor.  It is the proven writer of +0x84/+0x90.
	P FIELD_IMPL_ACTOR_INITIALIZE = 0x16A7D0;
	// Native field lifecycle resync: rebuilds movement state from the active
	// actor and runs the camera/scroll state machine.
	P FIELD_IMPL_SYNC_POSITION = 0x1734D0;
	// Final damage helper used by the proven chrono_modloader god-mode patch.
	P DAMAGE_TARGET = 0x8F980;

	// Native save-state helpers (RVA, Chrono Trigger 1.0 PC executable).
	// SAVE_STATE_TO_BUFFER copies the live state into the game's serialized
	// state layout; SAVE_STATE_FROM_BUFFER applies that layout back.
	P SAVE_STATE_INIT = 0x19A5A0;
	P SAVE_STATE_WRITE = 0x19CA20;
	P SAVE_STATE_READ = 0x19BC40;
	// Updates the native save metadata/current-slot bookkeeping before apply.
	P SAVE_STATE_SYNC = 0x19DA30;
	P SAVE_STATE_TO_BUFFER = 0x212C80;
	P SAVE_STATE_FROM_BUFFER = 0x212BA0;
	P SAVE_STATE_APPLY = 0x2130F0;
	P SAVE_STATE_APPLY_FLOW = 0x215E30;
	P SAVE_STATE_OFFSET = 0x28;

	P MSG_WINDOW_CLOSE = 0x195C70;
	P MSG_WINDOW_SETUP = 0x195E30;

	P NAME_INPUT_SCENE_UPDATE = 0x2C2C50;

	P SCENE_MANAGER_IS_DEMO_ON = 0x41BDD7;
	P SCENE_MANAGER_NOW_SCENE = 0x41C3E8;
	P SCENE_MANAGER_SCENE_STACK = 0x41F994;
	P SCENE_MANAGER_CREATE = 0x297860;
	P SCENE_MANAGER_NEXT_SCENE = 0x297B60;
	P SCENE_MANAGER_PUSH_SCENE = 0x298410;
	P SCENE_MANAGER_POP_SCENE = 0x298470;
	P SCENE_MANAGER_POP_ALL_SCENES = 0x2984E0;

	P SOUND_MANAGER_PLAY_SOUND = 0x1A1EF0;

	P SOUND_OBJ_CREATE_SOUND = 0x19E500;

	P SOUND_TASK_VFTABLE = 0x3A1E3C;
	P SOUND_TASK_STOP = 0x1A2350;

	P SQEX_LOGO_SCENE_CREATE = 0x2CC3C0;

	P TEXT_MANAGER_GET_MSG = 0x1B92D0;

	P CTR_CREATE_LABEL = 0xFA90;
	P CTR_RESOURCE_MANAGER_CREATE_TEXTURE = 0x19AC50;

	P SQEX_SD_DRIVER_SOUND_CONTROLLER_PLAY = 0x2F0930;
	P SQEX_SD_DRIVER_SOUND_CONTROLLER_RESUME = 0x2F0AC0;

	P VIRTUAL_CONTROLLER_INSTANCE_ = 0x3FB30C;
	P INVALID_CONTROLLER_INSTANCE_ = 0x41C3DC;
}
