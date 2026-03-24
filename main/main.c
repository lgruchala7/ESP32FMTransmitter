/*
 * File: main.c
 * Author: Lukasz Gruchala
 * Created: 2026-03-06
 */

/*==================================================================
    Includes
===================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_log.h"

#include "esp_bt.h"
#include "bt_app_core.h"
#include "bt_app_av.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"

#include "i2c_master_app.h"
#include "driver/gpio.h"
#include "si4713.h"
#include "si4713_cfg.h"
#include "sh1106.h"
#include "sh1106_cfg.h"
#include "driver/gptimer.h"

/*==================================================================
    Object-like macros
===================================================================*/

/* Log tag */
#define ENCODER_TAG "ENCODER"

/* Encoder pins */
#define GPIO_INPUT_ENCODER_SIA CONFIG_GPIO_INPUT_ENCODER_SIA
#define GPIO_INPUT_ENCODER_SIB CONFIG_GPIO_INPUT_ENCODER_SIB
#define GPIO_INPUT_ENCODER_SW CONFIG_GPIO_INPUT_ENCODER_SW

/* GPIO input/output level */
#define GPIO_LOW 0
#define GPIO_HIGH 1

/* Value by which the tx frequency is decreased or increased with every encoder step (in 10kHz units) */
#define ENCODER_STEP_VAL 10U
#define KNOB_DEBOUNCING_TIME_US 5000U

/*==================================================================
    Function-like macros
===================================================================*/

/* Waits for a specific time in ms  */
#define WAIT_MS(time)                                   \
    do                                                  \
    {                                                   \
        TickType_t start = xTaskGetTickCount();         \
        TickType_t timeout = pdMS_TO_TICKS((time));     \
        while (xTaskGetTickCount() < (start + timeout)) \
        {                                               \
        }                                               \
    } while (0)

/* Converts int value to its corresponding ASCII code value e.g. 0 -> '0' */
#define INT_TO_ASCII_CHAR(x) ((x) + 0x30U)

/*==================================================================
    Local types
===================================================================*/

/* Event for stack up */
enum
{
    BT_APP_EVT_STACK_UP = 0,
};

/* Encoder knob events */
enum
{
    TX_FREQ_INCREASE_EVT = 0,
    TX_FREQ_DECREASE_EVT = 1,
    PLAYBACK_STATE_CHANGE_EVT = 2,
};

/*==================================================================
    Local objects
===================================================================*/

static const char local_device_name[] = CONFIG_EXAMPLE_LOCAL_DEVICE_NAME;
static uint16_t tx_frequency = TX_TUNE_FREQ_DEFAULT_VAL;

/* Playback status strings */
static const char str_play[] = "PLAY";
static const char str_stop[] = "STOP";

static TaskHandle_t display_update_task_handle = NULL;
static i2c_master_dev_handle_t sh1106_dev_handle = NULL;
static i2c_master_dev_handle_t si4713_dev_handle = NULL;
static gptimer_handle_t debounce_timer = NULL;
static int knob_debounce_event;

/*==================================================================
    Local function declarations
===================================================================*/

/* Inline functions */
static inline void configure_si4713(void);
static inline void powerup_si4713(void);
static inline void tune_si4713(void);
static inline void audio_dynamic_range_control_si4713(void);
static inline void configure_sh1106(void);
static inline void write_initial_contents_sh1106(void);
static inline void init_encoder_control(void);

/* Handlers and callbacks */
static void bt_app_dev_cb(esp_bt_dev_cb_event_t event, esp_bt_dev_cb_param_t *param);
static void bt_app_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);
static bool knob_debounce_timer_cb(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_data);
static void bt_av_hdl_stack_evt(uint16_t event, void *p_param);
static void gpio_isr_handler(void *arg);

/* Auxiliary functions */
static char *bda2str(uint8_t *bda, char *str, size_t size);

/*==================================================================
    Function definitions
===================================================================*/

/**
 * @brief Handler for encoder GPIO ISR.
 *
 * This ISR handler checks the source of interrupt and starts the debounce timer.
 *
 * @param[in] arg Interrupt source GPIO.
 */
