cmake_minimum_required(VERSION 3.10)

project(Engine)

find_package(SDL3 CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(meshoptimizer CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_path(TINYGLTF_INCLUDE_DIRS "tiny_gltf.h")

set(CORE_SRC src/Main.cpp src/Application.h src/Application.cpp src/Globals.h src/log.cpp src/FileSystem.h src/FileSystem.cpp)
set(MODULE_SRC  src/ModuleWindow.h src/ModuleWindow.cpp src/ModuleInput.h src/ModuleInput.cpp src/ModuleVulkan.h src/ModuleVulkan.cpp src/ModuleEditorCamera.h src/ModuleEditorCamera.cpp)
set(IMPORTERS_SRC  src/ImportMesh.h src/ImportMesh.cpp)
source_group(Core FILES ${CORE_SRC})
source_group(Modules FILES ${MODULE_SRC})
source_group(Importers FILES ${IMPORTERS_SRC})

set(SRCS ${CORE_SRC} ${MODULE_SRC} ${IMPORTERS_SRC})

add_executable(Engine ${SRCS})

target_link_libraries(Engine PRIVATE SDL3::SDL3)
target_link_libraries(Engine PRIVATE Vulkan::Vulkan)
target_link_libraries(Engine PRIVATE meshoptimizer::meshoptimizer)
target_link_libraries(Engine PRIVATE glm::glm)

target_include_directories(Engine PRIVATE ${TINYGLTF_INCLUDE_DIRS})
