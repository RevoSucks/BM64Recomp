#include <atomic>
#include <mutex>

#include "ultramodern/ultramodern.hpp"
#include "recomp.h"
#include "recomp_input.h"
#include "zelda_config.h"
#include "recomp_ui.h"
#include "SDL.h"
#include "promptfont.h"
#include "GamepadMotion.hpp"

constexpr float axis_threshold = 0.5f;

struct ControllerState {
    SDL_GameController* controller;
    std::array<float, 3> latest_accelerometer;
    GamepadMotion motion;
    uint32_t prev_gyro_timestamp;
    ControllerState() : controller{}, latest_accelerometer{}, motion{}, prev_gyro_timestamp{} {
        motion.Reset();
        motion.SetCalibrationMode(GamepadMotionHelpers::CalibrationMode::Stillness | GamepadMotionHelpers::CalibrationMode::SensorFusion);
    };
};

// Per-port mode state
static std::array<std::atomic<recomp::ControllerPortMode>, recomp::max_ports> port_modes = {
    recomp::ControllerPortMode::Keyboard,
    recomp::ControllerPortMode::Off,
    recomp::ControllerPortMode::Off,
    recomp::ControllerPortMode::Off,
};

recomp::ControllerPortMode recomp::get_port_mode(int port) {
    if (port < 0 || port >= recomp::max_ports) return recomp::ControllerPortMode::Off;
    return port_modes[port].load();
}

void recomp::set_port_mode(int port, recomp::ControllerPortMode mode) {
    if (port < 0 || port >= recomp::max_ports) return;
    port_modes[port].store(mode);
}

int recomp::get_port_count() {
    return recomp::max_ports;
}

static struct {
    const Uint8* keys = nullptr;
    SDL_Keymod keymod = SDL_Keymod::KMOD_NONE;
    int numkeys = 0;
    std::atomic_int32_t mouse_wheel_pos = 0;
    std::mutex cur_controllers_mutex;
    std::vector<SDL_GameController*> cur_controllers{};
    std::unordered_map<SDL_JoystickID, ControllerState> controller_states;

    // Per-port controller assignment
    std::array<SDL_JoystickID, recomp::max_ports> port_controller_ids = {-1, -1, -1, -1};
    std::array<SDL_GameController*, recomp::max_ports> port_controllers = {nullptr, nullptr, nullptr, nullptr};

    std::array<float, 2> rotation_delta{};
    std::array<float, 2> mouse_delta{};
    std::mutex pending_input_mutex;
    std::array<float, 2> pending_rotation_delta{};
    std::array<float, 2> pending_mouse_delta{};

    // Per-port rumble state
    std::array<float, recomp::max_ports> cur_rumble = {0.0f, 0.0f, 0.0f, 0.0f};
    std::array<bool, recomp::max_ports> rumble_active = {false, false, false, false};
} InputState;

static struct {
    std::list<std::filesystem::path> files_dropped;
} DropState;

std::atomic<recomp::InputDevice> scanning_device = recomp::InputDevice::COUNT;
std::atomic<recomp::InputField> scanned_input;

enum class InputType {
    None = 0, // Using zero for None ensures that default initialized InputFields are unbound.
    Keyboard,
    Mouse,
    ControllerDigital,
    ControllerAnalog // Axis input_id values are the SDL value + 1
};

void set_scanned_input(recomp::InputField value) {
    scanning_device.store(recomp::InputDevice::COUNT);
    scanned_input.store(value);
}

recomp::InputField recomp::get_scanned_input() {
    recomp::InputField ret = scanned_input.load();
    scanned_input.store({});
    return ret;
}

void recomp::start_scanning_input(recomp::InputDevice device) {
    scanned_input.store({});
    scanning_device.store(device);
}

void recomp::stop_scanning_input() {
    scanning_device.store(recomp::InputDevice::COUNT);
}

void queue_if_enabled(SDL_Event* event) {
    if (!recomp::all_input_disabled()) {
        recompui::queue_event(*event);
    }
}

static std::atomic_bool cursor_enabled = true;

void recompui::set_cursor_visible(bool visible) {
    cursor_enabled.store(visible);
}

bool should_override_keystate(SDL_Scancode key, SDL_Keymod mod) {
    // Override Enter when Alt is held.
    if (key == SDL_Scancode::SDL_SCANCODE_RETURN) {
        if (mod & SDL_Keymod::KMOD_ALT) {
            return true;
        }
    }

    return false;        
}

