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

/* Timing */
#define T_CTS_LONG_MS 110U /* CTS bit setting delay in ms (POWER_UP) */
#define T_CTS_SHORT_MS 1U  /* CTS bit setting delay in ms (other commands) (300 us rounded up to ms) */
#define T_STC_LONG_MS 100U /* STC bit setting delay in ms (TX_TUNE_FREQ, TX_TUNE_MEASURE) */
#define T_STC_SHORT_MS 20U /* STC bit setting delay in ms (TX_TUNE_POWER) */
#define T_COMP_MS 10U      /* SET_PROPERTY max. execution time in ms */
#define T_INT_MS 1U        /* Interrupt duration in us after command is executed. (1 us rounded up to ms)  */

/* Default values of command arguments and properties */
#define POWER_UP_DEFAULT_VAL 0xC250U                /* Set to FM Transmit. Enable interrupts; Set to Analog Line Input. */
#define TX_LINE_INPUT_LEVEL_DEFAULT_VAL 0x215EU     /* Input Range = 419mV_PK, 74kΩ; Max peak input level = 350mV_PK = 0x15E */
#define GPO_IEN_DEFAULT_VAL 0x00C1U                 /* Set STCIEN, ERRIEN, CTSIEN */
#define REFCLK_FREQ_DEFAULT_VAL 0x7EF4U             /* REFCLK = 32.5 kHz */
#define REFCLK_PRESCALE_DEFAULT_VAL 0x0190U         /* Divide by 400 (example RCLK = 13 MHz, REFCLK = 32.5 kHz) */
#define TX_LINE_INPUT_MUTE_DEFAULT_VAL 0x0000U      /* Sets left and right channel mute */
#define TX_PREEMPHASIS_DEFAULT_VAL 0x0001U          /* 50 us */
#define TX_PILOT_FREQUENCY_DEFAULT_VAL 0x4A38U      /* Sets the pilot or tone generator frequency. */
#define TX_AUDIO_DEVIATION_DEFAULT_VAL 0x1AA9U      /* 68.25 kHz = 6825d = 0x1AA9 */
#define TX_PILOT_DEVIATION_DEFAULT_VAL 0x02A3U      /* 6.75 kHz = 675d = 0x2A3 */
#define TX_TUNE_POWER_DEFAULT_VAL 0x7300U           /* Set transmit voltage to 115 dBμV = 115d = 0x73; Set antenna tuning capacitor to auto */
#define TX_TUNE_FREQ_DEFAULT_VAL 0x277EU            /* Set frequency to 101.1 MHz = 10110d = 0x277E */
#define TX_COMPONENT_ENABLE_DEFAULT_VAL 0x0003U     /* Enable (Stereo) LMR and Pilot */
#define TX_ACOMP_THRESHOLD_DEFAULT_VAL 0xFFD8U      /* Threshold = –40 dBFS = 0xFFD8 */
#define TX_ACOMP_GAIN_DEFAULT_VAL 0xFFD8U           /* Gain = 15 dB = 0xF */
#define TX_ACOMP_RELEASE_TIME_DEFAULT_VAL 0x000FU   /* Release time = 1000 ms = 4 */
#define TX_ACOMP_ATTACK_TIME_DEFAULT_VAL 0x0002U    /* Attack time = 1.5 ms = 2 */
#define TX_ACOMP_ENABLE_DEFAULT_VAL 0x0003U         /* Enable the limiter and compressor. */
#define TX_LIMITER_RELEASE_TIME_DEFAULT_VAL 0x000DU /* Sets the limiter release time to 13 (39.38 ms). */
#define TX_ASQ_LEVEL_LOW_DEFAULT_VAL 0x00CEU        /* –50 dB = 0x00CE */
#define TX_ASQ_DURATION_LOW_DEFAULT_VAL 0x2710U     /* 10000 ms = 0x2710 */
#define TX_ASQ_LEVEL_HIGH_DEFAULT_VAL 0x00ECU       /* –20 dB = 0x00EC */
#define TX_ASQ_DURATION_HIGH_DEFAULT_VAL 0x1388U    /* 5000 ms = 0x1388 */
#define TX_ASQ_INTERRUPT_SOURCE_DEFAULT_VAL 0x0007U /* Enable overmodulation, high and low thresholds. */

/* Response status bit positions */
#define STATUS_CTS_BIT_POS 7U
#define STATUS_ERR_BIT_POS 6U
#define STATUS_RDSINT_BIT_POS 2U
#define STATUS_ASQINT_BIT_POS 1U
#define STATUS_STCINT_BIT_POS 0U

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