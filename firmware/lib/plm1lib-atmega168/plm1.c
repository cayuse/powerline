/*******************************************************************************
* Filename:     plm1.c
* Description:  File implementing the PLM-1 library.
* Version:      1.6.1
* Note:         
*******************************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "plm1.h"
//#include "port.h"

/*------------------------------------------------------------------------------
  Global variables declaration
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
  Local constants declaration
------------------------------------------------------------------------------*/

// Control codes used by PLM-1.
#define PLM_CC_EOF                     0x10                         // End Of Field. [In/Out]
#define PLM_CC_EOP                     0x11                         // End of Packet. [In/Out]
#define PLM_CC_RX_ERROR                0x12                         // Receiver error. [In]
#define PLM_CC_DISABLE_RECV            0x12                         // Disable receiver. [Out]
#define PLM_CC_RX_OVERRUN              0x13                         // Receiver over-run. [In]
#define PLM_CC_COLLISION               0x14                         // Collision. [In]
#define PLM_CC_RESET                   0x16                         // Soft reset. [Out]
#define PLM_CC_TX_UNDERRUN             0x17                         // Transmitter under-run. [In]
#define PLM_CC_TXRE                    0x18                         // Transmit Register Empty. [In]
#define PLM_CC_TX_OVERRUN              0x19                         // Transmitter over-run. [In]
#define PLM_CC_NOP                     0x1F                         // No Operation. [In/Out]

/*------------------------------------------------------------------------------
  Local types definition
------------------------------------------------------------------------------*/

// PLM-1 library' state.
typedef enum _plm1_state_ {
    PLM1_STATE_NOT_CONFIGURED = 0,
    PLM1_STATE_CONFIGURING,
    PLM1_STATE_IDLE,
    PLM1_STATE_NEGOTIATING,
    PLM1_STATE_TRANSMITTING,
    PLM1_STATE_RECEIVING
} plm1_state;

// Structure holding packet descriptor.
typedef struct _packet_desc_t_ {
    bool used;                                                      // true if this descriptor is in use.
    uint16_t start;                                                 // Offset of the packet inside buffer.
    uint8_t size;                                                   // Size of the packet.
} packet_desc_t;

// Structure holding variables for reception.
typedef struct _plm1_rx_t_ {
    bool invalid_packet;                                            // Receiving an invalid packet.
    uint8_t packet_index;                                           // Index of the oldest received packet.
    bool msb;                                                       // true if next nibble to be received is the MSB.
    packet_desc_t packet_desc[PLM_RX_MAX_PACKET_NBR];               // Reception packet descriptors.
    uint8_t desc_index;                                             // Index of the next available packet descriptor.
    uint8_t buffer[PLM_RX_BUFFER_SIZE];                             // Buffer for reception.
    uint16_t buffer_empty_size;                                     // Number of bytes available in the reception buffer.
    uint16_t buffer_empty_index;                                    // Index of the next available byte in reception buffer.
} plm1_rx_t;

// Structure holding variables for transmission.
typedef struct _plm1_tx_t_ {
    uint8_t config_nibble_index;                                    // Index of the next configuration nibble to send.
    uint16_t byte_sent;                                             // Number of bytes already sent in the current packet.
    uint8_t packet_index;                                           // Index of the packet to send.
    bool msb;                                                       // true if next nibble to be transmistted is the MSB.
    packet_desc_t packet_desc[PLM_TX_MAX_PACKET_NBR];               // Transmission packet descriptors.
    uint8_t desc_index;                                             // Index of the next available packet descriptor.
    uint8_t buffer[PLM_TX_BUFFER_SIZE];                             // Buffer for transmission.
    uint16_t buffer_empty_size;                                     // Number of bytes available in the transmission buffer.
    uint16_t buffer_empty_index;                                    // Index of the next available byte in transmission buffer.
} plm1_tx_t;

// Structure holding status of PLM-1.
typedef struct _plm1_sts_t_ {
    volatile plm1_state state;                                      // PLM-1 library' state.
    uint8_t cfg[PLM_CONFIG_DATA_LENGTH];                            // Current configuration string.
    bool spi_in_use;                                                // SPI transaction status.
    plm1_status status[2];                                          // Library status buffer.
} plm1_sts_t;

/*------------------------------------------------------------------------------
  Local macros declaration
------------------------------------------------------------------------------*/

// Enable/Disable PLM-1 Chip-select pin.
#ifndef PLM_CSPOL
#error "PLM_CSPOL is not defined"
#elif PLM_CSPOL == 1                                                // PLM-1 Chip-select pin active high.
#   define CS_ENABLE(_enable)   ((_enable) ?  SET_OUTPUT(PLM_CS) : CLR_OUTPUT(PLM_CS))
#elif PLM_CSPOL == 0                                                // PLM-1 Chip-select pin active low.
#   define CS_ENABLE(_enable)   ((_enable) ?  CLR_OUTPUT(PLM_CS) : SET_OUTPUT(PLM_CS))
#endif

// Mask PLM-1 and SPI interrupt.
#define MASK_INTERRUPTS()                                                      \
    PLM_INT_DISABLE();                                                         \
    PLM_SPI_INT_DISABLE()

// Unmask PLM-1 and SPI interrupt.
#define UNMASK_INTERRUPTS()                                                    \
    PLM_INT_ENABLE();                                                          \
    PLM_SPI_INT_ENABLE()

// Increment index of a ring buffer.
#define INCR(_index, _limit)                                                   \
    ((_index) >= (_limit) - 1) ? ((_index) = 0) : (++(_index))