bool sdl_event_filter(void* userdata, SDL_Event* event) {
    switch (event->type) {
    case SDL_EventType::SDL_KEYDOWN:
        {
            SDL_KeyboardEvent* keyevent = &event->key;

            // Skip repeated events when not in the menu
            if (!recompui::is_context_capturing_input() &&
                event->key.repeat) {
                break;
            }

            if ((keyevent->keysym.scancode == SDL_Scancode::SDL_SCANCODE_RETURN && (keyevent->keysym.mod & SDL_Keymod::KMOD_ALT)) ||
                keyevent->keysym.scancode == SDL_Scancode::SDL_SCANCODE_F11
            ) {
                recompui::toggle_fullscreen();
            }
            if (scanning_device != recomp::InputDevice::COUNT) {
                if (keyevent->keysym.scancode == SDL_Scancode::SDL_SCANCODE_ESCAPE) {
                    recomp::cancel_scanning_input();
                } else if (scanning_device == recomp::InputDevice::Keyboard) {
                    set_scanned_input({(uint32_t)InputType::Keyboard, keyevent->keysym.scancode});
                }
            } else {
                if (!should_override_keystate(keyevent->keysym.scancode, static_cast<SDL_Keymod>(keyevent->keysym.mod))) {
                    queue_if_enabled(event);
                }
            }
        }
        break;
    case SDL_EventType::SDL_CONTROLLERDEVICEADDED:
        {
            SDL_ControllerDeviceEvent* controller_event = &event->cdevice;
            SDL_GameController* controller = SDL_GameControllerOpen(controller_event->which);
            printf("Controller added: %d\n", controller_event->which);
            if (controller != nullptr) {
                SDL_JoystickID instance_id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller));
                printf("  Instance ID: %d\n", instance_id);
                ControllerState& state = InputState.controller_states[instance_id];
                state.controller = controller;

                if (SDL_GameControllerHasSensor(controller, SDL_SensorType::SDL_SENSOR_GYRO) && SDL_GameControllerHasSensor(controller, SDL_SensorType::SDL_SENSOR_ACCEL)) {
                    SDL_GameControllerSetSensorEnabled(controller, SDL_SensorType::SDL_SENSOR_GYRO, SDL_TRUE);
                    SDL_GameControllerSetSensorEnabled(controller, SDL_SensorType::SDL_SENSOR_ACCEL, SDL_TRUE);
                }

                // Assign to first port with mode=Controller and no assignment
                {
                    std::lock_guard lock{InputState.cur_controllers_mutex};
                    for (int p = 0; p < recomp::max_ports; p++) {
                        if (port_modes[p].load() == recomp::ControllerPortMode::Controller && InputState.port_controllers[p] == nullptr) {
                            InputState.port_controller_ids[p] = instance_id;
                            InputState.port_controllers[p] = controller;
                            printf("  Assigned to port %d\n", p);
                            break;
                        }
                    }
                }
            }
        }
        break;
    case SDL_EventType::SDL_CONTROLLERDEVICEREMOVED:
        {
            SDL_ControllerDeviceEvent* controller_event = &event->cdevice;
            printf("Controller removed: %d\n", controller_event->which);

            // Clear port assignment for this controller
            {
                std::lock_guard lock{InputState.cur_controllers_mutex};
                for (int p = 0; p < recomp::max_ports; p++) {
                    if (InputState.port_controller_ids[p] == controller_event->which) {
                        InputState.port_controller_ids[p] = -1;
                        InputState.port_controllers[p] = nullptr;
                        printf("  Cleared port %d assignment\n", p);
                        break;
                    }
                }
            }

            InputState.controller_states.erase(controller_event->which);
        }
        break;
    case SDL_EventType::SDL_QUIT: {
        if (!ultramodern::is_game_started()) {
            ultramodern::quit();
            return true;
        }

        zelda64::open_quit_game_prompt();
        recompui::activate_mouse();
        break;
    }
    case SDL_EventType::SDL_MOUSEWHEEL:
        {
            SDL_MouseWheelEvent* wheel_event = &event->wheel;    
            InputState.mouse_wheel_pos.fetch_add(wheel_event->y * (wheel_event->direction == SDL_MOUSEWHEEL_FLIPPED ? -1 : 1));
        }
        queue_if_enabled(event);
        break;
    case SDL_EventType::SDL_CONTROLLERBUTTONDOWN:
        if (scanning_device != recomp::InputDevice::COUNT) {
            auto menuToggleBinding0 = recomp::get_input_binding(recomp::GameInput::TOGGLE_MENU, 0, recomp::InputDevice::Controller);
            auto menuToggleBinding1 = recomp::get_input_binding(recomp::GameInput::TOGGLE_MENU, 1, recomp::InputDevice::Controller);
            // note - magic number: 0 is InputType::None
            if ((menuToggleBinding0.input_type != 0 && event->cbutton.button == menuToggleBinding0.input_id) ||
                (menuToggleBinding1.input_type != 0 && event->cbutton.button == menuToggleBinding1.input_id)) {
                recomp::cancel_scanning_input();
            } else if (scanning_device == recomp::InputDevice::Controller) {
                SDL_ControllerButtonEvent* button_event = &event->cbutton;
                auto scanned_input_index = recomp::get_scanned_input_index();
                if ((scanned_input_index == static_cast<int>(recomp::GameInput::TOGGLE_MENU) ||
                     scanned_input_index == static_cast<int>(recomp::GameInput::ACCEPT_MENU) ||
                     scanned_input_index == static_cast<int>(recomp::GameInput::APPLY_MENU)) && (
                     button_event->button == SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_DPAD_UP ||
                     button_event->button == SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_DPAD_DOWN ||
                     button_event->button == SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_DPAD_LEFT ||
                     button_event->button == SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) {
                    break;
                }

                set_scanned_input({(uint32_t)InputType::ControllerDigital, button_event->button});
            }
        } else {
            queue_if_enabled(event);
        }
        break;
    case SDL_EventType::SDL_CONTROLLERAXISMOTION:
        if (scanning_device == recomp::InputDevice::Controller) {
            auto scanned_input_index = recomp::get_scanned_input_index();
            if (scanned_input_index == static_cast<int>(recomp::GameInput::TOGGLE_MENU) ||
                scanned_input_index == static_cast<int>(recomp::GameInput::ACCEPT_MENU) ||
                scanned_input_index == static_cast<int>(recomp::GameInput::APPLY_MENU)) {
                break;
            }

            SDL_ControllerAxisEvent* axis_event = &event->caxis;
            float axis_value = axis_event->value * (1/32768.0f);
            if (axis_value > axis_threshold) {
                SDL_Event set_stick_return_event;
                set_stick_return_event.type = SDL_USEREVENT;
                set_stick_return_event.user.code = axis_event->axis;
                set_stick_return_event.user.data1 = nullptr;
                set_stick_return_event.user.data2 = nullptr;
                recompui::queue_event(set_stick_return_event);
                
                set_scanned_input({(uint32_t)InputType::ControllerAnalog, axis_event->axis + 1});
            }
            else if (axis_value < -axis_threshold) {
                SDL_Event set_stick_return_event;
                set_stick_return_event.type = SDL_USEREVENT;
                set_stick_return_event.user.code = axis_event->axis;
                set_stick_return_event.user.data1 = nullptr;
                set_stick_return_event.user.data2 = nullptr;
                recompui::queue_event(set_stick_return_event);

                set_scanned_input({(uint32_t)InputType::ControllerAnalog, -axis_event->axis - 1});
            }
        } else {
            queue_if_enabled(event);
        }
        break;
    case SDL_EventType::SDL_CONTROLLERSENSORUPDATE:
        if (event->csensor.sensor == SDL_SensorType::SDL_SENSOR_ACCEL) {
            // Convert acceleration to g's.
            float x = event->csensor.data[0] / SDL_STANDARD_GRAVITY;
            float y = event->csensor.data[1] / SDL_STANDARD_GRAVITY;
            float z = event->csensor.data[2] / SDL_STANDARD_GRAVITY;
            ControllerState& state = InputState.controller_states[event->csensor.which];
            state.latest_accelerometer[0] = x;
            state.latest_accelerometer[1] = y;
            state.latest_accelerometer[2] = z;
        }
        else if (event->csensor.sensor == SDL_SensorType::SDL_SENSOR_GYRO) {
            // constexpr float gyro_threshold = 0.05f;
            // Convert rotational velocity to degrees per second.
            constexpr float rad_to_deg = 180.0f / M_PI;
            float x = event->csensor.data[0] * rad_to_deg;
            float y = event->csensor.data[1] * rad_to_deg;
            float z = event->csensor.data[2] * rad_to_deg;
            ControllerState& state = InputState.controller_states[event->csensor.which];
            uint64_t cur_timestamp = event->csensor.timestamp;
            uint32_t delta_ms = cur_timestamp - state.prev_gyro_timestamp;
            state.motion.ProcessMotion(x, y, z, state.latest_accelerometer[0], state.latest_accelerometer[1], state.latest_accelerometer[2], delta_ms * 0.001f);
            state.prev_gyro_timestamp = cur_timestamp;

            float rot_x = 0.0f;
            float rot_y = 0.0f;
            state.motion.GetPlayerSpaceGyro(rot_x, rot_y);

            {
                std::lock_guard lock{ InputState.pending_input_mutex };
                InputState.pending_rotation_delta[0] += rot_x;
                InputState.pending_rotation_delta[1] += rot_y;
            }
        }
        break;
    case SDL_EventType::SDL_MOUSEMOTION:
        if (!recomp::game_input_disabled()) {
            SDL_MouseMotionEvent* motion_event = &event->motion;
            std::lock_guard lock{ InputState.pending_input_mutex };
            InputState.pending_mouse_delta[0] += motion_event->xrel;
            InputState.pending_mouse_delta[1] += motion_event->yrel;
        }
        queue_if_enabled(event);
        break;
    case SDL_EventType::SDL_DROPBEGIN:
        DropState.files_dropped.clear();
        break;
    case SDL_EventType::SDL_DROPFILE:
        DropState.files_dropped.emplace_back(std::filesystem::path(std::u8string_view((const char8_t *)(event->drop.file))));
        SDL_free(event->drop.file);
        break;
    case SDL_EventType::SDL_DROPCOMPLETE:
        recompui::drop_files(DropState.files_dropped);
        break;
    case SDL_EventType::SDL_CONTROLLERBUTTONUP:
        // Always queue button up events to avoid missing them during binding.
        recompui::queue_event(*event);
        break;
    default:
        queue_if_enabled(event);
        break;
    }
    return false;
}

