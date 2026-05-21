cmake_minimum_required(VERSION 3.22)

include(FetchContent)
FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
    GIT_SHALLOW    TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(BUILD_GMOCK ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

# ── Unit tests ─────────────────────────────────────────────────────────────────
file(GLOB_RECURSE UNIT_SOURCES CONFIGURE_DEPENDS unit/*.cpp)
if(UNIT_SOURCES)
    add_executable(mqtt_unit_tests ${UNIT_SOURCES})
    target_link_libraries(mqtt_unit_tests
        PRIVATE
            mqtt_native_socket
            GTest::gtest_main
            GTest::gmock
    )
    target_compile_features(mqtt_unit_tests PRIVATE cxx_std_20)
    gtest_discover_tests(mqtt_unit_tests)
endif()

# ── Integration tests ──────────────────────────────────────────────────────────
file(GLOB_RECURSE INTEGRATION_SOURCES CONFIGURE_DEPENDS integration/*.cpp)
if(INTEGRATION_SOURCES)
    add_executable(mqtt_integration_tests ${INTEGRATION_SOURCES})
    target_link_libraries(mqtt_integration_tests
        PRIVATE
            mqtt_native_socket
            GTest::gtest_main
            GTest::gmock
    )
    target_compile_features(mqtt_integration_tests PRIVATE cxx_std_20)
    gtest_discover_tests(mqtt_integration_tests
        PROPERTIES LABELS "integration"
    )
endif()
