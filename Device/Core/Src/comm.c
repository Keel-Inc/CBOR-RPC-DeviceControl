#include "comm.h"
#include "cbor.h"
#include "image.h"
#include <stdio.h>
#include <string.h>

// Private macros ------------------------------------------------------------------------------------------------------

#define RESET_MESSAGE_STATE()                                                                                          \
    do {                                                                                                               \
        bytes_received = 0;                                                                                            \
        expected_length = 0;                                                                                           \
        state = 0;                                                                                                     \
    } while (0)

#define SKIP_KEY_VALUE_PAIR(cbor_value_ptr)                                                                            \
    do {                                                                                                               \
        err = cbor_value_advance(cbor_value_ptr);                                                                      \
        if (err != CborNoError) {                                                                                      \
            return err;                                                                                                \
        }                                                                                                              \
        err = cbor_value_advance(cbor_value_ptr);                                                                      \
        if (err != CborNoError) {                                                                                      \
            return err;                                                                                                \
        }                                                                                                              \
        continue;                                                                                                      \
    } while (0)

// Private variables ---------------------------------------------------------------------------------------------------

// Place large buffers in SDRAM
static uint8_t cbor_buffer[CBOR_BUFFER_SIZE] __attribute__((section(".sdram")));
static uint8_t usart6_rx_buffer[USART6_RX_BUFFER_SIZE] __attribute__((section(".sdram")));
static volatile uint32_t usart6_rx_head = 0;
static volatile uint32_t usart6_rx_tail = 0;

// External variables --------------------------------------------------------------------------------------------------

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart6;

// Private functions ---------------------------------------------------------------------------------------------------

static void send_cbor_response(const char *status, const char *message)
{
    printf("USART6 DEBUG: Preparing response - status: %s, message: %s\r\n", status, message);

    // Create response object
    CborEncoder encoder;
    uint8_t response_buffer[256];

    cbor_encoder_init(&encoder, response_buffer, sizeof(response_buffer), 0);

    CborEncoder map_encoder;
    cbor_encoder_create_map(&encoder, &map_encoder, 2);

    // Add status
    cbor_encode_text_string(&map_encoder, "status", 6);
    cbor_encode_text_string(&map_encoder, status, strlen(status));

    // Add message
    cbor_encode_text_string(&map_encoder, "message", 7);
    cbor_encode_text_string(&map_encoder, message, strlen(message));

    cbor_encoder_close_container(&encoder, &map_encoder);

    size_t encoded_size = cbor_encoder_get_buffer_size(&encoder, response_buffer);

    printf("USART6 DEBUG: Response encoded, size: %lu bytes\r\n", (unsigned long) encoded_size);

    // Send length prefix (4 bytes, big-endian)
    uint8_t length_prefix[4];
    length_prefix[0] = (uint8_t) (encoded_size >> 24);
    length_prefix[1] = (uint8_t) (encoded_size >> 16);
    length_prefix[2] = (uint8_t) (encoded_size >> 8);
    length_prefix[3] = (uint8_t) (encoded_size & 0xFF);

    printf("USART6 DEBUG: Sending length prefix: %02X %02X %02X %02X\r\n", length_prefix[0], length_prefix[1],
           length_prefix[2], length_prefix[3]);

    HAL_UART_Transmit(&huart6, length_prefix, 4, HAL_MAX_DELAY);
    HAL_UART_Transmit(&huart6, response_buffer, encoded_size, HAL_MAX_DELAY);

    printf("USART6 DEBUG: Response sent successfully\r\n");
}

