cmake_minimum_required(VERSION 3.16)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" I2P_VERSION_RAW)
string(STRIP "${I2P_VERSION_RAW}" I2P_VERSION)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
project(i2ptorrents-gui VERSION ${I2P_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Test)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

set(I2P_CORE_SOURCES
    cpp/app_constants.cpp
    cpp/i18n.cpp
    cpp/theme.cpp
    cpp/config.cpp
    cpp/models.cpp
    cpp/piece_map.cpp
    cpp/rpc.cpp
    cpp/resources.cpp
)

set(I2P_CHROME_SOURCES
    native/qt_chrome.cpp
)

if(APPLE)
    list(APPEND I2P_CHROME_SOURCES native/macos_vibrancy.mm)
endif()

qt_add_executable(i2ptorrents-gui
    cpp/main.cpp
    cpp/chrome.cpp
    cpp/mainwindow.cpp
    ${I2P_CORE_SOURCES}
    ${I2P_CHROME_SOURCES}
    assets/resources.qrc
)

target_include_directories(i2ptorrents-gui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/native
)

target_link_libraries(i2ptorrents-gui PRIVATE
    Qt6::Widgets
    Qt6::Network
)

target_compile_definitions(i2ptorrents-gui PRIVATE I2P_APP_VERSION="${I2P_VERSION}")

if(APPLE)
    target_link_libraries(i2ptorrents-gui PRIVATE
        "-framework AppKit"
        "-framework QuartzCore"
    )
    target_compile_options(i2ptorrents-gui PRIVATE -fobjc-arc)
endif()

if(WIN32)
    target_link_libraries(i2ptorrents-gui PRIVATE dwmapi user32)
endif()

set_target_properties(i2ptorrents-gui PROPERTIES
    OUTPUT_NAME I2PTorrents
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE FALSE
)

enable_testing()

qt_add_executable(i2ptorrents-tests
    tests/test_main.cpp
    tests/test_rpc.cpp
    tests/test_models.cpp
    tests/test_settings.cpp
    tests/test_i18n.cpp
    tests/test_theme.cpp
    tests/test_piece_map.cpp
    ${I2P_CORE_SOURCES}
    assets/resources.qrc
)

target_include_directories(i2ptorrents-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/cpp
)

target_link_libraries(i2ptorrents-tests PRIVATE
    Qt6::Widgets
    Qt6::Network
    Qt6::Test
)

target_compile_definitions(i2ptorrents-tests PRIVATE I2P_APP_VERSION="${I2P_VERSION}")

set_target_properties(i2ptorrents-tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_test(NAME core COMMAND i2ptorrents-tests WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
