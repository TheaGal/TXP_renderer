cmake_minimum_required(VERSION 3.28 FATAL_ERROR)

# Project.
project(txp_renderer
    VERSION 0.1.0
    LANGUAGES CXX C)

# When turning this option on, the library will be compiled using assertions. By default asserts are enabled in Debug build.
option(ENABLE_PROFILING "Enable profiling" OFF)

# Print out all options.
message(
    "\n"
    "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n"
    "|               ::OPTION VALUES::                \n"
    "| ENABLE_PROFILING    : ${ENABLE_PROFILING}\n"
    "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n")

# C/C++ standard.
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED True)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    # MacOS requires enabling experimental library for clang-17 (default).
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexperimental-library")
endif()

# Ensure 64-bit build.
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Only 64-bit build allowed.")
    return()
endif()

# Multithreaded build.
set(CMAKE_BUILD_PARALLEL_LEVEL 16)
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
endif()

# Profiling.
if(ENABLE_PROFILING)
    message(FATAL_ERROR "@TODO: Implement profiling libraries here!")
endif()

# Dependencies.
set(DEPENDENCY_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cpp_glm/include  # Header only.
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/esfw/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/fastgltf/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui  # Header only.
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ImGuizmo
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb  # Header only (essentially).
    # ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stduuid/include  # @NOTE: Removing this so that it's not easy to mix up the uuid.h header files.
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tinyobjloader
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tomlplusplus/include  # Header only.
)

add_subdirectory(third_party/esfw)
add_subdirectory(third_party/fastgltf)
add_subdirectory(third_party/stduuid)

# Platform-specific dependencies.
if(${CMAKE_SYSTEM_NAME} MATCHES "Windows|Darwin")
    message("GFX Lib: Vulkan 1.3")
    set(GFX_LIB_DEFINITIONS
        TXP_GFX_BACKEND_VULKAN=1
    )

    find_package(Vulkan REQUIRED)
    add_subdirectory(third_party/glfw)
    add_subdirectory(third_party/vk-bootstrap)
    add_subdirectory(third_party/VulkanMemoryAllocator)

    set(PLAT_SPECIFIC_INCLUDE_DIRS
        ${Vulkan_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glfw/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/vk-bootstrap
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/VulkanMemoryAllocator/include
    )
    set(PLAT_SPECIFIC_LINK_LIBS
        glfw
        ${GLFW_LIBRARIES}
        Vulkan::Vulkan
        vk-bootstrap::vk-bootstrap
        GPUOpen::VulkanMemoryAllocator
    )
else()
    message(FATAL_ERROR "GFX Lib not implemented yet!!")
endif()


# Main source files.
set(MAIN_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/include/txp_renderer/renderer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/txp_renderer/types.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/txp_renderer_public.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/as_exec_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render_object/deformed_render_model.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render_object/render_model.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render_object/render_object.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer/gfx.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer/gfx_vulkan.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer/renderer.cpp
)

# Executable build.
add_executable(${PROJECT_NAME}
    # Main source files.
    ${MAIN_SOURCES}

    # Dear ImGui implementation source files.
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_glfw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_glfw.h
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_opengl3_loader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_opengl3.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_opengl3.h

    # Dear ImGui base source files.
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imconfig.h
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui_demo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui_draw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui_internal.h
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui_tables.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui_widgets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui.h
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imstb_rectpack.h
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imstb_textedit.h
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imstb_truetype.h
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/misc/cpp/imgui_stdlib.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/misc/cpp/imgui_stdlib.h

    # ImGuizmo source files.
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ImGuizmo/ImGuizmo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ImGuizmo/ImGuizmo.h
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ImGuizmo/ImSequencer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ImGuizmo/ImSequencer.h

    # stb header files.
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb/stb_image.h
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb/stb_image_resize2.h

    # Tinyobjloader source files.
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tinyobjloader/tiny_obj_loader.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tinyobjloader/tiny_obj_loader.h
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${DEPENDENCY_INCLUDE_DIRS}
        ${PLAT_SPECIFIC_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME}
    efsw-static
    fastgltf
    stduuid
    ${PLAT_SPECIFIC_LINK_LIBS}
)


target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        ${GFX_LIB_DEFINITIONS}
)

# Set this project as Visual Studio startup project.
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
