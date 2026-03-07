/*
 * File: si4713.c
 * Author: Lukasz Gruchala
 * Created: 2026-03-06
 */

/*==================================================================
    Includes
===================================================================*/

#include "si4713.h"
#include "si4713_cfg.h"
#include "freertos/FreeRTOS.h"

/*==================================================================
Object-like macros
===================================================================*/

/*==================================================================
Function-like macros
===================================================================*/

/*==================================================================
    Local types
===================================================================*/

/* FM transmitter command summary */
typedef enum
{
    POWER_UP = 0x01,        /* Power up device and mode selection. Modes include FM transmit and analog/digital audio interface configuration. */
    GET_REV = 0x10,         /* Returns revision information on the device. */
    POWER_DOWN = 0x11,      /* Power down device. */
    SET_PROPERTY = 0x12,    /* Sets the value of a property. */
    GET_PROPERTY = 0x13,    /* Retrieves a property’s value. */
    GET_INT_STATUS = 0x14,  /* Read interrupt status bits. */
    PATCH_ARGS = 0x15,      /* Reserved command used for patch file downloads. */
    PATCH_DATA = 0x16,      /* Reserved command used for patch file downloads. */
    TX_TUNE_FREQ = 0x30,    /* Tunes to given transmit frequency. */
    TX_TUNE_POWER = 0x31,   /* Sets the output power level and tunes the antenna capacitor */
    TX_TUNE_MEASURE = 0x32, /* Measure the received noise level at the specified frequency. */
    TX_TUNE_STATUS = 0x33,  /* Queries the status of a previously sent TX Tune Freq, TX Tune Power, or TX Tune Measure command. */
    TX_ASQ_STATUS = 0x34,   /* Queries the TX status and input audio signal metrics. */
    TX_RDS_BUFF = 0x35,     /* Si4713 Only. Queries the status of the RDS Group Buffer and loads new data into buffer. */
    TX_RDS_PS = 0x36,       /* Si4713 Only. Set up default PS strings. */
    GPO_CTL = 0x80,         /* Configures GPO3 as output or Hi-Z. */
    GPO_SET = 0x81,         /* Sets GPO3 output level (low or high) */
} si4713_command_t;

/* FM transmitter property summary */
typedef enum
{
    GPO_IEN = 0x0001,                   /* Enables interrupt sources. 0x0000 */
    DIGITAL_INPUT_FORMAT = 0x0101,      /* Configures the digital input format. 0x0000 */
    DIGITAL_INPUT_SAMPLE_RATE = 0x0103, /* Configures the digital input sample rate in 10 Hz steps. Default is 0. 0x0000 */
    REFCLK_FREQ = 0x0201,               /* Sets frequency of the reference clock in Hz. The range is31130 to 34406 Hz, or 0 to disable the AFC. Default is 32768 Hz. 0x8000 */
    REFCLK_PRESCALE = 0x0202,           /* Sets the prescaler value for the reference clock. 0x0001 */
    TX_COMPONENT_ENABLE = 0x2100,       /* Enable transmit multiplex signal components. Default has pilot and L-R enabled. 0x0003 */
    TX_AUDIO_DEVIATION = 0x2101,        /* Configures audio frequency deviation level. Units are in 10 Hz increments. Default is 6285 (68.25 kHz). 0x1AA9 */
    TX_PILOT_DEVIATION = 0x2102,        /* Configures pilot tone frequency deviation level. Units are in 10 Hz increments. Default is 675 (6.75 kHz) 0x02A3 */
    TX_RDS_DEVIATION = 0x2103,          /* Si4713 Only. Configures the RDS/RBDS frequency deviation level. Units are in 10 Hz increments. Default is 2 kHz. 0x00C8 */
    TX_LINE_INPUT_LEVEL = 0x2104,       /* Configures maximum analog line input level to the LIN/RIN pins to reach the maximum deviation level programmed into the audio deviation property TX Audio Deviation. Default is 636 mV PK . 0x327C */
    TX_LINE_INPUT_MUTE = 0x2105,        /* Sets line input mute. L and R inputs may be independently muted. Default is not muted. 0x0000 */
    TX_PREEMPHASIS = 0x2106,            /* Configures preemphasis time constant. Default is 0 (75 μS). 0x0000 */
    TX_PILOT_FREQUENCY = 0x2107,        /* Configures the frequency of the stereo pilot. Default is 19000 Hz. 0x4A38 */
    TX_ACOMP_ENABLE = 0x2200,           /* Enables audio dynamic range control. Default is 0 (disabled). 0x0002 */
    TX_ACOMP_THRESHOLD = 0x2201,        /* Sets the threshold level for audio dynamic range control. Default is –40 dB. 0xFFD8 */
    TX_ACOMP_ATTACK_TIME = 0x2202,      /* Sets the attack time for audio dynamic range control. Default is 0 (0.5 ms). 0x0000 */
    TX_ACOMP_RELEASE_TIME = 0x2203,     /* Sets the release time for audio dynamic range control. Default is 4 (1000 ms). 0x0004 */
    TX_ACOMP_GAIN = 0x2204,             /* Sets the gain for audio dynamic range control. Default is 15 dB. 0x000F */
    TX_LIMITER_RELEASE_TIME = 0x2205,   /* Sets the limiter release time. Default is 102 (5.01 ms) 0x0066 */
    TX_ASQ_INTERRUPT_SOURCE = 0x2300,   /* Configures measurements related to signal quality metrics. Default is none selected. 0x0000 */
    TX_ASQ_LEVEL_LOW = 0x2301,          /* Configures low audio input level detection threshold. This threshold can be used to detect silence on the incoming audio. 0x0000 */
    TX_ASQ_DURATION_LOW = 0x2302,       /* Configures the duration which the input audio level must be below the low threshold in order to detect a low audio condition. 0x0000 */
    TX_ASQ_LEVEL_HIGH = 0x2303,         /* Configures high audio input level detection threshold. This threshold can be used to detect activity on the incoming audio. 0x0000 */
    TX_ASQ_DURATION_HIGH = 0x2304,      /* Configures the duration which the input audio level must be above the high threshold in order to detect a high audio condition. 0x0000 */
    TX_RDS_INTERRUPT_SOURCE = 0x2C00,   /* Si4713 Only. Configure RDS interrupt sources. Default is none selected. 0x0000 */
    TX_RDS_PI = 0x2C01,                 /* Si4713 Only. Sets transmit RDS program identifier. 0x40A7 */
    TX_RDS_PS_MIX = 0x2C02,             /* Si4713 Only. Configures mix of RDS PS Group with RDS Group Buffer. 0x0003 */
    TX_RDS_PS_MISC = 0x2C03,            /* Si4713 Only. Miscellaneous bits to transmit along with RDS_PS Groups. 0x1008 */
    TX_RDS_PS_REPEAT_COUNT = 0x2C04,    /* Si4713 Only. Number of times to repeat transmission of a PS message before transmitting the next PS message. 0x0003 */
    TX_RDS_PS_MESSAGE_COUNT = 0x2C05,   /* Si4713 Only. Number of PS messages in use. 0x0001 */
    TX_RDS_PS_AF = 0x2C06,              /* Si4713 Only. RDS Program Service Alternate Frequency. This provides the ability to inform the receiver of a single alternate frequency using AF Method A coding and is transmitted along with the RDS_PS Groups. 0xE0E0 */
    TX_RDS_FIFO_SIZESi4713 = 0x2C07,    /* Only. Number of blocks reserved for the FIFO. Note that the value written must be one larger than the desired FIFO size. 0x0000 */
} si4713_argument_t;

