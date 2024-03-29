# PackageProject.cmake will be used to make our target installable
set(TARGET_NAME EngineRuntime)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- Add source files ----

# Note: globbing sources is considered bad practice as CMake's generators may not detect new files
# automatically. Keep that in mind when changing files, or explicitly mention them here.
file(GLOB_RECURSE HEADER_FILES "*.h")
file(GLOB_RECURSE SOURCE_FILES "*.cpp")

# file(GLOB imgui_sources CONFIGURE_DEPENDS  "${ENGINE_ROOT_DIR}/3rd_party/imgui/*.cpp")
# file(GLOB imgui_impl CONFIGURE_DEPENDS
# "${ENGINE_ROOT_DIR}/3rd_party/imgui/backends/imgui_impl_glfw.cpp"
# "${ENGINE_ROOT_DIR}/3rd_party/imgui/backends/imgui_impl_glfw.h"
# "${ENGINE_ROOT_DIR}/3rd_party/imgui/backends/imgui_impl_vulkan.cpp"
# "${ENGINE_ROOT_DIR}/3rd_party/imgui/backends/imgui_impl_vulkan.h"
# )

# source_group(TREE "${ENGINE_ROOT_DIR}/schema" FILES ${SCHEMA_FILES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${HEADER_FILES} ${SOURCE_FILES})

# ---- Create library ----

# Note: for header-only libraries change all PUBLIC flags to INTERFACE and create an interface
# target: add_library(${TARGET_NAME} INTERFACE)
add_library(${TARGET_NAME} ${HEADER_FILES} ${SOURCE_FILES}) # ${imgui_impl} ${imgui_sources}

set_target_properties(${TARGET_NAME} PROPERTIES CXX_STANDARD 17)
set_target_properties(${TARGET_NAME} PROPERTIES FOLDER "Engine")

# being a cross-platform target, we enforce standards conformance on MSVC
target_compile_options(${TARGET_NAME} PUBLIC "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/permissive->")
target_compile_options(${TARGET_NAME} PUBLIC "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/WX->")

# target_compile_definitions(${TARGET_NAME} PUBLIC JPH_DISABLE_CUSTOM_ALLOCATOR)

# Link dependencies
target_link_libraries(${TARGET_NAME} PUBLIC json11)
target_link_libraries(${TARGET_NAME} PUBLIC mustache)
target_link_libraries(${TARGET_NAME} PUBLIC spdlog::spdlog_header_only)

# target_link_libraries(${TARGET_NAME} PUBLIC fmt::fmt-header-only)
target_link_libraries(${TARGET_NAME} PUBLIC glfw)
target_link_libraries(${TARGET_NAME} PUBLIC volk::volk_headers)
target_link_libraries(${TARGET_NAME} PUBLIC volk::volk)
target_link_libraries(${TARGET_NAME} PUBLIC libzip::zip)
target_link_libraries(${TARGET_NAME} PUBLIC SPIRV-Headers)
target_link_libraries(${TARGET_NAME} PUBLIC Vulkan-Headers)
target_link_libraries(${TARGET_NAME} PUBLIC VulkanMemoryAllocator)
target_link_libraries(${TARGET_NAME} PUBLIC Eigen3::Eigen)
target_link_libraries(${TARGET_NAME} PUBLIC imgui)

# target_link_libraries(${TARGET_NAME} PUBLIC zip)
target_link_libraries(${TARGET_NAME} PUBLIC
  glslang
  OSDependent
  MachineIndependent
  GenericCodeGen
  OGLCompiler
  glslang-default-resource-limits
  SPVRemapper
  SPIRV
  HLSL
)

target_link_directories(${TARGET_NAME} PUBLIC "$<IF:$<CONFIG:Debug>,${JSON11_LIB_DEBUG_DIRS},${JSON11_LIB_DIRS}>")
target_link_directories(${TARGET_NAME} PUBLIC "$<IF:$<CONFIG:Debug>,${JOLTPHYSICS_LIB_DEBUG_DIRS},${JOLTPHYSICS_LIB_DIRS}>")

