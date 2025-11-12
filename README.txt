This is my progres on the vulkan indirect meshlet renderer exercice on Sunday 3, (after roughly 1 week and 2 days from the start)

Acheived:
Base vulkan engine
Load meshes from gltf
Generate the mesh meshlets
Render the mesh using mesh shaders
Render the meshlets using task shaders
Lambertian fragment shader using the normal from the meshlet
GPU driven of 50000 meshlet meshes adding a compute shader with culling for models using the frustum aabb method

HOW TO USE:
Little camera movind with WASD and the keyboard arrows

ON PROGRES:
Culling each meshlet individually on the gpu on each task shader invocation

HOW TO COMPILE THE ENGINE:
The engine uses cmake as a buildsystem generator + vcpkg as a package manager
1. make sure you have installed cmake and vcpkg
2. on the "CMakeUserPresets.json" file change the variable VCPKG_ROOT to the path where vcpkg is installed on your PC
3. open a cmd and run cmake --preset=default which will generate the build folder with the visual studio(default) or the selected buildsystem
4. set the working directory to the root folder VulkanEngine ($(ProjectDir)..)
5. on the visual studio set the engine project to the startup project