// Reset reception buffer.
#define RX_RESET()                                                             \
    plm_rx.packet_index = 0;                                                   \
    plm_rx.msb = true;                                                         \
    for(plm_i = 0; plm_i < PLM_RX_MAX_PACKET_NBR; plm_i++) {                   \
        plm_rx.packet_desc[plm_i].size = 0;                                    \
        plm_rx.packet_desc[plm_i].used = false;                                \
    }                                                                          \
    plm_rx.desc_index = 0;                                                     \
    plm_rx.buffer_empty_size = PLM_RX_BUFFER_SIZE;                             \
    plm_rx.buffer_empty_index = 0

// Clear current packet descriptor.
#define RX_CLEAR_PKT()                                                         \
    if(plm_rx.packet_desc[plm_rx.packet_index].used == false) {                \
        plm_rx.packet_desc[plm_rx.packet_index].size = 0;                      \
        plm_rx.invalid_packet = false;                                         \
        plm_rx.msb = true;                                                     \
    } 

// Reset transmission buffer.
#define TX_RESET()                                                             \
    plm_tx.packet_index = 0;                                                   \
    plm_tx.msb = true;                                                         \
    plm_tx.byte_sent = 0;                                                      \
    for(plm_i = 0; plm_i < PLM_TX_MAX_PACKET_NBR; plm_i++)                     \
        plm_tx.packet_desc[plm_i].used = false;                                \
    plm_tx.desc_index = 0;                                                     \
    plm_tx.buffer_empty_size = PLM_TX_BUFFER_SIZE;                             \
    plm_tx.buffer_empty_index = 0

// Remove current packet from transmission buffer.
#define TX_REMOVE_PKT()                                                        \
    plm_tx.buffer_empty_size += plm_tx.packet_desc[plm_tx.packet_index].size;  \
    plm_tx.packet_desc[plm_tx.packet_index].used = false;                      \
    INCR(plm_tx.packet_index, PLM_TX_MAX_PACKET_NBR);                          \
    plm_tx.byte_sent = 0;                                                      \
    plm_tx.msb = true

// Convert byte count into nibble count.
#define NIB(_byteCount)                                                        \
    (2 * (_byteCount))

// Get nibble value at specific index into an array.
#define GET_NIBBLE(_array, _index)                                             \
    ((_index) & 0x01) ? ((_array)[(_index) >> 1] & 0x0F) : ((_array)[(_index) >> 1] >> 4)

// Start a byte transmission to SPI interface.
#define SPI_TX_START(_data)                                                    \
    CS_ENABLE(true);                                                           \
    plm_sts.spi_in_use = true;                                                         \
    PLM_SPI_TX_FUNC(_data)

// Continue a SPI transaction by sending a new byte.
#define SPI_TX_NEXT(_data)                                                     \
    PLM_SPI_TX_FUNC(_data)

// Stop SPI transaction.
#define SPI_TX_STOP()                                                          \
    CS_ENABLE(false);                                                          \
    plm_sts.spi_in_use = false

/*------------------------------------------------------------------------------
  Local variables declaration
------------------------------------------------------------------------------*/

static plm1_rx_t plm_rx;                                            // Reception struct.
static plm1_tx_t plm_tx;                                            // Transmission struct.
static plm1_sts_t plm_sts;                                          // PLM-1 status struct.
static uint8_t plm_i;                                               // Used by **_RESET() macros.

/*------------------------------------------------------------------------------
  Local functions declaration
------------------------------------------------------------------------------*/

static void state_configuring(uint8_t rxNibble);
static void state_idle(uint8_t rxNibble);
static void state_negotiating(uint8_t rxNibble);
static void state_transmitting(uint8_t rxNibble);
static void state_receiving(uint8_t rxNibble);

static void record_status(plm1_status sts);
static void store_rx_nibble(uint8_t nibble);
static uint8_t get_tx_nibble(void);
static bool update_tx_nibble(void);
static void eop_received(void);

static void build_cfg_string(void);
static uint8_t crc4(uint8_t nibble, uint8_t oldCrc);
static uint8_t log2_round(float n, bool ceil);

/*------------------------------------------------------------------------------
  Global functions
------------------------------------------------------------------------------*/

/*******************************************************************************
* Name:         plm1_init()        
* Description:  Initialize the PLM-1 library according to the USER parameters
*               defined on top of the plm1.h file.
* Parameters:   None.
* Return:       None.
* Note:         
*******************************************************************************/
void plm1_init(void)
{   
    // Hold reset line of PLM-1.
    CLR_OUTPUT(PLM_nRESET);
    
    // Initialize library variables.
    memset(&plm_rx, 0, sizeof(plm_rx));
    memset(&plm_tx, 0, sizeof(plm_tx));
    memset(&plm_sts, 0, sizeof(plm_sts));
    
    // Reset tx/rx variables.
    RX_RESET();
    TX_RESET();
    
    // Remove SPI chip select.
    CS_ENABLE(false);
}

/*******************************************************************************
* Name:         plm1_configure()        
* Description:  Send configuration to PLM-1.
* Parameters:   cfg: Configuration string. If NULL or invalid, the default
*                    configuration is used.
* Return:       true if configuration has been done successfully, false otherwise.
* Note:         This function block until configuration has been done.
*******************************************************************************/
bool plm1_configure(uint8_t* cfg)
{
    uint8_t byteIndex;
    bool useDefaultCfg = true;
    
    MASK_INTERRUPTS();
    
    // Start PLM-1 if not already done.
    SET_OUTPUT(PLM_nRESET);
    
    // Argument is valid?
    if(cfg != NULL)
    {
        // Configuration is valid?
        for(byteIndex = 0;  byteIndex < PLM_CONFIG_DATA_LENGTH; byteIndex++)
        {
            if(cfg[byteIndex] != 0)
            {
                // Configuration seems to be valid!
                useDefaultCfg = false;
                memcpy(plm_sts.cfg, cfg, PLM_CONFIG_DATA_LENGTH);
                break;
            }
        }
    }
    
    // Build and use default configuration string?
    if(useDefaultCfg)
    {
        build_cfg_string();
    }
    
    // Reset configuration variables.
    plm_tx.config_nibble_index = 0;
    plm_sts.state = PLM1_STATE_CONFIGURING;
    
    // Send Software Reset command and configuration will be done by plm1_spi_isr().
    SPI_TX_START(PLM_CC_RESET);
    UNMASK_INTERRUPTS();
    
    // Wait until configuration has finished.
    while(plm_sts.state == PLM1_STATE_CONFIGURING);
    
    return(plm_sts.state != PLM1_STATE_NOT_CONFIGURED);
}