target_include_directories(${TARGET_NAME} PUBLIC ${ENGINE_ROOT_DIR}/3rd_party/spdlog/include)
target_include_directories(${TARGET_NAME} PUBLIC ${ENGINE_ROOT_DIR}/3rd_party/fmt/include)
target_include_directories(${TARGET_NAME} PUBLIC ${ENGINE_ROOT_DIR}/3rd_party/eigen)
target_include_directories(${TARGET_NAME} PUBLIC ${ENGINE_ROOT_DIR}/3rd_party/json11)
target_include_directories(${TARGET_NAME} PUBLIC ${ENGINE_ROOT_DIR}/3rd_party/volk)
target_include_directories(${TARGET_NAME} PUBLIC ${ENGINE_ROOT_DIR}/3rd_party/glslang)
target_include_directories(${TARGET_NAME} PUBLIC ${ENGINE_ROOT_DIR}/3rd_party/imgui)
target_include_directories(${TARGET_NAME} PUBLIC ${ENGINE_ROOT_DIR}/3rd_party/stb)
target_include_directories(${TARGET_NAME} PUBLIC ${ENGINE_ROOT_DIR}/3rd_party/glfw/include)
target_include_directories(${TARGET_NAME} PUBLIC ${ENGINE_ROOT_DIR}/3rd_party/tinygltf)
target_include_directories(${TARGET_NAME} PUBLIC ${ENGINE_ROOT_DIR}/3rd_party/tinyobjloader)
target_include_directories(${TARGET_NAME} PUBLIC ${ENGINE_ROOT_DIR}/3rd_party/SPIRV-Headers/include)
target_include_directories(${TARGET_NAME} PUBLIC ${ENGINE_ROOT_DIR}/3rd_party/Vulkan-Headers/include)
target_include_directories(${TARGET_NAME} PUBLIC ${ENGINE_ROOT_DIR}/3rd_party/VulkanMemoryAllocator/include)

# target_include_directories(${TARGET_NAME} PUBLIC ${ENGINE_ROOT_DIR}/3rd_party/libzip/lib)

# target_include_directories(${TARGET_NAME} PUBLIC ${STB_INCLUDE_DIRS})
# target_include_directories(${TARGET_NAME} PUBLIC ${TINYGLTF_INCLUDE_DIRS})
# target_include_directories(${TARGET_NAME} PUBLIC ${LUA_INCLUDE_DIR})
# target_include_directories(${TARGET_NAME} PUBLIC ${JOLTPHYSICS_INCLUDE_DIRS})

# add_dependencies(${TARGET_NAME} ${SHADER_COMPILE_TARGET})

# target_include_directories(
# ${TARGET_NAME}
# PUBLIC $<BUILD_INTERFACE:${ENGINE_ROOT_DIR}/shader/generated/cpp>
# )
target_include_directories(
  ${TARGET_NAME}
  PUBLIC $<BUILD_INTERFACE:${ENGINE_ROOT_DIR}/source>
  $<INSTALL_INTERFACE:include/${TARGET_NAME}-${PROJECT_VERSION}>
)

# target_include_directories(
# ${TARGET_NAME}
# PUBLIC $<BUILD_INTERFACE:${ENGINE_ROOT_DIR}/source/runtime>
# $<INSTALL_INTERFACE:include/${TARGET_NAME}-${PROJECT_VERSION}>
# )

# target_include_directories(
# ${TARGET_NAME}
# PUBLIC $<BUILD_INTERFACE:${ENGINE_ROOT_DIR}/source/runtime/function/render/include>
# $<INSTALL_INTERFACE:include/${TARGET_NAME}-${PROJECT_VERSION}>
# )

# ---- Create an installable target ----
# this allows users to install and find the library via `find_package()`.

# the location where the project's version header will be placed should match the project's regular header paths
string(TOLOWER ${TARGET_NAME}/version.h VERSION_HEADER_LOCATION)

# ## precompile
# set global vari used by precompile
set(ENGINE_RUNTIME_HEADS "${HEADER_FILES}" PARENT_SCOPE)
