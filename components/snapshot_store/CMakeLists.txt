idf_component_register(SRCS "src/snapshot_store.cpp" INCLUDE_DIRS "include" REQUIRES measurement_core)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