static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    uint64_t timer_count;

    gptimer_get_raw_count(debounce_timer, &timer_count);

    if (0U == timer_count)
    {
        if ((GPIO_INPUT_ENCODER_SIA == gpio_num) && (GPIO_HIGH == gpio_get_level(GPIO_INPUT_ENCODER_SIB)))
        {
            knob_debounce_event = TX_FREQ_DECREASE_EVT;
            gptimer_start(debounce_timer);
        }
        else if ((GPIO_INPUT_ENCODER_SIB == gpio_num) && (GPIO_HIGH == gpio_get_level(GPIO_INPUT_ENCODER_SIA)))
        {
            knob_debounce_event = TX_FREQ_INCREASE_EVT;
            gptimer_start(debounce_timer);
        }
        else if (GPIO_INPUT_ENCODER_SW == gpio_num)
        {
            /* toggle playback play/pause status */
            knob_debounce_event = PLAYBACK_STATE_CHANGE_EVT;
            gptimer_start(debounce_timer);
        }
        else
        {
            /* Do nothing */
        }
    }
}

/**
 * @brief Callback for debounce timer alarm event.
 *
 * This timer callback function performs knob debounce check. If a knob event is confirmed, the display task is notified.
 *
 * @param[in] timer Timer handle.
 * @param[in] edata Alarm event data, fed by driver.
 * @param[in] user_data User data.
 */
static bool IRAM_ATTR knob_debounce_timer_cb(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_data)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    switch (knob_debounce_event)
    {
    case TX_FREQ_DECREASE_EVT:
        if (GPIO_LOW == gpio_get_level(GPIO_INPUT_ENCODER_SIA))
        {
            vTaskNotifyGiveIndexedFromISR(display_update_task_handle, TX_FREQ_DECREASE_EVT, &xHigherPriorityTaskWoken);
        }
        break;

    case TX_FREQ_INCREASE_EVT:
        if (GPIO_LOW == gpio_get_level(GPIO_INPUT_ENCODER_SIB))
        {
            vTaskNotifyGiveIndexedFromISR(display_update_task_handle, TX_FREQ_INCREASE_EVT, &xHigherPriorityTaskWoken);
        }
        break;

    case PLAYBACK_STATE_CHANGE_EVT:
        if (GPIO_LOW == gpio_get_level(GPIO_INPUT_ENCODER_SW))
        {
            vTaskNotifyGiveIndexedFromISR(display_update_task_handle, PLAYBACK_STATE_CHANGE_EVT, &xHigherPriorityTaskWoken);
        }
        break;

    default:
        break;
    }

    gptimer_stop(debounce_timer);
    gptimer_set_raw_count(debounce_timer, 0U);

    return false;
}

/**
 * @brief Display refresh task handler.
 *
 * This function waits for encoder related notifications and updates display contents if a change has occurred.
 *
 * @param[in] arg Task argument - not used.
 */