/*******************************************************************************
* Name:         plm1_interrupt()        
* Description:  Handler of PLM-1 packet reception and transmission.
*               ** This function must be called from PLM-1 external interrupt **
* Parameters:   None.
* Return:       None.
* Note:         
*******************************************************************************/
void plm1_interrupt(void)
{
    MASK_INTERRUPTS();
    
    // Send a NOP command only if PLM-1 is configured and a SPI transaction is
    // not already started.
    if((plm_sts.state >= PLM1_STATE_IDLE) && (plm_sts.spi_in_use == false))
    {
        SPI_TX_START(PLM_CC_NOP);
    }
    
    UNMASK_INTERRUPTS();
}

/*******************************************************************************
* Name:         plm1_spi_isr()        
* Description:  Parser of data received from SPI port.
*               ** This function must be called from SPI reception ISR **
* Parameters:   rxNibble: Received 5 bits nibble.
* Return:       None.
* Note:         
*******************************************************************************/
void plm1_spi_isr(uint8_t rxNibble)
{    
    MASK_INTERRUPTS();

    switch(plm_sts.state)
    {
      case PLM1_STATE_CONFIGURING:
        // During configuration...
        state_configuring(rxNibble);
        break;
        
      case PLM1_STATE_IDLE:
        // Not transmitting nor receiving...      
        state_idle(rxNibble);
        break;
        
      case PLM1_STATE_RECEIVING:
        // Receiving data from powerline...
        state_receiving(rxNibble);
        break;
        
      case PLM1_STATE_NEGOTIATING:
        // Try to start a transmission...
        state_negotiating(rxNibble);
        break;
        
      case PLM1_STATE_TRANSMITTING:
        // During a transmission...
        state_transmitting(rxNibble);
        break;
      
      case PLM1_STATE_NOT_CONFIGURED:
      default:
        // Plm-1 not yet configured...
        break;
    }
    
    UNMASK_INTERRUPTS();
}

/*******************************************************************************
* Name:         plm1_send_data()        
* Description:  Send a packet on the powerline (basic function).
* Parameters:   data: Data to send.
*               length: Length of the data array.
* Return:       true if the packet has been queued successfully, false otherwise.
* Note:         
*******************************************************************************/
bool plm1_send_data(uint8_t* data, uint8_t length)
{
   return plm1_send_packet(data, length, PLM1_PRIO_NORMAL, PLM_TX_CHANNEL, false);
}

/*******************************************************************************
* Name:         plm1_send_packet()        
* Description:  Send a packet on the powerline (complete function).
* Parameters:   data: Data to send.
*               length: Length of the data array.
*               prio: Packet priority.
*               channel: Channel number used to send packet.
*               rawMode: When set to true, "prio" and "channel" arguments are not
*                        considered and the "data" array contains the entire 
*                        packet (including channel number and packet priority).
* Return:       true if the packet has been queued successfully, false otherwise.
* Note:         
*******************************************************************************/
bool plm1_send_packet(uint8_t* data, uint8_t length, plm1_priority prio, uint8_t channel, bool rawMode)
{
    bool txSuccess = false;
    uint32_t txIndexEnd;
    uint16_t txLength = length;
    
    // Compute packet size.
    if(rawMode == false)
    {
        // Add size of the PLM1 header.
        txLength += PLM_PACKET_HEADER_SIZE;
    }
    
    // Buffer space available?
    if((txLength <= plm_tx.buffer_empty_size) && (txLength <= PLM_MAX_PACKET_SIZE) && (length > 0))
    {
        // Packet descriptor available?
        if(plm_tx.packet_desc[plm_tx.desc_index].used == false)
        {
            // Fill packet descriptor.
            plm_tx.packet_desc[plm_tx.desc_index].start = plm_tx.buffer_empty_index;
            plm_tx.packet_desc[plm_tx.desc_index].size = (uint8_t)txLength;

            // Copy PLM1 header if necessary.
            if(rawMode == false)
            {
                plm_tx.buffer[plm_tx.buffer_empty_index] = (uint8_t)prio;
                INCR(plm_tx.buffer_empty_index, PLM_TX_BUFFER_SIZE);
                plm_tx.buffer[plm_tx.buffer_empty_index] = channel;
                INCR(plm_tx.buffer_empty_index, PLM_TX_BUFFER_SIZE);
            }
            
            // Copy data into buffer.
            txIndexEnd = (uint32_t)plm_tx.buffer_empty_index + (uint32_t)length - 1;
            if(txIndexEnd < PLM_TX_BUFFER_SIZE)
            {
                // Linear array available into buffer.
                memcpy(&plm_tx.buffer[plm_tx.buffer_empty_index], data, length);
            }
            else
            {
                uint16_t tempLength = PLM_TX_BUFFER_SIZE - plm_tx.buffer_empty_index;
                
                // Copy a part of the packet at the buffer's end.
                memcpy(&plm_tx.buffer[plm_tx.buffer_empty_index], data, tempLength);
                
                // Copy remaining data at the buffer's begining.
                memcpy(plm_tx.buffer, data + tempLength, length - tempLength);
            }
                        
            // Update transmission struct.
            plm_tx.packet_desc[plm_tx.desc_index].used = true;
            INCR(plm_tx.desc_index, PLM_TX_MAX_PACKET_NBR);
            plm_tx.buffer_empty_size -= txLength;
            plm_tx.buffer_empty_index = (plm_tx.buffer_empty_index + length) % PLM_TX_BUFFER_SIZE;
            
            // Packet successfully queued!
            txSuccess = true;
            
            // Send packet if PLM-1 is idle and a SPI transaction is not already started.
            if((plm_sts.state == PLM1_STATE_IDLE) && (plm_sts.spi_in_use == false))
            {
                SPI_TX_START(get_tx_nibble());
                plm_sts.state = PLM1_STATE_NEGOTIATING;
            }
        }
    }
    
    return (txSuccess);  
}

