module;

#include "build_config.hpp"
#include <MinHook.h>
#include <cocos/base/CCDirector.h>
#include <Windows.h>

export module ctext.hooks;

import :ctr;
import :ctr.resource_manager;
import :detchman_resource;
import :field_impl;
import :msg_window;
import :name_input_scene;
import :render;
import :sound_mananger;
import :sound_task;
import :sqex_logo_scene;
import :text_manager;
import ctext.mod_menu;

namespace {
    using DirectorMainLoop = void(__thiscall*)(cocos2d::Director*);
    DirectorMainLoop originalMainLoop{};

    void __fastcall MainLoopHook(cocos2d::Director* director, void*) {
        ctext::mod_menu::HandleFieldInput();
        originalMainLoop(director);
        ctext::mod_menu::RefreshStatusOverlay();
    }

    void EnableGlobalInputHook() {
        auto* cocos = GetModuleHandleA("libcocos2d.dll");
        if (!cocos) return;
        auto* target = reinterpret_cast<void*>(GetProcAddress(
            cocos, "?mainLoop@Director@cocos2d@@QAEXXZ"));
        if (!target) return;
        if (MH_CreateHook(target, reinterpret_cast<void*>(&MainLoopHook),
                          reinterpret_cast<void**>(&originalMainLoop)) == MH_OK)
            MH_EnableHook(target);
    }
}


export namespace ctext::hooks {
	void InitialiseHooks() {
		InitialiseDetchmanResourceHooks();
	}

	void EnableHooks() {
		EnableGlobalInputHook();
		EnableCtrHooks();
		EnableCtrResourceManagerHooks();
		EnableDetchmanResourceHooks();
		EnableFieldImplHooks();
		EnableRenderHooks();
		EnableSoundManagerHooks();
		EnableSoundTaskHooks();
		EnableSqexLogoSceneHooks();

#ifdef FEATURE_VOICE_ACTING
		EnableMsgWindowHooks();
		EnableNameInputSceneHooks();
		EnableTextManagerHooks();
#endif
	}

	void UninitialiseHooks() {
		UninitialiseDetchmanResourceHooks();
	}
}
