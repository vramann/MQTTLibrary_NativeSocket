cmake_minimum_required(VERSION 3.22)
project(MqttNativeSocket VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Build options ─────────────────────────────────────────────────────────────
option(MQTT_BUILD_TESTS    "Build unit and integration tests" ON)
option(MQTT_BUILD_EXAMPLES "Build example applications"       ON)
option(MQTT_ENABLE_ASAN    "Enable AddressSanitizer"          OFF)
option(MQTT_ENABLE_TSAN    "Enable ThreadSanitizer"           OFF)
option(MQTT_ENABLE_UBSAN   "Enable UndefinedBehaviorSanitizer" OFF)

# ── Sanitizers ────────────────────────────────────────────────────────────────
if(MQTT_ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()
if(MQTT_ENABLE_TSAN)
    add_compile_options(-fsanitize=thread)
    add_link_options(-fsanitize=thread)
endif()
if(MQTT_ENABLE_UBSAN)
    add_compile_options(-fsanitize=undefined)
    add_link_options(-fsanitize=undefined)
endif()

# ── Compiler warnings ─────────────────────────────────────────────────────────
add_compile_options(
    -Wall -Wextra -Wpedantic
    -Wno-unused-parameter
    -Wconversion -Wshadow
)

# ── Dependencies via FetchContent (no Paho) ───────────────────────────────────
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.14.1
    GIT_SHALLOW    TRUE
)
set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(spdlog)

FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG        0.8.0
    GIT_SHALLOW    TRUE
)
set(YAML_CPP_BUILD_TESTS   OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS   OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
set(YAML_CPP_INSTALL       OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(yaml-cpp)

# ── System packages ───────────────────────────────────────────────────────────
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# ── Library target ─────────────────────────────────────────────────────────────
file(GLOB_RECURSE MQTT_LIB_SOURCES CONFIGURE_DEPENDS src/*.cpp)

add_library(mqtt_native_socket STATIC ${MQTT_LIB_SOURCES})

target_include_directories(mqtt_native_socket
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(mqtt_native_socket
    PUBLIC
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        yaml-cpp::yaml-cpp
        Threads::Threads
    PRIVATE
        OpenSSL::SSL
        OpenSSL::Crypto
)

target_compile_features(mqtt_native_socket PUBLIC cxx_std_20)

# ── Sub-projects ───────────────────────────────────────────────────────────────
if(MQTT_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(MQTT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
