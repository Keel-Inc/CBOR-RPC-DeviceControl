#include "comm.h"
#include <stdio.h>
#include <string.h>

// Private variables ---------------------------------------------------------------------------------------------------

static uint8_t usart6_rx_buffer[USART6_RX_BUFFER_SIZE];
static uint8_t usart6_tx_buffer[USART6_TX_BUFFER_SIZE];
static volatile uint16_t usart6_rx_head = 0;
static volatile uint16_t usart6_rx_tail = 0;

// External variables --------------------------------------------------------------------------------------------------

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart6;

// Public functions ----------------------------------------------------------------------------------------------------

// Redirect printf to UART1
int _write(int file, char *ptr, int len)
{
    (void) file; // Suppress unused parameter warning
    HAL_UART_Transmit(&huart1, (uint8_t *) ptr, len, HAL_MAX_DELAY);
    return len;
}

void USART6_Start_Receive_IT(void)
{
    HAL_UART_Receive_IT(&huart6, &usart6_rx_buffer[usart6_rx_head], 1);
}

uint16_t USART6_Available(void)
{
    return (uint16_t) (USART6_RX_BUFFER_SIZE + usart6_rx_head - usart6_rx_tail) % USART6_RX_BUFFER_SIZE;
}

uint8_t USART6_Read_Byte(void)
{
    if (usart6_rx_head == usart6_rx_tail) {
        return 0; // No data available
    }

    uint8_t data = usart6_rx_buffer[usart6_rx_tail];
    usart6_rx_tail = (usart6_rx_tail + 1) % USART6_RX_BUFFER_SIZE;
    return data;
}

void USART6_Send_String(const char *str)
{
    HAL_UART_Transmit(&huart6, (uint8_t *) str, strlen(str), HAL_MAX_DELAY);
}

void USART6_Process_Message(void)
{
    static char message_buffer[256];
    static uint16_t message_index = 0;

    while (USART6_Available() > 0) {
        uint8_t received_char = USART6_Read_Byte();
        if (received_char == '\n' || received_char == '\r') {
            // End of message
            if (message_index > 0) {
                message_buffer[message_index] = '\0';

                // Print received message to console
                printf("USART6 Received: %s\r\n", message_buffer);

                // Echo the message back
                const int prefix_overhead = 2; // 1 for newline, 1 for null terminator
                int max_message_len = USART6_TX_BUFFER_SIZE - prefix_overhead;
                snprintf((char *) usart6_tx_buffer, USART6_TX_BUFFER_SIZE, "%.*s\n", max_message_len, message_buffer);
                USART6_Send_String((char *) usart6_tx_buffer);

                // Reset message buffer
                message_index = 0;
            }
        }
        else if (message_index < sizeof(message_buffer) - 1) {
            // Add character to message buffer
            message_buffer[message_index++] = received_char;
        }
    }
}

void CommInit(void)
{
    USART6_Start_Receive_IT();
}

// UART callback function
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART6) {
        // Move to next position in circular buffer
        usart6_rx_head = (usart6_rx_head + 1) % USART6_RX_BUFFER_SIZE;

        // Continue receiving immediately - minimize time in interrupt
        HAL_UART_Receive_IT(&huart6, &usart6_rx_buffer[usart6_rx_head], 1);
    }
}