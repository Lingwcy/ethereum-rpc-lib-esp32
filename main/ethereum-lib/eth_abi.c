#include "eth_abi.h"
#include "eth_rpc.h"  // 添加对eth_rpc.h的引用
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <esp_log.h>
#include <mbedtls/md.h>

static const char *TAG = "ETH_ABI";

// 填充到32字节边界的辅助函数
static void pad_to_32_bytes(uint8_t* buffer, size_t data_len) {
    size_t padding_len = 32 - (data_len % 32);
    if (padding_len < 32) {
        memset(buffer + data_len, 0, padding_len);
    }
}

// 将普通字符串转换为十六进制字符串
static char* string_to_hex(const char* input) {
    if (!input) return NULL;
    
    size_t len = strlen(input);
    char* hex = malloc(len * 2 + 3); // "0x" + (每个字符2位十六进制) + null终止符
    
    if (!hex) return NULL;
    
    strcpy(hex, "0x");
    
    for (size_t i = 0; i < len; i++) {
        sprintf(hex + 2 + i*2, "%02x", (unsigned char)input[i]);
    }
    
    return hex;
}

// 使用以太坊节点的web3_sha3方法计算Keccak256哈希值
esp_err_t abi_encode_function_selector(web3_context_t* context, const char* signature, uint8_t selector[4]) {
    if (!context || !signature || !selector) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 将函数签名转换为十六进制字符串
    char* hex_signature = string_to_hex(signature);
    if (!hex_signature) {
        ESP_LOGE(TAG, "Failed to convert signature to hex");
        return ESP_ERR_NO_MEM;
    }
    
    // 使用web3_sha3 RPC方法计算哈希
    char hash_str[70] = {0}; // 足够存储"0x"前缀+64个十六进制字符+null终止符
    esp_err_t err = eth_get_web3_sha3(context, hex_signature, hash_str, sizeof(hash_str));
    free(hex_signature);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to calculate function selector hash: %s", esp_err_to_name(err));
        return err;
    }
    
    // 将十六进制字符串转换为二进制数据 (跳过"0x"前缀)
    for (int i = 0; i < 4; i++) {
        uint8_t byte = 0;
        sscanf(&hash_str[2 + i*2], "%2hhx", &byte);
        selector[i] = byte;
    }
    
    return ESP_OK;
}

esp_err_t abi_encode_param(const abi_param_t* param, uint8_t* output, size_t output_len, size_t* bytes_written) {
    if (!param || !output || !bytes_written || output_len < 32) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *bytes_written = 0;
    
    // 清零输出缓冲区
    memset(output, 0, 32);
    
    switch (param->type) {
        case ABI_TYPE_UINT:
        case ABI_TYPE_INT:
            {
                // 整数类型 - 右对齐
                size_t bytes = param->size / 8;
                if (bytes > 32) bytes = 32;
                
                // 复制数据到输出缓冲区的右侧
                memcpy(output + (32 - bytes), param->value, bytes);
                *bytes_written = 32;
            }
            break;
            
        case ABI_TYPE_ADDRESS:
            // 地址类型 - 20字节，右对齐
            memcpy(output + 12, param->value, 20);
            *bytes_written = 32;
            break;
            
        case ABI_TYPE_BOOL:
            // 布尔类型 - 1字节，右对齐
            output[31] = (*(bool*)param->value) ? 1 : 0;
            *bytes_written = 32;
            break;
            
        case ABI_TYPE_BYTES:
            if (param->length <= 32) {
                // 定长字节数组，左对齐
                memcpy(output, param->value, param->length);
                *bytes_written = 32;
            } else {
                // 动态字节数组需要特殊处理
                // 这里只实现长度编码
                // 实际数据需要在调用abi_encode_params中处理
                if (output_len < 32) {
                    return ESP_ERR_INVALID_SIZE;
                }
                
                // 编码长度
                uint32_t len = param->length;
                for (int i = 0; i < 32; i++) {
                    output[31-i] = (len >> (i*8)) & 0xFF;
                }
                *bytes_written = 32; // 仅返回长度编码部分的大小
            }
            break;
            
        case ABI_TYPE_STRING:
            // 字符串类型 - 动态类型
            // 这里只实现长度编码
            if (output_len < 32) {
                return ESP_ERR_INVALID_SIZE;
            }
            
            // 编码长度
            uint32_t len = param->length;
            for (int i = 0; i < 32; i++) {
                output[31-i] = (len >> (i*8)) & 0xFF;
            }
            *bytes_written = 32; // 仅返回长度编码部分的大小
            break;
            
        case ABI_TYPE_ARRAY:
            // 数组类型 - 简化处理
            ESP_LOGE(TAG, "Array type not fully implemented");
            return ESP_ERR_NOT_SUPPORTED;
            
        default:
            ESP_LOGE(TAG, "Unknown ABI type");
            return ESP_ERR_INVALID_ARG;
    }
    
    return ESP_OK;
}

