# Get shader compiler
find_program(GLSL_VALIDATOR glslangValidator REQUIRED)
if (NOT GLSL_VALIDATOR)
	message(FATAL_ERROR "glslangValidator not found.")
endif()

file(GLOB_RECURSE SHADERS "*.vert" "*.frag" "*.comp" "*.rgen" "*.rchit" "*.rmiss")

foreach(SHADER ${SHADERS})
	get_filename_component(FILE_NAME ${SHADER} NAME)
	set(SPIRV ${CMAKE_CURRENT_BINARY_DIR}/${FILE_NAME}.spv)

	add_custom_command(
		OUTPUT ${SPIRV}
		COMMAND ${GLSL_VALIDATOR} --target-env vulkan1.2 -l ${SHADER} -o ${SPIRV}
		DEPENDS ${SHADER}
	)

	list(APPEND SPIRV_FILES ${SPIRV})
endforeach()

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/glsl_shader.frag.spv 
    COMMAND ${GLSL_VALIDATOR} --target-env vulkan1.2 -l ${CMAKE_CURRENT_SOURCE_DIR}/../external/imgui/backends/vulkan/glsl_shader.frag -o ${CMAKE_CURRENT_BINARY_DIR}/glsl_shader.frag.spv 
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../external/imgui/backends/vulkan/glsl_shader.frag)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/glsl_shader.vert.spv 
    COMMAND ${GLSL_VALIDATOR} --target-env vulkan1.2 -l ${CMAKE_CURRENT_SOURCE_DIR}/../external/imgui/backends/vulkan/glsl_shader.vert -o ${CMAKE_CURRENT_BINARY_DIR}/glsl_shader.vert.spv 
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../external/imgui/backends/vulkan/glsl_shader.vert)
list(APPEND SPIRV_FILES ${CMAKE_CURRENT_BINARY_DIR}/glsl_shader.frag.spv  ${CMAKE_CURRENT_BINARY_DIR}/glsl_shader.vert.spv)

add_custom_target(shaders DEPENDS ${SPIRV_FILES})