void recomp::handle_events() {
    SDL_Event cur_event;
    static bool started = false;
    static bool exited = false;
    while (SDL_PollEvent(&cur_event) && !exited) {
        exited = sdl_event_filter(nullptr, &cur_event);

        // Lock the cursor if all three conditions are true: mouse aiming is enabled, game input is not disabled, and the game has been started. 
        bool cursor_locked = (recomp::get_mouse_sensitivity() != 0) && !recomp::game_input_disabled() && ultramodern::is_game_started();

        // Hide the cursor based on its enable state, but override visibility to false if the cursor is locked.
        bool cursor_visible = cursor_enabled;
        if (cursor_locked) {
            cursor_visible = false;
        }

        SDL_ShowCursor(cursor_visible ? SDL_ENABLE : SDL_DISABLE);
        SDL_SetRelativeMouseMode(cursor_locked ? SDL_TRUE : SDL_FALSE);
    }

    if (!started && ultramodern::is_game_started()) {
        started = true;
        recompui::process_game_started();
    }
}

constexpr SDL_GameControllerButton SDL_CONTROLLER_BUTTON_SOUTH = SDL_CONTROLLER_BUTTON_A;
constexpr SDL_GameControllerButton SDL_CONTROLLER_BUTTON_EAST = SDL_CONTROLLER_BUTTON_B;
constexpr SDL_GameControllerButton SDL_CONTROLLER_BUTTON_WEST = SDL_CONTROLLER_BUTTON_X;
constexpr SDL_GameControllerButton SDL_CONTROLLER_BUTTON_NORTH = SDL_CONTROLLER_BUTTON_Y;

const recomp::DefaultN64Mappings recomp::default_n64_keyboard_mappings = {
    .a = {
        {.input_type = (uint32_t)InputType::Keyboard, .input_id = SDL_SCANCODE_SPACE}
    },
    .b = {
        {.input_type = (uint32_t)InputType::Keyboard, .input_id = SDL_SCANCODE_LSHIFT}
    },
    .l = {
        {.input_type = (uint32_t)InputType::Keyboard, .input_id = SDL_SCANCODE_E}
    },
    .r = {
        {.input_type = (uint32_t)InputType::Keyboard, .input_id = SDL_SCANCODE_R}
    },
    .z = {
        {.input_type = (uint32_t)InputType::Keyboard, .input_id = SDL_SCANCODE_Q}
    },
    .start = {
        {.input_type = (uint32_t)InputType::Keyboard, .input_id = SDL_SCANCODE_RETURN}
    },
    .c_left = {
        {.input_type = (uint32_t)InputType::Keyboard, .input_id = SDL_SCANCODE_LEFT}
    },
    .c_right = {
        {.input_type = (uint32_t)InputType::Keyboard, .input_id = SDL_SCANCODE_RIGHT}
    },
    .c_up = {
        {.input_type = (uint32_t)InputType::Keyboard, .input_id = SDL_SCANCODE_UP}
    },
    .c_down = {
        {.input_type = (uint32_t)InputType::Keyboard, .input_id = SDL_SCANCODE_DOWN}
    },
    .dpad_left = {
        {.input_type = (uint32_t)InputType::Keyboard, .input_id = SDL_SCANCODE_J}
    },
    .dpad_right = {
        {.input_type = (uint32_t)InputType::Keyboard, .input_id = SDL_SCANCODE_L}
    },
    .dpad_up = {
        {.input_type = (uint32_t)InputType::Keyboard, .input_id = SDL_SCANCODE_I}
    },
    .dpad_down = {
        {.input_type = (uint32_t)InputType::Keyboard, .input_id = SDL_SCANCODE_K}
    },
    .analog_left = {
        {.input_type = (uint32_t)InputType::Keyboard, .input_id = SDL_SCANCODE_A}
    },
    .analog_right = {
        {.input_type = (uint32_t)InputType::Keyboard, .input_id = SDL_SCANCODE_D}
    },
    .analog_up = {
        {.input_type = (uint32_t)InputType::Keyboard, .input_id = SDL_SCANCODE_W}
    },
    .analog_down = {
        {.input_type = (uint32_t)InputType::Keyboard, .input_id = SDL_SCANCODE_S}
    },
    .toggle_menu = {
        {.input_type = (uint32_t)InputType::Keyboard, .input_id = SDL_SCANCODE_ESCAPE}
    },
    .accept_menu = {
        {.input_type = (uint32_t)InputType::Keyboard, .input_id = SDL_SCANCODE_RETURN}
    },
    .apply_menu = {
        {.input_type = (uint32_t)InputType::Keyboard, .input_id = SDL_SCANCODE_F}
    }
};