/*******************************************************************************
* Name:         plm1_receive()        
* Description:  Get received packets.
* Parameters:   dataPacket: Pointer to an array to which complete packets are
*                           copied (packet does not include the PLM-1 header).
*               priority: Priority of the received packet. NULL value is
*                         supported.
*               channel: Software channel on wich packet has been received.
*                        NULL value is supported.
* Return:       Length of the packet loaded in "dataPacket".
* Note:         
*******************************************************************************/
uint8_t plm1_receive(uint8_t* dataPacket, plm1_priority* prio, uint8_t* channel)
{
    uint8_t length;
    uint32_t rxIndexEnd;
    packet_desc_t* pkt = &plm_rx.packet_desc[plm_rx.packet_index]; 
    
    // Complete packet received?
    if(pkt->used)
    {
        // Get packet priority.
        if(prio != NULL)
        {
            *prio = (plm1_priority)plm_rx.buffer[pkt->start];
        }
        INCR(pkt->start, PLM_RX_BUFFER_SIZE);
        
        // Get channel number.
        if(channel != NULL)
        {
            *channel = plm_rx.buffer[pkt->start];
        }
        INCR(pkt->start, PLM_RX_BUFFER_SIZE);
        
        // Copy received packet without PLM-1 header.
        length = pkt->size - PLM_PACKET_HEADER_SIZE;
        rxIndexEnd = pkt->start + length - 1;
        if(rxIndexEnd < PLM_RX_BUFFER_SIZE)
        {
            // Linear buffer.
            memcpy(dataPacket, &plm_rx.buffer[pkt->start], length);
        }
        else
        {
            uint16_t tempLength = PLM_RX_BUFFER_SIZE - pkt->start;
                
            // Copy the part of the packet present at buffer's end.
            memcpy(dataPacket, &plm_rx.buffer[pkt->start], tempLength);
            
            // Copy remaining data present at buffer's begining.
            memcpy(dataPacket + tempLength, plm_rx.buffer, length - tempLength);        
        }

        // Free reception buffer and packet descriptor.
        plm_rx.buffer_empty_size += pkt->size;
        pkt->size = 0;
        pkt->used = false;
        INCR(plm_rx.packet_index, PLM_RX_MAX_PACKET_NBR);
    }
    else
    {
        // No packet available.
        length = 0;
    }
    
    return (length);
}

/*******************************************************************************
* Name:         plm1_get_status()        
* Description:  Returns library status.
* Parameters:   None.
* Return:       Library status.
* Note:         
*******************************************************************************/
plm1_status plm1_get_status(void)
{
    plm1_status sts;
    
    MASK_INTERRUPTS();
    
    // "status[0]" is the oldest and "status[1]" the newest.
    sts = plm_sts.status[0];
    plm_sts.status[0] = plm_sts.status[1];
    plm_sts.status[1] = PLM1_STS_OK;
    
    UNMASK_INTERRUPTS();
    
    return (sts);
}

/*******************************************************************************
* Name:         plm1_tx_idle()        
* Description:  Get transmitter state.
* Parameters:   None.
* Return:       true if transmission buffer is empty, false otherwise.
* Note:         
*******************************************************************************/
bool plm1_tx_idle(void)
{
    return (plm_tx.buffer_empty_size == PLM_TX_BUFFER_SIZE);
}

/*******************************************************************************
* Name:         plm1_get_configuration()        
* Description:  Get configuration string curently used.
* Parameters:   cfg: Array used to return current configuration string.
* Return:       true if a configuration string is available, false if PLM-1 is
*               not currently configured.
* Note:         
*******************************************************************************/
bool plm1_get_configuration(uint8_t* cfg)
{
    bool configured;
    
    MASK_INTERRUPTS();
    
    if(plm_sts.state <= PLM1_STATE_CONFIGURING)
    {
        // Not yet configured.
        memset(cfg, 0, PLM_CONFIG_DATA_LENGTH);
        configured = false;
    }
    else
    {
        // Get configuration.
        memcpy(cfg, plm_sts.cfg, PLM_CONFIG_DATA_LENGTH);
        configured = true;
    }
    
    UNMASK_INTERRUPTS();
    
    return (configured);
}

/*------------------------------------------------------------------------------
  Local functions
------------------------------------------------------------------------------*/

