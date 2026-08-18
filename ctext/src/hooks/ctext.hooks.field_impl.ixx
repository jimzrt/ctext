module;

#include "helpers.hpp"
#include <intrin.h>
#include <fstream>

export module ctext.hooks:field_impl;

import ct;
import ct.addr;
import ctext.config;
import ctext.mod_menu;

using namespace ct;
using namespace ct::addr;

namespace {
	C_FN_HOOK_A(
		void, FieldImpl, UserScrollDiagonal,
		FIELD_IMPL_USER_SCROLL_DIAGONAL,
		int, x, int, y, bool, a3, bool, a4, bool, a5
	) {
		if (ctext::mod_menu::IsOpen())
			return;

		if (!ctext::Config::Get().FixesRevertDiagonalMovement) {
			C_CALL_ORIG(x, y, a3, a4, a5);
			return;
		}

		dword854[36] = x;
		dword854[37] = y;
		dword854[38] += x;
		dword854[41] += y;
		dword854[40] = dword854[38];
		dword854[43] = dword854[41];
	}

	C_FN_HOOK_A(
		void, FieldImpl, MovementUpdate,
		FIELD_IMPL_MOVEMENT_UPDATE
	) {
		// Polling here catches function keys while the player is standing
		// still. The same input boundary is used by field and world-map scenes.
		if (ctext::mod_menu::HandleFieldInput())
			return;
		C_CALL_ORIG();
	}

	struct NativeResumeScene {};
	C_FN_HOOK_A(
		void, NativeResumeScene, Resume,
		SAVE_LOAD_RESUME,
		void*, context
	) {
		std::ofstream log("ctext_native_resume_hook.log", std::ios::app);
		if (log) {
			log << std::hex
				<< "this=" << reinterpret_cast<std::uintptr_t>(this)
				<< " context=" << reinterpret_cast<std::uintptr_t>(context)
				<< " slot=" << *reinterpret_cast<std::uint32_t*>(
					reinterpret_cast<std::uint8_t*>(this) + 4)
				<< '\n';
		}
		C_CALL_ORIG(context);
	}

	FN_HOOK_A(
		__fastcall, void, god_mode_damage_target,
		DAMAGE_TARGET,
		void*, context
	) {
		const auto caller = _ReturnAddress();
		CALL_ORIG(god_mode_damage_target, context);
		if (!ctext::Config::Get().GameplayGodMode || !context ||
			caller != reinterpret_cast<void*>(ADDR(0x8CA51))) return;
		auto* state = *reinterpret_cast<uint8_t**>(
			static_cast<uint8_t*>(context) + 0x4c);
		if (!state) return;
		const uint32_t target =
			*reinterpret_cast<uint32_t*>(state + 0x1468) & 0xffff;
		if ((target & 0x7f) != 0) return;
		auto* amount = reinterpret_cast<int32_t*>(state);
		if (target <= 0x100) *amount = 0;
		else if (target >= 0x180 && target <= 0x500) *amount = 9999;
	}
}

export namespace ctext::hooks {
	void EnableFieldImplHooks() {
		ENABLE_C_FN_HOOK(FieldImpl, MovementUpdate);
		ENABLE_C_FN_HOOK(FieldImpl, UserScrollDiagonal);
		ENABLE_C_FN_HOOK(NativeResumeScene, Resume);
		ENABLE_FN_HOOK(god_mode_damage_target);
	}
}
