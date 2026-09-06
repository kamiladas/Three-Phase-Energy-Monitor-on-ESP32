idf_component_register(
    SRCS "src/sample_source_adc.cpp"
    INCLUDE_DIRS "include"
    REQUIRES sample_source board_config esp_adc esp_timer)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
