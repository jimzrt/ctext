module;

#include <cocos/2d/CCDrawNode.h>
#include <cocos/2d/CCLabel.h>
#include <cocos/2d/CCLayer.h>
#include <cocos/2d/CCNode.h>
#include <cocos/base/CCDirector.h>
#include <cocos/base/CCScheduler.h>
#include <cocos/ui/UICheckBox.h>
#include <cocos/ui/UISlider.h>

#include <Windows.h>
#include <commdlg.h>
#include "helpers.hpp"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>

export module ctext.mod_menu;

import ctext.config;
import ctext.texture_filter;
import ct;
import ct.addr;
import ct.scene;

namespace ctext::mod_menu {
    bool HandleFieldInput();
    void SetCurrentFieldImpl(void* fieldImpl);
    void RestoreFieldPosition(void* fieldImpl);
}

namespace {
    constexpr int kMainItemCount = 6;
    constexpr int kMaxItemCount = 8;
    constexpr int kVisibleItemCount = 5;

    class ModMenuLayer;
    ModMenuLayer* menuLayer{};
    cocos2d::Layer* statusOverlay{};
    cocos2d::Scene* statusScene{};
    bool pausedByMenu{};
    std::set<void*> pausedTargets;
    void CloseMenu();

    void ApplyGameSpeed(float scale) {
        if (auto* director = cocos2d::Director::getInstance()) {
            if (auto* scheduler = director->getScheduler()) scheduler->setTimeScale(scale);
        }
    }

    // The game's normal save path first copies the live state at
    // save-manager+0x28 into a temporary state object, then calls the native
    // encrypted writer.  Reuse those exact routines so quick saves include
    // unsaved in-memory progress (not merely the last disk save).
    constexpr int kQuickSlotCount = 3;
    constexpr int kQuickSlotBase = 21; // outside the normal 0..20 slots
    constexpr int kNativeBookmarkSlot = 20;
    constexpr std::size_t kSaveStateBytes = 0x8000;
    int quickSlot{};
    // The executable constructs this as a stack object. Keep equivalent
    // alignment for its SIMD/string members instead of relying on byte-array
    // alignment.
    alignas(16) std::array<std::uint8_t, kSaveStateBytes> quickState{};
    bool quickLoadPending{};
    void* currentFieldImpl{};
    bool savedFieldPositionValid{};
    std::int32_t savedFieldX{};
    std::int32_t savedFieldY{};
    std::int32_t savedFieldTileX{};
    std::int32_t savedFieldTileY{};
    std::uint32_t savedResumeX{};
    std::uint32_t savedResumeY{};
    std::uint32_t savedResumeDirection{};
    bool restorePositionPending{};
    void QuickLoadLog(const std::string& message);

    // FieldImpl's map layer owns the runtime actor nodes.  The bookmark only
    // stores an 8-bit tile anchor, so inspect this tree while saving to locate
    // the player transform that must be restored for sub-tile quick loads.
    void LogFieldMapNodes(cocos2d::Node* node, int depth, int& count) {
        if (!node || depth > 7 || count >= 400) return;
        const auto p = node->getPosition();
        const auto size = node->getContentSize();
        QuickLoadLog("node " + std::to_string(reinterpret_cast<std::uintptr_t>(node)) +
                     " d=" + std::to_string(depth) +
                     " p=" + std::to_string(p.x) + "," + std::to_string(p.y) +
                     " size=" + std::to_string(size.width) + "," +
                     std::to_string(size.height) +
                     " tag=" + std::to_string(node->getTag()) +
                     " z=" + std::to_string(node->getLocalZOrder()) +
                     " name=" + node->getName() +
                     " children=" + std::to_string(node->getChildrenCount()));
        ++count;
        for (auto* child : node->getChildren()) LogFieldMapNodes(child, depth + 1, count);
    }

    void LogFieldRuntimeTree(void* fieldImpl) {
        if (!fieldImpl) return;
        auto* bytes = static_cast<std::uint8_t*>(fieldImpl);
        auto* fieldMap = *reinterpret_cast<cocos2d::Node**>(bytes + 0xb9c);
        if (!fieldMap) {
            QuickLoadLog("field map node=null");
            return;
        }
        QuickLoadLog("field map node=" +
                     std::to_string(reinterpret_cast<std::uintptr_t>(fieldMap)));
        int count = 0;
        LogFieldMapNodes(fieldMap, 0, count);
        if (auto* director = cocos2d::Director::getInstance()) {
            if (auto* scene = director->getRunningScene()) {
                QuickLoadLog("running scene node=" +
                             std::to_string(reinterpret_cast<std::uintptr_t>(scene)));
                count = 0;
                LogFieldMapNodes(scene, 0, count);
            }
        }
    }

