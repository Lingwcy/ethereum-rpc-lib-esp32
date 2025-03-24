#ifndef ETH_RPC_H
#define ETH_RPC_H

#include "web3.h"
#include <stdint.h>

/**
 * @brief 获取ETH区块链的当前区块号
 * 
 * @param context web3上下文
 * @param block_number 返回的区块号
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t eth_get_block_number(web3_context_t* context, uint64_t* block_number);

/**
 * @brief 获取账户余额
 * 
 * @param context web3上下文
 * @param address 以太坊地址
 * @param balance 返回的余额(wei)
 * @param balance_len 余额缓冲区长度
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t eth_get_balance(web3_context_t* context, const char* address, char* balance, size_t balance_len);

/**
 * @brief 获取交易收据
 * 
 * @param context web3上下文
 * @param tx_hash 交易哈希
 * @param receipt 返回的收据JSON字符串
 * @param receipt_len 收据缓冲区长度
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t eth_get_transaction_receipt(web3_context_t* context, const char* tx_hash, 
                                     char* receipt, size_t receipt_len);

#endif /* ETH_RPC_H */
