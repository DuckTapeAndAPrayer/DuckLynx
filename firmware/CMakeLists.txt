cmake_minimum_required(VERSION 3.10)
set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")
set(CMAKE_TOOLCHAIN_FILE armnative.cmake)

project(DuckLynxFW LANGUAGES C)
set(CMAKE_C_STANDARD 23)
# set(CMAKE_BUILD_TYPE Release)
set(CMAKE_BUILD_TYPE MinSizeRel)
# set(CMAKE_BUILD_TYPE Debug)

if(${CMAKE_BUILD_TYPE} STREQUAL Debug)
    set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -D DEBUG")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# FIX: Make DuckLynx Firmware build on platforms other than arch linux
set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -isystem /usr/lib/gcc/arm-none-eabi/14.2.0/include -isystem /usr/lib/gcc/arm-none-eabi/14.2.0/include-fixed -isystem /usr/arm-none-eabi/include/ -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -ffunction-sections -fdata-sections -nostartfiles -Wall -Wpedantic -Werror=return-type")

# Define a variable containing a list of source files for the project
set(SRC_FILES
        src/main.c
        src/startup.c
        src/debugUART.c
        src/logo.c
        src/other.c
        
        src/hardware/adc.c
        src/hardware/dma.c
        src/hardware/eeprom.c
        src/hardware/encoder.c
        src/hardware/gpio.c
        src/hardware/led.c
        src/hardware/motor.c
        src/hardware/timer.c
        src/hardware/uart.c
        src/hardware/sysctl.c
        
        src/rhsp/rhsp.c
        src/rhsp/rhspUART.c        
)

# Define the build target for the executable
add_executable(${PROJECT_NAME}
        ${SRC_FILES})

# set_property(TARGET ${CMAKE_PROJECT_NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE "-T" "${CMAKE_SOURCE_DIR}/src/linkerconfig.ld" "-Xlinker" "--gc-sections")
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/src")

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        #COMMAND avr-objcopy -j .text -j .data -O ihex ${PROJECT_NAME} ${PROJECT_NAME}.hex && avrdude -p m2560 -c stk500v2 -P /dev/ttyACM0 -b 115200 -F -D -U flash:w:${PROJECT_NAME}.hex && exit 0
        COMMAND arm-none-eabi-objcopy -O binary ${PROJECT_NAME} ${PROJECT_NAME}.bin && echo "uploading doesnt work yet"
        COMMENT "Uploading ..."
)