    void LogFieldCoordinateCandidates(void* fieldImpl, std::uint8_t* canvas) {
        if (!fieldImpl || !canvas) return;
        auto* bytes = static_cast<std::uint8_t*>(fieldImpl);
        auto* state = *reinterpret_cast<std::uint8_t**>(bytes + 0x850);
        auto* movement = *reinterpret_cast<std::uint8_t**>(bytes + 0x854);
        if (!state || !movement) return;
        const auto active = *reinterpret_cast<std::int32_t*>(state + 0x11ec);
        QuickLoadLog("coord state active=" + std::to_string(active) +
                     " map=" + std::to_string(*reinterpret_cast<std::int32_t*>(state + 0x1010)) +
                     " tile=" + std::to_string(*reinterpret_cast<std::int32_t*>(state + 0x1014)) +
                     "," + std::to_string(*reinterpret_cast<std::int32_t*>(state + 0x1018)));
        QuickLoadLog("coord movement d=" +
                     std::to_string(*reinterpret_cast<std::int32_t*>(movement + 0x98)) + "," +
                     std::to_string(*reinterpret_cast<std::int32_t*>(movement + 0xa4)) +
                     " anchor=" + std::to_string(*reinterpret_cast<std::int32_t*>(movement + 0x148)) + "," +
                     std::to_string(*reinterpret_cast<std::int32_t*>(movement + 0x14c)) +
                     " target=" + std::to_string(*reinterpret_cast<std::int32_t*>(movement + 0x150)) + "," +
                     std::to_string(*reinterpret_cast<std::int32_t*>(movement + 0x154)) +
                     " bounds=" + std::to_string(*reinterpret_cast<std::int32_t*>(movement + 0x38)) + "," +
                     std::to_string(*reinterpret_cast<std::int32_t*>(movement + 0x40)));
        if (active >= 0 && active < 0x80 && (active & 1) == 0) {
            auto* record = canvas + 0x6940 + (active / 2) * 0x154;
            QuickLoadLog("coord record=" + std::to_string(reinterpret_cast<std::uintptr_t>(record)) +
                         " p84=" + std::to_string(*reinterpret_cast<std::int32_t*>(record + 0x84)) +
                         " p90=" + std::to_string(*reinterpret_cast<std::int32_t*>(record + 0x90)) +
                         " p94=" + std::to_string(*reinterpret_cast<std::int32_t*>(record + 0x94)) +
                         " p98=" + std::to_string(*reinterpret_cast<std::int32_t*>(record + 0x98)) +
                         " p9c=" + std::to_string(*reinterpret_cast<std::int32_t*>(record + 0x9c)) +
                         " p148=" + std::to_string(*reinterpret_cast<std::int32_t*>(record + 0x148)) +
                         " p14c=" + std::to_string(*reinterpret_cast<std::int32_t*>(record + 0x14c)));
        }
        QuickLoadLog("coord manager d=" +
                     std::to_string(*reinterpret_cast<std::int32_t*>(canvas + 0x1331c)) + "," +
                     std::to_string(*reinterpret_cast<std::int32_t*>(canvas + 0x13328)) +
                     " pos=" + std::to_string(*reinterpret_cast<std::int32_t*>(canvas + 0x133c4)) + "," +
                     std::to_string(*reinterpret_cast<std::int32_t*>(canvas + 0x133c8)));
    }

    void QuickLoadLog(const std::string& message) {
        wchar_t executablePath[MAX_PATH]{};
        const auto length = GetModuleFileNameW(nullptr, executablePath, MAX_PATH);
        const auto logPath = length != 0
            ? std::filesystem::path(executablePath).parent_path() / "ctext_quickload.log"
            : std::filesystem::current_path() / "ctext_quickload.log";
        std::ofstream log(logPath, std::ios::app);
        if (log) log << message << '\n';
    }
    std::string notificationText;
    int notificationFrames{};
    void QueueNotification(const std::string& text);

    using SaveStateInit = void(__thiscall*)(void*);
    using SaveStateCopy = void(__thiscall*)(void*, void*);
    using SaveStateFile = int(__fastcall*)(int, void*);
    using SaveStateSync = void(__thiscall*)(void*);
    // chrono.exe 0x57AC20 (RVA 0x17AC20): stdcall actor-record update.
    // It applies the pending x/y motion and mirrors the result into the
    // record's rendering coordinates.
    using FieldActorApplyMotion = void(__stdcall*)(void*);

    bool NativeQuickSave() {
        auto* manager = ct::ChronoCanvas::getInstance();
        if (!manager) return false;
        auto* canvas = reinterpret_cast<std::uint8_t*>(manager);
        if (currentFieldImpl) {
            auto* movement = *reinterpret_cast<std::uint8_t**>(
                static_cast<std::uint8_t*>(currentFieldImpl) + 0x854);
            auto* fieldState = *reinterpret_cast<std::uint8_t**>(
                static_cast<std::uint8_t*>(currentFieldImpl) + 0x850);
            if (fieldState) {
                savedFieldTileX = *reinterpret_cast<std::int32_t*>(fieldState + 0x1014);
                savedFieldTileY = *reinterpret_cast<std::int32_t*>(fieldState + 0x1018);
                QuickLoadLog("captured tile " + std::to_string(savedFieldTileX) + "," +
                             std::to_string(savedFieldTileY));
            }
            if (movement) {
                // +0x98/+0xa4 are per-frame input deltas and are normally zero
                // at rest. +0x150/+0x154 are the live actor coordinates used
                // by the field collision/encounter code.
                const auto active = *reinterpret_cast<std::int32_t*>(fieldState + 0x11ec);
                if (active >= 0 && active < 0x80 && (active & 1) == 0) {
                    auto* record = canvas + 0x6940 + (active / 2) * 0x154;
                    savedFieldX = *reinterpret_cast<std::int32_t*>(record + 0x84);
                    savedFieldY = *reinterpret_cast<std::int32_t*>(record + 0x90);
                } else {
                    savedFieldX = *reinterpret_cast<std::int32_t*>(movement + 0x150);
                    savedFieldY = *reinterpret_cast<std::int32_t*>(movement + 0x154);
                }
                savedFieldPositionValid = true;
                LOG_DEBUG("[ctext] quick actor position captured: " << savedFieldX << ", " << savedFieldY);
                QuickLoadLog("captured actor " + std::to_string(savedFieldX) + "," +
                             std::to_string(savedFieldY));
            }
            LogFieldRuntimeTree(currentFieldImpl);
            LogFieldCoordinateCandidates(currentFieldImpl, canvas);
        }
        auto* liveState = reinterpret_cast<std::uint8_t*>(manager) + ct::addr::SAVE_STATE_OFFSET;
        auto init = ADDR_AS(SaveStateInit, ct::addr::SAVE_STATE_INIT);
        auto toBuffer = ADDR_AS(SaveStateCopy, ct::addr::SAVE_STATE_TO_BUFFER);
        auto write = ADDR_AS(SaveStateFile, ct::addr::SAVE_STATE_WRITE);
        auto sync = ADDR_AS(SaveStateSync, ct::addr::SAVE_STATE_SYNC);
        // The native save path refreshes the serialized field/bookmark
        // cursor before copying the live state. Without this, map changes
        // are saved but the last synchronized field position is retained.
        sync(canvas + 0x68dc);
        // These are the exact bookmark-resume values consumed by FieldScene
        // while it constructs the player. Keep an explicit copy because the
        // serialized state does not appear to refresh them on this build.
        savedResumeX = *reinterpret_cast<std::uint32_t*>(canvas + 0x1099c);
        savedResumeY = *reinterpret_cast<std::uint32_t*>(canvas + 0x109a0);
        savedResumeDirection = *reinterpret_cast<std::uint32_t*>(canvas + 0x109a4);
        QuickLoadLog("captured resume " + std::to_string(savedResumeX) + "," +
                     std::to_string(savedResumeY) + "," +
                     std::to_string(savedResumeDirection));
        init(quickState.data());
        toBuffer(liveState, quickState.data());
        // The native routine returns zero on a successful write.
        return write(kQuickSlotBase + quickSlot, quickState.data()) == 0;
    }

