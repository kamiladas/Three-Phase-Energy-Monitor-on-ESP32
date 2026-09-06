idf_component_register(SRCS "src/time_service.cpp" INCLUDE_DIRS "include" REQUIRES esp_netif nvs_flash)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