const recomp::DefaultN64Mappings recomp::default_n64_controller_mappings = {
    .a = {
        {.input_type = (uint32_t)InputType::ControllerDigital, .input_id = SDL_CONTROLLER_BUTTON_SOUTH},
    },
    .b = {
        {.input_type = (uint32_t)InputType::ControllerDigital, .input_id = SDL_CONTROLLER_BUTTON_WEST},
    },
    .l = {
        {.input_type = (uint32_t)InputType::ControllerDigital, .input_id = SDL_CONTROLLER_BUTTON_LEFTSHOULDER},
    },
    .r = {
        {.input_type = (uint32_t)InputType::ControllerDigital, .input_id = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER},
    },
    .z = {
        {.input_type = (uint32_t)InputType::ControllerAnalog, .input_id = SDL_CONTROLLER_AXIS_TRIGGERLEFT + 1},
        {.input_type = (uint32_t)InputType::ControllerAnalog, .input_id = SDL_CONTROLLER_AXIS_TRIGGERRIGHT + 1},
    },
    .start = {
        {.input_type = (uint32_t)InputType::ControllerDigital, .input_id = SDL_CONTROLLER_BUTTON_START},
    },
    .c_left = {
        {.input_type = (uint32_t)InputType::ControllerAnalog, .input_id = -(SDL_CONTROLLER_AXIS_RIGHTX + 1)},
    },
    .c_right = {
        {.input_type = (uint32_t)InputType::ControllerAnalog, .input_id = SDL_CONTROLLER_AXIS_RIGHTX + 1}
    },
    .c_up = {
        {.input_type = (uint32_t)InputType::ControllerAnalog, .input_id = -(SDL_CONTROLLER_AXIS_RIGHTY + 1)}
    },
    .c_down = {
        {.input_type = (uint32_t)InputType::ControllerAnalog, .input_id = (SDL_CONTROLLER_AXIS_RIGHTY + 1)}
    },
    .dpad_left = {
        {.input_type = (uint32_t)InputType::ControllerDigital, .input_id = SDL_CONTROLLER_BUTTON_DPAD_LEFT},
    },
    .dpad_right = {
        {.input_type = (uint32_t)InputType::ControllerDigital, .input_id = SDL_CONTROLLER_BUTTON_DPAD_RIGHT},
    },
    .dpad_up = {
        {.input_type = (uint32_t)InputType::ControllerDigital, .input_id = SDL_CONTROLLER_BUTTON_DPAD_UP},
    },
    .dpad_down = {
        {.input_type = (uint32_t)InputType::ControllerDigital, .input_id = SDL_CONTROLLER_BUTTON_DPAD_DOWN},
    },
    .analog_left = {
        {.input_type = (uint32_t)InputType::ControllerAnalog, .input_id = -(SDL_CONTROLLER_AXIS_LEFTX + 1)},
    },
    .analog_right = {
        {.input_type = (uint32_t)InputType::ControllerAnalog, .input_id = SDL_CONTROLLER_AXIS_LEFTX + 1},
    },
    .analog_up = {
        {.input_type = (uint32_t)InputType::ControllerAnalog, .input_id = -(SDL_CONTROLLER_AXIS_LEFTY + 1)},
    },
    .analog_down = {
        {.input_type = (uint32_t)InputType::ControllerAnalog, .input_id = SDL_CONTROLLER_AXIS_LEFTY + 1},
    },
    .toggle_menu = {
        {.input_type = (uint32_t)InputType::ControllerDigital, .input_id = SDL_CONTROLLER_BUTTON_BACK},
    },
    .accept_menu = {
        {.input_type = (uint32_t)InputType::ControllerDigital, .input_id = SDL_CONTROLLER_BUTTON_SOUTH},
    },
    .apply_menu = {
        {.input_type = (uint32_t)InputType::ControllerDigital, .input_id = SDL_CONTROLLER_BUTTON_WEST},
        {.input_type = (uint32_t)InputType::ControllerDigital, .input_id = SDL_CONTROLLER_BUTTON_START}
    }
};

// Port 1 default keyboard mappings: arrow keys + nearby keys
const recomp::DefaultN64Mappings default_n64_keyboard_mappings_p2 = {
    .a = {
        {.input_type = (uint32_t)InputType::Keyboard, .input_id = SDL_SCANCODE_RSHIFT}
    },
    .b = {
        {.input_type = (uint32_t)InputType::Keyboard, .input_id = SDL_SCANCODE_RCTRL}
    },
    .l = {
        {.input_type = (uint32_t)InputType::Keyboard, .input_id = SDL_SCANCODE_PAGEUP}
    },
    .r = {
        {.input_type = (uint32_t)InputType::Keyboard, .input_id = SDL_SCANCODE_PAGEDOWN}
    },
    .z = {
        {.input_type = (uint32_t)InputType::Keyboard, .input_id = SDL_SCANCODE_RALT}
    },
    .start = {
        {.input_type = (uint32_t)InputType::Keyboard, .input_id = SDL_SCANCODE_KP_ENTER}
    },
    .c_left = {
        {.input_type = (uint32_t)InputType::Keyboard, .input_id = SDL_SCANCODE_KP_4}
    },
    .c_right = {
        {.input_type = (uint32_t)InputType::Keyboard, .input_id = SDL_SCANCODE_KP_6}
    },
    .c_up = {
        {.input_type = (uint32_t)InputType::Keyboard, .input_id = SDL_SCANCODE_KP_8}
    },
    .c_down = {
        {.input_type = (uint32_t)InputType::Keyboard, .input_id = SDL_SCANCODE_KP_2}
    },
    .dpad_left = {},
    .dpad_right = {},
    .dpad_up = {},
    .dpad_down = {},
    .analog_left = {
        {.input_type = (uint32_t)InputType::Keyboard, .input_id = SDL_SCANCODE_LEFT}
    },
    .analog_right = {
        {.input_type = (uint32_t)InputType::Keyboard, .input_id = SDL_SCANCODE_RIGHT}
    },
    .analog_up = {
        {.input_type = (uint32_t)InputType::Keyboard, .input_id = SDL_SCANCODE_UP}
    },
    .analog_down = {
        {.input_type = (uint32_t)InputType::Keyboard, .input_id = SDL_SCANCODE_DOWN}
    },
    .toggle_menu = {},
    .accept_menu = {},
    .apply_menu = {}
};

