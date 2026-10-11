
#include "sensor_manager.h"

#include "esp_adc/adc_oneshot.h"

#define CURRENT_ADC_CHANNEL     ADC_CHANNEL_6  // GPIO34
#define TEMPERATURE_ADC_CHANNEL ADC_CHANNEL_7  // GPIO35

static adc_oneshot_unit_handle_t adc_handle;

esp_err_t sensor_manager_init(void)
{
    adc_oneshot_unit_init_cfg_t unit_config = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };

    esp_err_t err = adc_oneshot_new_unit(&unit_config, &adc_handle);
    if (err != ESP_OK) {
        return err;
    }

    adc_oneshot_chan_cfg_t channel_config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    err = adc_oneshot_config_channel(
        adc_handle, CURRENT_ADC_CHANNEL, &channel_config);
    if (err != ESP_OK) {
        return err;
    }

    return adc_oneshot_config_channel(
        adc_handle, TEMPERATURE_ADC_CHANNEL, &channel_config);
}

esp_err_t sensor_manager_read(sensor_readings_t *readings)
{
    if (readings == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = adc_oneshot_read(
        adc_handle, CURRENT_ADC_CHANNEL, &readings->current_raw);
    if (err != ESP_OK) {
        return err;
    }

    return adc_oneshot_read(
        adc_handle, TEMPERATURE_ADC_CHANNEL,
        &readings->temperature_raw);
}