    bool NativeQuickLoad() {
        auto* manager = ct::ChronoCanvas::getInstance();
        if (!manager) return false;
        auto init = ADDR_AS(SaveStateInit, ct::addr::SAVE_STATE_INIT);
        auto fromFile = ADDR_AS(SaveStateFile, ct::addr::SAVE_STATE_READ);
        auto sync = ADDR_AS(SaveStateSync, ct::addr::SAVE_STATE_SYNC);
        // 0x615E30 ends in `ret 8`: both arguments are stack arguments and
        // the callee removes them. Declaring this __cdecl corrupts ESP as
        // soon as the function returns because the caller removes them too.
        using SaveStateApplyFlow = void(__stdcall*)(void*, int);
        auto applyFlow = ADDR_AS(SaveStateApplyFlow, ct::addr::SAVE_STATE_APPLY_FLOW);
        init(quickState.data());
        if (fromFile(kQuickSlotBase + quickSlot, quickState.data()) != 0) return false;

        // Hidden quick-save files use slots 21..23, but the game's bookkeeping
        // only supports normal slots 0..19 plus bookmark slot 20. Publish the
        // logical bookmark slot before rebuilding common.bin; exposing 21..23
        // here would violate native array/range assumptions.
        auto* canvas = reinterpret_cast<std::uint8_t*>(manager);
        *reinterpret_cast<std::uint32_t*>(canvas + 0x68ec) =
            static_cast<std::uint32_t>(kNativeBookmarkSlot);
        sync(canvas + 0x68dc);

        // Modes 2/3 are the native bookmark variants. They set the additional
        // resume flag that makes field creation consume the saved map cursor
        // and coordinates, which ordinary save-point loading does not need.
        applyFlow(quickState.data(), 3);

        if (savedFieldPositionValid) {
            *reinterpret_cast<std::uint32_t*>(canvas + 0x1099c) = savedResumeX;
            *reinterpret_cast<std::uint32_t*>(canvas + 0x109a0) = savedResumeY;
            *reinterpret_cast<std::uint32_t*>(canvas + 0x109a4) = savedResumeDirection;
            *reinterpret_cast<std::uint32_t*>(canvas + 0x679c) = 1;
            restorePositionPending = true;
            QuickLoadLog("applied resume " + std::to_string(savedResumeX) + "," +
                         std::to_string(savedResumeY) + "," +
                         std::to_string(savedResumeDirection));
        }

        // Applying the serialized state only updates ChronoCanvas.  Native
        // bookmark loading then leaves the save/load scene and constructs a
        // fresh FieldScene; that constructor consumes the bookmark resume
        // flag and restores the saved map and coordinates.  F7 can be used
        // without opening the native save/load UI, so perform that final
        // scene replacement directly on the main thread.
        auto* director = cocos2d::Director::getInstance();
        if (!director) return false;
        auto* fieldScene = ct::scene::SceneManager::create(0x11, 0);
        if (!fieldScene) return false;
        director->replaceScene(fieldScene);
        return true;
    }

    void SetCurrentFieldImpl(void* fieldImpl) {
        currentFieldImpl = fieldImpl;
    }

    void RestoreFieldPosition(void* fieldImpl) {
        if (!restorePositionPending || !fieldImpl) return;
        auto* state = *reinterpret_cast<std::uint8_t**>(
            static_cast<std::uint8_t*>(fieldImpl) + 0x850);
        auto* canvas = reinterpret_cast<std::uint8_t*>(ct::ChronoCanvas::getInstance());
        if (!state || !canvas) return;
        const auto active = *reinterpret_cast<std::int32_t*>(state + 0x11ec);
        if (active < 0 || active >= 0x80 || (active & 1) != 0) return;
        auto* record = canvas + 0x6940 + (active / 2) * 0x154;

        // Do not write the position fields ourselves.  Queue exactly one
        // native actor-motion step: this is the same routine field movement
        // uses to update the transform and all of its dependent coordinates.
        const auto currentX = static_cast<std::uint16_t>(
            *reinterpret_cast<std::uint32_t*>(record + 0x84));
        const auto currentY = static_cast<std::uint16_t>(
            *reinterpret_cast<std::uint32_t*>(record + 0x90));
        const auto targetX = static_cast<std::uint16_t>(savedFieldX);
        const auto targetY = static_cast<std::uint16_t>(savedFieldY);
        const auto deltaX = static_cast<std::int16_t>(targetX - currentX);
        const auto deltaY = static_cast<std::int16_t>(targetY - currentY);
        *reinterpret_cast<std::int32_t*>(record + 0xa4) = deltaX;
        *reinterpret_cast<std::int32_t*>(record + 0xbc) = deltaY;
        *reinterpret_cast<std::int32_t*>(record + 0xe0) = 0;
        *reinterpret_cast<std::int32_t*>(record + 0xc8) = 1;
        ADDR_AS(FieldActorApplyMotion, ct::addr::FIELD_ACTOR_APPLY_MOTION)(record);
        // The step is complete. Do not leave a velocity behind for a later
        // native movement operation to consume.
        *reinterpret_cast<std::int32_t*>(record + 0xa4) = 0;
        *reinterpret_cast<std::int32_t*>(record + 0xbc) = 0;
        *reinterpret_cast<std::int32_t*>(record + 0xe0) = 0;
        restorePositionPending = false;
        QuickLoadLog("restored actor native current=" +
                     std::to_string(*reinterpret_cast<std::uint32_t*>(record + 0x84)) + "," +
                     std::to_string(*reinterpret_cast<std::uint32_t*>(record + 0x90)) +
                     " target=" + std::to_string(targetX) + "," + std::to_string(targetY) +
                     " delta=" + std::to_string(deltaX) + "," + std::to_string(deltaY));
    }

