#ifndef __ZELDA_CONFIG_H__
#define __ZELDA_CONFIG_H__

#include <filesystem>
#include <string_view>
#include "ultramodern/config.hpp"
#include "recomp_input.h"

namespace zelda64 {
    constexpr std::u8string_view program_id = u8"Quest64Recompiled";
    constexpr std::string_view program_name = "Quest 64: Recompiled";

    // TODO: Move loading configs to the runtime once we have a way to allow per-project customization.
    void load_config();
    void save_config();
    
    void reset_input_bindings();
    void reset_cont_input_bindings();
    void reset_kb_input_bindings();
    void reset_single_input_binding(recomp::InputDevice device, recomp::GameInput input);

    std::filesystem::path get_app_folder_path();
    
    bool get_debug_mode_enabled();
    void set_debug_mode_enabled(bool enabled);
    
    enum class FilmGrainMode {
        On,
        Off,
        OptionCount
    };

    NLOHMANN_JSON_SERIALIZE_ENUM(zelda64::FilmGrainMode, {
        {zelda64::FilmGrainMode::On, "On"},
        {zelda64::FilmGrainMode::Off, "Off"}
    });

    enum class TargetingMode {
        Switch,
        Hold,
        OptionCount
    };

    NLOHMANN_JSON_SERIALIZE_ENUM(zelda64::TargetingMode, {
        {zelda64::TargetingMode::Switch, "Switch"},
        {zelda64::TargetingMode::Hold, "Hold"}
    });

    TargetingMode get_targeting_mode();
    void set_targeting_mode(TargetingMode mode);

    enum class AimInvertMode {
        On,
        Off,
        OptionCount
    };

    enum class RadioBoxMode {
        Expand,
        Original,
        OptionCount
    };

    NLOHMANN_JSON_SERIALIZE_ENUM(zelda64::RadioBoxMode, {
        {zelda64::RadioBoxMode::Expand, "Expand"},
        {zelda64::RadioBoxMode::Original, "Original"}
    });

    NLOHMANN_JSON_SERIALIZE_ENUM(zelda64::AimInvertMode, {
        {zelda64::AimInvertMode::On, "On"},
        {zelda64::AimInvertMode::Off, "Off"},
    });

    RadioBoxMode get_radio_comm_box_mode();
    void set_radio_comm_box_mode(RadioBoxMode mode);

    AimInvertMode get_analog_camera_invert_mode();
    void set_analog_camera_invert_mode(AimInvertMode mode);

    enum class AnalogCamMode {
        On,
        Off,
		OptionCount
    };

    NLOHMANN_JSON_SERIALIZE_ENUM(zelda64::AnalogCamMode, {
        {zelda64::AnalogCamMode::On, "On"},
        {zelda64::AnalogCamMode::Off, "Off"}
    });

    FilmGrainMode get_film_grain_mode();
    void set_film_grain_mode(FilmGrainMode mode);

    AimInvertMode get_invert_y_axis_mode();
    void set_invert_y_axis_mode(AimInvertMode mode);

    void open_quit_game_prompt();
};

#endif