static void display_update_task_handler(void *arg)
{
    bool playback_state = ON_STATE;
    const char *curr_char = NULL;
    bool frequency_changed = false;

    for (;;)
    {
        if (0U != ulTaskNotifyTakeIndexed(PLAYBACK_STATE_CHANGE_EVT, pdTRUE, pdMS_TO_TICKS(10)))
        {
            uint8_t column_address = COLUMN_ADDRESS_MIN;
            uint8_t page_address = PLAYBACK_STATE_PAGE_ADDRESS;
            sh1106_clear_page(sh1106_dev_handle, page_address);
            sh1106_clear_page(sh1106_dev_handle, (page_address + 1U));

            if (ON_STATE == playback_state)
            {
                curr_char = str_stop;
                playback_state = OFF_STATE;
            }
            else if (OFF_STATE == playback_state)
            {
                curr_char = str_play;
                playback_state = ON_STATE;
            }

            while (0 != *curr_char)
            {
                sh1106_write_character_big(sh1106_dev_handle, *curr_char, page_address, &column_address);
                curr_char++;
            }

            ESP_LOGI(ENCODER_TAG, "Playback state changed to %s.", (ON_STATE == playback_state) ? str_play : str_stop);
        }

        if (0U != ulTaskNotifyTakeIndexed(TX_FREQ_INCREASE_EVT, pdTRUE, pdMS_TO_TICKS(10)))
        {
            if (tx_frequency < TX_TUNE_FREQ_MAX)
            {
                tx_frequency += ENCODER_STEP_VAL;
            }
            else
            {
                tx_frequency = TX_TUNE_FREQ_MIN;
            }

            frequency_changed = true;
        }
        if (0U != ulTaskNotifyTakeIndexed(TX_FREQ_DECREASE_EVT, pdTRUE, pdMS_TO_TICKS(10)))
        {
            if (tx_frequency > TX_TUNE_FREQ_MIN)
            {
                tx_frequency -= ENCODER_STEP_VAL;
            }
            else
            {
                tx_frequency = TX_TUNE_FREQ_MAX;
            }

            frequency_changed = true;
        }

        if (true == frequency_changed)
        {
            /* Change FM transmitter actual frequency */
            si4713_tx_tune_freq(si4713_dev_handle, tx_frequency);

            /* Change displayed frequency */
            uint8_t column_address = COLUMN_ADDRESS_MIN;
            uint8_t page_address = TX_FREQ_PAGE_ADDRESS;

            sh1106_clear_page(sh1106_dev_handle, page_address);
            sh1106_clear_page(sh1106_dev_handle, (page_address + 1U));

            /* Write integral part of frequency */
            uint16_t integral_part = tx_frequency / 100U;
            uint8_t integral_part_digits[3] = {0U};

            integral_part_digits[0] = integral_part / 100U;
            integral_part_digits[1] = (integral_part / 10U) - (integral_part_digits[0] * 10U);
            integral_part_digits[2] = integral_part - (integral_part_digits[0] * 100U) - (integral_part_digits[1] * 10U);

            for (int i = 0; i < sizeof(integral_part_digits); i++)
            {
                uint8_t digit = integral_part_digits[i];

                /* Skip displaying 0 if frequency smaller than 100 MHz*/
                if ((0 == i) && (0U == digit))
                {
                    continue;
                }

                char digit_ascii = INT_TO_ASCII_CHAR(digit);
                sh1106_write_character_big(sh1106_dev_handle, digit_ascii, page_address, &column_address);
            }

            /* Write decimal separator character */
            sh1106_write_character_big(sh1106_dev_handle, '.', page_address, &column_address);

            /* Write decimal part of frequency */
            uint16_t decimal_part = tx_frequency - (integral_part * 100U);
            uint8_t decimal_part_digits[2] = {0U};

            decimal_part_digits[0] = decimal_part / 10U;
            decimal_part_digits[1] = decimal_part - (decimal_part_digits[0] * 10U);

            for (int i = 0; i < sizeof(decimal_part_digits); i++)
            {
                uint8_t digit = decimal_part_digits[i];
                char digit_ascii = INT_TO_ASCII_CHAR(digit);
                sh1106_write_character_big(sh1106_dev_handle, digit_ascii, page_address, &column_address);
            }

            frequency_changed = false;

            ESP_LOGI(ENCODER_TAG, "TX frequency changed to %u.%u", integral_part, decimal_part);
        }
    }
}

/* Auxiliary raw data to string conversion function */
static char *bda2str(uint8_t *bda, char *str, size_t size)
{
    if (bda == NULL || str == NULL || size < 18)
    {
        return NULL;
    }

    uint8_t *p = bda;
    sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x",
            p[0], p[1], p[2], p[3], p[4], p[5]);
    return str;
}

/* Device callback function */
static void bt_app_dev_cb(esp_bt_dev_cb_event_t event, esp_bt_dev_cb_param_t *param)
{
    switch (event)
    {
    case ESP_BT_DEV_NAME_RES_EVT:
    {
        if (param->name_res.status == ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGI(BT_AV_TAG, "Get local device name success: %s", param->name_res.name);
        }
        else
        {
            ESP_LOGE(BT_AV_TAG, "Get local device name failed, status: %d", param->name_res.status);
        }
        break;
    }
    default:
    {
        ESP_LOGI(BT_AV_TAG, "event: %d", event);
        break;
    }
    }
}

