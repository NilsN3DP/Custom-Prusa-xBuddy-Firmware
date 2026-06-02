#include "adaptive_input_shaper.hpp"

#if HAS_INPUT_SHAPER_CALIBRATION() && HAS_LOCAL_ACCELEROMETER()

#include <Marlin/src/feature/input_shaper/input_shaper_config.hpp>
#include <Marlin/src/module/planner.h>
#include <Marlin/src/module/prusa/accelerometer.h>
#include <common/marlin_server.hpp>
#include <config_store/store_instance.hpp>
#include <logging/log.hpp>
#include <timing.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <optional>

LOG_COMPONENT_REF(Marlin);

namespace feature::adaptive_input_shaper {
namespace {

constexpr size_t window_samples = 256;
constexpr size_t max_samples_per_step = 48;
constexpr size_t frequency_bins = 13;
constexpr float bin_step_hz = 1.0f;
constexpr float search_half_span_hz = static_cast<float>(frequency_bins / 2) * bin_step_hz;
constexpr float min_peak_to_average_ratio = 1.65f;
constexpr float min_average_power = 25.0f;
constexpr float max_step_hz = 0.35f;
constexpr float minimum_change_hz = 0.25f;
constexpr float safety_peak_jump_hz = 5.5f;
constexpr uint8_t safety_fault_windows = 3;
constexpr uint32_t update_cooldown_ms = 1'500;
constexpr uint32_t retry_cooldown_ms = 10'000;
constexpr const char *log_path = "/usb/adaptive_is_log.csv";

Status latest_status;

struct FrequencyBin {
    float frequency = 0.0f;
    float coefficient = 0.0f;
    float s1 = 0.0f;
    float s2 = 0.0f;

    void reset(float target_frequency, float sample_rate) {
        frequency = input_shaper::clamp_frequency_to_safe_values(target_frequency);
        const float omega = 2.0f * static_cast<float>(M_PI) * frequency / sample_rate;
        coefficient = 2.0f * cosf(omega);
        s1 = 0.0f;
        s2 = 0.0f;
    }

    void add(float sample) {
        const float s0 = sample + coefficient * s1 - s2;
        s2 = s1;
        s1 = s0;
    }

    float power() const {
        return s1 * s1 + s2 * s2 - coefficient * s1 * s2;
    }
};

struct AxisAnalyzer {
    std::array<FrequencyBin, frequency_bins> bins {};
    size_t samples = 0;
    float energy = 0.0f;

    void reset(float center_hz, float sample_rate) {
        const float start_hz = center_hz - search_half_span_hz;
        for (size_t i = 0; i < bins.size(); ++i) {
            bins[i].reset(start_hz + static_cast<float>(i) * bin_step_hz, sample_rate);
        }
        samples = 0;
        energy = 0.0f;
    }

    void add(float sample) {
        for (auto &bin : bins) {
            bin.add(sample);
        }
        energy += sample * sample;
        ++samples;
    }

    std::optional<float> detected_peak() const {
        if (samples < window_samples || energy / static_cast<float>(samples) < min_average_power) {
            return std::nullopt;
        }

        const auto peak_it = std::max_element(bins.begin(), bins.end(), [](const FrequencyBin &lhs, const FrequencyBin &rhs) {
            return lhs.power() < rhs.power();
        });
        const float peak_power = peak_it->power();
        float power_sum = 0.0f;
        for (const auto &bin : bins) {
            power_sum += bin.power();
        }
        const float average_power = power_sum / static_cast<float>(bins.size());
        if (average_power <= 0.0f || peak_power < average_power * min_peak_to_average_ratio) {
            return std::nullopt;
        }

        return peak_it->frequency;
    }
};

struct Session {
    PrusaAccelerometer accelerometer;
    std::optional<input_shaper::AxisConfig> original_x;
    std::optional<input_shaper::AxisConfig> original_y;
    AxisAnalyzer analyzer_x;
    AxisAnalyzer analyzer_y;
    float sample_rate = 0.0f;
    uint32_t next_update_ms = 0;
    uint8_t unstable_windows = 0;
    FILE *log_file = nullptr;
    Mode mode = Mode::off;
    bool safety_brake = true;
    bool started = false;

    Session(Mode session_mode, bool safety)
        : mode(session_mode)
        , safety_brake(safety) {
        original_x = input_shaper::get_axis_config(X_AXIS);
        original_y = input_shaper::get_axis_config(Y_AXIS);
        sample_rate = accelerometer.get_sampling_rate();

        if (accelerometer.get_error() == PrusaAccelerometer::Error::none && sample_rate > 0.0f && original_x.has_value() && original_y.has_value()) {
            accelerometer.clear();
            open_log();
            reset_windows();
            started = true;
            latest_status.faulted = false;
            log_info(Marlin, "Adaptive Input Shaper started: X=%f Hz Y=%f Hz sample_rate=%f",
                static_cast<double>(original_x->frequency),
                static_cast<double>(original_y->frequency),
                static_cast<double>(sample_rate));
        } else {
            log_warning(Marlin, "Adaptive Input Shaper unavailable");
        }
    }

