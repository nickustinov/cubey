# WHOLE_ARCHIVE: the core calls the platform layer (network, audio) but links after main.
idf_component_register(SRCS main.cpp network.cpp audio.cpp power.cpp update.cpp board_amoled_2_16.cpp INCLUDE_DIRS .
    REQUIRES core esp_timer esp_wifi esp_netif nvs_flash esp_driver_i2c esp_pm mbedtls esp_http_client app_update esp_app_format
    WHOLE_ARCHIVE)