/* GAP callback function */
static void bt_app_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    uint8_t *bda = NULL;

    switch (event)
    {
    /* when authentication completed, this event comes */
    case ESP_BT_GAP_AUTH_CMPL_EVT:
    {
        if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGI(BT_AV_TAG, "authentication success: %s", param->auth_cmpl.device_name);
            ESP_LOG_BUFFER_HEX(BT_AV_TAG, param->auth_cmpl.bda, ESP_BD_ADDR_LEN);
        }
        else
        {
            ESP_LOGE(BT_AV_TAG, "authentication failed, status: %d", param->auth_cmpl.stat);
        }
        ESP_LOGI(BT_AV_TAG, "link key type of current link is: %d", param->auth_cmpl.lk_type);
        break;
    }
    case ESP_BT_GAP_ENC_CHG_EVT:
    {
        char *str_enc[3] = {"OFF", "E0", "AES"};
        bda = (uint8_t *)param->enc_chg.bda;
        ESP_LOGI(BT_AV_TAG, "Encryption mode to [%02x:%02x:%02x:%02x:%02x:%02x] changed to %s",
                 bda[0], bda[1], bda[2], bda[3], bda[4], bda[5], str_enc[param->enc_chg.enc_mode]);
        break;
    }

#if (CONFIG_EXAMPLE_A2DP_SINK_SSP_ENABLED == true)
    /* when Security Simple Pairing user confirmation requested, this event comes */
    case ESP_BT_GAP_CFM_REQ_EVT:
        ESP_LOGI(BT_AV_TAG, "ESP_BT_GAP_CFM_REQ_EVT Please compare the numeric value: %06" PRIu32, param->cfm_req.num_val);
        esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
        break;
    /* when Security Simple Pairing passkey notified, this event comes */
    case ESP_BT_GAP_KEY_NOTIF_EVT:
        ESP_LOGI(BT_AV_TAG, "ESP_BT_GAP_KEY_NOTIF_EVT passkey: %06" PRIu32, param->key_notif.passkey);
        break;
    /* when Security Simple Pairing passkey requested, this event comes */
    case ESP_BT_GAP_KEY_REQ_EVT:
        ESP_LOGI(BT_AV_TAG, "ESP_BT_GAP_KEY_REQ_EVT Please enter passkey!");
        break;
#endif

    /* when GAP mode changed, this event comes */
    case ESP_BT_GAP_MODE_CHG_EVT:
        ESP_LOGI(BT_AV_TAG, "ESP_BT_GAP_MODE_CHG_EVT mode: %d, interval: %.2f ms",
                 param->mode_chg.mode, param->mode_chg.interval * 0.625);
        break;
    /* when ACL connection completed, this event comes */
    case ESP_BT_GAP_ACL_CONN_CMPL_STAT_EVT:
        bda = (uint8_t *)param->acl_conn_cmpl_stat.bda;
        ESP_LOGI(BT_AV_TAG, "ESP_BT_GAP_ACL_CONN_CMPL_STAT_EVT Connected to [%02x:%02x:%02x:%02x:%02x:%02x], status: 0x%x",
                 bda[0], bda[1], bda[2], bda[3], bda[4], bda[5], param->acl_conn_cmpl_stat.stat);
        break;
    /* when ACL disconnection completed, this event comes */
    case ESP_BT_GAP_ACL_DISCONN_CMPL_STAT_EVT:
        bda = (uint8_t *)param->acl_disconn_cmpl_stat.bda;
        ESP_LOGI(BT_AV_TAG, "ESP_BT_GAP_ACL_DISC_CMPL_STAT_EVT Disconnected from [%02x:%02x:%02x:%02x:%02x:%02x], reason: 0x%x",
                 bda[0], bda[1], bda[2], bda[3], bda[4], bda[5], param->acl_disconn_cmpl_stat.reason);
        break;
    /* others */
    default:
    {
        ESP_LOGI(BT_AV_TAG, "event: %d", event);
        break;
    }
    }
}