    ~Session() {
        restore_original_config();
        close_log();
    }

    void restore_original_config() {
        if (started && mode == Mode::correct) {
            input_shaper::set_axis_config(X_AXIS, original_x);
            input_shaper::set_axis_config(Y_AXIS, original_y);
            log_info(Marlin, "Adaptive Input Shaper restored start frequencies");
        }
        started = false;
    }

    void open_log() {
        log_file = fopen(log_path, "a");
        if (!log_file) {
            return;
        }
        if (ftell(log_file) == 0) {
            fprintf(log_file, "time_ms,mode,safety,active,x_frequency,y_frequency,x_peak,y_peak,changed_x,changed_y,unstable_windows,faulted\n");
        }
    }

    void close_log() {
        if (log_file) {
            fflush(log_file);
            fclose(log_file);
            log_file = nullptr;
        }
    }

    void reset_windows() {
        const auto current_x = input_shaper::get_axis_config(X_AXIS);
        const auto current_y = input_shaper::get_axis_config(Y_AXIS);
        analyzer_x.reset(current_x.value_or(*original_x).frequency, sample_rate);
        analyzer_y.reset(current_y.value_or(*original_y).frequency, sample_rate);
    }

    static float limited_frequency_step(float current, float target) {
        const float delta = std::clamp(target - current, -max_step_hz, max_step_hz);
        return input_shaper::clamp_frequency_to_safe_values(current + delta);
    }

    bool apply_axis(AxisEnum axis, std::optional<input_shaper::AxisConfig> current_config, std::optional<float> peak) {
        if (mode != Mode::correct) {
            return false;
        }
        if (!current_config.has_value() || !peak.has_value()) {
            return false;
        }

        const float next_frequency = limited_frequency_step(current_config->frequency, *peak);
        if (fabsf(next_frequency - current_config->frequency) < minimum_change_hz) {
            return false;
        }

        current_config->frequency = next_frequency;
        input_shaper::set_axis_config(axis, current_config);
        log_info(Marlin, "Adaptive Input Shaper %c adjusted to %f Hz from peak %f Hz",
            axis == X_AXIS ? 'X' : 'Y',
            static_cast<double>(next_frequency),
            static_cast<double>(*peak));
        return true;
    }

    void write_log(uint32_t now, const std::optional<input_shaper::AxisConfig> &current_x, const std::optional<input_shaper::AxisConfig> &current_y, std::optional<float> peak_x, std::optional<float> peak_y, bool changed_x, bool changed_y) {
        if (!log_file) {
            return;
        }
        fprintf(log_file, "%lu,%u,%u,%u,%.3f,%.3f,%.3f,%.3f,%u,%u,%u,%u\n",
            static_cast<unsigned long>(now),
            static_cast<unsigned>(mode),
            safety_brake ? 1 : 0,
            started ? 1 : 0,
            static_cast<double>(current_x ? current_x->frequency : 0.0f),
            static_cast<double>(current_y ? current_y->frequency : 0.0f),
            static_cast<double>(peak_x.value_or(0.0f)),
            static_cast<double>(peak_y.value_or(0.0f)),
            changed_x ? 1 : 0,
            changed_y ? 1 : 0,
            unstable_windows,
            latest_status.faulted ? 1 : 0);
        fflush(log_file);
    }

    bool safety_fault(const std::optional<input_shaper::AxisConfig> &current_x, const std::optional<input_shaper::AxisConfig> &current_y, std::optional<float> peak_x, std::optional<float> peak_y) {
        if (!safety_brake || !current_x.has_value() || !current_y.has_value() || !peak_x.has_value() || !peak_y.has_value()) {
            unstable_windows = 0;
            return false;
        }

        const bool unstable = fabsf(*peak_x - current_x->frequency) > safety_peak_jump_hz || fabsf(*peak_y - current_y->frequency) > safety_peak_jump_hz;
        unstable_windows = unstable ? static_cast<uint8_t>(std::min<uint8_t>(unstable_windows + 1, safety_fault_windows)) : 0;
        return unstable_windows >= safety_fault_windows;
    }

