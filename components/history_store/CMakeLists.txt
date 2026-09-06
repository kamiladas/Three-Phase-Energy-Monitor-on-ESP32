idf_component_register(SRCS "src/history_store.cpp" INCLUDE_DIRS "include" REQUIRES measurement_core freertos)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