    void ProcessDeferredActions() {
        if (!quickLoadPending) return;
        quickLoadPending = false;
        const bool ok = NativeQuickLoad();
        LOG_DEBUG("[ctext] quick load slot " << quickSlot << ": " << (ok ? "ok" : "failed"));
        QueueNotification(ok ? "Quick load complete - slot " + std::to_string(quickSlot + 1)
                              : "Quick load unavailable - slot " + std::to_string(quickSlot + 1));
    }

    void QueueNotification(const std::string& text) {
        notificationText = text;
        notificationFrames = 180;
    }

    cocos2d::Label* CreateMenuLabel(const std::string& text, float size) {
        const auto& cfg = ctext::Config::Get();
        if (cfg.FontUseCustomFont && cfg.IsCustomFontAvailable()) {
            const auto path = (std::filesystem::current_path() / cfg.FontCustomFont).generic_string();
            if (auto* label = cocos2d::Label::create()) {
                cocos2d::TTFConfig ttf;
                ttf.fontFilePath = path;
                ttf.fontSize = size;
                if (label->setTTFConfig(ttf)) {
                    label->setString(text);
                    return label;
                }
                label->release();
            }
        }
        return cocos2d::Label::createWithSystemFont(text, "Arial", size);
    }

    class FilterCheckBox final : public cocos2d::Layer {
    public:
        CREATE_FUNC(FilterCheckBox);

        bool init() override {
            if (!cocos2d::Layer::init()) return false;
            setContentSize(cocos2d::Size(40.0f, 40.0f));
            checkbox_ = cocos2d::ui::CheckBox::create(
                "assets/cocos2d-ui/CheckBox_Normal.png",
                "assets/cocos2d-ui/CheckBox_Press.png",
                "assets/cocos2d-ui/CheckBoxNode_Normal.png",
                "assets/cocos2d-ui/CheckBox_Disable.png",
                "assets/cocos2d-ui/CheckBoxNode_Disable.png");
            checkbox_->setPosition(cocos2d::Vec2(20.0f, 20.0f));
            addChild(checkbox_);
            setOn(false);
            return true;
        }

        void setOn(bool value) {
            on_ = value;
            checkbox_->setSelected(on_);
        }

    private:
        cocos2d::ui::CheckBox* checkbox_{};
        bool on_{};
    };

    class ModMenuLayer final : public cocos2d::Layer {
    public:
        CREATE_FUNC(ModMenuLayer);

        bool init() override {
            if (!cocos2d::Layer::init()) return false;

            auto* director = cocos2d::Director::getInstance();
            const auto size = director->getVisibleSize();
            const auto origin = director->getVisibleOrigin();
            const auto panelSize = cocos2d::Size(size.width * 0.74f, size.height * 0.76f);

            auto* panel = cocos2d::LayerColor::create(
                cocos2d::Color4B(8, 14, 30, 246), panelSize.width, panelSize.height);
            panel->setPosition(origin.x + (size.width - panelSize.width) * 0.5f,
                               origin.y + (size.height - panelSize.height) * 0.5f);
            addChild(panel);

            auto* frame = cocos2d::DrawNode::create(2.0f);
            frame->drawRect(cocos2d::Vec2(4, 4),
                            cocos2d::Vec2(panelSize.width - 4, panelSize.height - 4),
                            cocos2d::Color4F(0.86f, 0.67f, 0.22f, 1.0f));
            frame->drawLine(cocos2d::Vec2(20, panelSize.height - 62),
                            cocos2d::Vec2(panelSize.width - 20, panelSize.height - 62),
                            cocos2d::Color4F(0.32f, 0.52f, 0.78f, 1.0f));
            panel->addChild(frame);

            title_ = CreateMenuLabel("CTExt", 22.0f);
            title_->setAnchorPoint(cocos2d::Vec2(0.5f, 1.0f));
            title_->setPosition(panelSize.width * 0.5f, panelSize.height - 20.0f);
            title_->setColor(cocos2d::Color3B(246, 214, 116));
            panel->addChild(title_);

            subtitle_ = CreateMenuLabel("EXTENDER MENU", 11.0f);
            subtitle_->setAnchorPoint(cocos2d::Vec2(0.5f, 1.0f));
            subtitle_->setPosition(panelSize.width * 0.5f, panelSize.height - 43.0f);
            subtitle_->setColor(cocos2d::Color3B(150, 180, 220));
            panel->addChild(subtitle_);

            const char* names[kMaxItemCount] = {
                "Font",
                "Pixel Filtering",
                "Diagonal Fix",
                "Close Menu",
                "", "", "", ""
            };
            const float firstY = panelSize.height - 106.0f;
            for (int i = 0; i < kVisibleItemCount; ++i) {
                items_[i] = CreateMenuLabel(names[i], 15.0f);
                items_[i]->setOverflow(cocos2d::Label::Overflow::RESIZE_HEIGHT);
                items_[i]->setWidth(panelSize.width - 120.0f);
                items_[i]->setAnchorPoint(cocos2d::Vec2(0.0f, 0.5f));
                items_[i]->setPosition(52.0f, firstY - i * 26.0f);
                panel->addChild(items_[i]);
            }
            cursor_ = CreateMenuLabel(">", 15.0f);
            cursor_->setColor(cocos2d::Color3B(255, 220, 96));
            panel->addChild(cursor_);
            checkbox_ = FilterCheckBox::create();
            checkbox_->setPosition(panelSize.width - 105.0f, firstY - 10.0f);
            panel->addChild(checkbox_);

            speedSlider_ = cocos2d::ui::Slider::create();
            if (speedSlider_) {
                speedSlider_->loadBarTexture("assets/cocos2d-ui/Slider_Back.png");
                speedSlider_->loadSlidBallTextures(
                    "assets/cocos2d-ui/SliderNode_Normal.png",
                    "assets/cocos2d-ui/SliderNode_Press.png",
                    "assets/cocos2d-ui/SliderNode_Disable.png");
                speedSlider_->loadProgressBarTexture("assets/cocos2d-ui/Slider_PressBar.png");
                speedSlider_->setContentSize(cocos2d::Size(panelSize.width - 150.0f, 22.0f));
                speedSlider_->setPosition(cocos2d::Vec2(panelSize.width * 0.5f, 62.0f));
                speedSlider_->setMaxPercent(100);
                speedSlider_->addEventListener([this](cocos2d::Ref*, cocos2d::ui::Slider::EventType type) {
                    if (type != cocos2d::ui::Slider::EventType::ON_PERCENTAGE_CHANGED || !speedSlider_) return;
                    SetSpeedFromSlider(speedSlider_->getPercent());
                });
                panel->addChild(speedSlider_);
            }
            speedValue_ = CreateMenuLabel("100%", 13.0f);
            speedValue_->setAnchorPoint(cocos2d::Vec2(0.5f, 0.0f));
            speedValue_->setPosition(panelSize.width * 0.5f, 84.0f);
            speedValue_->setColor(cocos2d::Color3B(246, 214, 116));
            panel->addChild(speedValue_);

            hint_ = CreateMenuLabel("UP/DOWN SELECT   ENTER OPEN   SPACE CLOSE   F1 GOD  F5 SAVE  F6 SLOT  F7 LOAD", 9.0f);
            hint_->setAnchorPoint(cocos2d::Vec2(0.5f, 0.0f));
            hint_->setPosition(panelSize.width * 0.5f, 16.0f);
            hint_->setColor(cocos2d::Color3B(155, 170, 195));
            panel->addChild(hint_);
            RefreshSelection();
            return true;
        }