/* Structure of Si4713 status response */
typedef struct
{
    uint8_t stcint : 1; /* Seek/Tune Complete Interrupt. 0 = Tune complete has not been triggered. 1 = Tune complete has been triggered. */
    uint8_t asqint : 1; /* Signal Quality Interrupt. 0 = Signal quality measurement has not been triggered. 1 = Signal quality measurement has been triggered. */
    uint8_t rdsint : 1; /* RDS Interrupt. 0 = RDS interrupt has not been triggered. 1 = RDS interrupt has been triggered. */
    uint8_t : 3;        /* Reserved */
    uint8_t err : 1;    /* Error. 0 = No error, 1 = Error */
    uint8_t cts : 1;    /* Clear to Send. 0 = Wait before sending next command. 1 = Clear to send next command */
} si4713_status_response_t;

/*==================================================================
    Local objects
===================================================================*/

/*==================================================================
    Local function declarations
===================================================================*/

static inline esp_err_t si4713_read_status(i2c_master_dev_handle_t dev_handle, si4713_status_response_t *status_ptr);

/*==================================================================
    Function definitions
===================================================================*/

/**
 * @brief Read status value from Si4713
 *
 * @param[in] dev_handle I2C master device handle
 * @param[out] status_ptr status variable pointer
 * @return Si4713 status read operation result.
 */
static inline esp_err_t si4713_read_status(i2c_master_dev_handle_t dev_handle, si4713_status_response_t *status_ptr)
{
    return i2c_read_response(dev_handle, (uint8_t *)status_ptr, 1);
}

/**
 * @brief Perform Si4713 power-up
 *
 * This function sends power-up in analog mode command to Si4713
 * and checks the response status.
 *
 * @param[in] dev_handle I2C master device handle
 * @return
 *      - ESP_OK: Power-up succesful
 *      - ESP_FAIL: Power-up failed
 */
esp_err_t si4713_powerup_analog(i2c_master_dev_handle_t dev_handle)
{
    /*  0xC2 -> Set to FM Transmit. Enable interrupts.
        0x50 -> Set to Analog Line Input */
    uint8_t args[2] = {0xC2U, 0x50U};
    esp_err_t ret_val = ESP_FAIL;
    si4713_status_response_t status;

    ESP_ERROR_CHECK(i2c_send_cmd(dev_handle, POWER_UP, args, 2U));

    uint32_t start = xTaskGetTickCount();
    uint32_t timeout = pdMS_TO_TICKS(T_CTS_LONG_MS);
    do
    {
        ESP_ERROR_CHECK(si4713_read_status(dev_handle, &status));
    } while ((0U == status.cts) && ((xTaskGetTickCount() - start) < timeout));

    if (1U == status.cts)
    {
        ret_val = ESP_OK;
    }
    ESP_LOGI(SI4713_TAG, "SI4713 power-up status = %X", status);

    return ret_val;
}