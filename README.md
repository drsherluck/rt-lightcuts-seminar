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

## Running
In the root directory run these commands, these will compile the program and the shaders (glslangValidator is needed to compile the shaders)
```
mkdir bin
cmake ..
make 
make shaders
./engine
```

Note: only tested on Linux, may not work on Windows
