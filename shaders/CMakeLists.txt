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

add_custom_target(shaders DEPENDS ${SPIRV_FILES})