    void update_status(const std::optional<input_shaper::AxisConfig> &current_x, const std::optional<input_shaper::AxisConfig> &current_y, std::optional<float> peak_x, std::optional<float> peak_y, bool changed_x, bool changed_y) {
        latest_status.mode = mode;
        latest_status.active = started;
        latest_status.safety_brake = safety_brake;
        latest_status.x_frequency = current_x ? current_x->frequency : 0.0f;
        latest_status.y_frequency = current_y ? current_y->frequency : 0.0f;
        latest_status.x_peak = peak_x.value_or(0.0f);
        latest_status.y_peak = peak_y.value_or(0.0f);
        if (changed_x || changed_y) {
            ++latest_status.updates;
        }
    }

    void evaluate_window() {
        const uint32_t now = ticks_ms();
        if (ticks_diff(now, next_update_ms) < 0) {
            reset_windows();
            return;
        }
        if (planner.draining()) {
            reset_windows();
            return;
        }

        const auto current_x = input_shaper::get_axis_config(X_AXIS);
        const auto current_y = input_shaper::get_axis_config(Y_AXIS);
        const auto peak_x = analyzer_x.detected_peak();
        const auto peak_y = analyzer_y.detected_peak();

        if (safety_fault(current_x, current_y, peak_x, peak_y)) {
            latest_status.faulted = true;
            write_log(now, current_x, current_y, peak_x, peak_y, false, false);
            log_warning(Marlin, "Adaptive Input Shaper safety brake stopped unstable tracking");
            restore_original_config();
            return;
        }

        const bool changed_x = apply_axis(X_AXIS, current_x, peak_x);
        const bool changed_y = apply_axis(Y_AXIS, current_y, peak_y);
        update_status(input_shaper::get_axis_config(X_AXIS), input_shaper::get_axis_config(Y_AXIS), peak_x, peak_y, changed_x, changed_y);
        write_log(now, input_shaper::get_axis_config(X_AXIS), input_shaper::get_axis_config(Y_AXIS), peak_x, peak_y, changed_x, changed_y);
        if (changed_x || changed_y) {
            next_update_ms = now + update_cooldown_ms;
        }
        reset_windows();
    }

    void step() {
        if (!started) {
            return;
        }
        latest_status.active = true;
        if (accelerometer.get_error() != PrusaAccelerometer::Error::none) {
            log_warning(Marlin, "Adaptive Input Shaper stopped after accelerometer error");
            latest_status.faulted = true;
            restore_original_config();
            return;
        }

        PrusaAccelerometer::RawAcceleration raw;
        for (size_t i = 0; i < max_samples_per_step; ++i) {
            const auto result = accelerometer.get_sample_printer_coords(raw);
            if (result == PrusaAccelerometer::GetSampleResult::buffer_empty) {
                break;
            }
            if (result == PrusaAccelerometer::GetSampleResult::error) {
                log_warning(Marlin, "Adaptive Input Shaper stopped after sample error");
                latest_status.faulted = true;
                restore_original_config();
                return;
            }

            const auto acceleration = raw.to_acceleration();
            analyzer_x.add(acceleration.val[X_AXIS]);
            analyzer_y.add(acceleration.val[Y_AXIS]);
            if (analyzer_x.samples >= window_samples && analyzer_y.samples >= window_samples) {
                evaluate_window();
            }
        }
    }
};

std::optional<Session> session;
uint32_t next_start_attempt_ms = 0;

Mode configured_mode() {
    const uint8_t raw_mode = config_store().adaptive_input_shaper_mode.get();
    if (raw_mode > static_cast<uint8_t>(Mode::correct)) {
        return Mode::off;
    }
    return static_cast<Mode>(raw_mode);
}

void stop_session() {
    if (session.has_value()) {
        session.reset();
    }
    latest_status.active = false;
}

} // namespace

void step() {
    const Mode mode = configured_mode();
    const Mode previous_mode = latest_status.mode;
    if (mode != previous_mode) {
        latest_status.faulted = false;
    }
    latest_status.mode = mode;
    latest_status.safety_brake = config_store().adaptive_input_shaper_safety_brake.get();

    if (mode == Mode::off) {
        latest_status.faulted = false;
        stop_session();
        return;
    }

    if (!marlin_server::is_printing() || latest_status.faulted) {
        stop_session();
        return;
    }

    if (session.has_value() && (session->mode != mode || session->safety_brake != latest_status.safety_brake)) {
        stop_session();
        return;
    }

    if (!session.has_value()) {
        const uint32_t now = ticks_ms();
        if (ticks_diff(now, next_start_attempt_ms) < 0) {
            return;
        }

        session.emplace(mode, latest_status.safety_brake);
        if (!session->started) {
            session.reset();
            next_start_attempt_ms = now + retry_cooldown_ms;
        }
        return;
    }

    session->step();
    if (!session->started) {
        session.reset();
        next_start_attempt_ms = ticks_ms() + retry_cooldown_ms;
    }
}

Status get_status() {
    return latest_status;
}

} // namespace feature::adaptive_input_shaper

#endif
