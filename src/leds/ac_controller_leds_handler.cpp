#include "leds/ac_controller_leds_handler.hpp"

#include <marlin_server_state.h>
#include <marlin_vars.hpp>
#include <marlin_server.hpp>
#include <puppies/ac_controller.hpp>
#include <ac_controller/types.hpp>
#include <algorithm>
#include <optional>

namespace leds {

using namespace marlin_server;

namespace {

struct CustomEffectState {
    AcControllerLedsHandler::CustomEffect effect;
    ColorRGBW color;
    uint8_t progress_percent;
    uint32_t start_ms;
    uint32_t duration_ms;
};

std::optional<CustomEffectState> custom_effect;

void apply_effect(AcControllerLedsHandler::CustomEffect effect, const ColorRGBW &color, uint8_t progress_percent) {
    switch (effect) {
    case AcControllerLedsHandler::CustomEffect::off:
        buddy::puppies::ac_controller.turn_off_bed_leds();
        break;
    case AcControllerLedsHandler::CustomEffect::static_color:
        buddy::puppies::ac_controller.set_rgbw_led({ color.r, color.g, color.b, color.w });
        break;
    case AcControllerLedsHandler::CustomEffect::progress_percent:
        buddy::puppies::ac_controller.set_progress_percent(std::min<uint8_t>(progress_percent, 100), { color.r, color.g, color.b, color.w });
        break;
    }
}

} // namespace

static ac_controller::AnimationType marlin_to_anim_state() {

    const marlin_server::State printer_state = marlin_vars().print_state;

    switch (printer_state) {
    case State::Printing:
    case State::PrintInit:
    case State::SerialPrintInit:
    case State::Finishing_WaitIdle:
    case State::Pausing_Begin:
    case State::Pausing_Failed_Code:
    case State::Pausing_WaitIdle:
    case State::Pausing_ParkHead:
    case State::Resuming_BufferData:
    case State::Resuming_Begin:
    case State::Resuming_Reheating:
    case State::Resuming_UnparkHead_XY:
    case State::Resuming_UnparkHead_ZE:
        return ac_controller::AnimationType::PROGRESS_PERCENT;

    case State::Idle:
        return ac_controller::AnimationType::OFF;

    case State::Aborting_Begin:
    case State::Aborted:
    case State::CrashRecovery_Begin:
    case State::CrashRecovery_Retracting:
    case State::CrashRecovery_Lifting:
    case State::CrashRecovery_ToolchangePowerPanic:
    case State::CrashRecovery_XY_Measure:
    case State::CrashRecovery_XY_HOME:
    case State::CrashRecovery_HOMEFAIL:
    case State::CrashRecovery_Axis_NOK:
    case State::CrashRecovery_Repeated_Crash:
    case State::PowerPanic_acFault:
    case State::PowerPanic_Resume:
    case State::PowerPanic_AwaitingResume:
    case State::Finished:
    case State::Exit:
    case State::MediaErrorRecovery_BufferData:
    case State::PrintPreviewInit:
    case State::PrintPreviewConfirmed:
    case State::PrintPreviewQuestions:
    case State::PrintPreviewToolsMapping:
    case State::PrintPreviewImage:
    case State::Paused:
    case State::Aborting_ParkHead:
    case State::Aborting_UnloadFilament:
    case State::Aborting_WaitIdle:
    case State::Aborting_Preview:
    case State::Finishing_UnloadFilament:
    case State::Finishing_ParkHead:
        return ac_controller::AnimationType::STATIC_COLOR;
    }

    bsod("Unknown printer state in AcControllerLedsHandler");
    return ac_controller::AnimationType::OFF;
}

void AcControllerLedsHandler::set_custom_effect(CustomEffect effect, ColorRGBW color, uint8_t progress_percent, uint32_t duration_ms) {
    custom_effect.emplace(CustomEffectState {
        effect,
        color,
        std::min<uint8_t>(progress_percent, 100),
        ticks_ms(),
        duration_ms,
    });
    apply_effect(effect, color, progress_percent);
}

void AcControllerLedsHandler::update(ColorRGBW &color, uint8_t progress_percent) {
    if (custom_effect) {
        const uint32_t time_ms = ticks_ms();
        if (custom_effect->duration_ms == 0 || time_ms - custom_effect->start_ms < custom_effect->duration_ms) {
            apply_effect(custom_effect->effect, custom_effect->color, custom_effect->progress_percent);
            return;
        }
        custom_effect.reset();
    }

    switch (marlin_to_anim_state()) {
    case ac_controller::AnimationType::OFF:
        buddy::puppies::ac_controller.turn_off_bed_leds();
        break;
    case ac_controller::AnimationType::STATIC_COLOR:
        buddy::puppies::ac_controller.set_rgbw_led({ color.r, color.g, color.b, color.w });
        break;
    case ac_controller::AnimationType::PROGRESS_PERCENT:
        buddy::puppies::ac_controller.set_progress_percent(progress_percent);
        break;
    }
}

} // namespace leds
