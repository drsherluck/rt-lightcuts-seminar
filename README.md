# Implementation of Real-Time Stochastic Lightcuts
This is for the course Seminar Computer Graphics at the TU Delft, where we have to present a paper and create a small implementation that helps understand the paper.<br>
The paper implemented (partialy): <a href="https://dl.acm.org/doi/10.1145/3384543">Real-Time Stochastic Lightcuts</a> (2020) by Daqi Lin and Cem Yuksel 

## Dependencies
* <a href="https://github.com/glfw/glfw">GLFW</a>
* <a href="https://github.com/ocornut/imgui/">Dear ImGui</a>
* <a href="https://github.com/KhronosGroup/SPIRV-Reflect">SPIRV-Reflect</a>
* <a href="https://github.com/nothings/stb">stb</a>
* <a href="https://github.com/KhronosGroup/glslang">glslang</a>
* <a href="https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator">vma</a>
* <a href="https://github.com/tinyobjloader/tinyobjloader">tinyobjloader</a>

## Compile instructions
First install the necessary dependencies: <br>
* Glslang (needed to compile the shaders) from their <a href="https://github.com/KhronosGroup/glslang/releases">release page</a> and add the bin folder to your PATH.<br>
* Vulkan sdk <a href="https://vulkan.lunarg.com/sdk/home#windows">here</a>.

Clone the project with submodles:
```
git clone --recursive https://github.com/drsherluck/rt-lightcuts-seminar
```
#### Compile and run
Linux: from root directory run these commands:
```
mkdir bin
cmake ..
make 
./engine
```
Windows: use Visual Studio with CMake
