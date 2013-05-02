/*******************************************************************************
* Filename:     plmcfg.h
* Description:  File defining the PLM-1 configuration.
* Version:      1.6.0
* Note:         
*******************************************************************************/

#ifndef _PLMCFG_H_
#define _PLMCFG_H_


/*******************************************************************************
 * USER PARAMETERS
 * 
 * Parameters to be modified by the user.
 ******************************************************************************/
/* PLM-1 configuration informations. */
#define PLM_CFG_FCOMM       144
#define PLM_CFG_FOSC        8000000

/* PLM-1 configuration parameters (Basic). */
#define XDIV                49
#define CPB                 36
#define CPT0                7
#define CPT1                6
/*******************************************************************************
 * END OF USER PARAMETERS
 ******************************************************************************/

 
/* PLM-1 configuration parameters (Advanced) */
#define WDIV                0
#define DEL                 (CPT1 / 2)
#define FC_HIGH             (XDIV / 2)
#define FC_LOW              (uint8_t)(XDIV * 1.5)
#define N0                  (CPT0 + 2)
#define N1                  (CPT1 + 2)
#define HT0PB               (uint8_t)((CPB * 2 / N0) - 1)
/*#define FC                  0x15  *//* Computed by build_cfg_string() */
/*#define OFFSET              210   *//* Computed by build_cfg_string() */
/*#define DPHG                2     *//* Computed by build_cfg_string() */
/*#define FTH                 68    *//* Computed by build_cfg_string() */
/*#define FITH                136   *//* Computed by build_cfg_string() */
#if CPT0 > CPT1
#   define _PRIME_SIGN      -
#else
#   define _PRIME_SIGN      +
#endif


/* PLM-1 configuration parameters (MAC) */
#define PATTERN             0xAA
/*#define THRESHOLD           5     *//* Computed by build_cfg_string() */
#define MASK                0xFC
#define NOPRIO              0
#define NOPREAM             0
#define NOMAC               0
#define RND                 0xA5


/* PLM-1 configuration parameters (Hardware) */
#define INPOL               1
#define LTRINT              0
#define INTPOL              1
#define INTYPE              1
#define TXOUTPOL            0
#define TXOUTYPE            1
#define TXENPOL             1
#define TIMCFG              240


/* Parameters tests. */
#if XDIV > 0x7F
#   error PLM-1 CONFIGURATION: 'XDIV' value is too high!
#endif
#if XDIV < 1
#   error PLM-1 CONFIGURATION: 'XDIV' value is too low!
#endif
#if CPB < 8
#   error PLM-1 CONFIGURATION: 'CPB' value is too low!
#endif
#if CPB > 0x03FF
#   error PLM-1 CONFIGURATION: 'CPB' value is too high!
#endif
#if CPT0 > 0xFF
#   error PLM-1 CONFIGURATION: 'CPT0' value is too high!
#endif
#if CPT1 > 0xFF
#   error PLM-1 CONFIGURATION: 'CPT1' value is too high!
#endif
#if WDIV > 3
#   error PLM-1 CONFIGURATION: 'WDIV' value is too high!
#endif


#endif
