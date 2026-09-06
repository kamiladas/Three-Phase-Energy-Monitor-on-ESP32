idf_component_register(
    SRCS "src/web_ui.cpp"
    INCLUDE_DIRS "include"
    EMBED_FILES "assets/dashboard.html" "assets/statistics.html" "assets/statistics_pro.html"
    REQUIRES esp_http_server snapshot_store history_store sd_store)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