static void send_test_response(const char *status, const char *message, const char *received_message)
{
    printf("USART6 DEBUG: Preparing test response - status: %s, message: %s, received_message: %s\r\n", status, message,
           received_message);

    // Create response object
    CborEncoder encoder;
    uint8_t response_buffer[512];

    cbor_encoder_init(&encoder, response_buffer, sizeof(response_buffer), 0);

    CborEncoder map_encoder;
    cbor_encoder_create_map(&encoder, &map_encoder, 3);

    // Add status
    cbor_encode_text_string(&map_encoder, "status", 6);
    cbor_encode_text_string(&map_encoder, status, strlen(status));

    // Add message
    cbor_encode_text_string(&map_encoder, "message", 7);
    cbor_encode_text_string(&map_encoder, message, strlen(message));

    // Add received_message
    cbor_encode_text_string(&map_encoder, "received_message", 16);
    cbor_encode_text_string(&map_encoder, received_message, strlen(received_message));

    cbor_encoder_close_container(&encoder, &map_encoder);

    size_t encoded_size = cbor_encoder_get_buffer_size(&encoder, response_buffer);

    printf("USART6 DEBUG: Test response encoded, size: %lu bytes\r\n", (unsigned long) encoded_size);

    // Send length prefix (4 bytes, big-endian)
    uint8_t length_prefix[4];
    length_prefix[0] = (uint8_t) (encoded_size >> 24);
    length_prefix[1] = (uint8_t) (encoded_size >> 16);
    length_prefix[2] = (uint8_t) (encoded_size >> 8);
    length_prefix[3] = (uint8_t) (encoded_size & 0xFF);

    printf("USART6 DEBUG: Sending test response length prefix: %02X %02X %02X %02X\r\n", length_prefix[0],
           length_prefix[1], length_prefix[2], length_prefix[3]);

    HAL_UART_Transmit(&huart6, length_prefix, 4, HAL_MAX_DELAY);
    HAL_UART_Transmit(&huart6, response_buffer, encoded_size, HAL_MAX_DELAY);

    printf("USART6 DEBUG: Test response sent successfully\r\n");
}

static CborError process_display_params(CborValue *params_map)
{
    CborError err;
    bool found_image_data = false;

    printf("USART6 DEBUG: Starting parameter processing\r\n");

    while (!cbor_value_at_end(params_map)) {
        if (!cbor_value_is_text_string(params_map)) {
            printf("USART6 DEBUG: Skipping non-text key\r\n");
            SKIP_KEY_VALUE_PAIR(params_map);
        }

        char param_name[32];
        size_t param_length = sizeof(param_name);
        err = cbor_value_copy_text_string(params_map, param_name, &param_length, NULL);
        if (err != CborNoError) {
            printf("USART6 DEBUG: Error reading parameter name: %d\r\n", err);
            return err;
        }

        printf("USART6 DEBUG: Found parameter: %s\r\n", param_name);

        if (strcmp(param_name, "image_data") == 0) {
            printf("USART6 DEBUG: Processing image_data parameter\r\n");
            // Move to image data value
            err = cbor_value_advance(params_map);
            if (err != CborNoError) {
                printf("USART6 DEBUG: Error advancing to image data value: %d\r\n", err);
                return err;
            }

            // Process image data
            if (!cbor_value_is_byte_string(params_map)) {
                printf("USART6 DEBUG: Image data is not a byte string\r\n");
                send_cbor_response("error", "Image data must be byte string");
                return CborErrorIllegalType;
            }

            size_t image_data_length;
            err = cbor_value_get_string_length(params_map, &image_data_length);
            if (err != CborNoError) {
                printf("USART6 DEBUG: Error getting image data length: %d\r\n", err);
                send_cbor_response("error", "Invalid image data format");
                return err;
            }

            printf("USART6 DEBUG: Image data length: %lu bytes\r\n", (unsigned long) image_data_length);

            if (image_data_length > IMAGE_DATA_SIZE) {
                printf("USART6 DEBUG: Image data too large: %zu > %d\r\n", image_data_length, IMAGE_DATA_SIZE);
                send_cbor_response("error", "Image data too large");
                return CborErrorDataTooLarge;
            }

            // Copy image data directly to framebuffer
            uint8_t *image_buffer = get_image_buffer();
            err = cbor_value_copy_byte_string(params_map, image_buffer, &image_data_length, NULL);
            if (err != CborNoError) {
                printf("USART6 DEBUG: Error copying image data: %d\r\n", err);
                send_cbor_response("error", "Failed to read image data");
                return err;
            }

            printf("USART6 DEBUG: Image data copied successfully, updating display\r\n");
            update_display();
            found_image_data = true;

            // Continue parsing to look for additional parameters
            err = cbor_value_advance(params_map);
            if (err != CborNoError) {
                printf("USART6 DEBUG: Error advancing after image data: %d\r\n", err);
                return err;
            }
        }
        else {
            printf("USART6 DEBUG: Skipping unknown parameter: %s\r\n", param_name);
            SKIP_KEY_VALUE_PAIR(params_map);
        }
    }

    printf("USART6 DEBUG: Parameter processing complete - image_data: %s\r\n", found_image_data ? "YES" : "NO");

    // Send appropriate response based on what we found
    if (found_image_data) {
        send_cbor_response("success", "Image displayed successfully");
    }
    else {
        printf("USART6 DEBUG: No valid parameters found\r\n");
        send_cbor_response("error", "No valid parameters found (expected image_data)");
        return CborErrorUnknownType;
    }

    return CborNoError;
}

