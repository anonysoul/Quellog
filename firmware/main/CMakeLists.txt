set(SOURCES
    "main.cc"
    "application.cc"
    "settings.cc"
    "boards/common/board.cc"
    "boards/zectrix-s3-epaper-4.2/zectrix-s3-epaper-4.2.cc"
    "display/bitmap_font.cc"
    "display/display.cc"
    "display/ui_page_registry.cc"
    "display/pages/overview_page.cc"
    "display/pages/recent_records_page.cc"
    "display/pages/settings_page.cc"
    "display/pages/stats_page.cc"
)

idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS
        "."
        "boards/common"
        "display"
        "display/pages"
    REQUIRES
        driver
        esp_app_format
        esp_partition
        esp_timer
        nvs_flash
        quellog_wifi
        spi_flash
)

target_compile_definitions(
    ${COMPONENT_LIB}
    PRIVATE
    BOARD_NAME=\"zectrix-s3-epaper-4.2\"
)
