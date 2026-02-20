cmake_minimum_required(VERSION 3.28 FATAL_ERROR)

# Project.
project(txp_renderer
    VERSION 0.1.0
    LANGUAGES CXX C)

# When turning this option on, the library will be compiled using assertions. By default asserts are enabled in Debug build.
option(ENABLE_PROFILING "Enable profiling" OFF)

# When turning this option on, it will attempt to look for nlohmann::json externally instead of using the subdirectory here.
option(TXP_REND_USE_EXTERNAL_JSON "Use an external JSON library" OFF)


# Print out all options.
message(
    "\n"
    "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n"
    "|               ::OPTION VALUES::                \n"
    "| ENABLE_PROFILING           : ${ENABLE_PROFILING}\n"
    "| TXP_REND_USE_EXTERNAL_JSON : ${TXP_REND_USE_EXTERNAL_JSON}\n"
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
# if(MSVC)  # @NOTE: this can cause errors so don't do this lol.  -Thea 2026/02/14
#     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
# endif()

# Profiling.
if(ENABLE_PROFILING)
    message(FATAL_ERROR "@TODO: Implement profiling libraries here!")
endif()

# Dependencies.
set(DEPENDENCY_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cpp_glm/include  # Header only.
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/efsw/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/fastgltf/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/thea_imgui  # Header only.
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ImGuizmo
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/json/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/KTX-Software/lib/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb  # Header only (essentially).
    # ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stduuid/include  # @NOTE: Removing this so that it's not easy to mix up the uuid.h header files.
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tinyobjloader
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tomlplusplus/include  # Header only.
)

add_subdirectory(third_party/efsw)
add_subdirectory(third_party/fastgltf)
add_subdirectory(third_party/KTX-Software/lib ktx)
add_subdirectory(third_party/stduuid)

if(TXP_REND_USE_EXTERNAL_JSON)
  find_package(nlohmann_json 3.12.0 REQUIRED)
else()
  set(JSON_BuildTests OFF CACHE INTERNAL "")
  add_subdirectory(third_party/json)
endif()


# Platform-specific dependencies.
if(${CMAKE_SYSTEM_NAME} MATCHES "Windows|Darwin|Linux")
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


# Use local ASSET_DIR if standalone.
string(COMPARE EQUAL "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}" TXP_RENDERER_STANDALONE)

if (TXP_RENDERER_STANDALONE)
    message("TXP Renderer standalone mode! Using standalone asset directory.")
    set(ASSET_DIR ${CMAKE_CURRENT_SOURCE_DIR}/assets)
endif()

# Create symlink to assets folder.
if(DEFINED ASSET_DIR)
    set(dir_name ${ASSET_DIR})
    set(target_dir_name ${CMAKE_BINARY_DIR}/assets)
    if(WIN32)
        string(REPLACE "/" "\\" win32_dir_name ${dir_name})
        string(REPLACE "/" "\\" win32_target_dir_name ${target_dir_name})
        if(EXISTS ${dir_name})
            execute_process(
                COMMAND cmd /C rmdir ${win32_target_dir_name}
                WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
        endif()
        execute_process(
            COMMAND cmd /C mklink /J ${win32_target_dir_name} ${win32_dir_name}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    else()
        # The way you create a symlink in every single OS except Windows.s
        file(CREATE_LINK ${dir_name} ${target_dir_name} SYMBOLIC)
    endif()
endif()


# Resource files inside ASSET_DIR.
set(ASSET_DIR_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/models/probuilder_example.mtl
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/models/probuilder_example.wobj
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/models/simple_combat_char.glb
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/default_shader.shader
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/default_shader.shadrefl
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/gradient.shader
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/gradient.shadrefl
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/textures/default_tex.ktx2
)

# macOS: add all files in assets dir into app bundle package.
if (TXP_RENDERER_STANDALONE)
    foreach (FILE ${ASSET_DIR_FILES})
        file(RELATIVE_PATH NEW_FILE "${ASSET_DIR}" ${FILE})
        get_filename_component(NEW_FILE_PATH ${NEW_FILE} DIRECTORY)
        set_source_files_properties(${FILE}
            PROPERTIES
                MACOSX_PACKAGE_LOCATION "Resources/assets/${NEW_FILE_PATH}"
        )
    endforeach()
endif()


# Main source files.
set(MAIN_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/include/txp_renderer/renderer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/txp_renderer/types.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/txp_renderer_public.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/animation_driven_hitcapsule/hitcapsule.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/animation_driven_hitcapsule/hitcapsule.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/animation_frame_action/runtime_data.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/animation_frame_action/runtime_data.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/as_exec_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bt_lib/btglm.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bt_lib/btjson.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bt_lib/btjson.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bt_lib/btlogger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bt_lib/btlogger.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bt_lib/btservice_finder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bt_lib/btuuid.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bt_lib/btuuid.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mutex_wrapper/mutex_wrapper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render_object/animator_template.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render_object/animator_template.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render_object/animator_template_types.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render_object/deformed_render_model.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render_object/load_gltf_model.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render_object/load_gltf_model.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render_object/load_obj_model.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render_object/load_obj_model.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render_object/render_model.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render_object/render_model.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render_object/render_object.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render_object/render_object.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render_object/skeletal_animation.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render_object/skeletal_animator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render_object/skeletal_animator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render_object/vertex.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer/gfx.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer/gfx_vulkan.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer/gfx_vulkan/vk_image.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer/gfx_vulkan/vk_image.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer/gfx_vulkan/vk_structs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer/gfx_vulkan/vk_structs.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer/gfx_vulkan_impl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer/gfx_vulkan_impl.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer/renderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/shader/shader_gradient.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/shader/shader_gradient_vulkan.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/shader_creation/reflection_structs.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/shader_creation/shader_creation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/shader_creation/shader_creation.h
)

# Executable build.
add_executable(${PROJECT_NAME}
    MACOSX_BUNDLE

    # Main source files.
    ${MAIN_SOURCES}

    # Resource files.
    ${ASSET_DIR_FILES}

    # Dear ImGui implementation source files.
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/thea_imgui/backends/imgui_impl_glfw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/thea_imgui/backends/imgui_impl_glfw.h
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/thea_imgui/backends/imgui_impl_vulkan.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/thea_imgui/backends/imgui_impl_vulkan.h

    # Dear ImGui base source files.
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/thea_imgui/imconfig.h
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/thea_imgui/imgui_demo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/thea_imgui/imgui_draw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/thea_imgui/imgui_internal.h
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/thea_imgui/imgui_tables.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/thea_imgui/imgui_widgets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/thea_imgui/imgui.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/thea_imgui/imgui.h
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/thea_imgui/imstb_rectpack.h
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/thea_imgui/imstb_textedit.h
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/thea_imgui/imstb_truetype.h
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/thea_imgui/misc/cpp/imgui_stdlib.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/thea_imgui/misc/cpp/imgui_stdlib.h

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
        ${CMAKE_CURRENT_SOURCE_DIR}/src/bt_lib  # This is @TEMPORARY at least until bt_lib is made into its own library.
        ${DEPENDENCY_INCLUDE_DIRS}
        ${PLAT_SPECIFIC_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME}
    efsw-static
    fastgltf
    ktx
    stduuid
    nlohmann_json::nlohmann_json
    ${PLAT_SPECIFIC_LINK_LIBS}
)


target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        ${GFX_LIB_DEFINITIONS}
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.hawsoo.txp_renderer"
    MACOSX_BUNDLE_BUNDLE_NAME ${PROJECT_NAME}
    MACOSX_BUNDLE_BUNDLE_VERSION "0.1.0"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1"
    # RESOURCE "${ASSET_DIR_FILES}"  @CHECK: commented these out since it caused resource files to get placed at the root of the bundle. So it should be good enough if changes?  -Thea 2026/02/08
)

# Set this project as Visual Studio startup project.
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