static CborError handle_test_method(CborValue *map_value)
{
    CborError err;
    char test_message[128];
    bool found_test_message = false;

    printf("USART6 DEBUG: Handling test method\r\n");

    while (!cbor_value_at_end(map_value)) {
        if (!cbor_value_is_text_string(map_value)) {
            printf("USART6 DEBUG: Skipping non-text key in test method handler\r\n");
            SKIP_KEY_VALUE_PAIR(map_value);
        }

        char key_name[32];
        size_t key_length = sizeof(key_name);
        err = cbor_value_copy_text_string(map_value, key_name, &key_length, NULL);
        if (err != CborNoError) {
            printf("USART6 DEBUG: Error reading test method handler key: %d\r\n", err);
            return err;
        }

        printf("USART6 DEBUG: Found test method handler key: %s\r\n", key_name);

        if (strcmp(key_name, "params") == 0) {
            printf("USART6 DEBUG: Found params field in test method\r\n");
            // Move to params value
            err = cbor_value_advance(map_value);
            if (err != CborNoError) {
                printf("USART6 DEBUG: Error advancing to test params value: %d\r\n", err);
                return err;
            }

            // Enter params map
            if (!cbor_value_is_map(map_value)) {
                printf("USART6 DEBUG: Test params is not a map\r\n");
                send_cbor_response("error", "Test params must be a map");
                return CborErrorIllegalType;
            }

            printf("USART6 DEBUG: Test params is a map - good\r\n");

            CborValue params_map;
            err = cbor_value_enter_container(map_value, &params_map);
            if (err != CborNoError) {
                printf("USART6 DEBUG: Error entering test params container: %d\r\n", err);
                send_cbor_response("error", "Failed to parse test params");
                return err;
            }

            printf("USART6 DEBUG: Entered test params container\r\n");

            // Look for test_message parameter
            while (!cbor_value_at_end(&params_map)) {
                if (!cbor_value_is_text_string(&params_map)) {
                    printf("USART6 DEBUG: Skipping non-text key in test params\r\n");
                    SKIP_KEY_VALUE_PAIR(&params_map);
                }

                char param_name[32];
                size_t param_length = sizeof(param_name);
                err = cbor_value_copy_text_string(&params_map, param_name, &param_length, NULL);
                if (err != CborNoError) {
                    printf("USART6 DEBUG: Error reading test parameter name: %d\r\n", err);
                    return err;
                }

                printf("USART6 DEBUG: Found test parameter: %s\r\n", param_name);

                if (strcmp(param_name, "test_message") == 0) {
                    printf("USART6 DEBUG: Processing test_message parameter\r\n");
                    // Move to test message value
                    err = cbor_value_advance(&params_map);
                    if (err != CborNoError) {
                        printf("USART6 DEBUG: Error advancing to test message value: %d\r\n", err);
                        return err;
                    }

                    // Process test message
                    if (!cbor_value_is_text_string(&params_map)) {
                        printf("USART6 DEBUG: Test message is not a text string\r\n");
                        send_cbor_response("error", "Test message must be a string");
                        return CborErrorIllegalType;
                    }

                    size_t test_message_length = sizeof(test_message);
                    err = cbor_value_copy_text_string(&params_map, test_message, &test_message_length, NULL);
                    if (err != CborNoError) {
                        printf("USART6 DEBUG: Error copying test message: %d\r\n", err);
                        send_cbor_response("error", "Failed to read test message");
                        return err;
                    }

                    printf("USART6 Received test message: %s\r\n", test_message);
                    found_test_message = true;

                    // Continue parsing to look for additional parameters
                    err = cbor_value_advance(&params_map);
                    if (err != CborNoError) {
                        printf("USART6 DEBUG: Error advancing after test message: %d\r\n", err);
                        return err;
                    }
                }
                else {
                    printf("USART6 DEBUG: Skipping unknown test parameter: %s\r\n", param_name);
                    SKIP_KEY_VALUE_PAIR(&params_map);
                }
            }

            if (found_test_message) {
                send_test_response("success", "Test RPC call processed successfully", test_message);
            }
            else {
                printf("USART6 DEBUG: No test_message parameter found\r\n");
                send_cbor_response("error", "No test_message parameter found");
                return CborErrorUnknownType;
            }

            return CborNoError;
        }
        else {
            printf("USART6 DEBUG: Skipping unknown test method handler key: %s\r\n", key_name);
            SKIP_KEY_VALUE_PAIR(map_value);
        }
    }

    printf("USART6 DEBUG: Params not found in test method call\r\n");
    send_cbor_response("error", "Params not found in test method call");
    return CborErrorUnknownType;
}