        void MoveSelection(int delta) {
            const int count = submenu_ ? submenuItemCount_ : kMainItemCount;
            selected_ = (selected_ + delta + count) % count;
            RefreshSelection();
        }

        bool EnterSelection() {
            if (submenu_) {
                if (selected_ == submenuItemCount_ - 1) {
                    submenu_ = false;
                    selected_ = 0;
                    firstVisible_ = 0;
                    RefreshSelection();
                } else if (selectedMain_ == 0) {
                    RefreshInstalledFonts();
                    if (selected_ == 0) {
                        ctext::Config::Get().SetFontUseCustomFont(false);
                        LeaveSubmenu(true);
                    } else if (selected_ < 1 + static_cast<int>(fontPaths_.size())) {
                        ctext::Config::Get().SetCustomFontPath(fontPaths_[selected_ - 1]);
                        LeaveSubmenu(true);
                    } else if (selected_ == 1 + static_cast<int>(fontPaths_.size())) {
                        ChooseCustomFont();
                    }
                } else if (selectedMain_ == 1 && selected_ == 0) {
                    const bool enabled = !ctext::Config::Get().GraphicsForceNearestFilter;
                    ctext::texture_filter::SetNearest(enabled);
                    if (checkbox_) checkbox_->setOn(enabled);
                    LeaveSubmenu();
                } else if (selectedMain_ == 2 && selected_ < 2) {
                    ctext::Config::Get().SetFixesRevertDiagonalMovement(selected_ == 0);
                    LeaveSubmenu();
                } else if (selectedMain_ == 3 && selected_ == 0) {
                    // The speed is adjusted with the native slider or left/right.
                } else if (selectedMain_ == 3) {
                    LeaveSubmenu();
                } else if (selectedMain_ == 4 && selected_ < 2) {
                    ctext::Config::Get().SetGameplayGodMode(selected_ == 1);
                    LeaveSubmenu();
                }
                return false;
            }
            if (selected_ == kMainItemCount - 1)
                return true;
            const int choice = selected_;
            if (choice == 1) {
                const bool enabled = !ctext::Config::Get().GraphicsForceNearestFilter;
                ctext::texture_filter::SetNearest(enabled);
                if (checkbox_) checkbox_->setOn(enabled);
                RefreshSelection();
                return false;
            }
            if (choice == 2) {
                ctext::Config::Get().SetFixesRevertDiagonalMovement(
                    !ctext::Config::Get().FixesRevertDiagonalMovement);
                RefreshSelection();
                return false;
            }
            if (choice == 4) {
                ctext::Config::Get().SetGameplayGodMode(
                    !ctext::Config::Get().GameplayGodMode);
                RefreshSelection();
                return false;
            }
            submenu_ = true;
            selected_ = 0;
            firstVisible_ = 0;
            selectedMain_ = choice;
            RefreshSelection();
            return false;
        }

    private:
        void LeaveSubmenu(bool rebuild = false) {
            submenu_ = false;
            selected_ = 0;
            firstVisible_ = 0;
            if (rebuild) {
                if (auto* director = cocos2d::Director::getInstance()) {
                    if (auto* scene = director->getRunningScene()) {
                        removeFromParentAndCleanup(true);
                        menuLayer = ModMenuLayer::create();
                        if (menuLayer) scene->addChild(menuLayer, 10000);
                        return;
                    }
                }
            }
            ApplyMenuFont();
            RefreshSelection();
        }

        void ApplyMenuFont() {
            const auto& cfg = ctext::Config::Get();
            const auto apply = [&](cocos2d::Label* label) {
                if (!label) return;
                if (cfg.FontUseCustomFont && cfg.IsCustomFontAvailable()) {
                    cocos2d::TTFConfig ttf;
                    ttf.fontFilePath = (std::filesystem::current_path() / cfg.FontCustomFont).generic_string();
                    ttf.fontSize = label->getSystemFontSize();
                    label->setTTFConfig(ttf);
                } else {
                    label->setSystemFontName("Arial");
                }
            };
            for (auto* label : items_) apply(label);
            apply(title_);
            apply(subtitle_);
            apply(hint_);
            apply(cursor_);
        }

