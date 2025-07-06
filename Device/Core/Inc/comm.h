#ifndef __COMM_H__
#define __COMM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "image.h"
#include "main.h"

// Constants -----------------------------------------------------------------------------------------------------------

#define CBOR_BUFFER_SIZE (IMAGE_DATA_SIZE + 256) // Add 256 bytes for length prefix and other overhead

#define USART1_RX_BUFFER_SIZE 256
#define USART1_TX_BUFFER_SIZE 256

#define USART6_RX_BUFFER_SIZE CBOR_BUFFER_SIZE
#define USART6_TX_BUFFER_SIZE 256

// API -----------------------------------------------------------------------------------------------------------------

void USART1_Send_String(const char *str);

void USART6_Start_Receive_IT(void);
uint32_t USART6_Available(void);
uint8_t USART6_Read_Byte(void);
void USART6_Send_String(const char *str);
void USART6_Process_Message(void);

void CommInit(void);

#ifdef __cplusplus
}
#endif

#endif // __COMM_H__