static CborError handle_clear_display_method(CborValue *map_value)
{
    printf("USART6 DEBUG: Handling clear_display method\r\n");

    // Clear the image buffer and update display
    clear_image_buffer();
    update_display();

    // Send success response
    send_cbor_response("success", "Display cleared successfully");

    return CborNoError;
}

static CborError handle_display_default_method(CborValue *map_value)
{
    printf("USART6 DEBUG: Handling display_default method\r\n");

    // Display the default image
    display_default_image();

    // Send success response
    send_cbor_response("success", "Default image displayed successfully");

    return CborNoError;
}

static CborError handle_display_image_method(CborValue *map_value)
{
    CborError err;

    printf("USART6 DEBUG: Handling display_image method\r\n");

    while (!cbor_value_at_end(map_value)) {
        if (!cbor_value_is_text_string(map_value)) {
            printf("USART6 DEBUG: Skipping non-text key in method handler\r\n");
            SKIP_KEY_VALUE_PAIR(map_value);
        }

        char key_name[32];
        size_t key_length = sizeof(key_name);
        err = cbor_value_copy_text_string(map_value, key_name, &key_length, NULL);
        if (err != CborNoError) {
            printf("USART6 DEBUG: Error reading method handler key: %d\r\n", err);
            return err;
        }

        printf("USART6 DEBUG: Found method handler key: %s\r\n", key_name);

        if (strcmp(key_name, "params") == 0) {
            printf("USART6 DEBUG: Found params field\r\n");
            // Move to params value
            err = cbor_value_advance(map_value);
            if (err != CborNoError) {
                printf("USART6 DEBUG: Error advancing to params value: %d\r\n", err);
                return err;
            }

            // Enter params map
            if (!cbor_value_is_map(map_value)) {
                printf("USART6 DEBUG: Params is not a map\r\n");
                send_cbor_response("error", "Params must be a map");
                return CborErrorIllegalType;
            }

            printf("USART6 DEBUG: Params is a map - good\r\n");

            CborValue params_map;
            err = cbor_value_enter_container(map_value, &params_map);
            if (err != CborNoError) {
                printf("USART6 DEBUG: Error entering params container: %d\r\n", err);
                send_cbor_response("error", "Failed to parse params");
                return err;
            }

            printf("USART6 DEBUG: Entered params container, calling process_display_params\r\n");
            return process_display_params(&params_map);
        }
        else {
            printf("USART6 DEBUG: Skipping unknown method handler key: %s\r\n", key_name);
            SKIP_KEY_VALUE_PAIR(map_value);
        }
    }

    printf("USART6 DEBUG: Params not found in method call\r\n");
    send_cbor_response("error", "Params not found in method call");
    return CborErrorUnknownType;
}