esp_err_t abi_encode_params(const abi_param_t* params, size_t param_count, uint8_t* output, size_t output_len, size_t* bytes_written) {
    if (!params || !output || !bytes_written || param_count == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *bytes_written = 0;
    
    // 计算头部长度 (每个参数32字节)
    size_t head_size = param_count * 32;
    
    // 检查缓冲区大小
    if (output_len < head_size) {
        return ESP_ERR_INVALID_SIZE;
    }
    
    // 先处理静态参数
    size_t dynamic_data_offset = head_size;
    size_t current_offset = 0;
    
    // 第一遍: 计算动态数据偏移量
    for (size_t i = 0; i < param_count; i++) {
        const abi_param_t* param = &params[i];
        
        if (param->is_dynamic || 
            (param->type == ABI_TYPE_STRING) || 
            (param->type == ABI_TYPE_BYTES && param->length > 32)) {
            // 动态参数: 在头部保存偏移量
            dynamic_data_offset += (param->type == ABI_TYPE_STRING || param->type == ABI_TYPE_BYTES) ? 
                                   32 + ((param->length + 31) / 32) * 32 : 32;
        }
    }
    
    // 第二遍: 编码参数
    size_t current_dynamic_offset = head_size;
    
    for (size_t i = 0; i < param_count; i++) {
        const abi_param_t* param = &params[i];
        
        if (!param->is_dynamic && 
            !(param->type == ABI_TYPE_STRING) && 
            !(param->type == ABI_TYPE_BYTES && param->length > 32)) {
            // 静态参数: 直接编码
            size_t param_size = 0;
            esp_err_t err = abi_encode_param(param, output + current_offset, output_len - current_offset, &param_size);
            if (err != ESP_OK) {
                return err;
            }
            current_offset += param_size;
        } else {
            // 动态参数: 在头部保存偏移量
            // 编码偏移量 (相对于开始位置)
            for (int j = 0; j < 32; j++) {
                output[current_offset + 31-j] = (current_dynamic_offset >> (j*8)) & 0xFF;
            }
            current_offset += 32;
            
            // 处理动态数据
            if (current_dynamic_offset + 32 > output_len) {
                return ESP_ERR_INVALID_SIZE;
            }
            
            if (param->type == ABI_TYPE_STRING || param->type == ABI_TYPE_BYTES) {
                // 编码长度
                for (int j = 0; j < 32; j++) {
                    output[current_dynamic_offset + 31-j] = (param->length >> (j*8)) & 0xFF;
                }
                current_dynamic_offset += 32;
                
                // 编码数据
                if (current_dynamic_offset + param->length > output_len) {
                    return ESP_ERR_INVALID_SIZE;
                }
                
                memcpy(output + current_dynamic_offset, param->value, param->length);
                size_t padded_len = ((param->length + 31) / 32) * 32;
                
                // 填充到32字节边界
                pad_to_32_bytes(output + current_dynamic_offset, param->length);
                current_dynamic_offset += padded_len;
            }
        }
    }
    
    *bytes_written = current_dynamic_offset > head_size ? current_dynamic_offset : current_offset;
    return ESP_OK;
}

esp_err_t abi_encode_function_call(web3_context_t* context, const char* signature, 
                                const abi_param_t* params, size_t param_count, 
                                uint8_t* output, size_t output_len, size_t* bytes_written) {
    if (!context || !signature || !output || !bytes_written) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Initialize bytes written to zero
    *bytes_written = 0;
    
    // 确保输出缓冲区足够大
    if (output_len < 4) {
        return ESP_ERR_INVALID_SIZE;
    }
    
    // Clear the entire output buffer first
    memset(output, 0, output_len);
    
    // 计算函数选择器 (使用更新后的函数签名)
    uint8_t selector[4] = {0};
    esp_err_t err = abi_encode_function_selector(context, signature, selector);
    if (err != ESP_OK) {
        return err;
    }
    
    // 复制选择器到输出
    memcpy(output, selector, 4);
    
    // 如果没有参数，直接返回
    if (param_count == 0 || !params) {
        *bytes_written = 4;
        return ESP_OK;
    }
    
    // 编码参数
    size_t params_size = 0;
    err = abi_encode_params(params, param_count, output + 4, output_len - 4, &params_size);
    if (err != ESP_OK) {
        return err;
    }
    
    *bytes_written = 4 + params_size;
    return ESP_OK;
}

esp_err_t abi_binary_to_hex(const uint8_t* binary, size_t binary_len, char* hex, size_t hex_len) {
    if (!binary || !hex || hex_len < (binary_len * 2 + 3)) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 添加0x前缀
    hex[0] = '0';
    hex[1] = 'x';
    
    // 转换二进制为十六进制
    for (size_t i = 0; i < binary_len; i++) {
        sprintf(hex + 2 + i*2, "%02x", binary[i]);
    }
    
    return ESP_OK;
}

// 将十六进制字符串转换为二进制数据
esp_err_t abi_hex_to_binary(const char* hex, uint8_t* binary, size_t binary_len, size_t* bytes_written) {
    if (!hex || !binary || !bytes_written) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *bytes_written = 0;
    
    // 跳过0x前缀
    if (strncmp(hex, "0x", 2) == 0) {
        hex += 2;
    }
    
    size_t hex_len = strlen(hex);
    if (hex_len % 2 != 0) {
        ESP_LOGE(TAG, "Invalid hex string length (must be even)");
        return ESP_ERR_INVALID_ARG;
    }
    
    size_t expected_binary_len = hex_len / 2;
    if (binary_len < expected_binary_len) {
        ESP_LOGE(TAG, "Binary buffer too small: need %d, have %d", expected_binary_len, binary_len);
        return ESP_ERR_INVALID_SIZE;
    }
    
    for (size_t i = 0; i < expected_binary_len; i++) {
        char byte_str[3] = {hex[i*2], hex[i*2+1], 0};
        unsigned int byte_val;
        if (sscanf(byte_str, "%02x", &byte_val) != 1) {
            ESP_LOGE(TAG, "Invalid hex characters at position %d: %s", i*2, byte_str);
            return ESP_ERR_INVALID_ARG;
        }
        binary[i] = (uint8_t)byte_val;
    }
    
    *bytes_written = expected_binary_len;
    return ESP_OK;
}

// 从ABI编码数据中提取32字节的整数值
static uint64_t abi_extract_uint64(const uint8_t* data, size_t offset, size_t data_len) {
    // 添加边界检查
    if (!data || offset + 32 > data_len) {
        ESP_LOGE(TAG, "Invalid data pointer or offset out of bounds: offset=%d, data_len=%d", 
                 offset, data_len);
        return 0;
    }
    
    uint64_t value = 0;
    
    // 提取最后8字节 (小端序)
    for (int i = 0; i < 8; i++) {
        if (offset + 32 - 8 + i < data_len) { // 安全检查
            value = (value << 8) | data[offset + 32 - 8 + i];
        }
    }
    
    return value;
}

// 从ABI编码数据中解码字符串
esp_err_t abi_decode_string(const uint8_t* data, size_t data_len, size_t offset, 
                           char* output, size_t output_len) {
    if (!data || !output || output_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 初始化输出缓冲区
    output[0] = '\0';
    
    // 安全检查
    if (offset >= data_len) {
        ESP_LOGE(TAG, "String offset out of bounds: %d >= %d", offset, data_len);
        return ESP_ERR_INVALID_SIZE;
    }
    
    // 字符串的位置偏移量
    uint64_t string_pos = abi_extract_uint64(data, offset, data_len);
    ESP_LOGI(TAG, "String position offset: %llu", string_pos);
    
    // 确保偏移量在合理范围内
    if (string_pos >= data_len) {
        ESP_LOGE(TAG, "String offset out of bounds: %llu >= %d", string_pos, data_len);
        return ESP_ERR_INVALID_SIZE;
    }
    
    // 字符串长度
    uint64_t string_len = abi_extract_uint64(data, string_pos, data_len);
    ESP_LOGI(TAG, "String length: %llu", string_len);
    
    // 确保字符串长度在合理范围内 
    if (string_len == 0) {
        // 空字符串
        output[0] = '\0';
        return ESP_OK;
    }
    
    if (string_pos + 32 + string_len > data_len) {
        ESP_LOGE(TAG, "String data out of bounds: %llu + 32 + %llu > %d", 
                 string_pos, string_len, data_len);
        return ESP_ERR_INVALID_SIZE;
    }
    
    // 确保字符串长度合理 (防止异常大的值)
    if (string_len > 10240) { // 10KB 上限
        ESP_LOGE(TAG, "String length too large: %llu", string_len);
        return ESP_ERR_INVALID_SIZE;
    }
    
    // 确保输出缓冲区足够大
    if (string_len + 1 > output_len) {
        ESP_LOGW(TAG, "Output buffer too small: %llu + 1 > %d, truncating", 
                 string_len, output_len);
        string_len = output_len - 1;
    }
    
    // 复制字符串数据
    memcpy(output, data + string_pos + 32, string_len);
    output[string_len] = '\0'; // 添加字符串结束符
    
    return ESP_OK;
}

// 从ABI编码数据中解码多个返回值
esp_err_t abi_decode_returns(const uint8_t* data, size_t data_len, 
                            abi_decoded_value_t* outputs, size_t output_count, 
                            size_t* decoded_count) {
    if (!data || !outputs || !decoded_count || data_len < 32) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *decoded_count = 0;
    
    // 记录数据长度和内容前几个字节用于调试
    ESP_LOGI(TAG, "Decoding ABI data, length: %d bytes", data_len);
    if (data_len >= 32) {
        ESP_LOGI(TAG, "First 32 bytes: %02x %02x %02x %02x ...", 
                 data[0], data[1], data[2], data[3]);
    }
    
    // 处理每个返回值
    for (size_t i = 0; i < output_count; i++) {
        // 计算当前值的头部位置
        size_t head_pos = i * 32;
        
        // 确保头部在数据范围内
        if (head_pos + 32 > data_len) {
            ESP_LOGE(TAG, "Return value head out of bounds: %d + 32 > %d", 
                     head_pos, data_len);
            break; // 继续处理已解码的值，而不是返回错误
        }
        
        // 初始化输出值
        outputs[i].type = ABI_TYPE_STRING; // 默认假设是字符串 (可以根据需要拓展)
        outputs[i].is_dynamic = true;
        
        // 分配内存并解码字符串
        size_t str_buffer_size = 2048; // 增加缓冲区大小，确保足够空间
        outputs[i].value.string = malloc(str_buffer_size);
        
        if (!outputs[i].value.string) {
            ESP_LOGE(TAG, "Failed to allocate memory for string");
            break; // 继续处理已解码的值
        }
        
        // 初始化字符串缓冲区
        memset(outputs[i].value.string, 0, str_buffer_size);
        
        // 解码字符串
        esp_err_t err = abi_decode_string(data, data_len, head_pos, 
                                       outputs[i].value.string, str_buffer_size);
        
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to decode string at index %d: %s", 
                     i, esp_err_to_name(err));
            // 不释放内存，让上层函数处理
            outputs[i].value.string[0] = '\0'; // 设为空字符串
        }
        
        outputs[i].length = strlen(outputs[i].value.string);
        (*decoded_count)++;
    }
    
    return ESP_OK; // 即使部分解码失败，也返回成功，让上层函数处理已解码的值
}

// 释放解码后的值占用的内存
void abi_free_decoded_value(abi_decoded_value_t* value) {
    if (!value) return;
    
    if (value->is_dynamic) {
        if (value->type == ABI_TYPE_STRING || value->type == ABI_TYPE_BYTES) {
            if (value->value.string) {
                free(value->value.string);
                value->value.string = NULL;
            }
        }
    }
}
