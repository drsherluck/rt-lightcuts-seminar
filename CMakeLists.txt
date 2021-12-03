# Need 3.7 for FindVulkan 
cmake_minimum_required(VERSION 3.7 FATAL_ERROR)
project(engine)

### Set compilation options ###
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR 
		CMAKE_CXX_COMPILER_ID STREQUAL "Clang")

	set(CMAKE_CXX_STANDARD 17)
	set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

	add_compile_options(
		-Wall 
		-Wextra 
		-pedantic
		-Werror=format-security
		-Werror=format
	)
	
	string(APPEND CMAKE_CXX_FLAGS_DEBUG " -D_DEBUG -O0 -ggdb")
	string(APPEND CMAKE_CXX_FLAGS_RELEASE 
		" -03 -ffast-math -fno-unsafe-math-optimizations -fomit-frame-pointer")
endif()

if(MSVC)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    # temporary fix
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /STACK:10000000")
	string(APPEND CMAKE_CXX_FLAGS_DEBUG " -D_DEBUG")
endif()

# Add after setting options
add_executable(${CMAKE_PROJECT_NAME})
# https://stackoverflow.com/a/40947954/15620835
string(LENGTH "${CMAKE_SOURCE_DIR}/" SOURCE_PATH_SIZE)
add_compile_definitions("SOURCE_PATH_SIZE=${SOURCE_PATH_SIZE}")
# use glfw
add_compile_definitions(USE_GLFW)

# Search for vulkan 
find_package(Vulkan)
if(!Vulkan_FOUND)
	message(FATAL_ERROR "Vulkan library not found.")
endif()
message(STATUS "Found vulkan library and headers")

### Compile shaders ###
add_subdirectory(shaders)
add_dependencies(engine shaders)

### Copy resource files into build folder ###
add_custom_command(TARGET engine POST_BUILD 
	COMMAND ${CMAKE_COMMAND} -E copy_directory
	"${CMAKE_CURRENT_LIST_DIR}/resources/" "$<TARGET_FILE_DIR:engine>/resources/")

### Add sources ###
file(GLOB SOURCES src/*.cpp)
get_directory_property(CURRENT_DEFINITIONS COMPILE_DEFINITIONS)
message(STATUS "Compile definitions " ${CURRENT_DEFINITIONS})
list(FIND CURRENT_DEFINITIONS "USE_GLFW" USING_GLFW)
if (NOT USING_GLFW EQUAL -1)
    message(STATUS "USING GLFW")
    list(REMOVE_ITEM SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/input.cpp 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/window.cpp)
else()
    message(STATUS "NOT USING GLFW")
    list(REMOVE_ITEM SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/glfw_input.cpp 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/glfw_window.cpp)
endif()
target_include_directories(engine PRIVATE ${Vulkan_INCLUDE_DIRS})
target_sources(engine PRIVATE ${SOURCES})


### stb_image, tinyobj, spir_reflect and vma ###
file(GLOB EXTERNAL_SOURCES external/*.c external/*.cpp)
add_library(ext_lib STATIC ${EXTERNAL_SOURCES})
target_include_directories(ext_lib INTERFACE "${PROJECT_SOURCE_DIR}/external")

### Compile GLFW ###
### Add Dear ImGui ###
if (NOT USING_GLFW EQUAL -1)
    set( GLFW_BUILD_DOCS OFF CACHE BOOL  "GLFW lib only" )
    set( GLFW_INSTALL OFF CACHE BOOL  "GLFW lib only" )
    add_subdirectory(external/glfw)
    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE glfw)

    add_library(imgui STATIC)
    target_include_directories(imgui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui" ${Vulkan_INCLUDE_DIRS})
    target_sources(imgui PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/imgui.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/imgui.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/imgui_demo.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/imgui_draw.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/imgui_widgets.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/imgui_tables.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/backends/imgui_impl_vulkan.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/backends/imgui_impl_glfw.cpp"
    )
    target_link_libraries(imgui PUBLIC ${Vulkan_LIBRARY} glfw)
endif()

### Link external libraries ###
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${Vulkan_LIBRARY} ext_lib imgui)

if(UNIX)
	find_package(PkgConfig)
	pkg_check_modules(XCB REQUIRED xcb xcb-icccm)
	target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${XCB_LIBRARIES})
endif()

