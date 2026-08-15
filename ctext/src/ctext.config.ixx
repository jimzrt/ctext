module;

#include <nlohmann/json.hpp>

export module ctext.config;

import ctext.singleton;

import std;


export namespace ctext {
	class Config final : public Singleton<Config> {
		friend class Singleton<Config>;


	public:
		bool FixesRevertDiagonalMovement;
		bool FixesFixBgmResumeAfterBattle;

		bool GraphicsForceNearestFilter;
		float GameplayTimeScale;
		bool GameplayGodMode;

		void SetGraphicsForceNearestFilter(bool enabled) {
			GraphicsForceNearestFilter = enabled;
			std::ifstream input("ctext.json");
			if (!input) return;
			auto cfg = nlohmann::json::parse(input, nullptr, true, true, true);
			input.close();
			cfg["graphics"]["force_nearest_filter"] = enabled;
			std::ofstream output("ctext.json");
			if (output) output << cfg.dump(2) << '\n';
		}

		void SetGameplayTimeScale(float scale) {
			GameplayTimeScale = scale;
			std::ifstream input("ctext.json");
			if (!input) return;
			auto cfg = nlohmann::json::parse(input, nullptr, true, true, true);
			input.close();
			cfg["gameplay"]["time_scale"] = scale;
			std::ofstream output("ctext.json");
			if (output) output << cfg.dump(2) << '\n';
		}

		void SetGameplayGodMode(bool enabled) {
			GameplayGodMode = enabled;
			std::ifstream input("ctext.json");
			if (!input) return;
			auto cfg = nlohmann::json::parse(input, nullptr, true, true, true);
			input.close();
			cfg["gameplay"]["god_mode"] = enabled;
			std::ofstream output("ctext.json");
			if (output) output << cfg.dump(2) << '\n';
		}

		void SetFontUseCustomFont(bool enabled) {
			FontUseCustomFont = enabled;
			std::ifstream input("ctext.json");
			if (!input) return;
			auto cfg = nlohmann::json::parse(input, nullptr, true, true, true);
			input.close();
			cfg["font"]["use_custom_font"] = enabled;
			std::ofstream output("ctext.json");
			if (output) output << cfg.dump(2) << '\n';
		}

		void SetCustomFontPath(const std::string& path) {
			FontCustomFont = path;
			FontUseCustomFont = true;
			std::ifstream input("ctext.json");
			if (!input) return;
			auto cfg = nlohmann::json::parse(input, nullptr, true, true, true);
			input.close();
			cfg["font"]["use_custom_font"] = true;
			cfg["font"]["custom_font"] = path;
			std::ofstream output("ctext.json");
			if (output) output << cfg.dump(2) << '\n';
		}

		void SetFixesRevertDiagonalMovement(bool enabled) {
			FixesRevertDiagonalMovement = enabled;
			std::ifstream input("ctext.json");
			if (!input) return;
			auto cfg = nlohmann::json::parse(input, nullptr, true, true, true);
			input.close();
			cfg["fixes"]["revert_diagonal_movement"] = enabled;
			std::ofstream output("ctext.json");
			if (output) output << cfg.dump(2) << '\n';
		}

		bool IsCustomFontAvailable() const {
			if (FontCustomFont.empty()) return false;
			return std::filesystem::is_regular_file(
				std::filesystem::current_path() / FontCustomFont);
		}

		bool FontForceNearestFilter;
		bool FontUseCustomFont;
		std::string FontCustomFont;
		bool FontUseFixedFontSize;
		int FontFixedFontSize;

		bool MiscDisableFieldActionIndicator;

		bool ModsEnabled;
		bool ModsEnableCtpLoading;
		std::vector<std::string> ModsLoadOrder;


	private:
		Config() {
			std::ifstream file("ctext.json");
			auto cfg = nlohmann::json::parse(file, nullptr, true, true, true);

			FixesRevertDiagonalMovement = cfg["fixes"]["revert_diagonal_movement"];
			FixesFixBgmResumeAfterBattle = cfg["fixes"]["fix_bgm_resume_after_battle"];

			GraphicsForceNearestFilter = cfg["graphics"]["force_nearest_filter"];
			GameplayTimeScale = cfg.contains("gameplay")
				? cfg["gameplay"].value("time_scale", 1.0f) : 1.0f;
			GameplayGodMode = cfg.contains("gameplay")
				? cfg["gameplay"].value("god_mode", false) : false;

			FontForceNearestFilter = cfg["font"]["force_nearest_filter"];
			FontUseCustomFont = cfg["font"]["use_custom_font"];
			FontCustomFont = cfg["font"]["custom_font"];
			FontUseFixedFontSize = cfg["font"]["use_fixed_font_size"];
			FontFixedFontSize = cfg["font"]["fixed_font_size"];

			MiscDisableFieldActionIndicator = cfg["misc"]["disable_field_action_indicator"];

			ModsEnabled = cfg["mods"]["enabled"];
			ModsEnableCtpLoading = cfg["mods"]["enable_ctp_loading"];
			ModsLoadOrder = cfg["mods"]["load_order"];

			file.close();
		}
	};
}
