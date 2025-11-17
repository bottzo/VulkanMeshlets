This is my progres on the vulkan indirect meshlet renderer

Branches:
The github repository has two branches
The main branch compleates the exercice using instancing and a vertex shader
The meshShader branch compleates the exercice using meshlets with mesh and task shaders

Acheived:
Base vulkan engine
Load meshes from gltf
Generate the mesh meshlets
Render the mesh using mesh and task shaders
Render the mesh vertex shaders
per meshlet frustum culling on a compute shader using the frustum aabb method
Lambertian fragment shader using the normal from the meshlet
GPU driven of 100000 meshlet meshes

HOW TO USE:
Little camera movind with WASD and the keyboard arrows

ON PROGRES:
indirect draw with draw count = 1

HOW TO COMPILE THE ENGINE:
The engine uses cmake as a buildsystem generator + vcpkg as a package manager
1. make sure you have installed cmake and vcpkg
2. on the "CMakeUserPresets.json" file change the variable VCPKG_ROOT to the path where vcpkg is installed on your PC
3. open a cmd and run cmake --preset=default which will generate the build folder with the visual studio(default) or the selected buildsystem
4. set the working directory to the root folder VulkanEngine ($(ProjectDir)..)
5. on the visual studio set the engine project to the startup project