/*******************************************************************************
* Name:         state_configuring()        
* Description:  Execute state "Configuring".
* Parameters:   rxNibble: Nibble received from PLM-1.
* Return:       None.
* Note:         
*******************************************************************************/
static void state_configuring(uint8_t rxNibble)
{
    // During configuration, send next configuration nibble.
    if(plm_tx.config_nibble_index < NIB(PLM_CONFIG_DATA_LENGTH))
    {
        SPI_TX_NEXT(GET_NIBBLE(plm_sts.cfg, plm_tx.config_nibble_index));
        ++plm_tx.config_nibble_index;
    }
    else
    {
        // Configuration finished.
        SPI_TX_STOP();

        // Test pin CNFGD if set.
#ifdef PLM_CNFGD
        if(GET_INPUT(PLM_CNFGD) == 0)
        {
            // Configuration fails...
            plm_sts.state = PLM1_STATE_NOT_CONFIGURED;
        }
        else
#endif
        {
            // Configuration seems to be done successfully!
            plm_sts.state = PLM1_STATE_IDLE;
                
            // Reset tx/rx variables.
            RX_RESET();
            TX_RESET();
        }
    }
}

/*******************************************************************************
* Name:         state_idle()        
* Description:  Execute state "Idle".
* Parameters:   rxNibble: Nibble received from PLM-1.
* Return:       None.
* Note:         
*******************************************************************************/
static void state_idle(uint8_t rxNibble)
{
    // Reception started?
    if((rxNibble & 0x10) == 0)
    {
        // Data nibble received.
        store_rx_nibble(rxNibble);
        SPI_TX_STOP();
        plm_sts.state = PLM1_STATE_RECEIVING;
    }
    else
    {
        // Special character received.
        switch(rxNibble)
        {              
          case PLM_CC_RX_OVERRUN:
            // PLM-1 was receiving and an error happends.
            record_status(PLM1_STS_RX_OVERRUN);
            break;
            
          case PLM_CC_RX_ERROR:
            // A start of packet has been detected without following data.
            // Ignore this error since it's caused by noise on powerline.
          default:
            // Other special characters are ignored.
            break;
        }
        
        // Start transmitting if a packet is available.
        if(plm_tx.packet_desc[plm_tx.packet_index].used)
        {
            // Write first nibble.
            SPI_TX_NEXT(get_tx_nibble());
            plm_sts.state = PLM1_STATE_NEGOTIATING;
        }
        else
        {
            // Do not transmit packet.
            SPI_TX_STOP();
        }
    }
}

/*******************************************************************************
* Name:         state_negotiating()        
* Description:  Execute state "Negotiating".
* Parameters:   rxNibble: Nibble received from PLM-1.
* Return:       None.
* Note:         
*******************************************************************************/
static void state_negotiating(uint8_t rxNibble)
{
    // Transmission started?
    if((rxNibble & 0x10) == 0)
    {
        // Data nibble received; PLM-1 does not acquire the line.
        store_rx_nibble(rxNibble);
        SPI_TX_STOP();
        plm_sts.state = PLM1_STATE_RECEIVING;
    }
    else
    {
        // Special character received.
        switch(rxNibble)
        {  
          case PLM_CC_COLLISION:
            // Collision happends... retry.
            SPI_TX_NEXT(get_tx_nibble());
            record_status(PLM1_STS_COLLISION);
            break;
            
          case PLM_CC_TXRE:
            // Transmission started! Send next nibble.
            update_tx_nibble();
            SPI_TX_NEXT(get_tx_nibble());
            plm_sts.state = PLM1_STATE_TRANSMITTING;
            break;
            
          case PLM_CC_RX_ERROR:
            // PLM-1 was already receiving and an error happends... retry.
            SPI_TX_NEXT(get_tx_nibble());
            record_status(PLM1_STS_ERROR_RECEIVED);
            break;
            
          case PLM_CC_RX_OVERRUN:
            // PLM-1 was already receiving and an error happends... retry.
            SPI_TX_NEXT(get_tx_nibble());
            record_status(PLM1_STS_RX_OVERRUN);
            break;
            
          default:
            // Other special characters are ignored.
            SPI_TX_STOP();
            break;
        }
    }
}

/*******************************************************************************
* Name:         state_transmitting()        
* Description:  Execute state "Transmitting".
* Parameters:   rxNibble: Nibble received from PLM-1.
* Return:       None.
* Note:         
*******************************************************************************/
static void state_transmitting(uint8_t rxNibble)
{
    // Only special character should be received in this state.
    if((rxNibble & 0x10) == 0)
    {
        // Data nibble received, should not happend!!
        SPI_TX_STOP();
    }
    else
    {
        switch(rxNibble)
        {
          case PLM_CC_TXRE:
            // Nibble transmitted!
            if(update_tx_nibble())
            {
                // Packet successfully transmitted!                
                // Send next packet if one is available.
                if(plm_tx.packet_desc[plm_tx.packet_index].used)
                {
                    // Write first nibble.
                    SPI_TX_NEXT(get_tx_nibble());
                    plm_sts.state = PLM1_STATE_NEGOTIATING;
                }
                else
                {
                    // Do not transmit packet.
                    SPI_TX_STOP();
                    plm_sts.state = PLM1_STATE_IDLE;
                }
            }
            else
            {
                // Send next nibble!
                SPI_TX_NEXT(get_tx_nibble());
            }
            break;
            
          case PLM_CC_TX_UNDERRUN:
            // Retransmit current packet to PLM-1
            plm_tx.byte_sent = 0;
            plm_tx.msb = true;
            SPI_TX_NEXT(get_tx_nibble());
            record_status(PLM1_STS_TX_UNDERRUN);
            break;
                
          case PLM_CC_TX_OVERRUN:
            // Retransmit current packet to PLM-1
            plm_tx.byte_sent = 0;
            plm_tx.msb = true;
            SPI_TX_NEXT(get_tx_nibble());
            record_status(PLM1_STS_TX_OVERRUN);
            break;
            
          default:
            // Other special characters are ignored.
            SPI_TX_STOP();
            break;
        }
    }
}