const recomp::DefaultN64Mappings& recomp::get_default_keyboard_mappings_for_port(int port) {
    switch (port) {
        case 0: return recomp::default_n64_keyboard_mappings;
        case 1: return default_n64_keyboard_mappings_p2;
        default: {
            // Ports 2-3: empty defaults
            static const recomp::DefaultN64Mappings empty{};
            return empty;
        }
    }
}

void recomp::poll_inputs() {
    InputState.keys = SDL_GetKeyboardState(&InputState.numkeys);
    InputState.keymod = SDL_GetModState();

    {
        std::lock_guard lock{ InputState.cur_controllers_mutex };
        InputState.cur_controllers.clear();

        for (const auto& [id, state] : InputState.controller_states) {
            (void)id; // Avoid unused variable warning.
            SDL_GameController* controller = state.controller;
            if (controller != nullptr) {
                InputState.cur_controllers.push_back(controller);
            }
        }
    }

    // Read the deltas while resetting them to zero.
    {
        std::lock_guard lock{ InputState.pending_input_mutex };
        
        InputState.rotation_delta = InputState.pending_rotation_delta;
        InputState.pending_rotation_delta = { 0.0f, 0.0f };

        InputState.mouse_delta = InputState.pending_mouse_delta;
        InputState.pending_mouse_delta = { 0.0f, 0.0f };
    }
    
    // Quicksaving is disabled for now and will likely have more limited functionality
    // when restored, rather than allowing saving and loading at any point in time.
    #if 0
    if (InputState.keys) {
        static bool save_was_held = false;
        static bool load_was_held = false;
        bool save_is_held = InputState.keys[SDL_SCANCODE_F5] != 0;
        bool load_is_held = InputState.keys[SDL_SCANCODE_F7] != 0;
        if (save_is_held && !save_was_held) {
            zelda64::quicksave_save();
        }
        else if (load_is_held && !load_was_held) {
            zelda64::quicksave_load();
        }
        save_was_held = save_is_held;
    }
    #endif
}

void recomp::set_rumble(int controller_num, bool on) {
    if (controller_num >= 0 && controller_num < recomp::max_ports) {
        InputState.rumble_active[controller_num] = on;
    }
}

ultramodern::input::connected_device_info_t recomp::get_connected_device_info(int controller_num) {
    if (controller_num >= 0 && controller_num < recomp::max_ports) {
        if (port_modes[controller_num].load() != recomp::ControllerPortMode::Off) {
            return ultramodern::input::connected_device_info_t {
                .connected_device = ultramodern::input::Device::Controller,
                .connected_pak = ultramodern::input::Pak::RumblePak,
            };
        }
    }

    return ultramodern::input::connected_device_info_t {
        .connected_device = ultramodern::input::Device::None,
        .connected_pak = ultramodern::input::Pak::None,
    };
}

std::string recomp::get_port_controller_name(int port) {
    if (port >= 0 && port < recomp::max_ports && InputState.port_controllers[port] != nullptr) {
        const char* name = SDL_GameControllerName(InputState.port_controllers[port]);
        if (name != nullptr) {
            return std::string(name);
        }
    }
    return "None";
}

// Build a snapshot of all connected controllers from controller_states
static std::vector<std::pair<SDL_JoystickID, SDL_GameController*>> get_all_controllers_snapshot() {
    std::vector<std::pair<SDL_JoystickID, SDL_GameController*>> result;
    for (const auto& [id, state] : InputState.controller_states) {
        if (state.controller != nullptr) {
            result.emplace_back(id, state.controller);
        }
    }
    return result;
}

std::vector<std::string> recomp::get_connected_controller_names() {
    std::lock_guard lock{InputState.cur_controllers_mutex};
    auto controllers = get_all_controllers_snapshot();
    std::vector<std::string> names;
    for (auto& [id, controller] : controllers) {
        const char* name = SDL_GameControllerName(controller);
        names.emplace_back(name ? name : "Unknown Controller");
    }
    return names;
}

