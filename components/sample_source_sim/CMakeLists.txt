idf_component_register(
    SRCS "src/sample_source_sim.cpp"
    INCLUDE_DIRS "include"
    REQUIRES sample_source measurement_core)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
