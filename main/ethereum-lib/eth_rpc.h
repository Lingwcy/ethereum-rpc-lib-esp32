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

/**
 * @brief 返回当前客户端版本
 * 
 * @param context web3上下文
 * @param client_version 返回的客户端版本
 * @return esp_err_t ESP_OK成功，其他值失败                                     
 */
esp_err_t eth_get_client_version(web3_context_t* context, char* client_version, size_t version_len);


/**
 * @brief 返回给定数据的Keccak256哈希值
 * 
 * @param context web3上下文
 * @param post_data 要转化为哈希的数据
 * @param hash 返回的哈希值
 * @return esp_err_t ESP_OK成功，其他值失败                                     
 */
esp_err_t eth_get_web3_sha3(web3_context_t* context, const char* post_data, char* hash, size_t hash_len);

/**
 * @brief 返回当前网络id
 * 
 * @param context web3上下文
 * @param version_id 要转化为哈希的数据
 * @return esp_err_t ESP_OK 成功，其他值失败                                     
 */
esp_err_t eth_get_net_version(web3_context_t* context, char* network_id, size_t network_id_len);

/**
 * @brief 查看客户端是否正在主动监听网络连接
 * 
 * @param context web3上下文
 * @param result 返回的结果(boolean)
 * @return esp_err_t ESP_OK 成功，其他值失败                                     
 */
esp_err_t eth_get_net_listening(web3_context_t* context, bool* result, size_t result_len);

/**
 * @brief 返回连接到当前客户端节点的对等节点的数量
 * 
 * @param context web3上下文
 * @param quantity 返回的结果(*char hex)
 * @return esp_err_t ESP_OK 成功，其他值失败                                     
 */
esp_err_t eth_get_net_peerCount(web3_context_t* context, char* quantity, size_t quantity_len);

/**
 * @brief 返回当前以太坊协议版本
 * 
 * @param context web3上下文
 * @param result 返回的结果(*char decimal)
 * @return esp_err_t ESP_OK 成功，其他值失败                                     
 */
esp_err_t eth_get_eth_protocolVersion(web3_context_t* context, char* result, size_t result_len);

/**
 * @brief 返回一个对象，其中包含有关同步状态的数据或 false
 * 
 * @param context web3上下文
 * @param result 返回的结果(json object)
 * @return esp_err_t ESP_OK 成功，其他值失败                                     
 */
esp_err_t eth_get_eth_syncing(web3_context_t* context, char* result, size_t result_len);


/**
 * @brief 返回当前燃料价格的估计值 单位 wei
 * 
 * @param context web3上下文
 * @param quantity 返回的结果(*char hex)
 * @return esp_err_t ESP_OK 成功，其他值失败                                     
 */
esp_err_t get_eth_gasPrice(web3_context_t* context, char* quantity, size_t quantity_len);


/**
 * @brief 返回从一个地址发送的交易数量
 * 
 * @param context web3上下文
 * @param address 以太坊地址
 * @param quantity 返回的结果(*char hex)
 * @return esp_err_t ESP_OK 成功，其他值失败                                     
 */
esp_err_t eth_getTransactionCount(web3_context_t* context, const char* address, char* quantity, size_t quantity_len);

/**
 * @brief 签名
 * 
 * @param context web3上下文
 * @param address 以太坊地址
 * @param data 要签名的数据
 * @param signed_data 返回的结果(*char hex)
 * @return esp_err_t ESP_OK 成功，其他值失败                                     
 */
esp_err_t eth_sign(web3_context_t* context, const char* address, const char* data, char* signed_data, size_t signed_data_len);

/**
 * @brief 为交易签名但不发送
 * 
 * @param context Web3上下文
 * @param from 发送地址
 * @param to 接收地址 (为NULL时表示创建合约)
 * @param gas 燃料限制 (可为NULL，默认为90000)
 * @param gas_price 燃料价格 (可为NULL)
 * @param value 发送的以太币值 (可为NULL)
 * @param data 交易数据 (可为NULL)
 * @param nonce 随机数 (可为NULL)
 * @param signed_tx 返回的已签名交易
 * @param signed_tx_len 已签名交易缓冲区长度
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t eth_signTransaction(web3_context_t* context,
                             const char* from,
                             const char* to,
                             const char* gas,
                             const char* gas_price,
                             const char* value,
                             const char* data,
                             const char* nonce,
                             char* signed_tx,
                             size_t signed_tx_len);

/**
 * @brief 发送已签名的交易
 * 
 * @param context Web3上下文
 * @param signed_data 已签名的交易数据（十六进制字符串）
 * @param tx_hash 返回的交易哈希
 * @param tx_hash_len 交易哈希缓冲区长度
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t eth_sendRawTransaction(web3_context_t* context, const char* signed_data, char* tx_hash, size_t tx_hash_len);

/**
 * @brief 返回位于给定地址的代码 (untest)
 * 
 * @param context web3上下文
 * @param address 以太坊地址(20字节)
 * @param block_id 区块号或"latest"/"earliest"/"pending"/"safe"/"finalized"
 * @param code 返回的代码
 * @param code_len 代码缓冲区长度
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t eth_getCode(web3_context_t* context, const char* address, 
                      const char* block_id, char* code, size_t code_len);

#endif /* ETH_RPC_H */
