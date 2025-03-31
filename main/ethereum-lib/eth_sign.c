#include "eth_sign.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <esp_log.h>

#include <mbedtls/md.h>
#include <mbedtls/pk.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/error.h>

static const char *TAG = "ETH_SIGN";

// Hex character to value
static int hex2int(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

// Hex string to binary
static esp_err_t hex_to_binary(const char* hex_str, uint8_t* output, size_t* output_len) {
    size_t len = strlen(hex_str);
    if (len % 2 != 0) {
        ESP_LOGE(TAG, "Invalid hex string length (must be even)");
        return ESP_ERR_INVALID_ARG;
    }
    
    size_t binary_len = len / 2;
    if (*output_len < binary_len) {
        ESP_LOGE(TAG, "Output buffer too small");
        return ESP_ERR_INVALID_SIZE;
    }
    
    for (size_t i = 0; i < binary_len; i++) {
        int high = hex2int(hex_str[i*2]);
        int low = hex2int(hex_str[i*2+1]);
        
        if (high < 0 || low < 0) {
            ESP_LOGE(TAG, "Invalid hex character at position %d", i*2);
            return ESP_ERR_INVALID_ARG;
        }
        
        output[i] = (high << 4) | low;
    }
    
    *output_len = binary_len;
    return ESP_OK;
}

// A simplified version that creates a simulated signature (for testing purposes only)
esp_err_t eth_sign_personal_message(
    const char* private_key_hex, 
    const uint8_t* message, 
    size_t message_len,
    uint8_t* signature, 
    size_t signature_buf_len, 
    size_t* signature_len
) {
    if (!private_key_hex || !message || !signature || !signature_len) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (signature_buf_len < 65) {
        ESP_LOGE(TAG, "Signature buffer too small");
        return ESP_ERR_INVALID_SIZE;
    }
    
    // Log what we're about to sign
    ESP_LOGI(TAG, "Signing message with length: %d", message_len);
    
    // Create an extremely simple predictable signature
    // Fill entire buffer with zeros first
    memset(signature, 0, signature_buf_len);
    
    // Just set a few key bytes to make it recognizable but keep it simple
    signature[0] = 0xAA;  // Start of R
    signature[31] = 0xBB; // End of R
    signature[32] = 0xCC; // Start of S
    signature[63] = 0xDD; // End of S
    signature[64] = 27;   // v value (recovery ID)
    
    // Set the signature length
    *signature_len = 65;
    
    ESP_LOGI(TAG, "Generated simplified test signature of length: 65 bytes");
    return ESP_OK;
}

// Function to verify an Ethereum signature
esp_err_t eth_verify_personal_message(
    const char* address,
    const uint8_t* message,
    size_t message_len,
    const uint8_t* signature,
    size_t signature_len
) {
    // This is a placeholder - not implemented
    return ESP_OK;
}
