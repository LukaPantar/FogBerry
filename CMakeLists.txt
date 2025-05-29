
# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

set(FREERTOS_KERNEL_PATH "../FreeRTOS-KernelV11.2.0")

cmake_minimum_required(VERSION 3.13)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # compile_commands

# Pull in SDK (must be before project)
include(pico_sdk_import.cmake)

# Pull in FreeRTOS
include(FreeRTOS_Kernel_import.cmake)

project(fogberry C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(fogberry
        main.c
        mfrc522.c
        )


target_include_directories(fogberry PRIVATE
        ${CMAKE_CURRENT_LIST_DIR})

target_compile_definitions(fogberry PRIVATE
        PICO_STDIO_STACK_BUFFER_SIZE=64 # use a small printf on stack buffer
)

set(WIFI_SSID $ENV{WIFI_SSID})
set(WIFI_PASSWORD $ENV{WIFI_PASSWORD})
set(MQTT_SERVER $ENV{MQTT_SERVER})
set(MQTT_PORT $ENV{MQTT_PORT})

target_compile_definitions(fogberry PRIVATE
        mainWIFI_SSID=\"${WIFI_SSID}\"
        mainWIFI_PASSWORD=\"${WIFI_PASSWORD}\"
        mainMQTT_SERVER=\"${MQTT_SERVER}\"
        mainMQTT_PORT=\"${MQTT_PORT}\"
)

target_compile_options(fogberry PUBLIC
        ### Gnu/Clang C Options
        $<$<COMPILE_LANG_AND_ID:C,GNU>:-fdiagnostics-color=always>
        $<$<COMPILE_LANG_AND_ID:C,Clang>:-fcolor-diagnostics>

        $<$<COMPILE_LANG_AND_ID:C,Clang,GNU>:-Wall>
        $<$<COMPILE_LANG_AND_ID:C,Clang,GNU>:-Wextra>
        $<$<COMPILE_LANG_AND_ID:C,Clang>:-Weverything>
        )

target_link_libraries(fogberry 
    pico_stdlib 
    FreeRTOS-Kernel 
    FreeRTOS-Kernel-Heap4 
    hardware_adc
    hardware_gpio
    hardware_spi
    hardware_irq
    pico_cyw43_arch_lwip_threadsafe_background
#     pico_cyw43_arch_lwip_sys_freertos
#     pico_lwip_arch
    pico_lwip_mqtt
#     pico_lwip_iperf
#     pico_lwip_freertos
)

# Generates multiple executables and output files
pico_add_extra_outputs(fogberry)

# Use USB uart
pico_enable_stdio_usb(fogberry 0)
pico_enable_stdio_uart(fogberry 1)
