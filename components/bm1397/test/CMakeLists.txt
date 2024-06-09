idf_component_register(
    SRCS 
        "test_bm1366.c"
        "../../../main/adc.c"
        "../../../main/nvs_config.c"
        "../../../main/EMC2101.c"
        "../../../main/DS4432U.c"
    INCLUDE_DIRS 
        "."
        "../include"
        "../../../main"
        "../../../main/tasks"

    REQUIRES unity cmock stratum bm1397 nvs_flash esp_adc esp_event connect
)

# Include the header files from "main" directory
target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../main")

# Include the header files from "main/tasks" directory
target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../main/tasks")