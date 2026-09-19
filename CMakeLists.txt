cmake_minimum_required(VERSION 3.21)
project(MapMaker LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT CMAKE_MSVC_RUNTIME_LIBRARY)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()

find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

add_executable(MapMaker
    src/main.cpp
    src/Core/Application.cpp
    src/Core/Application_Scene.cpp
    src/Core/Application_UI.cpp
    src/Core/Application_Panels.cpp
    src/Core/Application_Viewport.cpp
    src/Core/Application_Actions.cpp
    src/Core/Config.cpp
    src/Core/Shortcuts.cpp
    src/Render/GL.cpp
    src/Render/Shader.cpp
    src/Render/Mesh.cpp
    src/Render/Framebuffer.cpp
    src/Render/InstanceRenderer.cpp
    src/Render/AssetImporter.cpp
    src/Render/AssetRegistry.cpp
    src/Scene/Camera.cpp
    src/Scene/Scene.cpp
    src/Tools/ToolManager.cpp
    src/Save/SaveIO.cpp
)

target_include_directories(MapMaker PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(MapMaker PRIVATE
    glfw
    imgui::imgui
    nlohmann_json::nlohmann_json
    glm::glm
    assimp::assimp
    OpenGL::GL
)

target_compile_definitions(MapMaker PRIVATE
    GLFW_INCLUDE_NONE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    _CRT_SECURE_NO_WARNINGS
)

if (MSVC)
    target_compile_options(MapMaker PRIVATE /W3 /permissive- /utf-8 /MP)
endif()

set_target_properties(MapMaker PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin
)

add_custom_command(TARGET MapMaker POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/config
        $<TARGET_FILE_DIR:MapMaker>/config
)