/*******************************************************************************
* Name:         state_receiving()        
* Description:  Execute state "Receiving".
* Parameters:   rxNibble: Nibble received from PLM-1.
* Return:       None.
* Note:         
*******************************************************************************/
static void state_receiving(uint8_t rxNibble)
{
    // Receiving packet from powerline.
    if((rxNibble & 0x10) == 0)
    {
        // Data nibble received.
        store_rx_nibble(rxNibble);
        SPI_TX_STOP();
    }
    else
    {
        // Special character received.
        switch(rxNibble)
        {
          case PLM_CC_EOP:
            // End of packet received!
            eop_received();
            plm_sts.state = PLM1_STATE_IDLE;
            break;
            
          case PLM_CC_RX_ERROR:
            // Invalid packet received by PLM-1.
            RX_CLEAR_PKT();
            record_status(PLM1_STS_ERROR_RECEIVED);
            plm_sts.state = PLM1_STATE_IDLE;
            break;
            
          case PLM_CC_RX_OVERRUN:
            // Firmware doesn't get data from PLM-1 fast enough.
            RX_CLEAR_PKT();
            record_status(PLM1_STS_RX_OVERRUN);
            plm_sts.state = PLM1_STATE_IDLE;
            break;
            
          default:
            // Other special characters are ignored.
            break;
        }
        
        // Start transmitting if a packet is available and reception is over.
        if((plm_sts.state == PLM1_STATE_IDLE) && (plm_tx.packet_desc[plm_tx.packet_index].used))
        {
            // Write first nibble.
            SPI_TX_NEXT(get_tx_nibble());
            plm_sts.state = PLM1_STATE_NEGOTIATING;
        }
        else
        {
            // Do not transmit packet.
            SPI_TX_STOP();
        }
    }
}


/*******************************************************************************
* Name:         record_status()        
* Description:  Record a new library status.
* Parameters:   New library status.
* Return:       None.
* Note:         
*******************************************************************************/
static void record_status(plm1_status sts)
{
    // "status[0]" is the oldest and "status[1]" the newest.
    if(plm_sts.status[0] == PLM1_STS_OK)
    {
        plm_sts.status[0] = sts;
    }
    else if (plm_sts.status[1] == PLM1_STS_OK)
    {
        plm_sts.status[1] = sts;
    }
    else
    {
        // Delete the oldest status.
        plm_sts.status[0] = plm_sts.status[1];
        plm_sts.status[1] = sts;
    }
}

/*******************************************************************************
* Name:         store_rx_nibble()        
* Description:  Store received data nibble.
* Parameters:   nibble: Data nibble received from PLM-1.
* Return:       None.
* Note:         
*******************************************************************************/
static void store_rx_nibble(uint8_t nibble)
{
    uint16_t bufIndex;
    
    // Is this packet already declared invalid?
    if(!plm_rx.invalid_packet)
    {
        // Buffer full or packet too long?
        if((plm_rx.packet_desc[plm_rx.desc_index].used == false) &&
           (plm_rx.packet_desc[plm_rx.desc_index].size < plm_rx.buffer_empty_size) &&
           (plm_rx.packet_desc[plm_rx.desc_index].size < PLM_MAX_PACKET_SIZE))
        {
            bufIndex = (plm_rx.buffer_empty_index + plm_rx.packet_desc[plm_rx.desc_index].size) % PLM_RX_BUFFER_SIZE;
            if(plm_rx.msb) {
                // Most significant nibble case.
                // Store it in the buffer, shifting of 4 bits.
                plm_rx.buffer[bufIndex] = nibble << 4;
            }
            else {
                // Least significant nibble case.
                // Merge with the most significant nibble and store in buffer.
                plm_rx.buffer[bufIndex] |= nibble;
                ++plm_rx.packet_desc[plm_rx.desc_index].size;
            }
            plm_rx.msb = !plm_rx.msb;            
        }
        else
        {
            // Buffer full or packet too long to be received by our code.
            plm_rx.invalid_packet = true;
            record_status(PLM1_STS_PACKET_MISSED);
        }
    }
}

/*******************************************************************************
* Name:         get_tx_nibble()        
* Description:  Get next nibble to send to PLM-1.
* Parameters:   None.
* Return:       Nibble to send to PLM-1.

* Note:         
*******************************************************************************/
static uint8_t get_tx_nibble(void)
{
    uint16_t dataIndex;
    uint8_t data;
    
    // Nibbles remaining in current packet?
    if(plm_tx.byte_sent < plm_tx.packet_desc[plm_tx.packet_index].size)
    {
        // Get data from tx buffer.
        dataIndex = (plm_tx.packet_desc[plm_tx.packet_index].start + plm_tx.byte_sent);
        if(dataIndex >= PLM_TX_BUFFER_SIZE)
        {
            dataIndex -= PLM_TX_BUFFER_SIZE;
        }
        data = plm_tx.buffer[dataIndex];
        
        // Get nibble into byte.
        if(plm_tx.msb)
        {
            data = data >> 4;
        }
        data &= 0x0F;
    }
    else
    {
        // Send an End Of Packet.
        data = PLM_CC_EOP;
    }
    
    return (data);
}

/*******************************************************************************
* Name:         update_tx_nibble()        
* Description:  Update transmission struct.
* Parameters:   None.
* Return:       true if complete packet has been sent, false otherwise.
* Note:         
*******************************************************************************/
static bool update_tx_nibble(void)
{
    bool pktSent = false;
    
    // Data nibble sent?
    if(plm_tx.byte_sent < plm_tx.packet_desc[plm_tx.packet_index].size)
    {
        // Complete byte has been sent?
        if(plm_tx.msb == false)
        {
            // Send next byte on upcoming transaction.
            plm_tx.byte_sent++;
        }
        plm_tx.msb = !plm_tx.msb;
    }
    else
    {
        // An End Of Packet has just been sent.
        pktSent = true;
        
        // Remove packet from tx buffer.
        TX_REMOVE_PKT();
    }
    
    return (pktSent);
}

