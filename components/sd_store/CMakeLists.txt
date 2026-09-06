idf_component_register(SRCS "src/sd_store.cpp" INCLUDE_DIRS "include"
    EMBED_FILES "fixtures/initial_history.csv"
    REQUIRES fatfs esp_driver_sdmmc sdmmc measurement_core time_service freertos)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
