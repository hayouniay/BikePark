cmake_minimum_required(VERSION 3.25)

project(BikeParking VERSION 2.4.0 LANGUAGES CXX)

# C++26 requires GCC 14+ or Clang 18+. If CMake rejects the value 26,
# install a newer CMake (for example: pipx install cmake).
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # consumed by clangd and clang-tidy
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Widgets Sql Test)
qt_standard_project_setup()

set(BIKEPARKING_WARNINGS $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Wshadow>)

# ------------------------------------------------------------------ options
# Build the application (set OFF for a tests-only build).
option(BIKEPARKING_BUILD_APP "Build the Bike Parking application" ON)
# Build the repository tests.
option(BIKEPARKING_BUILD_TESTS "Build the repository tests" ON)

# Name of the dist/ folder the install goes to: debug, release or test.
# Defaults to the lower-cased CMAKE_BUILD_TYPE, so `cmake --install build/release` gives dist/release.
set(BIKEPARKING_DIST_NAME "" CACHE STRING "dist/ folder for the install: debug, release or test")
if(BIKEPARKING_DIST_NAME STREQUAL "")
    string(TOLOWER "${CMAKE_BUILD_TYPE}" _default_dist_name)
    set(BIKEPARKING_DIST_NAME "${_default_dist_name}" CACHE STRING
        "dist/ folder for the install: debug, release or test" FORCE)
endif()
if(NOT BIKEPARKING_DIST_NAME MATCHES "^(debug|release|test)$")
    message(FATAL_ERROR "BIKEPARKING_DIST_NAME must be debug, release or test (got '${BIKEPARKING_DIST_NAME}'). "
                        "Pass -DBIKEPARKING_DIST_NAME=<name> or set CMAKE_BUILD_TYPE.")
endif()

# Without an explicit prefix, install into dist/<BIKEPARKING_DIST_NAME>.
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${PROJECT_SOURCE_DIR}/dist/${BIKEPARKING_DIST_NAME}" CACHE PATH
        "Install location" FORCE)
endif()

# ------------------------------------------------------------------ application
if(BIKEPARKING_BUILD_APP)
    qt_add_executable(BikeParking
        src/main.cpp
        src/core/Account.h
        src/core/Slot.h
        src/core/ParkingRepository.h
        src/core/ParkingRepository.cpp
        src/ui/SlotListModel.h
        src/ui/SlotListModel.cpp
        src/ui/SlotDelegate.h
        src/ui/SlotDelegate.cpp
        src/ui/Icons.h
        src/ui/Icons.cpp
        src/ui/Theme.h
        src/ui/Theme.cpp
        src/ui/AccountDialogs.h
        src/ui/AccountDialogs.cpp
        src/ui/MainWindow.h
        src/ui/MainWindow.cpp
    )

    qt_add_resources(BikeParking app_resources resources/resources.qrc)

    target_include_directories(BikeParking PRIVATE src)
    target_compile_definitions(BikeParking PRIVATE BIKEPARKING_VERSION="${PROJECT_VERSION}")
    target_link_libraries(BikeParking PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Sql)
    target_compile_options(BikeParking PRIVATE ${BIKEPARKING_WARNINGS})

    install(TARGETS BikeParking
        RUNTIME DESTINATION bin
        COMPONENT application)
endif()

# ------------------------------------------------------------------ tests
if(BIKEPARKING_BUILD_TESTS)
    enable_testing()

    qt_add_executable(BikeParkingTests
        tests/tst_repository.cpp
        src/core/Account.h
        src/core/Slot.h
        src/core/ParkingRepository.h
        src/core/ParkingRepository.cpp
    )
    target_include_directories(BikeParkingTests PRIVATE src)
    target_link_libraries(BikeParkingTests PRIVATE Qt6::Core Qt6::Sql Qt6::Test)
    target_compile_options(BikeParkingTests PRIVATE ${BIKEPARKING_WARNINGS})
    add_test(NAME parking_repository COMMAND BikeParkingTests)

    install(TARGETS BikeParkingTests
        RUNTIME DESTINATION bin
        COMPONENT tests)
endif()

# ------------------------------------------------------------------ install after build
# The "dist" target is part of the default build (ALL). It depends on the application and the
# tests, so it runs only after they have finished building, and then installs into
# dist/<BIKEPARKING_DIST_NAME>. No separate install step is needed.
set(BIKEPARKING_DIST_DEPENDS "")
if(BIKEPARKING_BUILD_APP)
    list(APPEND BIKEPARKING_DIST_DEPENDS BikeParking)
endif()
if(BIKEPARKING_BUILD_TESTS)
    list(APPEND BIKEPARKING_DIST_DEPENDS BikeParkingTests)
endif()

add_custom_target(dist ALL
    COMMAND ${CMAKE_COMMAND} --install "${CMAKE_BINARY_DIR}"
    DEPENDS ${BIKEPARKING_DIST_DEPENDS}
    COMMENT "Installing into ${CMAKE_INSTALL_PREFIX}"
    VERBATIM)
