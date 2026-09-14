cmake_minimum_required(VERSION 3.20)
project(My_SFML_GUI LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Check for SFML
set(SFML_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/SFML")
if(NOT EXISTS "${SFML_DIR}/CMakeLists.txt")
    message(FATAL_ERROR 
        "SFML not found in thirdparty/SFML directory.\n"
        "Please manually download SFML 3.0.0 from:\n"
        "  https://github.com/SFML/SFML/releases/tag/3.0.0\n"
        "And extract it to: ${SFML_DIR}\n"
        "\n"
        "Or clone it with:\n"
        "  git clone --depth 1 --branch 3.0.0 https://github.com/SFML/SFML.git ${SFML_DIR}"
    )
endif()
message(STATUS "Found SFML in ${SFML_DIR}")

# Disable SFML modules we don't need (avoid FetchContent for audio deps)
set(SFML_BUILD_AUDIO OFF CACHE BOOL "Disable SFML Audio module" FORCE)
set(SFML_BUILD_NETWORK OFF CACHE BOOL "Disable SFML Network module" FORCE)

add_subdirectory(${SFML_DIR} ${CMAKE_CURRENT_BINARY_DIR}/sfml-build)

# Check for nlohmann/json
set(NLOHMANN_JSON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/json")
if(NOT EXISTS "${NLOHMANN_JSON_DIR}/CMakeLists.txt")
    message(FATAL_ERROR 
        "nlohmann/json not found in thirdparty/json directory.\n"
        "Please manually download nlohmann/json v3.11.3 from:\n"
        "  https://github.com/nlohmann/json/releases/tag/v3.11.3\n"
        "And extract it to: ${NLOHMANN_JSON_DIR}\n"
        "\n"
        "Or clone it with:\n"
        "  git clone --depth 1 --branch v3.11.3 https://github.com/nlohmann/json.git ${NLOHMANN_JSON_DIR}"
    )
endif()
message(STATUS "Found nlohmann/json in ${NLOHMANN_JSON_DIR}")
add_subdirectory(${NLOHMANN_JSON_DIR} ${CMAKE_CURRENT_BINARY_DIR}/json-build)

# Planning domain logic is deliberately independent from SFML so the CLI and
# GUI can share the same storage and operations.
add_library(planner_core
    src/planner/Planner.cpp
    include/planner/Planner.hpp
)
target_include_directories(planner_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(planner_core PUBLIC
    nlohmann_json::nlohmann_json
)

add_executable(myplan-cli src/cli/main.cpp)
target_link_libraries(myplan-cli PRIVATE planner_core)
if(WIN32)
    target_link_libraries(myplan-cli PRIVATE shell32)
endif()

add_executable(myplan-gui WIN32
    src/gui/main.cpp
    src/engine/gui/MyGUI.cpp
)
target_include_directories(myplan-gui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_options(myplan-gui PRIVATE -Wa,-mbig-obj)
target_link_libraries(myplan-gui PRIVATE
    planner_core
    sfml-graphics
    sfml-window
    sfml-system
    nlohmann_json::nlohmann_json
)
if(WIN32)
    target_link_libraries(myplan-gui PRIVATE shell32)
endif()
add_custom_command(TARGET myplan-gui POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/resources
            $<TARGET_FILE_DIR:myplan-gui>/resources
)

enable_testing()
add_executable(planner-tests tests/PlannerTests.cpp)
target_link_libraries(planner-tests PRIVATE planner_core)
add_test(NAME planner-tests COMMAND planner-tests)
