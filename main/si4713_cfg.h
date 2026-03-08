/*
 * File: si4713_cfg.h
 * Author: Lukasz Gruchala
 * Created: 2026-03-06
 */

#ifndef __SI4713_CFG_H__
#define __SI4713_CFG_H__

/*==================================================================
    Includes
===================================================================*/

#include <inttypes.h>

/*==================================================================
    Object-like macros
===================================================================*/

#define SI4713_TAG "SI4713" /* log tag */

#define GPIO_OUTPUT_SI4713_RST CONFIG_GPIO_OUTPUT_SI4713_RST /* reset pin */
#define SI4173_SENSOR_ADDR 0x63                              /* I2C address of the SI4173 transmitter */

/* timing */
#define T_CTS_LONG_MS 110U /* CTS bit setting delay in ms (POWER_UP) */
#define T_CTS_SHORT_MS 1U  /* CTS bit setting delay in ms (other commands) (300 us rounded up to ms) */
#define T_STC_LONG_MS 100U /* STC bit setting delay in ms (TX_TUNE_FREQ, TX_TUNE_MEASURE) */
#define T_STC_SHORT_MS 20U /* STC bit setting delay in ms (TX_TUNE_POWER) */
#define T_COMP_MS 10U      /* SET_PROPERTY max. execution time in ms */
#define T_INT_US 1U        /* Interrupt duration in us after command is executed  */

/* Property default values */
#define TX_LINE_INPUT_LEVEL_DEFAULT_VAL 0x215EU /* Input Range = 419mV_PK, 74kΩ; Max peak input level = 350mV_PK = 0x15E */
#define GPO_IEN_DEFAULT_VAL 0x00C1U             /* Set STCIEN, ERRIEN, CTSIEN */
#define REFCLK_FREQ_DEFAULT_VAL 0x7EF4U         /* REFCLK = 32.5 kHz */
#define REFCLK_PRESCALE_DEFAULT_VAL 0x0190U     /* Divide by 400 (example RCLK = 13 MHz, REFCLK = 32.5 kHz) */
#define TX_LINE_INPUT_MUTE_DEFAULT_VAL 0x0000U  /* Sets left and right channel mute */
#define TX_PREEMPHASIS_DEFAULT_VAL 0x0001U      /* 50 us */
#define TX_PILOT_FREQUENCY_DEFAULT_VAL 0x4A38U  /* Sets the pilot or tone generator frequency. */
#define TX_AUDIO_DEVIATION_DEFAULT_VAL 0x1AA9U  /* 68.25 kHz = 6825d = 0x1AA9 */
#define TX_PILOT_DEVIATION_DEFAULT_VAL 0x02A3U  /* 6.75 kHz = 675d = 0x2A3 */

/*==================================================================
    Function-like macros
===================================================================*/

/*==================================================================
    Exported types
===================================================================*/

/*==================================================================
    Exported objects
===================================================================*/

/*==================================================================
    Function declarations
===================================================================*/

#endif /* __SI4713_CFG_H__ */