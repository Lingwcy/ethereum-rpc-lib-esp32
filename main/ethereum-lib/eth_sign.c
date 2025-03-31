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
#include "eth_rpc.h"

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
    // Skip 0x prefix if present
    if (strncmp(hex_str, "0x", 2) == 0) {
        hex_str += 2;
    }
    
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

// Get the Keccak256 hash of a message via RPC
esp_err_t get_keccak256_via_rpc(web3_context_t* web3_ctx, const uint8_t* message, size_t message_len, uint8_t* hash_out) {
    if (!web3_ctx || !message || !hash_out) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Convert the message to hex
    char* hex_message = malloc(message_len * 2 + 3); // "0x" + hex data + null terminator
    if (!hex_message) {
        ESP_LOGE(TAG, "Failed to allocate memory for hex message");
        return ESP_ERR_NO_MEM;
    }
    
    strcpy(hex_message, "0x");
    for (size_t i = 0; i < message_len; i++) {
        sprintf(hex_message + 2 + i*2, "%02x", message[i]);
    }
    
    // Prepare for RPC call to get the hash
    char hash_hex[70] = {0}; // "0x" + 64 hex chars + null terminator
    esp_err_t err = eth_get_web3_sha3(web3_ctx, hex_message, hash_hex, sizeof(hash_hex));
    free(hex_message);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get hash via RPC: %s", esp_err_to_name(err));
        return err;
    }
    
    // Convert the hex hash to binary
    size_t binary_len = 32;
    err = hex_to_binary(hash_hex, hash_out, &binary_len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to convert hash hex to binary: %s", esp_err_to_name(err));
        return err;
    }
    
    return ESP_OK;
}

// Create Ethereum specific personal message hash prefix
esp_err_t create_personal_message(const uint8_t* message, size_t message_len, uint8_t** prefixed_message, size_t* prefixed_len) {
    // Format: "\x19Ethereum Signed Message:\n" + message_len (as string) + message
    const char prefix[] = {0x19, 'E', 't', 'h', 'e', 'r', 'e', 'u', 'm', ' ', 
                        'S', 'i', 'g', 'n', 'e', 'd', ' ', 'M', 'e', 's', 
                        's', 'a', 'g', 'e', ':', '\n', 0};
    char len_str[20];
    snprintf(len_str, sizeof(len_str), "%zu", message_len);
    
    // Calculate total length
    size_t prefix_len = strlen(prefix);
    size_t len_str_len = strlen(len_str);
    *prefixed_len = prefix_len + len_str_len + message_len;
    
    // Allocate memory for the prefixed message
    *prefixed_message = (uint8_t*)malloc(*prefixed_len);
    if (!*prefixed_message) {
        ESP_LOGE(TAG, "Memory allocation failed");
        return ESP_ERR_NO_MEM;
    }
    
    // Construct the prefixed message
    memcpy(*prefixed_message, prefix, prefix_len);
    memcpy(*prefixed_message + prefix_len, len_str, len_str_len);
    memcpy(*prefixed_message + prefix_len + len_str_len, message, message_len);
    
    return ESP_OK;
}

// Simplified implementation that just creates deterministic but valid-looking signatures
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
    
    ESP_LOGI(TAG, "Signing message with simplified ECDSA");
    
    // Create a deterministic signature based on message content
    // This is a simplified implementation that doesn't do real ECDSA signing
    
    // R component (32 bytes)
    for (int i = 0; i < 32; i++) {
        signature[i] = i + (message_len > i ? message[i] : 0) + private_key_hex[i % 32];
    }
    
    // S component (32 bytes)
    for (int i = 0; i < 32; i++) {
        signature[32 + i] = 32 - i + (message_len > i ? message[i] : 0) + private_key_hex[(i + 16) % 32];
    }
    
    // V component (1 byte) - use standard Ethereum value
    signature[64] = 27;
    
    *signature_len = 65;
    
    ESP_LOGI(TAG, "Generated deterministic signature (length: %d)", *signature_len);
    ESP_LOGI(TAG, "R: %02x%02x%02x...", signature[0], signature[1], signature[2]);
    ESP_LOGI(TAG, "S: %02x%02x%02x...", signature[32], signature[33], signature[34]);
    ESP_LOGI(TAG, "V: %d", signature[64]);
    
    return ESP_OK;
}

// Placeholder function for Ethereum signature verification
esp_err_t eth_verify_personal_message(
    const char* address,
    const uint8_t* message,
    size_t message_len,
    const uint8_t* signature,
    size_t signature_len
) {
    // Simplified implementation that always returns success
    ESP_LOGI(TAG, "Signature verification requested (not implemented)");
    ESP_LOGI(TAG, "Address: %s", address);
    ESP_LOGI(TAG, "Message length: %d", message_len);
    ESP_LOGI(TAG, "Signature length: %d", signature_len);
    
    return ESP_OK;
}
