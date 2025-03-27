idf_component_register(
    SRCS 
        "main.c"
        "ethereum-lib/web3.c"
        "ethereum-lib/eth_rpc.c"
        "ethereum-lib/net_test.c"
        "ethereum-lib/eth_abi.c"
    INCLUDE_DIRS "."
    REQUIRES esp_http_client json nvs_flash esp_wifi esp-tls lwip 
)

# 增加组件特定堆大小
component_compile_options(-Wno-error=format= -Wno-format)

target_link_libraries(${COMPONENT_LIB} m)  # 链接math库