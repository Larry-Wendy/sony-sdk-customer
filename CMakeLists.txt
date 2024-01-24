cmake_minimum_required(VERSION 3.10)
project(RemoteCli LANGUAGES CXX)

### Append project cmake script dir ###
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

### Enumerate project files ###
include(enum_cli_hdr)
include(enum_cli_src)
include(enum_crsdk_hdr)

### VCPKG packages
include(C:/vcpkg/scripts/buildsystems/vcpkg.cmake)
find_package(Crow CONFIG REQUIRED)
find_package(OpenCV CONFIG REQUIRED)

### Define output target ###
set(remotecli "${PROJECT_NAME}")
add_executable(${remotecli}
    ${cli_hdrs}
    ${cli_srcs}
    ${crsdk_hdrs}
 "app/CRSDK/IDeviceCallback.h"  "app/CameraDevice.cpp")

set_target_properties(${remotecli} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    BUILD_RPATH "$ORIGIN"
    INSTALL_RPATH "$ORIGIN"
)

## Specify char is signed-char to fix mismatch with Raspbian
target_compile_options(${remotecli}
    PRIVATE
        -fsigned-char
)

target_include_directories(${remotecli}
    PRIVATE
        ${crsdk_hdr_dir} # defined in enum script
)

target_link_libraries(${PROJECT_NAME} PRIVATE Crow::Crow)
target_link_libraries(${PROJECT_NAME} PRIVATE ${OpenCV_LIBS})

### Configure external library directories ###
set(ldir ${CMAKE_CURRENT_SOURCE_DIR}/external)
set(cr_ldir ${ldir}/crsdk)

### Link CRSDK library
find_library(camera_remote Cr_Core HINTS ${cr_ldir})
target_link_libraries(${remotecli}
    PRIVATE
        ${camera_remote}
)

### Windows specific configuration ###
target_compile_definitions(${remotecli} PRIVATE UNICODE _UNICODE)

## Copy required library binaries
add_custom_command(TARGET ${remotecli} PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${cr_ldir} $<TARGET_FILE_DIR:${remotecli}>
)


## Install application
## '.' means, install to the root directory of CMAKE_INSTALL_PREFIX
install(TARGETS ${remotecli} DESTINATION .)
install(DIRECTORY ${cr_ldir}/ DESTINATION .)