/* Handler for bluetooth stack enabled events */
static void bt_av_hdl_stack_evt(uint16_t event, void *p_param)
{
    ESP_LOGD(BT_AV_TAG, "%s event: %d", __func__, event);

    switch (event)
    {
    /* when do the stack up, this event comes */
    case BT_APP_EVT_STACK_UP:
    {
        esp_bt_gap_set_device_name(local_device_name);
        esp_bt_dev_register_callback(bt_app_dev_cb);
        esp_bt_gap_register_callback(bt_app_gap_cb);

        esp_avrc_ct_register_callback(bt_app_rc_ct_cb);
        assert(esp_avrc_ct_init() == ESP_OK);
        esp_avrc_tg_register_callback(bt_app_rc_tg_cb);
        assert(esp_avrc_tg_init() == ESP_OK);

        esp_avrc_rn_evt_cap_mask_t evt_set = {0};
        esp_avrc_rn_evt_bit_mask_operation(ESP_AVRC_BIT_MASK_OP_SET, &evt_set, ESP_AVRC_RN_VOLUME_CHANGE);
        assert(esp_avrc_tg_set_rn_evt_cap(&evt_set) == ESP_OK);

        esp_a2d_register_callback(&bt_app_a2d_cb);
        assert(esp_a2d_sink_init() == ESP_OK);

#if CONFIG_EXAMPLE_A2DP_SINK_USE_EXTERNAL_CODEC == FALSE
        esp_a2d_sink_register_data_callback(bt_app_a2d_data_cb);
#else
        esp_a2d_mcc_t mcc = {0};
        mcc.type = ESP_A2D_MCT_SBC;
        mcc.cie.sbc_info.samp_freq = 0xf;
        mcc.cie.sbc_info.ch_mode = 0xf;
        mcc.cie.sbc_info.block_len = 0xf;
        mcc.cie.sbc_info.num_subbands = 0x3;
        mcc.cie.sbc_info.alloc_mthd = 0x3;
        mcc.cie.sbc_info.max_bitpool = 250;
        mcc.cie.sbc_info.min_bitpool = 2;
        /* register stream end point, only support mSBC currently */
        esp_a2d_sink_register_stream_endpoint(0, &mcc);
        esp_a2d_sink_register_audio_data_callback(bt_app_a2d_audio_data_cb);
#endif
        /* Get the default value of the delay value */
        esp_a2d_sink_get_delay_value();
        /* Get local device name */
        esp_bt_gap_get_device_name();

        /* set discoverable and connectable mode, wait to be connected */
        esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
        break;
    }
    /* others */
    default:
        ESP_LOGE(BT_AV_TAG, "%s unhandled event: %d", __func__, event);
        break;
    }
}

