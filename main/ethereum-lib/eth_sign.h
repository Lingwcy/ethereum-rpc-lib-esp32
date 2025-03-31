#ifndef ETH_SIGN_H
#define ETH_SIGN_H

#include <stdint.h>
#include <esp_err.h>

/**
 * @brief Sign a message using an Ethereum private key
 * 
 * @param private_key_hex Private key in hex format (without 0x prefix)
 * @param message Message to sign
 * @param message_len Length of the message
 * @param signature Output buffer for the signature
 * @param signature_buf_len Size of the signature buffer
 * @param signature_len Output parameter for the actual signature length
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t eth_sign_personal_message(
    const char* private_key_hex, 
    const uint8_t* message, 
    size_t message_len,
    uint8_t* signature, 
    size_t signature_buf_len, 
    size_t* signature_len
);

/**
 * @brief Verify an Ethereum signature
 * 
 * @param address Ethereum address that supposedly signed the message
 * @param message Original message that was signed
 * @param message_len Length of the message
 * @param signature The signature to verify
 * @param signature_len Length of the signature
 * @return esp_err_t ESP_OK if signature is valid, error code otherwise
 */
esp_err_t eth_verify_personal_message(
    const char* address,
    const uint8_t* message,
    size_t message_len,
    const uint8_t* signature,
    size_t signature_len
);

#endif /* ETH_SIGN_H */