static CborError process_cbor_rpc_message(const uint8_t *cbor_data, size_t data_length)
{
    printf("USART6 DEBUG: Starting CBOR RPC message processing, length: %zu\r\n", data_length);

    // Parse the CBOR message
    CborParser parser;
    CborValue value;

    CborError err = cbor_parser_init(cbor_data, data_length, 0, &parser, &value);
    if (err != CborNoError) {
        printf("USART6 Error parsing CBOR: %d\r\n", err);
        printf("USART6 DEBUG: CBOR parser init failed\r\n");
        send_cbor_response("error", "Failed to parse CBOR message");
        return err;
    }

    printf("USART6 DEBUG: CBOR parser initialized successfully\r\n");

    // Expect a map (RPC message)
    if (!cbor_value_is_map(&value)) {
        printf("USART6 Error: Expected map, got type %d\r\n", value.type);
        printf("USART6 DEBUG: Root value is not a map\r\n");
        send_cbor_response("error", "Expected RPC message format");
        return CborErrorIllegalType;
    }

    printf("USART6 DEBUG: Root value is a map - good\r\n");

    // Enter the map
    CborValue map_value;
    err = cbor_value_enter_container(&value, &map_value);
    if (err != CborNoError) {
        printf("USART6 Error entering map: %d\r\n", err);
        printf("USART6 DEBUG: Failed to enter map container\r\n");
        send_cbor_response("error", "Failed to parse message structure");
        return err;
    }

    printf("USART6 DEBUG: Entered map container successfully\r\n");

    // Look for "method" field and determine which method to call
    char method_name[32];
    bool found_method = false;

    printf("USART6 DEBUG: Searching for method field\r\n");

    while (!cbor_value_at_end(&map_value)) {
        if (!cbor_value_is_text_string(&map_value)) {
            printf("USART6 DEBUG: Skipping non-text key in root map\r\n");
            SKIP_KEY_VALUE_PAIR(&map_value);
        }

        char key_name[32];
        size_t key_length = sizeof(key_name);
        err = cbor_value_copy_text_string(&map_value, key_name, &key_length, NULL);
        if (err != CborNoError) {
            printf("USART6 DEBUG: Error reading root map key: %d\r\n", err);
            return err;
        }

        printf("USART6 DEBUG: Found root map key: %s\r\n", key_name);

        if (strcmp(key_name, "method") == 0) {
            printf("USART6 DEBUG: Found method field\r\n");
            // Move to value
            err = cbor_value_advance(&map_value);
            if (err != CborNoError) {
                printf("USART6 DEBUG: Error advancing to method value: %d\r\n", err);
                return err;
            }

            // Read method value
            if (!cbor_value_is_text_string(&map_value)) {
                printf("USART6 DEBUG: Method value is not a string\r\n");
                send_cbor_response("error", "Method must be a string");
                return CborErrorIllegalType;
            }

            size_t method_length = sizeof(method_name);
            err = cbor_value_copy_text_string(&map_value, method_name, &method_length, NULL);
            if (err != CborNoError) {
                printf("USART6 DEBUG: Error reading method value: %d\r\n", err);
                return err;
            }

            printf("USART6 DEBUG: Method value: %s\r\n", method_name);
            found_method = true;
            break;
        }

        // Move to next key-value pair
        err = cbor_value_advance(&map_value);
        if (err != CborNoError) {
            printf("USART6 DEBUG: Error advancing to next key: %d\r\n", err);
            return err;
        }
        err = cbor_value_advance(&map_value);
        if (err != CborNoError) {
            printf("USART6 DEBUG: Error advancing to next value: %d\r\n", err);
            return err;
        }
    }

    if (!found_method) {
        printf("USART6 DEBUG: Method field not found\r\n");
        send_cbor_response("error", "Method field not found in RPC message");
        return CborErrorUnknownType;
    }

    printf("USART6 DEBUG: Method validation passed, processing method: %s\r\n", method_name);

    // Reset to beginning of map to process the full message
    err = cbor_value_enter_container(&value, &map_value);
    if (err != CborNoError) {
        printf("USART6 DEBUG: Error re-entering map container: %d\r\n", err);
        return err;
    }

    // Route to appropriate method handler
    if (strcmp(method_name, "display_image") == 0) {
        return handle_display_image_method(&map_value);
    }
    else if (strcmp(method_name, "clear_display") == 0) {
        return handle_clear_display_method(&map_value);
    }
    else if (strcmp(method_name, "display_default") == 0) {
        return handle_display_default_method(&map_value);
    }
    else if (strcmp(method_name, "test") == 0) {
        return handle_test_method(&map_value);
    }
    else {
        printf("USART6 DEBUG: Unknown method: %s\r\n", method_name);
        send_cbor_response("error", "Unknown method");
        return CborErrorUnknownType;
    }
}

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