        void RefreshInstalledFonts() {
            fontPaths_.clear();
            fontNames_.clear();
            const auto addFont = [&](const std::filesystem::path& path) {
                if (!std::filesystem::is_regular_file(path)) return;
                const auto ext = path.extension().string();
                if (ext != ".ttf" && ext != ".otf" && ext != ".TTF" && ext != ".OTF") return;
                const auto relative = std::filesystem::relative(path, std::filesystem::current_path());
                const auto filename = path.filename().string();
                if ((filename == "ChronoType.ttf" || filename == "chronotype.ttf") &&
                    path.lexically_normal() !=
                        (std::filesystem::current_path() / "ChronoType.ttf").lexically_normal())
                    return;
                for (const auto& installed : fontNames_)
                    if (_stricmp(installed.c_str(), filename.c_str()) == 0) return;
                const auto pathString = relative.generic_string();
                if (std::find(fontPaths_.begin(), fontPaths_.end(), pathString) != fontPaths_.end()) return;
                fontPaths_.push_back(pathString);
                fontNames_.push_back(path.filename().string());
            };
            addFont(std::filesystem::current_path() / "ChronoType.ttf");
            const auto fontsDir = std::filesystem::current_path() / "fonts";
            if (std::filesystem::is_directory(fontsDir)) {
                for (const auto& entry : std::filesystem::directory_iterator(fontsDir))
                    addFont(entry.path());
            }
        }

        void ChooseCustomFont() {
            wchar_t fileName[MAX_PATH]{};
            OPENFILENAMEW dialog{};
            dialog.lStructSize = sizeof(dialog);
            dialog.hwndOwner = GetForegroundWindow();
            dialog.lpstrFilter = L"Font files (*.ttf;*.otf)\0*.ttf;*.otf\0All files (*.*)\0*.*\0";
            dialog.lpstrFile = fileName;
            dialog.nMaxFile = MAX_PATH;
            dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
            if (!GetOpenFileNameW(&dialog)) return;

            try {
                const std::filesystem::path source(fileName);
                const bool chronoType = source.filename().string() == "ChronoType.ttf";
                const auto destination = chronoType
                    ? std::filesystem::current_path() / "ChronoType.ttf"
                    : std::filesystem::current_path() / "fonts" / source.filename();
                if (!chronoType)
                    std::filesystem::create_directories(destination.parent_path());
                std::filesystem::copy_file(source, destination,
                    std::filesystem::copy_options::overwrite_existing);
                RefreshInstalledFonts();
                RefreshSelection();
            } catch (...) {
                // Leave the previous selection intact if the copy fails.
            }
        }

        void RefreshSelection() {
            static const char* mainNames[kMainItemCount] = {
                "Font", "Pixel Filtering", "Diagonal Fix", "Game Speed", "God Mode", "Close Menu"
            };
            static const char* diagonalNames[] = {
                "Enable Fix", "Disable Fix", "Back"
            };
            static const char* speedNames[] = {
                "Adjust Speed", "Back"
            };
            static const char* godNames[] = {
                "Disabled", "Enabled", "Back"
            };
            static const char* pixelNames[] = {
                "Nearest Filtering", "Back"
            };
            const char** names = mainNames;
            int count = kMainItemCount;
            if (submenu_) {
                if (selectedMain_ == 0) {
                    RefreshInstalledFonts();
                    submenuItemCount_ = 3 + static_cast<int>(fontPaths_.size());
                }
                else if (selectedMain_ == 1) { names = pixelNames; submenuItemCount_ = 2; }
                else if (selectedMain_ == 2) { names = diagonalNames; submenuItemCount_ = 3; }
                else if (selectedMain_ == 3) { names = speedNames; submenuItemCount_ = 2; }
                else { names = godNames; submenuItemCount_ = 3; }
                count = submenuItemCount_;
            }
            if (selected_ < firstVisible_) firstVisible_ = selected_;
            if (selected_ >= firstVisible_ + kVisibleItemCount)
                firstVisible_ = selected_ - kVisibleItemCount + 1;
            for (int i = 0; i < kVisibleItemCount; ++i) {
                const int index = firstVisible_ + i;
                items_[i]->setVisible(index < count);
                if (index >= count) continue;
                if (!items_[i]) continue;
                items_[i]->setColor(index == selected_
                    ? cocos2d::Color3B(255, 220, 96)
                    : cocos2d::Color3B(220, 225, 235));
                if (!submenu_ && index == 0) {
                    const auto& cfg = ctext::Config::Get();
                    const auto filename = std::filesystem::path(cfg.FontCustomFont).filename().string();
                    items_[i]->setString(cfg.FontUseCustomFont
                        && cfg.IsCustomFontAvailable() && !filename.empty()
                        ? std::string("Font: ") + filename : "Font: Default");
                } else if (!submenu_ && index == 1) {
                    items_[i]->setString(ctext::Config::Get().GraphicsForceNearestFilter
                        ? "Pixel Filtering: Nearest" : "Pixel Filtering: Linear");
                } else if (!submenu_ && index == 2) {
                    items_[i]->setString(ctext::Config::Get().FixesRevertDiagonalMovement
                        ? "Diagonal Fix: Enabled" : "Diagonal Fix: Disabled");
                } else if (!submenu_ && index == 3) {
                    items_[i]->setString("Game Speed: " + SpeedText());
                } else if (!submenu_ && index == 4) {
                    items_[i]->setString(ctext::Config::Get().GameplayGodMode
                        ? "God Mode: Enabled" : "God Mode: Disabled");
                } else if (submenu_ && selectedMain_ == 0) {
                    if (index == 0)
                        items_[i]->setString("Default Font");
                    else if (index < 1 + static_cast<int>(fontNames_.size()))
                        items_[i]->setString(fontNames_[index - 1]);
                    else if (index == 1 + static_cast<int>(fontNames_.size()))
                        items_[i]->setString("Install Font...");
                    else
                        items_[i]->setString("Back");
                } else {
                    items_[i]->setString(names[index]);
                }
            }
            if (cursor_)
                cursor_->setPosition(35.0f, items_[selected_ - firstVisible_]->getPositionY());
            if (title_)
                title_->setString(submenu_ ? "CTExt  /  MOD" : "CTExt");
            if (checkbox_) {
                checkbox_->setVisible(false);
                checkbox_->setOn(ctext::Config::Get().GraphicsForceNearestFilter);
            }
            const bool showSpeed = submenu_ && selectedMain_ == 3;
            if (speedSlider_) {
                speedSlider_->setVisible(showSpeed);
                if (showSpeed) speedSlider_->setPercent(SliderPercent());
            }
            if (speedValue_) {
                speedValue_->setVisible(showSpeed);
                speedValue_->setString(SpeedText());
            }
        }

