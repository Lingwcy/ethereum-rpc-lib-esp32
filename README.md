# ESP32 Ethereum Client Library

![ESP32](https://img.shields.io/badge/ESP32-Enabled-blue)
![Ethereum](https://img.shields.io/badge/Ethereum-Compatible-brightgreen)
![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.x-orange)

<p align="center">
  <svg fill="currentColor" width="200" height="310" viewBox="0 0 115 182" xmlns="http://www.w3.org/2000/svg">
    <path d="M57.5054 181V135.84L1.64064 103.171L57.5054 181Z" fill="#F0CDC2" stroke="#1616B4" stroke-linejoin="round"></path>
    <path d="M57.6906 181V135.84L113.555 103.171L57.6906 181Z" fill="#C9B3F5" stroke="#1616B4" stroke-linejoin="round"></path>
    <path d="M57.5055 124.615V66.9786L1 92.2811L57.5055 124.615Z" fill="#88AAF1" stroke="#1616B4" stroke-linejoin="round"></path>
    <path d="M57.6903 124.615V66.9786L114.196 92.2811L57.6903 124.615Z" fill="#C9B3F5" stroke="#1616B4" stroke-linejoin="round"></path>
    <path d="M1.00006 92.2811L57.5054 1V66.9786L1.00006 92.2811Z" fill="#F0CDC2" stroke="#1616B4" stroke-linejoin="round"></path>
    <path d="M114.196 92.2811L57.6906 1V66.9786L114.196 92.2811Z" fill="#B8FAF6" stroke="#1616B4" stroke-linejoin="round"></path>
  </svg>
  <img src="https://www.espressif.com/sites/all/themes/espressif/logo-black.svg" width="200" alt="Espressif Logo"/>
</p>

## 🚀 项目概述

ESP32 Ethereum Client Library 是一个强大的轻量级库，让您的 ESP32 设备能够直接与以太坊区块链网络交互。通过简单的 API 接口，您可以执行各种以太坊操作，包括查询账户余额、发送交易和与智能合约交互。

无论您是构建物联网区块链应用、加密货币硬件钱包，还是区块链数据监控设备，本库都能帮助您快速实现目标。

## ✨ 特性

- 🔗 完整的以太坊 JSON-RPC API 支持
- 💰 账户余额查询与管理
- 📤 交易签名与发送
- 🤝 智能合约交互（ABI 编码/解码）
- 🔍 区块链数据查询
- 🔐 安全交易签名
- 🌐 支持主网、测试网和私有网络
- 📈 占用资源少，性能高
- 🔄 异步操作支持
- 🛡️ 内存安全和稳定性优化
- 📦 精简的代码设计，适合资源受限设备

## 📋 支持的以太坊 RPC 方法

### 网络相关
- ✓ `web3_clientVersion` - 获取以太坊客户端版本
- ✓ `net_version` - 获取网络 ID
- ✓ `net_listening` - 查询节点是否正在监听网络连接
- ✓ `net_peerCount` - 获取连接的对等节点数量

### 区块链状态
- ✓ `eth_blockNumber` - 获取最新区块号
- ✓ `eth_getBalance` - 查询账户余额
- ✓ `eth_gasPrice` - 获取当前 gas 价格
- ✓ `eth_getCode` - 获取合约代码
- ✓ `eth_protocolVersion` - 获取以太坊协议版本
- ✓ `eth_syncing` - 获取同步状态

### 交易相关
- ✓ `eth_getTransactionCount` - 获取账户交易数量（nonce）
- ✓ `eth_sign` - 签名数据
- ✓ `eth_signTransaction` - 签名交易
- ✓ `eth_sendRawTransaction` - 发送已签名的交易
- ✓ `eth_getTransactionReceipt` - 获取交易收据
- ✓ `eth_call` - 调用智能合约（不改变状态）

### 智能合约相关
- ✓ ABI编码支持（地址、整数、布尔值、字节数组、字符串等类型）
- ✓ ABI解码支持（解析合约返回值）
- ✓ 函数选择器计算 (通过web3_sha3)
- ✓ 动态类型处理（字符串、变长字节数组等）

### 工具方法
- ✓ `web3_sha3` - 计算 Keccak-256 哈希
- ✓ 十六进制/二进制转换工具

## 🛠️ 硬件要求

- ESP32/ESP32-S2/ESP32-S3/ESP32-C3 开发板
- 至少 4MB 闪存
- 建议使用带有 PSRAM 的设备以获得更好性能
- 稳定的网络连接（WiFi 或以太网）

## 📦 软件依赖

- ESP-IDF v4.4 或更高版本（推荐 v5.x）
- FreeRTOS
- cJSON 库 (已包含)
- mbedTLS (ESP-IDF 自带)

## 🔧 安装步骤

1. **克隆仓库**

```bash
git clone git@github.com:Lingwcy/ethereum-rpc-lib-esp32.git
cd ethereum-rpc-lib
```

2. **设置 ESP-IDF 环境**

```bash
. $IDF_PATH/export.sh  # Linux/macOS
```

3. **配置项目**

```bash
idf.py menuconfig
# 配置 WiFi 凭据和以太坊节点 URL
```

4. **编译和烧录**

```bash
idf.py build
idf.py -p /dev/ttyUSB0 flash  # 替换为您的串口
```

5. **监控输出**

```bash
idf.py -p /dev/ttyUSB0 monitor
```

## 📝 使用示例

### 初始化库并连接到以太坊节点

```c
#include "ethereum-lib/web3.h"
#include "ethereum-lib/eth_rpc.h"

// 初始化 Web3 上下文
web3_context_t context;
const char* eth_url = "http://192.168.1.112:8545";  // 以太坊节点 URL
esp_err_t err = web3_init(&context, eth_url);
if (err != ESP_OK) {
    ESP_LOGE(TAG, "初始化 web3 失败: %s", esp_err_to_name(err));
    return;
}
```

### 查询账户余额

```c
// 查询以太坊地址余额
const char* address = "0xf39Fd6e51aad88F6F4ce6aB8827279cffFb92266";
char balance[128] = {0};
err = eth_get_balance(&context, address, balance, sizeof(balance));
if (err == ESP_OK) {
    ESP_LOGI(TAG, "账户余额: %s", balance);
}
```

### 发送以太币交易

```c
// 准备交易参数
const char* from_address = "0xf39Fd6e51aad88F6F4ce6aB8827279cffFb92266";
const char* to_address = "0x70997970C51812dc3A010C7d01b50e0d17dc79C8";
const char* value = "0xDE0B6B3A7640000";  // 1 ETH (1 ETH = 10^18 Wei)
const char* gas = "0x5208";  // 21000 gas - 标准转账所需的 gas 量

// 获取账户 nonce
char nonce[32] = {0};
eth_getTransactionCount(&context, from_address, nonce, sizeof(nonce));

// 签名交易
char signed_tx[1024] = {0};
err = eth_signTransaction(
    &context,
    from_address,
    to_address,
    gas,
    "0x1",  // gas price
    value,
    "0x",   // 空数据
    nonce,
    signed_tx,
    sizeof(signed_tx)
);

// 发送交易
char tx_hash[128] = {0};
err = eth_sendRawTransaction(&context, signed_tx, tx_hash, sizeof(tx_hash));
ESP_LOGI(TAG, "交易已发送，交易哈希: %s", tx_hash);
```

### 与智能合约交互（ABI编码）

```c
// 准备ERC20 transfer调用参数
char to_address[42] = "0x70997970C51812dc3A010C7d01b50e0d17dc79C8";
uint8_t to_addr_bytes[20];

// 移除0x前缀并转换为二进制
for (int i = 0; i < 20; i++) {
    sscanf(&to_address[2+i*2], "%2hhx", &to_addr_bytes[i]);
}

// 设置转账金额 (1.0 ETH)
const uint8_t one_eth[32] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x0d, 0xe0, 0xb6, 0xb3
};

// 定义参数
abi_param_t params[2] = {
    ABI_ADDRESS(to_addr_bytes),      // 接收地址
    ABI_UINT(256, one_eth)           // 金额
};

// 编码函数调用
uint8_t encoded[512] = {0};
size_t encoded_len = 0;
esp_err_t err = abi_encode_function_call(
    &context,
    "transfer(address,uint256)",
    params,
    2,
    encoded,
    sizeof(encoded),
    &encoded_len
);

// 转换为十六进制字符串
char hex_data[1024] = {0};
abi_binary_to_hex(encoded, encoded_len, hex_data, sizeof(hex_data));
ESP_LOGI(TAG, "编码后的函数调用数据: %s", hex_data);
```

### 调用智能合约函数（eth_call）

```c
// 合约地址和函数调用数据
const char* contract_address = "0x8aCd85898458400f7Db866d53FCFF6f0D49741FF";
const char* data = "0xae7fde52"; // getAuthorInformation()的函数选择器

// 调用合约函数
char result[4096] = {0};
err = eth_call(&context, contract_address, data, "latest", result, sizeof(result));

// 解析并处理响应
cJSON *json = cJSON_Parse(result);
if (json) {
    cJSON *result_obj = cJSON_GetObjectItem(json, "result");
    if (result_obj && cJSON_IsString(result_obj)) {
        ESP_LOGI(TAG, "合约返回数据: %s", result_obj->valuestring);
    }
    cJSON_Delete(json);
}
```

### 解码合约返回值

```c
// 将十六进制编码结果转换为二进制数据
uint8_t binary_data[4096] = {0};
size_t binary_len = 0;
abi_hex_to_binary(encoded_result, binary_data, sizeof(binary_data), &binary_len);

// 解码三个字符串返回值
abi_decoded_value_t decoded_values[3] = {0};
size_t decoded_count = 0;

err = abi_decode_returns(binary_data, binary_len, decoded_values, 3, &decoded_count);

// 显示解码后的数据
for (size_t i = 0; i < decoded_count; i++) {
    ESP_LOGI(TAG, "返回值 %d: %s", i+1, decoded_values[i].value.string);
}

// 释放分配的内存
for (size_t i = 0; i < decoded_count; i++) {
    abi_free_decoded_value(&decoded_values[i]);
}
```

## 🔍 故障排除

### 连接问题

- 确保以太坊节点正在运行且可访问
- 验证 ESP32 网络连接是否正常
- 检查防火墙设置是否允许连接
- 增加超时值处理网络延迟

### 交易失败

- 确认账户有足够余额（包括 gas 费用）
- 检查 nonce 是否正确
- 验证 gas 价格是否足够
- 确保交易数据格式正确

### 内存不足/稳定性问题

- 增加任务堆栈大小：`xTaskCreate(&task, "task_name", 16384, NULL, 5, NULL);`
- 使用更大的缓冲区接收响应数据
- 在连续的 RPC 调用之间添加延迟：`vTaskDelay(pdMS_TO_TICKS(500));`
- 如果可能，启用 ESP32 的 PSRAM
- 增加以下设置到 sdkconfig.defaults:
  ```
  CONFIG_ESP_MAIN_TASK_STACK_SIZE=7168
  CONFIG_FREERTOS_IDLE_TASK_STACKSIZE=2048
  CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE=4096
  ```

### ABI 编码/解码问题

- 确保参数类型和大小正确
- 对于动态类型，确保正确设置长度
- 验证函数签名格式正确
- 检查解码时提供了足够大的缓冲区

## 📢 注意事项

- 该库专为物联网设备设计，资源占用低但功能可能不如桌面级客户端完整
- 建议在测试网络上充分测试后再在主网使用
- 对于高价值交易，建议实施额外的安全措施
- 密切监控内存使用情况，特别是在处理大型响应时

## 🚀 最新更新

- ✅ 添加了完整的 ABI 解码功能
- ✅ 改进了内存管理和错误处理
- ✅ 优化了缓冲区大小以处理更大的响应
- ✅ 增加了任务堆栈大小以提高稳定性
- ✅ 添加了智能合约调用示例
- ✅ 改进了文档和使用示例

## 🤝 贡献

我们欢迎社区贡献！如果您想帮助改进这个库，请遵循以下步骤：

1. Fork 项目
2. 创建您的特性分支 (`git checkout -b feature/amazing-feature`)
3. 提交您的更改 (`git commit -m 'Add some amazing feature'`)
4. 推送到分支 (`git push origin feature/amazing-feature`)
5. 打开一个 Pull Request

## 📜 许可证

该项目采用 MIT 许可证 - 详情请参阅 [LICENSE](LICENSE) 文件。

## 📧 联系方式

如有任何问题或建议，请通过 [issues](https://github.com/yourusername/esp32-ethereum-lib/issues) 或 lingwcyovo@gmail.com 联系我们。

---

<p align="center">
  <img src="https://img.shields.io/badge/Made%20with%20%E2%9D%A4%EF%B8%8F%20for-ESP32%20and%20Ethereum-blue" alt="Made with love for ESP32 and Ethereum"/>
</p>