uint32_t USART6_Available(void)
{
    return (USART6_RX_BUFFER_SIZE + usart6_rx_head - usart6_rx_tail) % USART6_RX_BUFFER_SIZE;
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
    static uint32_t expected_length = 0;
    static uint32_t bytes_received = 0;
    static uint8_t state = 0; // 0 = reading length, 1 = reading data

    while (USART6_Available() > 0) {
        uint8_t received_byte = USART6_Read_Byte();

        if (state != 0) {
            // Reading CBOR data
            if (bytes_received < CBOR_BUFFER_SIZE) {
                cbor_buffer[bytes_received++] = received_byte;
            }
            else {
                printf("USART6 Error: Buffer overflow\r\n");
                printf("USART6 DEBUG: Buffer overflow - bytes_received: %lu, buffer size: %d\r\n", bytes_received,
                       CBOR_BUFFER_SIZE);

                // Flush receive buffer to get back in sync with host
                printf("USART6 DEBUG: Flushing receive buffer to resync\r\n");
                while (USART6_Available() > 0) {
                    USART6_Read_Byte();
                }

                RESET_MESSAGE_STATE();
                continue;
            }

            if (bytes_received != expected_length) {
                continue;
            }

            // We have the complete CBOR message, process it
            printf("USART6 Received complete CBOR message (%lu bytes)\r\n", expected_length);
            printf("USART6 DEBUG: Message complete, starting CBOR processing\r\n");

            // Parse the CBOR message
            CborError err = process_cbor_rpc_message(cbor_buffer, expected_length);
            if (err != CborNoError) {
                printf("USART6 Error processing CBOR message: %d\r\n", err);
                printf("USART6 DEBUG: CBOR processing failed with error %d\r\n", err);
            }
            else {
                printf("USART6 DEBUG: CBOR processing completed successfully\r\n");
            }

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
        printf("USART6 DEBUG: Length prefix received: %02X %02X %02X %02X\r\n", cbor_buffer[0], cbor_buffer[1],
               cbor_buffer[2], cbor_buffer[3]);

        // Print more bytes from buffer to see what's actually there
        printf("USART6 DEBUG: Next 16 bytes in buffer:");
        for (int i = 0; i < 16 && USART6_Available() > 0; i++) {
            printf(" %02X", usart6_rx_buffer[(usart6_rx_tail + i) % USART6_RX_BUFFER_SIZE]);
        }
        printf("\r\n");

        // Sanity check on message length (increased for image data)
        if (expected_length > CBOR_BUFFER_SIZE) {
            printf("USART6 Error: Message too large (%lu bytes)\r\n", expected_length);
            printf("USART6 DEBUG: Message too large - expected: %lu, buffer size: %d\r\n", expected_length,
                   CBOR_BUFFER_SIZE);
            send_cbor_response("error", "Message too large");

            // Flush receive buffer to get back in sync with host
            printf("USART6 DEBUG: Flushing receive buffer to resync\r\n");
            while (USART6_Available() > 0) {
                USART6_Read_Byte();
            }

            RESET_MESSAGE_STATE();
            continue;
        }

        printf("USART6 DEBUG: Length validation passed, switching to data reception mode\r\n");
        bytes_received = 0;
        state = 1;
    }
}

void CommInit(void)
{
    printf("USART6 DEBUG: Initializing communication\r\n");

    // Initialize SDRAM buffers to zero (since they're in NOLOAD section)
    memset(usart6_rx_buffer, 0, sizeof(usart6_rx_buffer));
    memset(cbor_buffer, 0, sizeof(cbor_buffer));

    // Reset buffer pointers
    usart6_rx_head = 0;
    usart6_rx_tail = 0;

    USART6_Start_Receive_IT();
    printf("USART6 DEBUG: Communication initialization complete\r\n");
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