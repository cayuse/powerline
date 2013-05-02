/*******************************************************************************
* Filename:     plm1.h
* Description:  File defining the PLM-1 library.
* Version:      1.6.1
* Note:         
*******************************************************************************/

#ifndef _PLM1_H_
#define _PLM1_H_

#include "plmcfg.h"
#include <avr/io.h>


/*******************************************************************************
 * USER PARAMETERS
 * 
 * Parameters to be modified by the user.
 ******************************************************************************/
#define PLM_CS                         B,2                      /* CS pin. */
#define PLM_nRESET                     B,0                      /* Reset pin. */
#define PLM_CNFGD                      D,7                      /* Pin configured. */
#define PLM_CSPOL                      0                        // Value of PLM-1 CSPOL pin.
#define PLM_PCKPOL                     1                        // Value of PLM-1 PCKPOL pin.
#define PLM_INT_DISABLE()              CLR_BIT(EIMSK,INT0)      /* Disable/Mask PLM-1 interrupt. */
#define PLM_INT_ENABLE()               SET_BIT(EIMSK,INT0)      /* Enable/Unmask PLM-1 interrupt. */
#define PLM_SPI_INT_DISABLE()          CLR_BIT(SPCR,SPIE)       /* Disable/Mask SPI reception interrupt. */
#define PLM_SPI_INT_ENABLE()           SET_BIT(SPCR,SPIE)       /* Enable/Unmask SPI reception interrupt. */
#define PLM_SPI_TX_FUNC(_byte)         SPDR = (_byte)           /* Function to send a byte to SPI port. */
#define PLM_RX_BUFFER_SIZE             128                      // Size of the reception buffer in bytes.
#define PLM_RX_MAX_PACKET_NBR          10                       // Max nb of packets in the rx buffer.
#define PLM_TX_BUFFER_SIZE             128                      // Size of the transmission buffer in bytes.
#define PLM_TX_MAX_PACKET_NBR          10                       // Max nb of packets in the tx buffer.
#define PLM_TX_CHANNEL                 4                        // Default transmission channel.
/*******************************************************************************
 * END OF USER PARAMETERS
 ******************************************************************************/

#define PLM_VERSION                    PLM_VERSION_PLM1         // Powerline modem version
#define PLM_MAX_PACKET_SIZE            63                       // Maximum size allowed for a packet.
#define PLM_PACKET_HEADER_SIZE         2                        // 1 byte for Packet Priority + 1 byte for Channel Number.
#define PLM_PACKET_DATA_SIZE           (PLM_MAX_PACKET_SIZE-PLM_PACKET_HEADER_SIZE) // Size allowed for data into packet.
#define PLM_CONFIG_DATA_LENGTH         19                       // Configuration string length in bytes.

// Define Powerline modem version.
#define PLM_VERSION_PLM1               0
#define PLM_VERSION_PLM1A              1

/*------------------------------------------------------------------------------
  Global variables definition
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
  Global types definition
------------------------------------------------------------------------------*/

// Packet priority.
typedef enum _plm1_priority_ {
    PLM1_PRIO_HIGHEST = 0x80,                                   // Highest priority.
    PLM1_PRIO_HIGH = 0x90,                                      // High priority.
    PLM1_PRIO_NORMAL = 0xA0,                                    // Normal priority.
    PLM1_PRIO_DEFERRED = 0xB0                                   // Lower priority.
} plm1_priority;

// Status code.
typedef enum {
    PLM1_STS_OK = 0x00,                                         // No special event to report.
    PLM1_STS_ERROR_RECEIVED = 0x02,                             // An error occurs while receiving data.
    PLM1_STS_RX_OVERRUN = 0x03,                                 // Receiver overrun.
    PLM1_STS_COLLISION = 0x04,                                  // Collision detected.
    PLM1_STS_TX_UNDERRUN = 0x07,                                // Transmitter underrun.
    PLM1_STS_TX_OVERRUN = 0x09,                                 // Transmitter overrun.
    PLM1_STS_PACKET_MISSED = 0x0C                               // A packet has been lost due to library's buffer overflow.
} plm1_status;

/*------------------------------------------------------------------------------
  Global functions definition
------------------------------------------------------------------------------*/

// Initialize the PLM-1 library according to the USER parameters defined on top of this file.
void plm1_init(void);

// Configure the PLM-1 using a configuration string.
bool plm1_configure(uint8_t* cfg);

// Handler of PLM-1 interrupt.
// ** This function must be called from PLM-1 interrupt ISR **
void plm1_interrupt(void);

// Parser of data received from SPI port.
// ** This function must be called from SPI reception ISR **
void plm1_spi_isr(uint8_t rxNibble);

// Send a packet on the powerline (basic function).
bool plm1_send_data(uint8_t* data, uint8_t length);

// Send a packet on the powerline (complete function).
bool plm1_send_packet(uint8_t* data, uint8_t length, plm1_priority prio, uint8_t channel, bool rawMode);

// Get received packets.
uint8_t plm1_receive(uint8_t* dataPacket, plm1_priority* prio, uint8_t* channel);


// Get library status.
plm1_status plm1_get_status(void);

// Get transmitter state.
bool plm1_tx_idle(void);

// Get configuration string curently used.
bool plm1_get_configuration(uint8_t* cfg);

#endif /* _PLM1_H_ */
