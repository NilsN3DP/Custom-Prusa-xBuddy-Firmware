#pragma once

#include <WindowItemFormatableLabel.hpp>
#include <WindowMenuSpin.hpp>
#include <cstdint>

class MI_CHAMBER_TARGET_TEMP : public WiSpin {
public:
    MI_CHAMBER_TARGET_TEMP(const char *label = N_("Chamber Temperature"));

protected:
    virtual void OnClick() override;
    virtual void Loop() override;
};

class MI_CHAMBER_TEMP : public WI_LAMBDA_LABEL_t {
    static constexpr int16_t invalid_temperature = INT16_MIN;

    int16_t temperature_tenths_ = invalid_temperature;
    uint32_t last_update_ms_ = 0;

    bool update_displayed_temperature();

public:
    MI_CHAMBER_TEMP(const char *label = N_("Chamber Temperature"));

protected:
    virtual void Loop() override;
};