/*******************************************************************************
* Name:         eop_received()        
* Description:  END OF PACKET received; update reception struct.
* Parameters:   None.
* Return:       None.
* Note:         
*******************************************************************************/
static void eop_received(void)
{
    // Reception buffer was full?
    if(plm_rx.packet_desc[plm_rx.desc_index].used == false)
    {
        if(plm_rx.invalid_packet)
        {
            // Invalid packet. Clear packet descriptor.
            plm_rx.packet_desc[plm_rx.desc_index].size = 0;
        }
        else if(plm_rx.packet_desc[plm_rx.desc_index].size)
        {
            // Valid packet! Update reception state and packet descriptor.
            plm_rx.packet_desc[plm_rx.desc_index].start = plm_rx.buffer_empty_index;
            plm_rx.packet_desc[plm_rx.desc_index].used = true;
            plm_rx.buffer_empty_size -= plm_rx.packet_desc[plm_rx.desc_index].size;
            plm_rx.buffer_empty_index = (plm_rx.buffer_empty_index + plm_rx.packet_desc[plm_rx.desc_index].size) % PLM_RX_BUFFER_SIZE;
            INCR(plm_rx.desc_index, PLM_RX_MAX_PACKET_NBR);
        }
    }
    
    // Reset reception variables.
    plm_rx.msb = true;
    plm_rx.invalid_packet = false;
}

/*******************************************************************************
* Name:         build_cfg_string()        
* Description:  Build default PLM-1 configuration string using plmcfg.h file.
* Parameters:   None.
* Return:       None.
* Note:         This function is not mandatory for PLM-1 library usage. It can
*               be removed to lighten memory requirement.
*******************************************************************************/
static void build_cfg_string(void)
{
    uint8_t byteIndex;
    uint8_t cfgCrc = 0;
    
    // Compute some advanced parameters if they are not already defined in plmcfg.h.
#ifndef DPHG
/*  +----------------------------------------------------+
    |          |      /   16 * (N0 - 1)(N1 - 1) \  |     |
    |   DPHG = | log  | ------------------------|  |     |
    |          |    2 |  WDIR                   |  |     |
    |          |_     \ 2     * XDIV * (N0 - N1)/ _|     |
    +----------------------------------------------------+ */
    uint8_t DPHG = (log2_round((16.0 * (N0 _PRIME_SIGN 1) * (N1 _PRIME_SIGN 1)) / ((short)(1 << WDIV) * XDIV * _PRIME_SIGN(-1) * (N0 - N1)), false) - 1);
#endif
#ifndef OFFSET
/*  +----------------------------------------------------------------+
    |             (DPHG + WDIV + 1)          (N0 - 1) + (N1 - 1)     |
    |   OFFSET = 2                  * XDIV * -------------------     |
    |                                        (N0 - 1)(N1 - 1)        |
    +----------------------------------------------------------------+ */
    short OFFSET = (short)( _PRIME_SIGN(-1) * ((long)1 << (DPHG + WDIV + 1)) * XDIV * (float)((N0 _PRIME_SIGN 1) + (N1 _PRIME_SIGN 1)) / ((short)(N0 _PRIME_SIGN 1) * (N1 _PRIME_SIGN 1)));
#endif
#ifndef FTH
/*  +-------------------------------------------------------------------------------------+
    |                                     /           WDIR                 \              |
    |          (DPHG + WDIV + 2)          |          2     * N0 * CPB      |              |
    |   FTH = 2                  * XDIV * |1 - ----------------------------| + OFFSET     |
    |                                     |     WDIR                       |              |
    |                                     \    2     * CPB * (N0 - 1) + N0 /              |
    +-------------------------------------------------------------------------------------+ */
    uint8_t FTH = (uint8_t)(((short)1 << (DPHG + WDIV + 2)) * XDIV * (1 - (float)((long)(1 << WDIV) * N0 * CPB)/((long)(1 << WDIV) * CPB * (N0 _PRIME_SIGN 1) + N0)) + OFFSET);
#endif
#ifndef FITH
    uint8_t FITH = (uint8_t)(FTH * 2);
#endif
#ifndef FC
/*  +---------------------------------------------------------------------------------------------------------+
    |                                          _             _                                                |
    |                                         |      / CPB \  |                       |  (n + 4)    6   |     |
    |   FC = [n2,n1,n0,m2,m1,m0]          n = | log  | --- |  | - 1          m = 15 - | 2        * ---  |     |
    |                                         |    2 \  6  /  |                       |_           CPB _|     |
    +---------------------------------------------------------------------------------------------------------+ */
    uint8_t temp_N = log2_round(CPB / 6.0, true) - 1;
    uint8_t FC = ((temp_N & 0x7) << 3) | ((15 - (uint8_t)(((uint16_t)1 << (temp_N + 4)) * 6 / CPB)) & 0x7);
#endif
#ifndef THRESHOLD
    uint8_t THRESHOLD;
    uint16_t temp_WPB = CPB;
    uint8_t temp_CPBn4 = 0;
    while(temp_WPB & 0xFFF0)
    {
        temp_CPBn4 = (uint8_t)(temp_WPB & 0x01);
        temp_WPB = temp_WPB >> 1;
    }
    THRESHOLD = (uint8_t)((temp_WPB + temp_CPBn4) / 2);
#endif
    
    // Fill configuration string.
    plm_sts.cfg[0] = PATTERN;
    plm_sts.cfg[1] = MASK;
    plm_sts.cfg[2] = (THRESHOLD << 3) | (XDIV >> 4);
    plm_sts.cfg[3] = (uint8_t)(XDIV << 4) | (DPHG & 0x0F);
    plm_sts.cfg[4] = (uint8_t)(OFFSET >> 3);
    plm_sts.cfg[5] = (uint8_t)(OFFSET << 5) | (FITH >> 3);
    plm_sts.cfg[6] = (uint8_t)(FITH << 5) | (FTH >> 2);
    plm_sts.cfg[7] = (uint8_t)(FTH << 6) | (FC & 0x3F);
    plm_sts.cfg[8] = (WDIV << 6) | (INPOL << 5) | (LTRINT << 4) | (INTPOL << 3) | (INTYPE << 2) | (TXOUTPOL << 1) | TXOUTYPE;
    plm_sts.cfg[9] = (TXENPOL << 7) | 0x60 | (NOPRIO << 4) | (NOPREAM << 3) | (NOMAC << 2) | (uint8_t)(CPB >> 8);
    plm_sts.cfg[10] = (uint8_t)CPB;
    plm_sts.cfg[11] = (DEL << 1) | (HT0PB >> 4);
    plm_sts.cfg[12] = (uint8_t)(HT0PB << 4) | (CPT1 >> 4);
    plm_sts.cfg[13] = (uint8_t)(CPT1 << 4) | (CPT0 >> 4);
    plm_sts.cfg[14] = (uint8_t)(CPT0 << 4) | (RND >> 4);
    plm_sts.cfg[15] = (uint8_t)(RND << 4) | (TIMCFG >> 4);
    plm_sts.cfg[16] = (uint8_t)(TIMCFG << 4) | (FC_LOW >> 4);
    plm_sts.cfg[17] = (uint8_t)(FC_LOW << 4) | (FC_HIGH >> 4);
    plm_sts.cfg[18] = (uint8_t)(FC_HIGH << 4);
                     
    // Compute CRC of configuration string.
    for(byteIndex = 0;  byteIndex < (PLM_CONFIG_DATA_LENGTH - 1); byteIndex++)
    {
        cfgCrc = crc4(plm_sts.cfg[byteIndex] >> 4, cfgCrc);
        cfgCrc = crc4(plm_sts.cfg[byteIndex] & 0x0F, cfgCrc);
    }
    cfgCrc = crc4(plm_sts.cfg[PLM_CONFIG_DATA_LENGTH - 1] >> 4, cfgCrc);
    
    // Complete configuration string by adding CRC value on last nibble.
    plm_sts.cfg[PLM_CONFIG_DATA_LENGTH - 1] = plm_sts.cfg[PLM_CONFIG_DATA_LENGTH - 1] | (cfgCrc & 0x0F);
}

