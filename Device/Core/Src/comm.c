#include "comm.h"
#include "cbor.h"
#include <stdio.h>
#include <string.h>

// Private macros ------------------------------------------------------------------------------------------------------

#define RESET_MESSAGE_STATE()                                                                                          \
    do {                                                                                                               \
        bytes_received = 0;                                                                                            \
        expected_length = 0;                                                                                           \
        state = 0;                                                                                                     \
    } while (0)

// Private variables ---------------------------------------------------------------------------------------------------

static uint8_t usart6_rx_buffer[USART6_RX_BUFFER_SIZE];
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
    static uint8_t cbor_buffer[256];
    static uint32_t expected_length = 0;
    static uint16_t bytes_received = 0;
    static uint8_t state = 0; // 0 = reading length, 1 = reading data

    while (USART6_Available() > 0) {
        uint8_t received_byte = USART6_Read_Byte();

        if (state != 0) {
            // Reading CBOR data
            cbor_buffer[bytes_received++] = received_byte;

            if (bytes_received != expected_length) {
                continue;
            }

            // We have the complete CBOR message, process it
            printf("USART6 Received complete CBOR message\r\n");

            // Parse the CBOR message
            CborParser parser;
            CborValue value;

            CborError err = cbor_parser_init(cbor_buffer, expected_length, 0, &parser, &value);
            if (err != CborNoError) {
                printf("USART6 Error parsing CBOR: %d\r\n", err);
                RESET_MESSAGE_STATE();
                continue;
            }

            if (!cbor_value_is_text_string(&value)) {
                printf("USART6 Error: Not a text string: %d\r\n", value.type);
                RESET_MESSAGE_STATE();
                continue;
            }

            // Extract the string
            char text_buffer[256];
            size_t text_length = sizeof(text_buffer);

            err = cbor_value_copy_text_string(&value, text_buffer, &text_length, NULL);
            if (err != CborNoError) {
                printf("USART6 Error copying text string: %d\r\n", err);
                RESET_MESSAGE_STATE();
                continue;
            }

            printf("USART6 Decoded text: %s\r\n", text_buffer);

            // Echo back the same string as CBOR
            CborEncoder encoder;
            uint8_t response_buffer[256];

            cbor_encoder_init(&encoder, response_buffer, sizeof(response_buffer), 0);
            err = cbor_encode_text_string(&encoder, text_buffer, text_length);

            if (err != CborNoError) {
                printf("USART6 Error encoding response: %d\r\n", err);
                RESET_MESSAGE_STATE();
                continue;
            }

            size_t encoded_size = cbor_encoder_get_buffer_size(&encoder, response_buffer);

            // Send length prefix (4 bytes, big-endian)
            uint8_t length_prefix[4];
            length_prefix[0] = (uint8_t) (encoded_size >> 24);
            length_prefix[1] = (uint8_t) (encoded_size >> 16);
            length_prefix[2] = (uint8_t) (encoded_size >> 8);
            length_prefix[3] = (uint8_t) (encoded_size & 0xFF);

            HAL_UART_Transmit(&huart6, length_prefix, 4, HAL_MAX_DELAY);
            HAL_UART_Transmit(&huart6, response_buffer, encoded_size, HAL_MAX_DELAY);

            // Reset for next message
            RESET_MESSAGE_STATE();
            continue;
        }

        // Reading 4-byte length prefix (big-endian)
        cbor_buffer[bytes_received++] = received_byte;

        if (bytes_received != 4) {
            continue;
        }

        // We have the complete length
        expected_length = ((uint32_t) cbor_buffer[0] << 24) | ((uint32_t) cbor_buffer[1] << 16) |
                          ((uint32_t) cbor_buffer[2] << 8) | cbor_buffer[3];

        printf("USART6 Expecting CBOR message of %lu bytes\r\n", expected_length);

        // Sanity check on message length
        if (expected_length > 200) {
            printf("USART6 Error: Message too large (%lu bytes)\r\n", expected_length);
            RESET_MESSAGE_STATE();
            continue;
        }

        bytes_received = 0;
        state = 1;
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