/* Si4713 powerup function */
static inline void powerup_si4713(void)
{
    gpio_config_t rst_pin_config =
        {
            .pin_bit_mask = (1ULL << GPIO_OUTPUT_SI4713_RST),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
    gpio_config(&rst_pin_config);

    /* put si4713 in reset */
    gpio_set_level(GPIO_OUTPUT_SI4713_RST, 0U);
    WAIT_MS(10);

    /* release si4713 from reset */
    gpio_set_level(GPIO_OUTPUT_SI4713_RST, 1U);
    WAIT_MS(10);

    if (ESP_OK == si4713_powerup_analog(si4713_dev_handle, POWER_UP_DEFAULT_VAL))
    {
        ESP_LOGI(SI4713_TAG, "Si4713 initialized successfully");
    }
    else
    {
        ESP_LOGI(SI4713_TAG, "Si4713 initialization failed");
    }
    si4713_set_property(si4713_dev_handle, TX_LINE_INPUT_LEVEL, TX_LINE_INPUT_LEVEL_DEFAULT_VAL);
}

/* Si4713 configuration function */
static inline void configure_si4713(void)
{
    si4713_get_rev(si4713_dev_handle);
    si4713_set_property(si4713_dev_handle, GPO_IEN, GPO_IEN_DEFAULT_VAL);
    si4713_set_property(si4713_dev_handle, TX_LINE_INPUT_MUTE, TX_LINE_INPUT_MUTE_DEFAULT_VAL);
    si4713_set_property(si4713_dev_handle, TX_PREEMPHASIS, TX_PREEMPHASIS_DEFAULT_VAL);
    si4713_set_property(si4713_dev_handle, TX_PILOT_FREQUENCY, TX_PILOT_FREQUENCY_DEFAULT_VAL);
    si4713_set_property(si4713_dev_handle, TX_AUDIO_DEVIATION, TX_AUDIO_DEVIATION_DEFAULT_VAL);
    si4713_set_property(si4713_dev_handle, TX_PILOT_DEVIATION, TX_PILOT_DEVIATION_DEFAULT_VAL);
}

/* Si4713 tuning function */
static inline void tune_si4713(void)
{
    uint8_t status_expected;

    si4713_tx_tune_power(si4713_dev_handle, TX_TUNE_POWER_DEFAULT_VAL);
    status_expected = (uint8_t)((1U << STATUS_CTS_BIT_POS) | (1U << STATUS_STCINT_BIT_POS));
    si4713_get_int_status(si4713_dev_handle, status_expected, T_STC_SHORT_MS);
    si4713_tx_tune_status(si4713_dev_handle); // to clean the STCINT bit
    si4713_tx_tune_freq(si4713_dev_handle, tx_frequency);
    status_expected = (uint8_t)((1U << STATUS_CTS_BIT_POS) | (1U << STATUS_STCINT_BIT_POS));
    si4713_get_int_status(si4713_dev_handle, status_expected, T_STC_LONG_MS);
    si4713_tx_tune_status(si4713_dev_handle); // to clean the STCINT bit
    si4713_set_property(si4713_dev_handle, TX_COMPONENT_ENABLE, TX_COMPONENT_ENABLE_DEFAULT_VAL);
}

/* Si4713 audio dynamic range control function */
static inline void audio_dynamic_range_control_si4713(void)
{
    uint8_t status_expected;

    si4713_set_property(si4713_dev_handle, TX_ACOMP_THRESHOLD, TX_ACOMP_THRESHOLD_DEFAULT_VAL);
    si4713_set_property(si4713_dev_handle, TX_ACOMP_GAIN, TX_ACOMP_GAIN_DEFAULT_VAL);
    si4713_set_property(si4713_dev_handle, TX_ACOMP_RELEASE_TIME, TX_ACOMP_RELEASE_TIME_DEFAULT_VAL);
    si4713_set_property(si4713_dev_handle, TX_ACOMP_ATTACK_TIME, TX_ACOMP_ATTACK_TIME_DEFAULT_VAL);
    si4713_set_property(si4713_dev_handle, TX_ACOMP_ENABLE, TX_ACOMP_ENABLE_DEFAULT_VAL);
    si4713_set_property(si4713_dev_handle, TX_LIMITER_RELEASE_TIME, TX_LIMITER_RELEASE_TIME_DEFAULT_VAL);
    si4713_set_property(si4713_dev_handle, TX_ASQ_LEVEL_LOW, TX_ASQ_LEVEL_LOW_DEFAULT_VAL);
    si4713_set_property(si4713_dev_handle, TX_ASQ_DURATION_LOW, TX_ASQ_DURATION_LOW_DEFAULT_VAL);
    si4713_set_property(si4713_dev_handle, TX_ASQ_LEVEL_HIGH, TX_ASQ_LEVEL_HIGH_DEFAULT_VAL);
    si4713_set_property(si4713_dev_handle, TX_ASQ_DURATION_HIGH, TX_ASQ_DURATION_HIGH_DEFAULT_VAL);
    si4713_set_property(si4713_dev_handle, TX_ASQ_INTERRUPT_SOURCE, TX_ASQ_INTERRUPT_SOURCE_DEFAULT_VAL);
    status_expected = (uint8_t)((1U << STATUS_CTS_BIT_POS) | (1U << STATUS_ASQINT_BIT_POS));
    si4713_get_int_status(si4713_dev_handle, status_expected, 1U);
    si4713_tx_asq_status(si4713_dev_handle); // to clean the ASQINT bit
}

/* SH1106 configuration function */
static inline void configure_sh1106(void)
{
    sh1106_display_off_on(sh1106_dev_handle, OFF_STATE);
    sh1106_set_clock_div_ratio_osc_freq(sh1106_dev_handle, DIV_RATIO(1U), OSC_FREQ_DEFAULT);
    sh1106_set_multiplex_ratio(sh1106_dev_handle, MUX_RATIO(64U));
    sh1106_set_display_offset(sh1106_dev_handle, DISPLAY_OFFSET(0U));
    sh1106_set_display_start_line(sh1106_dev_handle, DISPLAY_START_LINE(0U));
    sh1106_set_dc_dc_off_on(sh1106_dev_handle, ON_STATE);
    sh1106_set_segment_remap(sh1106_dev_handle, ADC_REVERSE_DIR);
    sh1106_set_common_output_scan_dir(sh1106_dev_handle, SCAN_DIR_DESCENDING);
    sh1106_set_common_pads_hw_config(sh1106_dev_handle, HW_CONFIG_MODE_ALTERNATIVE);
    sh1106_set_contrast_ctrl_register(sh1106_dev_handle, DISPLAY_CONTRAST_VERY_HIGH);
    sh1106_set_discharge_precharge_period(sh1106_dev_handle, DISCHARGE_PERIOD_DCLK(1U), PRECHARGE_PERIOD_DCLK(15U));
    sh1106_set_vcom_deselect_lvl(sh1106_dev_handle, 100U);
    sh1106_set_pump_voltage(sh1106_dev_handle, VPP_9V);
    sh1106_set_normal_reverse_display(sh1106_dev_handle, DISPLAY_DATA_NORMAL);
    sh1106_set_entire_display_off_on(sh1106_dev_handle, OFF_STATE);
}

/* THis function writes initial contents to the display */
static inline void write_initial_contents_sh1106(void)
{
    uint8_t column_address = COLUMN_ADDRESS_MIN;
    uint8_t page_address = HEADER_PAGE_ADDRESS;

    const char str_1[] = "FM Frequency";
    const char *curr_char = str_1;
    while (0 != *curr_char)
    {
        sh1106_write_character_big(sh1106_dev_handle, *curr_char, page_address, &column_address);
        curr_char++;
    }

    column_address = COLUMN_ADDRESS_MIN;
    page_address = NEXT_PAGE_BIG(page_address);

    char str_2[7];
    sprintf(str_2, "%d.%d", (TX_TUNE_FREQ_DEFAULT_VAL / 100), (TX_TUNE_FREQ_DEFAULT_VAL - (TX_TUNE_FREQ_DEFAULT_VAL / 100) * 100));
    ESP_LOGI(SH1106_TAG, "%s", str_2);
    curr_char = str_2;
    while (0 != *curr_char)
    {
        sh1106_write_character_big(sh1106_dev_handle, *curr_char, page_address, &column_address);
        curr_char++;
    }

    column_address = COLUMN_ADDRESS_MIN;
    page_address = NEXT_PAGE_BIG(page_address);

    curr_char = str_play;
    while (0 != *curr_char)
    {
        sh1106_write_character_big(sh1106_dev_handle, *curr_char, page_address, &column_address);
        curr_char++;
    }
}

/* Initializes encoder pins and GPIO interrupts. */
static inline void init_encoder_control(void)
{
    /* Initialize debounce timer */
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000, // 1MHz, 1 tick=1us
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &debounce_timer));

    /* Set callback for the timer */
    gptimer_event_callbacks_t cb = {
        .on_alarm = knob_debounce_timer_cb,
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(debounce_timer, &cb, NULL));
    ESP_ERROR_CHECK(gptimer_enable(debounce_timer));
    gptimer_alarm_config_t alarm_config = {
        .alarm_count = KNOB_DEBOUNCING_TIME_US, // period = 5ms=5000us
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(debounce_timer, &alarm_config));

    /* Frequency and playback control encoder configuration */
    gpio_config_t encoder_pins_config = {
        .pin_bit_mask = ((1ULL << GPIO_INPUT_ENCODER_SIA) | (1ULL << GPIO_INPUT_ENCODER_SIB) | (1ULL << GPIO_INPUT_ENCODER_SW)),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&encoder_pins_config);

    /* Configure interrupt handlers for encoder GPIOs */
    gpio_install_isr_service(0U);
    gpio_isr_handler_add(GPIO_INPUT_ENCODER_SIA, gpio_isr_handler, (void *)GPIO_INPUT_ENCODER_SIA);
    gpio_isr_handler_add(GPIO_INPUT_ENCODER_SIB, gpio_isr_handler, (void *)GPIO_INPUT_ENCODER_SIB);
    gpio_isr_handler_add(GPIO_INPUT_ENCODER_SW, gpio_isr_handler, (void *)GPIO_INPUT_ENCODER_SW);
}

