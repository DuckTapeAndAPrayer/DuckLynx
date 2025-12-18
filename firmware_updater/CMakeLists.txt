cmake_minimum_required(VERSION 3.10)
project(firmwareUpdater)
enable_language(C)

#Settings
set(CMAKE_C_STANDARD 23)
set(CMAKE_BUILD_TYPE debug)
# set(CMAKE_BUILD_TYPE release)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -masm=intel -Wall -fPIE -fPIC -Wno-comment -Wall -Wpedantic -Werror=return-type")

# Define a variable containing a list of source files for the project
set(SRC_FILES
        src/main.c)

# Define the build target for the executable
add_executable(${PROJECT_NAME}
        ${SRC_FILES})

target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_SOURCE_DIR}/src")

target_link_libraries(${PROJECT_NAME} ftdi1)
