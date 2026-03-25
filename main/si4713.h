/*
 * File: si4713.h
 * Author: Lukasz Gruchala
 * Created: 2026-03-06
 */

#ifndef __SI4713_H__
#define __SI4713_H__

/*==================================================================
    Includes
===================================================================*/

#include "i2c_master_app.h"

/*==================================================================
    Object-like macros
===================================================================*/

/*==================================================================
    Function-like macros
===================================================================*/

/*==================================================================
    Exported types
===================================================================*/

/* FM transmitter property summary */
typedef enum
{
    GPO_IEN = 0x0001,              /* Enables interrupt sources. 0x0000 */
    DIGITAL_INPUT_FORMAT = 0x0101, /* Configures the digital input format. 0x0000 */
    DIGITAL_INPUT_SAMPLE_RATE =
        0x0103, /* Configures the digital input sample rate in 10 Hz steps. Default is 0. 0x0000 */
    REFCLK_FREQ = 0x0201, /* Sets frequency of the reference clock in Hz. The range is31130 to 34406
                             Hz, or 0 to disable the AFC. Default is 32768 Hz. 0x8000 */
    REFCLK_PRESCALE = 0x0202,     /* Sets the prescaler value for the reference clock. 0x0001 */
    TX_COMPONENT_ENABLE = 0x2100, /* Enable transmit multiplex signal components. Default has pilot
                                     and L-R enabled. 0x0003 */
    TX_AUDIO_DEVIATION = 0x2101,  /* Configures audio frequency deviation level. Units are in 10 Hz
                                     increments. Default is 6285 (68.25 kHz). 0x1AA9 */
    TX_PILOT_DEVIATION = 0x2102, /* Configures pilot tone frequency deviation level. Units are in 10
                                    Hz increments. Default is 675 (6.75 kHz) 0x02A3 */
    TX_RDS_DEVIATION = 0x2103,   /* Si4713 Only. Configures the RDS/RBDS frequency deviation level.
                                    Units are in 10 Hz increments. Default is 2 kHz. 0x00C8 */
    TX_LINE_INPUT_LEVEL =
        0x2104, /* Configures maximum analog line input level to the LIN/RIN pins to reach the
                   maximum deviation level programmed into the audio deviation property TX Audio
                   Deviation. Default is 636 mV PK . 0x327C */
    TX_LINE_INPUT_MUTE = 0x2105, /* Sets line input mute. L and R inputs may be independently muted.
                                    Default is not muted. 0x0000 */
    TX_PREEMPHASIS =
        0x2106, /* Configures preemphasis time constant. Default is 0 (75 μS). 0x0000 */
    TX_PILOT_FREQUENCY =
        0x2107, /* Configures the frequency of the stereo pilot. Default is 19000 Hz. 0x4A38 */
    TX_ACOMP_ENABLE =
        0x2200, /* Enables audio dynamic range control. Default is 0 (disabled). 0x0002 */
    TX_ACOMP_THRESHOLD = 0x2201,    /* Sets the threshold level for audio dynamic range control.
                                       Default is –40 dB. 0xFFD8 */
    TX_ACOMP_ATTACK_TIME = 0x2202,  /* Sets the attack time for audio dynamic range control. Default
                                       is 0 (0.5 ms). 0x0000 */
    TX_ACOMP_RELEASE_TIME = 0x2203, /* Sets the release time for audio dynamic range control.
                                       Default is 4 (1000 ms). 0x0004 */
    TX_ACOMP_GAIN =
        0x2204, /* Sets the gain for audio dynamic range control. Default is 15 dB. 0x000F */
    TX_LIMITER_RELEASE_TIME =
        0x2205, /* Sets the limiter release time. Default is 102 (5.01 ms) 0x0066 */
    TX_ASQ_INTERRUPT_SOURCE = 0x2300, /* Configures measurements related to signal quality metrics.
                                         Default is none selected. 0x0000 */
    TX_ASQ_LEVEL_LOW =
        0x2301, /* Configures low audio input level detection threshold. This threshold can be used
                   to detect silence on the incoming audio. 0x0000 */
    TX_ASQ_DURATION_LOW =
        0x2302, /* Configures the duration which the input audio level must be below the low
                   threshold in order to detect a low audio condition. 0x0000 */
    TX_ASQ_LEVEL_HIGH =
        0x2303, /* Configures high audio input level detection threshold. This threshold can be used
                   to detect activity on the incoming audio. 0x0000 */
    TX_ASQ_DURATION_HIGH =
        0x2304, /* Configures the duration which the input audio level must be above the high
                   threshold in order to detect a high audio condition. 0x0000 */
    TX_RDS_INTERRUPT_SOURCE =
        0x2C00, /* Si4713 Only. Configure RDS interrupt sources. Default is none selected. 0x0000 */
    TX_RDS_PI = 0x2C01, /* Si4713 Only. Sets transmit RDS program identifier. 0x40A7 */
    TX_RDS_PS_MIX =
        0x2C02, /* Si4713 Only. Configures mix of RDS PS Group with RDS Group Buffer. 0x0003 */
    TX_RDS_PS_MISC =
        0x2C03, /* Si4713 Only. Miscellaneous bits to transmit along with RDS_PS Groups. 0x1008 */
    TX_RDS_PS_REPEAT_COUNT = 0x2C04,  /* Si4713 Only. Number of times to repeat transmission of a PS
                                         message before transmitting the next PS message. 0x0003 */
    TX_RDS_PS_MESSAGE_COUNT = 0x2C05, /* Si4713 Only. Number of PS messages in use. 0x0001 */
    TX_RDS_PS_AF =
        0x2C06, /* Si4713 Only. RDS Program Service Alternate Frequency. This provides the ability
                   to inform the receiver of a single alternate frequency using AF Method A coding
                   and is transmitted along with the RDS_PS Groups. 0xE0E0 */
    TX_RDS_FIFO_SIZESi4713 =
        0x2C07, /* Only. Number of blocks reserved for the FIFO. Note that the value written must be
                   one larger than the desired FIFO size. 0x0000 */
} si4713_property_t;

/*==================================================================
    Exported objects
===================================================================*/

/*==================================================================
    Function declarations
===================================================================*/

esp_err_t si4713_powerup_analog(i2c_master_dev_handle_t dev_handle, uint16_t val);
esp_err_t si4713_set_property(i2c_master_dev_handle_t dev_handle, si4713_property_t property,
                              uint16_t val);
esp_err_t si4713_get_rev(i2c_master_dev_handle_t dev_handle);
esp_err_t si4713_tx_tune_power(i2c_master_dev_handle_t dev_handle, uint16_t val);
esp_err_t si4713_tx_tune_freq(i2c_master_dev_handle_t dev_handle, uint16_t val);
esp_err_t si4713_get_int_status(i2c_master_dev_handle_t dev_handle, uint8_t status_expected,
                                uint32_t timeout_ms);
esp_err_t si4713_tx_tune_status(i2c_master_dev_handle_t dev_handle);
esp_err_t si4713_tx_asq_status(i2c_master_dev_handle_t dev_handle);

#endif /* __SI4713_H__ */