        std::string SpeedText() const {
            const int percent = static_cast<int>(ctext::Config::Get().GameplayTimeScale * 100.0f + 0.5f);
            return std::to_string(std::clamp(percent, 10, 300)) + "%";
        }

        int SliderPercent() const {
            const int percent = std::clamp(static_cast<int>(ctext::Config::Get().GameplayTimeScale * 100.0f + 0.5f), 10, 300);
            return (percent - 10 + 1) * 100 / 290;
        }

        void SetSpeedFromSlider(int sliderPercent) {
            const int percent = std::clamp(10 + ((sliderPercent * 290 + 145) / 100 / 10) * 10, 10, 300);
            ctext::Config::Get().SetGameplayTimeScale(static_cast<float>(percent) / 100.0f);
            ApplyGameSpeed(ctext::Config::Get().GameplayTimeScale);
            if (speedValue_) speedValue_->setString(SpeedText());
            if (!submenu_) RefreshSelection();
        }

    public:
        void NudgeSpeed(int deltaPercent) {
            const int current = std::clamp(static_cast<int>(ctext::Config::Get().GameplayTimeScale * 100.0f + 0.5f), 10, 300);
            const int next = std::clamp(current + deltaPercent, 10, 300);
            ctext::Config::Get().SetGameplayTimeScale(static_cast<float>(next) / 100.0f);
            ApplyGameSpeed(ctext::Config::Get().GameplayTimeScale);
            if (speedSlider_) speedSlider_->setPercent(SliderPercent());
            if (speedValue_) speedValue_->setString(SpeedText());
            RefreshSelection();
        }

        bool IsSpeedSubmenu() const { return submenu_ && selectedMain_ == 3; }

        cocos2d::Label* title_{};
        cocos2d::Label* subtitle_{};
        cocos2d::Label* hint_{};
        cocos2d::Label* cursor_{};
        FilterCheckBox* checkbox_{};
        cocos2d::ui::Slider* speedSlider_{};
        cocos2d::Label* speedValue_{};
        cocos2d::Label* items_[kVisibleItemCount]{};
        int selected_{};
        int selectedMain_{};
        int firstVisible_{};
        int submenuItemCount_{};
        bool submenu_{};
        std::vector<std::string> fontPaths_;
        std::vector<std::string> fontNames_;
    };

    class StatusOverlay final : public cocos2d::Layer {
    public:
        CREATE_FUNC(StatusOverlay);

        bool init() override {
            if (!cocos2d::Layer::init()) return false;
            speedBadge_ = cocos2d::LayerColor::create(cocos2d::Color4B(8, 14, 30, 150), 42.0f, 24.0f);
            godBadge_ = cocos2d::LayerColor::create(cocos2d::Color4B(8, 14, 30, 150), 42.0f, 24.0f);
            if (speedBadge_) { speedBadge_->setPosition(2.0f, 2.0f); addChild(speedBadge_); }
            if (godBadge_) { godBadge_->setPosition(48.0f, 2.0f); addChild(godBadge_); }
            speed_ = CreateMenuLabel("SPD 200%", 9.0f);
            god_ = CreateMenuLabel("GOD", 9.0f);
            notificationBadge_ = cocos2d::LayerColor::create(cocos2d::Color4B(8, 14, 30, 225), 300.0f, 30.0f);
            notification_ = CreateMenuLabel("", 12.0f);
            if (notificationBadge_) addChild(notificationBadge_);
            if (notification_) {
                notification_->setAnchorPoint(cocos2d::Vec2(0.5f, 0.5f));
                notification_->setColor(cocos2d::Color3B(246, 214, 116));
                addChild(notification_);
            }
            if (speed_) {
                speed_->setAnchorPoint(cocos2d::Vec2(0.5f, 0.5f));
                speed_->setPosition(23.0f, 14.0f);
                speed_->setColor(cocos2d::Color3B(230, 210, 150));
                addChild(speed_);
            }
            if (god_) {
                god_->setAnchorPoint(cocos2d::Vec2(0.5f, 0.5f));
                god_->setPosition(69.0f, 14.0f);
                god_->setColor(cocos2d::Color3B(160, 205, 170));
                addChild(god_);
            }
            Update();
            return true;
        }

        void Update() {
            const int percent = std::clamp(static_cast<int>(ctext::Config::Get().GameplayTimeScale * 100.0f + 0.5f), 10, 300);
            const bool speedActive = percent != 100;
            const bool godActive = ctext::Config::Get().GameplayGodMode;
            const bool showNotification = notificationFrames > 0 && !notificationText.empty();
            setVisible(speedActive || godActive || showNotification);
            if (speed_) {
                speed_->setVisible(speedActive);
                speed_->setString("SPD " + std::to_string(percent) + "%");
            }
            if (god_) {
                god_->setVisible(godActive);
                god_->setString("GOD MODE");
            }
            if (speedBadge_) speedBadge_->setVisible(speedActive);
            if (godBadge_) godBadge_->setVisible(godActive);
            if (auto* director = cocos2d::Director::getInstance()) {
                const auto size = director->getVisibleSize();
                const auto origin = director->getVisibleOrigin();
                setContentSize(size);
                setPosition(origin);
                if (speedBadge_) speedBadge_->setPosition(size.width - 110.0f, size.height - 34.0f);
                if (godBadge_) godBadge_->setPosition(size.width - 64.0f, size.height - 34.0f);
                if (speed_) speed_->setPosition(size.width - 89.0f, size.height - 22.0f);
                if (god_) god_->setPosition(size.width - 44.0f, size.height - 22.0f);
                if (notificationBadge_) {
                    notificationBadge_->setPosition((size.width - 300.0f) * 0.5f, size.height * 0.18f);
                    notificationBadge_->setVisible(showNotification);
                }
                if (notification_) {
                    notification_->setPosition(size.width * 0.5f, size.height * 0.18f + 15.0f);
                    notification_->setString(showNotification ? notificationText : "");
                    notification_->setVisible(showNotification);
                }
                if (notificationFrames > 0) --notificationFrames;
            }
        }

    private:
        cocos2d::LayerColor* speedBadge_{};
        cocos2d::LayerColor* godBadge_{};
        cocos2d::Label* speed_{};
        cocos2d::Label* god_{};
        cocos2d::LayerColor* notificationBadge_{};
        cocos2d::Label* notification_{};
    };

