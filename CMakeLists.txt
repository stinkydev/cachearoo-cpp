cmake_minimum_required(VERSION 3.20)
project(cachearoo-cpp VERSION 1.0.3 LANGUAGES CXX)

# Set C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable position independent code
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Add compiler flags for Google C++ style
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find required packages
find_package(Threads REQUIRED)

# Include ExternalProject module
include(ExternalProject)

# Check if nlohmann_json is already provided by parent project
if(NOT DEFINED NLOHMANN_JSON_INCLUDE_DIR)
    # Download and configure nlohmann/json only if not provided
    ExternalProject_Add(
        nlohmann_json_external
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
        CMAKE_ARGS
            -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/external/nlohmann_json
            -DJSON_BuildTests=OFF
            -DJSON_Install=ON
            -DCMAKE_CXX_STANDARD=20
        UPDATE_COMMAND ""
    )
    set(NLOHMANN_JSON_INCLUDE_DIR ${CMAKE_BINARY_DIR}/external/nlohmann_json/include)
else()
    message(STATUS "Using nlohmann_json from parent project: ${NLOHMANN_JSON_INCLUDE_DIR}")
endif()

# Download websocketpp (header-only library)
# Using 0.8.2 with C++20 compatibility fix applied via CMake script
ExternalProject_Add(
    websocketpp_external
    GIT_REPOSITORY https://github.com/stinkydev/websocketpp.git
    GIT_TAG master
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ${CMAKE_COMMAND} -E copy_directory <SOURCE_DIR> ${CMAKE_BINARY_DIR}/external/websocketpp
    UPDATE_COMMAND ""
)

# Download standalone ASIO (for websocketpp without boost)
ExternalProject_Add(
    asio_external
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG asio-1-29-0
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ${CMAKE_COMMAND} -E copy_directory <SOURCE_DIR>/asio/include ${CMAKE_BINARY_DIR}/external/asio/include
    UPDATE_COMMAND ""
)

# Set up include directories
set(WEBSOCKETPP_INCLUDE_DIR ${CMAKE_BINARY_DIR}/external/websocketpp)
set(ASIO_INCLUDE_DIR ${CMAKE_BINARY_DIR}/external/asio/include)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${NLOHMANN_JSON_INCLUDE_DIR})
include_directories(${WEBSOCKETPP_INCLUDE_DIR})
include_directories(${ASIO_INCLUDE_DIR})

# Create library
set(CACHEAROO_SOURCES
    src/cachearoo_client.cc
    src/cachearoo_connection.cc
    src/cachearoo_messaging.cc
)

set(CACHEAROO_HEADERS
    include/cachearoo.h
    include/cachearoo_client.h
    include/cachearoo_connection.h
    include/cachearoo_messaging.h
    include/cachearoo_types.h
)

# Static library
add_library(cachearoo STATIC ${CACHEAROO_SOURCES} ${CACHEAROO_HEADERS})

# Add dependencies to the library
if(TARGET nlohmann_json_external)
    add_dependencies(cachearoo nlohmann_json_external websocketpp_external asio_external)
else()
    add_dependencies(cachearoo websocketpp_external asio_external)
endif()

# Link libraries
target_link_libraries(cachearoo
    PUBLIC
        Threads::Threads
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(cachearoo PRIVATE ws2_32 wsock32)
elseif(UNIX)
    target_link_libraries(cachearoo PRIVATE pthread)
endif()

# Include directories for the library
target_include_directories(cachearoo
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${NLOHMANN_JSON_INCLUDE_DIR}>
        $<BUILD_INTERFACE:${WEBSOCKETPP_INCLUDE_DIR}>
        $<BUILD_INTERFACE:${ASIO_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:include>
        $<INSTALL_INTERFACE:include/external>
)

# Compiler definitions
target_compile_definitions(cachearoo PRIVATE
    ASIO_STANDALONE
    ASIO_HAS_STD_ADDRESSOF
    ASIO_HAS_STD_ARRAY
    ASIO_HAS_CSTDINT
    ASIO_HAS_STD_SHARED_PTR
    ASIO_HAS_STD_TYPE_TRAITS
    _WEBSOCKETPP_CPP11_TYPE_TRAITS_
    _WEBSOCKETPP_CPP11_FUNCTIONAL_
    _WEBSOCKETPP_CPP11_SYSTEM_ERROR_
    _WEBSOCKETPP_CPP11_RANDOM_DEVICE_
    _WEBSOCKETPP_CPP11_MEMORY_
)

# Windows-specific definitions
if(WIN32)
    target_compile_definitions(cachearoo PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _WIN32_WINNT=0x0601
    )
    
    # Suppress warnings from external libraries (WebSocket++, ASIO)
    target_compile_options(cachearoo PRIVATE
        /wd4127  # conditional expression is constant
        /wd4244  # conversion from 'type1' to 'type2', possible loss of data
        /wd4267  # conversion from 'size_t' to 'type', possible loss of data
    )
endif()

# Examples
option(BUILD_EXAMPLES "Build example applications" ON)
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Tests
option(BUILD_TESTS "Build tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Add clang-format target
find_program(CLANG_FORMAT_EXECUTABLE
    NAMES clang-format clang-format-18 clang-format-17 clang-format-16 clang-format-15 clang-format-14
    DOC "Path to clang-format executable"
)

if(CLANG_FORMAT_EXECUTABLE)
    # Collect all source files to format
    file(GLOB_RECURSE ALL_SOURCE_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
        ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/examples/*.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/examples/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp
    )

    # Add format target
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXECUTABLE} -i ${ALL_SOURCE_FILES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Formatting source code with clang-format"
        VERBATIM
    )

    # Add format-check target
    add_custom_target(format-check
        COMMAND ${CLANG_FORMAT_EXECUTABLE} --dry-run --Werror ${ALL_SOURCE_FILES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Checking source code format with clang-format"
        VERBATIM
    )

    message(STATUS "clang-format found: ${CLANG_FORMAT_EXECUTABLE}")
    message(STATUS "Added targets: 'format' and 'format-check'")
else()
    message(WARNING "clang-format not found. Code formatting targets will not be available.")
endif()

# Install configuration
install(TARGETS cachearoo
    EXPORT cachearoo-targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

# Install external dependencies headers
install(DIRECTORY ${NLOHMANN_JSON_INCLUDE_DIR}/
    DESTINATION include/external
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

install(DIRECTORY ${WEBSOCKETPP_INCLUDE_DIR}/websocketpp
    DESTINATION include/external
    FILES_MATCHING PATTERN "*.hpp"
)

install(DIRECTORY ${ASIO_INCLUDE_DIR}/
    DESTINATION include/external
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.ipp"
)

# Generate and install CMake config files
include(CMakePackageConfigHelpers)

# Create config file
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/cachearoo-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cachearoo-config.cmake
    INSTALL_DESTINATION lib/cmake/cachearoo
)

# Create version file
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/cachearoo-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Install config files
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/cachearoo-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/cachearoo-config-version.cmake
    DESTINATION lib/cmake/cachearoo
)

# Install targets export
install(EXPORT cachearoo-targets
    FILE cachearoo-targets.cmake
    NAMESPACE cachearoo::
    DESTINATION lib/cmake/cachearoo
)