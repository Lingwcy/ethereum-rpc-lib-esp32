#ifndef FARMKEEPER_DEVICE_H
#define FARMKEEPER_DEVICE_H

#include <stdint.h>
#include <stdbool.h>
#include "ethereum-lib/web3.h"
#include "esp_err.h"

/**
 * @brief Device challenge configuration structure
 */
typedef struct {
    web3_context_t *web3_ctx;      // Web3 context for Ethereum communication
    const char *contract_address;   // FarmKeeper contract address
    const char *device_private_key; // Device private key for signing
    const char *device_address;     // Device Ethereum address (derived from private key)
    uint32_t device_id;             // Device ID in the FarmKeeper system
    uint32_t poll_interval_ms;      // How often to check for challenges (milliseconds)
} farmkeeper_device_config_t;

/**
 * @brief Initialize device challenge module
 * 
 * @param config Device challenge configuration
 * @return ESP_OK on success or an error code
 */
esp_err_t farmkeeper_device_init(const farmkeeper_device_config_t *config);

/**
 * @brief Check for pending challenges and respond to them
 * 
 * This function should be called periodically to check for and respond to challenges
 * 
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if no challenge, or an error code
 */
esp_err_t farmkeeper_device_check_and_respond_challenge(void);

/**
 * @brief Check if device has a pending challenge
 * 
 * @param has_challenge Set to true if device has a challenge, false otherwise
 * @return ESP_OK on success or an error code
 */
esp_err_t farmkeeper_device_has_challenge(bool *has_challenge);

/**
 * @brief Get the current device challenge
 * 
 * @param challenge Buffer to store the challenge
 * @param challenge_len Size of the challenge buffer
 * @return ESP_OK on success or an error code
 */
esp_err_t farmkeeper_device_get_challenge(char *challenge, size_t challenge_len);

/**
 * @brief Sign and verify a challenge
 * 
 * @param challenge The challenge message to sign
 * @return ESP_OK on success or an error code
 */
esp_err_t farmkeeper_device_verify_challenge(const char *challenge);

/**
 * @brief Reset the device challenge flag
 * 
 * @return esp_err_t ESP_OK on success, or an error code
 */
esp_err_t farmkeeper_device_reset_challenge_flag(void);

#endif /* FARMKEEPER_DEVICE_H */
