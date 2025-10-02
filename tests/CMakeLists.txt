# Test configuration for cachearoo-cpp

include(FetchContent)

# Fetch Google Test
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

# Test executable
add_executable(cachearoo_tests
    test_types.cc
    test_settings.cc
    test_connection_mock.cc
)

target_link_libraries(cachearoo_tests
    PRIVATE
        cachearoo
        GTest::gtest_main
)

target_include_directories(cachearoo_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

# Suppress external library warnings on Windows
if(MSVC)
    target_compile_options(cachearoo_tests PRIVATE /wd4127 /wd4244 /wd4267)
endif()

# Discover tests
include(GoogleTest)
gtest_discover_tests(cachearoo_tests)
