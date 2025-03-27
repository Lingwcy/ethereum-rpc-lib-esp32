#ifndef ETH_ABI_H
#define ETH_ABI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <esp_err.h>
#include "web3.h"  // Add this include to make web3_context_t available

/**
 * @brief 以太坊ABI数据类型
 */
typedef enum {
    ABI_TYPE_UINT,     // 无符号整数 (uint8至uint256)
    ABI_TYPE_INT,      // 有符号整数 (int8至int256)
    ABI_TYPE_ADDRESS,  // 地址 (20字节)
    ABI_TYPE_BOOL,     // 布尔值
    ABI_TYPE_BYTES,    // 定长字节数组 (bytes1至bytes32)
    ABI_TYPE_STRING,   // 字符串
    ABI_TYPE_ARRAY,    // 数组 (简化处理)
} abi_type_t;

/**
 * @brief ABI参数结构体
 */
typedef struct {
    abi_type_t type;      // 参数类型
    uint16_t size;        // 类型大小 (位数，如uint256则为256)
    bool is_array;        // 是否为数组类型
    bool is_dynamic;      // 是否为动态类型(string, bytes, 动态数组)
    const void* value;    // 参数值指针
    size_t length;        // 对于数组/字符串/bytes，表示长度
} abi_param_t;

/**
 * @brief ABI解码后的值
 */
typedef struct {
    abi_type_t type;        // 值的类型
    union {
        uint8_t address[20];// 地址（20字节）
        bool b;             // 布尔值
        uint8_t *bytes;     // 字节数组
        char *string;       // 字符串
        struct {
            uint64_t u64;   // 最多支持64位整数
            uint8_t *big;   // 大整数（超过64位）
        } uint_val;
    } value;
    size_t length;          // 动态类型的长度
    bool is_dynamic;        // 是否为动态类型
} abi_decoded_value_t;

/**
 * @brief 计算函数选择器
 * 
 * @param context web3上下文
 * @param signature 函数签名 (如 "transfer(address,uint256)")
 * @param selector 输出的选择器 (4字节)
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t abi_encode_function_selector(web3_context_t* context, const char* signature, uint8_t selector[4]);

/**
 * @brief 对单个参数进行ABI编码
 * 
 * @param param 参数描述
 * @param output 输出缓冲区
 * @param output_len 输出缓冲区大小
 * @param bytes_written 实际写入的字节数
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t abi_encode_param(const abi_param_t* param, 
                         uint8_t* output, 
                         size_t output_len, 
                         size_t* bytes_written);

/**
 * @brief 对多个参数进行ABI编码
 * 
 * @param params 参数数组
 * @param param_count 参数数量
 * @param output 输出缓冲区
 * @param output_len 输出缓冲区大小
 * @param bytes_written 实际写入的字节数
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t abi_encode_params(const abi_param_t* params, 
                          size_t param_count, 
                          uint8_t* output, 
                          size_t output_len, 
                          size_t* bytes_written);

/**
 * @brief 创建完整的合约函数调用数据
 * 
 * @param context web3上下文
 * @param signature 函数签名
 * @param params 参数数组
 * @param param_count 参数数量
 * @param output 输出缓冲区
 * @param output_len 输出缓冲区大小
 * @param bytes_written 实际写入的字节数
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t abi_encode_function_call(web3_context_t* context, 
                                 const char* signature, 
                                 const abi_param_t* params, 
                                 size_t param_count, 
                                 uint8_t* output, 
                                 size_t output_len, 
                                 size_t* bytes_written);

/**
 * @brief 将编码后的二进制数据转换为十六进制字符串
 * 
 * @param binary 二进制数据
 * @param binary_len 二进制数据长度
 * @param hex 输出的十六进制字符串缓冲区
 * @param hex_len 十六进制字符串缓冲区大小
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t abi_binary_to_hex(const uint8_t* binary, 
                          size_t binary_len, 
                          char* hex, 
                          size_t hex_len);

/**
 * @brief 将十六进制字符串转换为二进制数据
 * 
 * @param hex 十六进制字符串 (可以带"0x"前缀)
 * @param binary 输出的二进制数据
 * @param binary_len 二进制缓冲区长度
 * @param bytes_written 已写入的字节数
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t abi_hex_to_binary(const char* hex, uint8_t* binary, size_t binary_len, size_t* bytes_written);

/**
 * @brief 从ABI编码数据中解码字符串
 *
 * @param data ABI编码的二进制数据
 * @param data_len 数据长度
 * @param offset 字符串数据的偏移量
 * @param output 输出的字符串缓冲区
 * @param output_len 字符串缓冲区长度
 * @return esp_err_t
 */
esp_err_t abi_decode_string(const uint8_t* data, size_t data_len, size_t offset, 
                            char* output, size_t output_len);

/**
 * @brief 从ABI编码数据中解码多个返回值
 *
 * @param data ABI编码的二进制数据
 * @param data_len 数据长度
 * @param outputs 输出的解码值数组
 * @param output_count 期望的输出值数量
 * @param decoded_count 实际解码的值数量
 * @return esp_err_t
 */
esp_err_t abi_decode_returns(const uint8_t* data, size_t data_len, 
                             abi_decoded_value_t* outputs, size_t output_count, 
                             size_t* decoded_count);

/**
 * @brief 释放解码后的值占用的内存
 *
 * @param value 解码后的值
 */
void abi_free_decoded_value(abi_decoded_value_t* value);

// 辅助宏，用于创建参数
#define ABI_UINT(bits, val) { .type = ABI_TYPE_UINT, .size = (bits), .is_array = false, .is_dynamic = false, .value = (val), .length = 0 }
#define ABI_INT(bits, val) { .type = ABI_TYPE_INT, .size = (bits), .is_array = false, .is_dynamic = false, .value = (val), .length = 0 }
#define ABI_ADDRESS(val) { .type = ABI_TYPE_ADDRESS, .size = 160, .is_array = false, .is_dynamic = false, .value = (val), .length = 0 }
#define ABI_BOOL(val) { .type = ABI_TYPE_BOOL, .size = 8, .is_array = false, .is_dynamic = false, .value = (val), .length = 0 }
#define ABI_BYTES(val, len) { .type = ABI_TYPE_BYTES, .size = 0, .is_array = false, .is_dynamic = true, .value = (val), .length = (len) }
#define ABI_STRING(val) { .type = ABI_TYPE_STRING, .size = 0, .is_array = false, .is_dynamic = true, .value = (val), .length = strlen((const char*)(val)) }

#endif /* ETH_ABI_H */
