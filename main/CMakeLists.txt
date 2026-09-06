idf_component_register(
    SRCS "app_main.cpp"
    REQUIRES measurement_core sample_source sample_source_sim sample_source_adc
             snapshot_store history_store sd_store web_ui time_service nvs_flash esp_netif esp_event protocol_examples_common)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