/*******************************************************************************
* Name:         crc4()        
* Description:  Compute CRC4 of a nibble.
* Parameters:   nibble: Data nibble.
*               oldCrc: Initial CRC value.
* Return:       Computed CRC value.
* Note:         This function is not mandatory for PLM-1 library usage. It can
*               be removed to lighten memory requirement.
*******************************************************************************/
static uint8_t crc4(uint8_t nibble, uint8_t oldCrc)
{
    uint8_t newCrc;

    // newCrc.0 = data.3 ^ data.0 ^ crc.0 ^ crc.3
    newCrc = 0x1 & ((nibble >> 3) ^ nibble ^ oldCrc ^ (oldCrc >> 3));

    // newCrc.1 = data.3 ^ data.1 ^ data.0 ^ crc.0 ^ crc.1 ^ crc.3
    newCrc |= 0x2 & (((nibble >> 3) ^ (nibble >> 1) ^ nibble ^ oldCrc ^ (oldCrc >> 1) ^ (oldCrc >> 3)) << 1);

    // newCrc.2 = data.2 ^ data.1 ^ crc.1 ^ crc.2
    newCrc |= 0x4 & (((nibble >> 2) ^ (nibble >> 1) ^ (oldCrc >> 1) ^ (oldCrc >> 2)) << 2);

    // newCrc.3 = data.3 ^ data.2 ^ crc.2 ^ crc.3
    newCrc |= 0x8 & (((nibble >> 3) ^ (nibble >> 2) ^ (oldCrc >> 2) ^ (oldCrc >> 3)) << 3);

    return (newCrc);
}

/*******************************************************************************
* Name:         log2_round()        
* Description:  Compute binary log (log2).
* Parameters:   n: log2(n).
*               ceil: Use a CEIL function to round result if true, otherwise use
*                     a FLOOR function.
* Return:       Computed binary log.
* Note:         This function is not mandatory for PLM-1 library usage. It can
*               be removed to lighten memory requirement.
*******************************************************************************/
static uint8_t log2_round(float n, bool ceil)
{
    uint8_t result;

    if(ceil)
    {
        // Use a "ceil" function to round result.
        result = (n <=   1.0) ? 0 :
                 (n <=   2.0) ? 1 :
                 (n <=   4.0) ? 2 :
                 (n <=   8.0) ? 3 :
                 (n <=  16.0) ? 4 :
                 (n <=  32.0) ? 5 :
                 (n <=  64.0) ? 6 :
                 (n <= 128.0) ? 7 :
                 (n <= 256.0) ? 8 :
                 (n <= 512.0) ? 9 : 10;
    }
    else
    {
        // Use a "floor" function to round result.
        result = (n <   2.0) ? 0 :
                 (n <   4.0) ? 1 :
                 (n <   8.0) ? 2 :
                 (n <  16.0) ? 3 :
                 (n <  32.0) ? 4 :
                 (n <  64.0) ? 5 :
                 (n < 128.0) ? 6 :
                 (n < 256.0) ? 7 :
                 (n < 512.0) ? 8 : 9;
    }
    
    return (result);
}
