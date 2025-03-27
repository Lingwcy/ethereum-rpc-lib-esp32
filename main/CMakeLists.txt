idf_component_register(
    SRCS 
        "main.c"
        "ethereum-lib/web3.c"
        "ethereum-lib/eth_rpc.c"
        "ethereum-lib/net_test.c"
    INCLUDE_DIRS "."
    REQUIRES esp_http_client json nvs_flash esp_wifi esp-tls lwip 
)
target_link_libraries(${COMPONENT_LIB} m)  # 链接math库