void app_main(void)
{
    char bda_str[18] = {0};
    /* initialize NVS — it is used to store PHY calibration data */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    /*
     * Only functions of Classical Bluetooth are used.
     * So release the controller memory for Bluetooth Low Energy.
     */
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    if ((err = esp_bt_controller_init(&bt_cfg)) != ESP_OK)
    {
        ESP_LOGE(BT_AV_TAG, "%s initialize controller failed: %s", __func__, esp_err_to_name(err));
        return;
    }
    if ((err = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT)) != ESP_OK)
    {
        ESP_LOGE(BT_AV_TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(err));
        return;
    }

    esp_bluedroid_config_t bluedroid_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
#if (CONFIG_EXAMPLE_A2DP_SINK_SSP_ENABLED == false)
    bluedroid_cfg.ssp_en = false;
#endif
    if ((err = esp_bluedroid_init_with_cfg(&bluedroid_cfg)) != ESP_OK)
    {
        ESP_LOGE(BT_AV_TAG, "%s initialize bluedroid failed: %s", __func__, esp_err_to_name(err));
        return;
    }

    if ((err = esp_bluedroid_enable()) != ESP_OK)
    {
        ESP_LOGE(BT_AV_TAG, "%s enable bluedroid failed: %s", __func__, esp_err_to_name(err));
        return;
    }