int recomp::get_port_controller_index(int port) {
    std::lock_guard lock{InputState.cur_controllers_mutex};
    if (port < 0 || port >= recomp::max_ports || InputState.port_controllers[port] == nullptr) {
        return -1;
    }
    auto controllers = get_all_controllers_snapshot();
    for (size_t i = 0; i < controllers.size(); i++) {
        if (controllers[i].second == InputState.port_controllers[port]) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void recomp::assign_controller_to_port(int port, int controller_index) {
    std::lock_guard lock{InputState.cur_controllers_mutex};
    if (port < 0 || port >= recomp::max_ports) return;
    auto controllers = get_all_controllers_snapshot();
    if (controller_index < 0 || controller_index >= static_cast<int>(controllers.size())) {
        InputState.port_controllers[port] = nullptr;
        InputState.port_controller_ids[port] = -1;
        return;
    }
    // Clear this controller from any other port first
    SDL_JoystickID new_id = controllers[controller_index].first;
    for (int p = 0; p < recomp::max_ports; p++) {
        if (p != port && InputState.port_controller_ids[p] == new_id) {
            InputState.port_controllers[p] = nullptr;
            InputState.port_controller_ids[p] = -1;
        }
    }
    InputState.port_controllers[port] = controllers[controller_index].second;
    InputState.port_controller_ids[port] = new_id;
}

static float smoothstep(float from, float to, float amount) {
    amount = (amount * amount) * (3.0f - 2.0f * amount);
    return std::lerp(from, to, amount);
}

// Update rumble to attempt to mimic the way n64 rumble ramps up and falls off
void recomp::update_rumble() {
    // Note: values are not accurate! just approximations based on feel
    uint32_t duration = 1000000; // Dummy duration value that lasts long enough to matter as the game will reset rumble on its own.
    for (int p = 0; p < recomp::max_ports; p++) {
        if (InputState.rumble_active[p]) {
            InputState.cur_rumble[p] += 0.17f;
            if (InputState.cur_rumble[p] > 1) InputState.cur_rumble[p] = 1;
        } else {
            InputState.cur_rumble[p] *= 0.92f;
            InputState.cur_rumble[p] -= 0.01f;
            if (InputState.cur_rumble[p] < 0) InputState.cur_rumble[p] = 0;
        }
        float smooth_rumble = smoothstep(0, 1, InputState.cur_rumble[p]);
        uint16_t rumble_strength = smooth_rumble * (recomp::get_rumble_strength() * 0xFFFF / 100);

        // Apply rumble only to the assigned controller for this port
        SDL_GameController* controller = InputState.port_controllers[p];
        if (controller != nullptr) {
            SDL_GameControllerRumble(controller, 0, rumble_strength, duration);
        }
    }
}

bool controller_button_state(int32_t input_id) {
    if (input_id >= 0 && input_id < SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_MAX) {
        SDL_GameControllerButton button = (SDL_GameControllerButton)input_id;
        bool ret = false;
        {
            std::lock_guard lock{ InputState.cur_controllers_mutex };
            for (const auto& controller : InputState.cur_controllers) {
                ret |= SDL_GameControllerGetButton(controller, button);
            }
        }

        return ret;
    }
    return false;
}

// Per-port version: reads only the assigned gamepad for the given port
bool controller_button_state_port(int32_t input_id, int port) {
    if (port < 0 || port >= recomp::max_ports) return false;
    if (input_id >= 0 && input_id < SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_MAX) {
        SDL_GameControllerButton button = (SDL_GameControllerButton)input_id;
        SDL_GameController* controller = InputState.port_controllers[port];
        if (controller != nullptr) {
            return SDL_GameControllerGetButton(controller, button) != 0;
        }
    }
    return false;
}

static std::atomic_bool right_analog_suppressed = false;

float controller_axis_state(int32_t input_id, bool allow_suppression) {
    if (abs(input_id) - 1 < SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_MAX) {
        SDL_GameControllerAxis axis = (SDL_GameControllerAxis)(abs(input_id) - 1);
        bool negative_range = input_id < 0;
        float ret = 0.0f;

        {
            std::lock_guard lock{ InputState.cur_controllers_mutex };
            for (const auto& controller : InputState.cur_controllers) {
                float cur_val = SDL_GameControllerGetAxis(controller, axis) * (1/32768.0f);
                if (negative_range) {
                    cur_val = -cur_val;
                }

                // Check if this input is a right analog axis and suppress it accordingly.
                if (allow_suppression && right_analog_suppressed.load() &&
                    (axis == SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_RIGHTX || axis == SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_RIGHTY)) {
                    cur_val = 0;
                }
                ret += std::clamp(cur_val, 0.0f, 1.0f);
            }
        }

        return std::clamp(ret, 0.0f, 1.0f);
    }
    return false;
}

// Per-port version: reads only the assigned gamepad for the given port
float controller_axis_state_port(int32_t input_id, bool allow_suppression, int port) {
    if (port < 0 || port >= recomp::max_ports) return 0.0f;
    if (abs(input_id) - 1 < SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_MAX) {
        SDL_GameControllerAxis axis = (SDL_GameControllerAxis)(abs(input_id) - 1);
        bool negative_range = input_id < 0;
        float ret = 0.0f;

        SDL_GameController* controller = InputState.port_controllers[port];
        if (controller != nullptr) {
            float cur_val = SDL_GameControllerGetAxis(controller, axis) * (1/32768.0f);
            if (negative_range) {
                cur_val = -cur_val;
            }

            if (allow_suppression && right_analog_suppressed.load() &&
                (axis == SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_RIGHTX || axis == SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_RIGHTY)) {
                cur_val = 0;
            }
            ret = std::clamp(cur_val, 0.0f, 1.0f);
        }

        return ret;
    }
    return false;
}

float recomp::get_input_analog(const recomp::InputField& field) {
    switch ((InputType)field.input_type) {
    case InputType::Keyboard:
        if (InputState.keys && field.input_id >= 0 && field.input_id < InputState.numkeys) {
            if (should_override_keystate(static_cast<SDL_Scancode>(field.input_id), InputState.keymod)) {
                return 0.0f;
            }
            return InputState.keys[field.input_id] ? 1.0f : 0.0f;
        }
        return 0.0f;
    case InputType::ControllerDigital:
        return controller_button_state(field.input_id) ? 1.0f : 0.0f;
    case InputType::ControllerAnalog:
        return controller_axis_state(field.input_id, true);
    case InputType::Mouse:
        // TODO mouse support
        return 0.0f;
    case InputType::None:
        return false;
    }
}

float recomp::get_input_analog(const std::span<const recomp::InputField> fields) {
    float ret = 0.0f;
    for (const auto& field : fields) {
        ret += get_input_analog(field);
    }
    return std::clamp(ret, 0.0f, 1.0f);
}

bool recomp::get_input_digital(const recomp::InputField& field) {
    switch ((InputType)field.input_type) {
    case InputType::Keyboard:
        if (InputState.keys && field.input_id >= 0 && field.input_id < InputState.numkeys) {
            if (should_override_keystate(static_cast<SDL_Scancode>(field.input_id), InputState.keymod)) {
                return false;
            }
            return InputState.keys[field.input_id] != 0;
        }
        return false;
    case InputType::ControllerDigital:
        return controller_button_state(field.input_id);
    case InputType::ControllerAnalog:
        // TODO adjustable threshold
        return controller_axis_state(field.input_id, true) >= axis_threshold;
    case InputType::Mouse:
        // TODO mouse support
        return false;
    case InputType::None:
        return false;
    }
}

bool recomp::get_input_digital(const std::span<const recomp::InputField> fields) {
    bool ret = 0;
    for (const auto& field : fields) {
        ret |= get_input_digital(field);
    }
    return ret;
}

// Per-port versions: for controller inputs, read only from the port's assigned gamepad
float recomp::get_input_analog_port(const recomp::InputField& field, int port) {
    switch ((InputType)field.input_type) {
    case InputType::Keyboard:
        if (InputState.keys && field.input_id >= 0 && field.input_id < InputState.numkeys) {
            if (should_override_keystate(static_cast<SDL_Scancode>(field.input_id), InputState.keymod)) {
                return 0.0f;
            }
            return InputState.keys[field.input_id] ? 1.0f : 0.0f;
        }
        return 0.0f;
    case InputType::ControllerDigital:
        return controller_button_state_port(field.input_id, port) ? 1.0f : 0.0f;
    case InputType::ControllerAnalog:
        return controller_axis_state_port(field.input_id, true, port);
    case InputType::Mouse:
        return 0.0f;
    case InputType::None:
        return false;
    }
}

float recomp::get_input_analog_port(const std::span<const recomp::InputField> fields, int port) {
    float ret = 0.0f;
    for (const auto& field : fields) {
        ret += get_input_analog_port(field, port);
    }
    return std::clamp(ret, 0.0f, 1.0f);
}

bool recomp::get_input_digital_port(const recomp::InputField& field, int port) {
    switch ((InputType)field.input_type) {
    case InputType::Keyboard:
        if (InputState.keys && field.input_id >= 0 && field.input_id < InputState.numkeys) {
            if (should_override_keystate(static_cast<SDL_Scancode>(field.input_id), InputState.keymod)) {
                return false;
            }
            return InputState.keys[field.input_id] != 0;
        }
        return false;
    case InputType::ControllerDigital:
        return controller_button_state_port(field.input_id, port);
    case InputType::ControllerAnalog:
        return controller_axis_state_port(field.input_id, true, port) >= axis_threshold;
    case InputType::Mouse:
        return false;
    case InputType::None:
        return false;
    }
}

bool recomp::get_input_digital_port(const std::span<const recomp::InputField> fields, int port) {
    bool ret = 0;
    for (const auto& field : fields) {
        ret |= get_input_digital_port(field, port);
    }
    return ret;
}

void recomp::get_gyro_deltas(float* x, float* y) {
    std::array<float, 2> cur_rotation_delta = InputState.rotation_delta;
    float sensitivity = (float)recomp::get_gyro_sensitivity() / 100.0f;
    *x = cur_rotation_delta[0] * sensitivity;
    *y = cur_rotation_delta[1] * sensitivity;
}

void recomp::get_mouse_deltas(float* x, float* y) {
    std::array<float, 2> cur_mouse_delta = InputState.mouse_delta;
    float sensitivity = (float)recomp::get_mouse_sensitivity() / 100.0f;
    *x = cur_mouse_delta[0] * sensitivity;
    *y = cur_mouse_delta[1] * sensitivity;
}

void recomp::apply_joystick_deadzone(float x_in, float y_in, float* x_out, float* y_out) {
    float joystick_deadzone = (float)recomp::get_joystick_deadzone() / 100.0f;

    if(fabsf(x_in) < joystick_deadzone) {
        x_in = 0.0f;
    }
    else {
        if(x_in > 0.0f) {
            x_in -= joystick_deadzone;
        } 
        else {
            x_in += joystick_deadzone;
        }

        x_in /= (1.0f - joystick_deadzone);
    }

    if(fabsf(y_in) < joystick_deadzone) {
        y_in = 0.0f;
    }
    else {
        if(y_in > 0.0f) {
            y_in -= joystick_deadzone;
        } 
        else {
            y_in += joystick_deadzone;
        }

        y_in /= (1.0f - joystick_deadzone);
    }

    *x_out = x_in;
    *y_out = y_in;
}

void recomp::get_right_analog(float* x, float* y) {
    float x_val =
        controller_axis_state((SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_RIGHTX + 1), false) -
        controller_axis_state(-(SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_RIGHTX + 1), false);
    float y_val =
        controller_axis_state((SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_RIGHTY + 1), false) -
        controller_axis_state(-(SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_RIGHTY + 1), false);
    recomp::apply_joystick_deadzone(x_val, y_val, x, y);
}

void recomp::set_right_analog_suppressed(bool suppressed) {
    right_analog_suppressed.store(suppressed);
}

bool recomp::game_input_disabled() {
    // Disable input if any menu that blocks input is open.
    return recompui::is_context_capturing_input();
}

bool recomp::all_input_disabled() {
    // Disable all input if an input is being polled.
    return scanning_device != recomp::InputDevice::COUNT;
}

std::string controller_button_to_string(SDL_GameControllerButton button) {
    switch (button) {
    case SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_A:
        return PF_GAMEPAD_A;
    case SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_B:
        return PF_GAMEPAD_B;
    case SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_X:
        return PF_GAMEPAD_X;
    case SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_Y:
        return PF_GAMEPAD_Y;
    case SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_BACK:
        return PF_XBOX_VIEW;
    case SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_GUIDE:
        return PF_GAMEPAD_HOME;
    case SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_START:
        return PF_XBOX_MENU;
    case SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_LEFTSTICK:
        return PF_ANALOG_L_CLICK;
    case SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_RIGHTSTICK:
        return PF_ANALOG_R_CLICK;
    case SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
        return PF_XBOX_LEFT_SHOULDER;
    case SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
        return PF_XBOX_RIGHT_SHOULDER;
    case SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_DPAD_UP:
        return PF_DPAD_UP;
    case SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_DPAD_DOWN:
        return PF_DPAD_DOWN;
    case SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_DPAD_LEFT:
        return PF_DPAD_LEFT;
    case SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
        return PF_DPAD_RIGHT;
    // case SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_MISC1:
    //     return "";
    // case SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_PADDLE1:
    //     return "";
    // case SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_PADDLE2:
    //     return "";
    // case SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_PADDLE3:
    //     return "";
    // case SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_PADDLE4:
    //     return "";
    case SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_TOUCHPAD:
        return PF_SONY_TOUCHPAD;
    default:
        return "Button " + std::to_string(button);
    }
}

std::unordered_map<SDL_Scancode, std::string> scancode_codepoints {
    {SDL_SCANCODE_LEFT, PF_KEYBOARD_LEFT},
    // NOTE: UP and RIGHT are swapped with promptfont.
    {SDL_SCANCODE_UP, PF_KEYBOARD_RIGHT},
    {SDL_SCANCODE_RIGHT, PF_KEYBOARD_UP},
    {SDL_SCANCODE_DOWN, PF_KEYBOARD_DOWN},
    {SDL_SCANCODE_A, PF_KEYBOARD_A},
    {SDL_SCANCODE_B, PF_KEYBOARD_B},
    {SDL_SCANCODE_C, PF_KEYBOARD_C},
    {SDL_SCANCODE_D, PF_KEYBOARD_D},
    {SDL_SCANCODE_E, PF_KEYBOARD_E},
    {SDL_SCANCODE_F, PF_KEYBOARD_F},
    {SDL_SCANCODE_G, PF_KEYBOARD_G},
    {SDL_SCANCODE_H, PF_KEYBOARD_H},
    {SDL_SCANCODE_I, PF_KEYBOARD_I},
    {SDL_SCANCODE_J, PF_KEYBOARD_J},
    {SDL_SCANCODE_K, PF_KEYBOARD_K},
    {SDL_SCANCODE_L, PF_KEYBOARD_L},
    {SDL_SCANCODE_M, PF_KEYBOARD_M},
    {SDL_SCANCODE_N, PF_KEYBOARD_N},
    {SDL_SCANCODE_O, PF_KEYBOARD_O},
    {SDL_SCANCODE_P, PF_KEYBOARD_P},
    {SDL_SCANCODE_Q, PF_KEYBOARD_Q},
    {SDL_SCANCODE_R, PF_KEYBOARD_R},
    {SDL_SCANCODE_S, PF_KEYBOARD_S},
    {SDL_SCANCODE_T, PF_KEYBOARD_T},
    {SDL_SCANCODE_U, PF_KEYBOARD_U},
    {SDL_SCANCODE_V, PF_KEYBOARD_V},
    {SDL_SCANCODE_W, PF_KEYBOARD_W},
    {SDL_SCANCODE_X, PF_KEYBOARD_X},
    {SDL_SCANCODE_Y, PF_KEYBOARD_Y},
    {SDL_SCANCODE_Z, PF_KEYBOARD_Z},
    {SDL_SCANCODE_0, PF_KEYBOARD_0},
    {SDL_SCANCODE_1, PF_KEYBOARD_1},
    {SDL_SCANCODE_2, PF_KEYBOARD_2},
    {SDL_SCANCODE_3, PF_KEYBOARD_3},
    {SDL_SCANCODE_4, PF_KEYBOARD_4},
    {SDL_SCANCODE_5, PF_KEYBOARD_5},
    {SDL_SCANCODE_6, PF_KEYBOARD_6},
    {SDL_SCANCODE_7, PF_KEYBOARD_7},
    {SDL_SCANCODE_8, PF_KEYBOARD_8},
    {SDL_SCANCODE_9, PF_KEYBOARD_9},
    {SDL_SCANCODE_ESCAPE, PF_KEYBOARD_ESCAPE},
    {SDL_SCANCODE_F1, PF_KEYBOARD_F1},
    {SDL_SCANCODE_F2, PF_KEYBOARD_F2},
    {SDL_SCANCODE_F3, PF_KEYBOARD_F3},
    {SDL_SCANCODE_F4, PF_KEYBOARD_F4},
    {SDL_SCANCODE_F5, PF_KEYBOARD_F5},
    {SDL_SCANCODE_F6, PF_KEYBOARD_F6},
    {SDL_SCANCODE_F7, PF_KEYBOARD_F7},
    {SDL_SCANCODE_F8, PF_KEYBOARD_F8},
    {SDL_SCANCODE_F9, PF_KEYBOARD_F9},
    {SDL_SCANCODE_F10, PF_KEYBOARD_F10},
    {SDL_SCANCODE_F11, PF_KEYBOARD_F11},
    {SDL_SCANCODE_F12, PF_KEYBOARD_F12},
    {SDL_SCANCODE_PRINTSCREEN, PF_KEYBOARD_PRINT_SCREEN},
    {SDL_SCANCODE_SCROLLLOCK, PF_KEYBOARD_SCROLL_LOCK},
    {SDL_SCANCODE_PAUSE, PF_KEYBOARD_PAUSE},
    {SDL_SCANCODE_INSERT, PF_KEYBOARD_INSERT},
    {SDL_SCANCODE_HOME, PF_KEYBOARD_HOME},
    {SDL_SCANCODE_PAGEUP, PF_KEYBOARD_PAGE_UP},
    {SDL_SCANCODE_DELETE, PF_KEYBOARD_DELETE},
    {SDL_SCANCODE_END, PF_KEYBOARD_END},
    {SDL_SCANCODE_PAGEDOWN, PF_KEYBOARD_PAGE_DOWN},
    {SDL_SCANCODE_SPACE, PF_KEYBOARD_SPACE},
    {SDL_SCANCODE_BACKSPACE, PF_KEYBOARD_BACKSPACE},
    {SDL_SCANCODE_TAB, PF_KEYBOARD_TAB},
    {SDL_SCANCODE_RETURN, PF_KEYBOARD_ENTER},
    {SDL_SCANCODE_CAPSLOCK, PF_KEYBOARD_CAPS},
    {SDL_SCANCODE_NUMLOCKCLEAR, PF_KEYBOARD_NUM_LOCK},
    {SDL_SCANCODE_LSHIFT, "L" PF_KEYBOARD_SHIFT},
    {SDL_SCANCODE_RSHIFT, "R" PF_KEYBOARD_SHIFT},
};

std::string keyboard_input_to_string(SDL_Scancode key) {
    if (scancode_codepoints.find(key) != scancode_codepoints.end()) {
        return scancode_codepoints[key];
    }
    return std::to_string(key);
}

std::string controller_axis_to_string(int axis) {
    bool positive = axis > 0;
    SDL_GameControllerAxis actual_axis = SDL_GameControllerAxis(abs(axis) - 1);
    switch (actual_axis) {
    case SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_LEFTX:
        return positive ? "\u21C0" : "\u21BC";
    case SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_LEFTY:
        return positive ? "\u21C2" : "\u21BE";
    case SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_RIGHTX:
        return positive ? "\u21C1" : "\u21BD";
    case SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_RIGHTY:
        return positive ? "\u21C3" : "\u21BF";
    case SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_TRIGGERLEFT:
        return positive ? "\u2196" : "\u21DC";
    case SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
        return positive ? "\u2197" : "\u21DD";
    default:
        return "Axis " + std::to_string(actual_axis) + (positive ? '+' : '-');
    }
}

std::string recomp::InputField::to_string() const {
    switch ((InputType)input_type) {
        case InputType::None:
            return "";
        case InputType::ControllerDigital:
            return controller_button_to_string((SDL_GameControllerButton)input_id);
        case InputType::ControllerAnalog:
            return controller_axis_to_string(input_id);
        case InputType::Keyboard:
            return keyboard_input_to_string((SDL_Scancode)input_id);
        default:
            return std::to_string(input_type) + "," + std::to_string(input_id);
    }
}