    void EnsureStatusOverlay() {
        auto* director = cocos2d::Director::getInstance();
        auto* scene = director ? director->getRunningScene() : nullptr;
        if (!scene) return;
        // A scene transition destroys the old scene and its children. Never
        // dereference the old overlay pointer while crossing that boundary.
        if (statusScene != scene) {
            statusOverlay = nullptr;
            statusScene = scene;
        }
        if (!statusOverlay) {
            statusOverlay = StatusOverlay::create();
            if (statusOverlay) scene->addChild(statusOverlay, 9999);
        }
        if (statusOverlay) static_cast<StatusOverlay*>(statusOverlay)->Update();
    }

    void CloseMenu() {
        if (menuLayer) {
            menuLayer->removeFromParentAndCleanup(true);
            menuLayer = nullptr;
        }
        if (pausedByMenu) {
            if (auto* director = cocos2d::Director::getInstance()) {
                if (auto* scheduler = director->getScheduler()) {
                    scheduler->resumeTargets(pausedTargets);
                    scheduler->setTimeScale(ctext::Config::Get().GameplayTimeScale);
                }
            }
            pausedTargets.clear();
            pausedByMenu = false;
        }
    }

    bool spaceWasDown{};
    bool upWasDown{};
    bool downWasDown{};
    bool enterWasDown{};
    bool leftWasDown{};
    bool rightWasDown{};
    bool functionWasDown[12]{};

    bool pressed(int key, bool& wasDown) {
        const bool down = (GetAsyncKeyState(key) & 0x8000) != 0;
        const bool result = down && !wasDown;
        wasDown = down;
        return result;
    }
}

export namespace ctext::mod_menu {
    void SetCurrentFieldImpl(void* fieldImpl) {
        ::SetCurrentFieldImpl(fieldImpl);
    }

    void RestoreFieldPosition(void* fieldImpl) {
        ::RestoreFieldPosition(fieldImpl);
    }

    void ProcessDeferredActions() {
        ::ProcessDeferredActions();
    }

    bool HandleFieldInput() {
        static bool speedApplied = false;
        if (!speedApplied) {
            ApplyGameSpeed(ctext::Config::Get().GameplayTimeScale);
            speedApplied = true;
        }
        const bool toggle = pressed(VK_SPACE, spaceWasDown);
        const bool up = pressed(VK_UP, upWasDown);
        const bool down = pressed(VK_DOWN, downWasDown);
        const bool enter = pressed(VK_RETURN, enterWasDown);
        const bool left = pressed(VK_LEFT, leftWasDown);
        const bool right = pressed(VK_RIGHT, rightWasDown);

        for (int i = 0; i < 12; ++i) {
            if (!pressed(VK_F1 + i, functionWasDown[i])) continue;
            if (i == 0) {
                ctext::Config::Get().SetGameplayGodMode(!ctext::Config::Get().GameplayGodMode);
            } else if (i == 1) {
                if (menuLayer) menuLayer->NudgeSpeed(10);
                else {
                    const int value = std::clamp(static_cast<int>(ctext::Config::Get().GameplayTimeScale * 100.0f + 0.5f) + 10, 10, 300);
                    ctext::Config::Get().SetGameplayTimeScale(static_cast<float>(value) / 100.0f);
                    ApplyGameSpeed(ctext::Config::Get().GameplayTimeScale);
                }
            } else if (i == 2) {
                if (menuLayer) menuLayer->NudgeSpeed(-10);
                else {
                    const int value = std::clamp(static_cast<int>(ctext::Config::Get().GameplayTimeScale * 100.0f + 0.5f) - 10, 10, 300);
                    ctext::Config::Get().SetGameplayTimeScale(static_cast<float>(value) / 100.0f);
                    ApplyGameSpeed(ctext::Config::Get().GameplayTimeScale);
                }
            } else if (i == 3) {
                ctext::Config::Get().SetGameplayTimeScale(1.0f);
                ApplyGameSpeed(1.0f);
            } else if (i == 4) {
                const bool ok = NativeQuickSave();
                LOG_DEBUG("[ctext] quick save slot " << quickSlot << ": " << (ok ? "ok" : "failed"));
                QueueNotification(ok ? "Quick save complete - slot " + std::to_string(quickSlot + 1)
                                      : "Quick save failed - slot " + std::to_string(quickSlot + 1));
            } else if (i == 5) {
                quickSlot = (quickSlot + 1) % kQuickSlotCount;
                LOG_DEBUG("[ctext] quick save slot selected: " << quickSlot);
                QueueNotification("Quick-save slot " + std::to_string(quickSlot + 1));
            } else if (i == 6) {
                if (menuLayer) CloseMenu();
                // The load flow replaces the active field/scene. Queue it for
                // the main-loop boundary instead of running it inside the
                // movement/input hook that detected F7.
                quickLoadPending = true;
                QueueNotification("Quick load pending - slot " + std::to_string(quickSlot + 1));
            }
        }

        if (toggle) {
            if (menuLayer) {
                CloseMenu();
            } else if (auto* director = cocos2d::Director::getInstance()) {
                if (auto* scene = director->getRunningScene()) {
                    if (auto* scheduler = director->getScheduler()) {
                        pausedTargets = scheduler->pauseAllTargets();
                        pausedByMenu = true;
                    }
                    menuLayer = ModMenuLayer::create();
                    if (menuLayer) {
                        scene->addChild(menuLayer, 10000);
                    }
                }
            }
        } else if (menuLayer) {
            if (up) menuLayer->MoveSelection(-1);
            if (down) menuLayer->MoveSelection(1);
            if (left && menuLayer->IsSpeedSubmenu()) menuLayer->NudgeSpeed(-10);
            if (right && menuLayer->IsSpeedSubmenu()) menuLayer->NudgeSpeed(10);
            if (enter && menuLayer->EnterSelection()) {
                CloseMenu();
            }
        }
        return menuLayer != nullptr;
    }

    void RefreshStatusOverlay() {
        ProcessDeferredActions();
        EnsureStatusOverlay();
    }

    bool IsOpen() {
        return menuLayer != nullptr;
    }

}