#if (CONFIG_EXAMPLE_A2DP_SINK_SSP_ENABLED == true)
    /* set default parameters for Secure Simple Pairing */
    esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
    esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_IO;
    esp_bt_gap_set_security_param(param_type, &iocap, sizeof(uint8_t));
#endif

    /* set default parameters for Legacy Pairing (use fixed pin code 1234) */
    esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_FIXED;
    esp_bt_pin_code_t pin_code;
    pin_code[0] = '1';
    pin_code[1] = '2';
    pin_code[2] = '3';
    pin_code[3] = '4';
    esp_bt_gap_set_pin(pin_type, 4, pin_code);

    ESP_LOGI(BT_AV_TAG, "Own address:[%s]", bda2str((uint8_t *)esp_bt_dev_get_address(), bda_str, sizeof(bda_str)));
    bt_app_task_start_up();
    /* bluetooth device name, connection mode and profile set up */
    bt_app_work_dispatch(bt_av_hdl_stack_evt, BT_APP_EVT_STACK_UP, NULL, 0, NULL);

    /* I2C */
    i2c_master_bus_handle_t bus_handle;
    i2c_init(&bus_handle);
    ESP_LOGI(I2C_MASTER_TAG, "I2C initialized successfully");

    // /* Si4713 */
    i2c_add_device(bus_handle, &si4713_dev_handle, SI4173_SENSOR_ADDR);
    ESP_LOGI(SI4713_TAG, "Si4713 added to I2C bus.");

    powerup_si4713();
    configure_si4713();
    tune_si4713();
    audio_dynamic_range_control_si4713();

    /* SH1106 */
    i2c_add_device(bus_handle, &sh1106_dev_handle, SH1106_SENSOR_ADDR);
    ESP_LOGI(SH1106_TAG, "SH1106 added to I2C bus.");

    configure_sh1106();
    sh1106_display_off_on(sh1106_dev_handle, OFF_STATE);
    WAIT_MS(100);
    sh1106_display_off_on(sh1106_dev_handle, ON_STATE);
    ESP_LOGI(SH1106_TAG, "SH1106 device ON.");

    for (int i = PAGE_ADDRESS_MIN; i <= PAGE_ADDRESS_MAX; i++)
    {
        sh1106_clear_page(sh1106_dev_handle, i);
    }

    write_initial_contents_sh1106();
    /* Create a task for updating the display contents */
    xTaskCreate(display_update_task_handler, "display_update_task", 3072U, NULL, 10, &display_update_task_handle);
    init_encoder_control();
}
