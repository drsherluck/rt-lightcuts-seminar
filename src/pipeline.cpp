#include "pipeline.h"
#include "file.h"
#include "log.h"

#include "spirv_reflect.h"

#include <algorithm>
#define SPV_INVALID_VALUE 0xFFFFFFFF

#define REFLECT_VERTEX_ATTRIBS

#ifdef REFLECT_VERTEX_ATTRIBS
static u32 format_size(VkFormat format)
{
    u32 res = 0;
    switch (format) 
    {
    case VK_FORMAT_R32G32_SFLOAT:       res = 8; break;
    case VK_FORMAT_R32G32B32_SFLOAT:    res = 12; break;
    case VK_FORMAT_R32G32B32A32_SFLOAT: res = 16; break;
    default: break;                                
    }
    return res;
}
#endif

void init_graphics_pipeline_description(gpu_context_t& ctx, pipeline_description_t& description)
{
    description.topology     = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    description.polygon_mode = VK_POLYGON_MODE_FILL;
    description.cull_mode    = VK_CULL_MODE_BACK_BIT;
    description.render_pass  = ctx.render_pass;
    description.viewport     = {0, 0, (f32)ctx.swapchain.extent.width, (f32)ctx.swapchain.extent.height, 0, 1};
    description.sciccor      = {{0, 0}, ctx.swapchain.extent};
}

void add_shader(pipeline_description_t& description, VkShaderStageFlagBits stage,
        std::string entry, const char* path)
{
    file_contents_t shader_code;
    if (!read_file(path, &shader_code))
    {
        LOG_ERROR("Could not load shader code");
    }

    shader_t shader;
    shader.entry = entry;
    shader.code = std::vector<u8>(shader_code.contents, shader_code.contents + shader_code.size);
    shader.stage = stage;
    description.shaders.push_back(shader);

    // use reflection to get descriptors, push_constants and inputs/outputs
    SpvReflectShaderModule module;
    SpvReflectResult res;
    res = spvReflectCreateShaderModule(shader_code.size, shader_code.contents, &module);
    if (res != SPV_REFLECT_RESULT_SUCCESS)
    {
        LOG_FATAL("Coud not load spv reflect module");
    }

    // push constant
    u32 count = 0;
    res = spvReflectEnumeratePushConstantBlocks(&module, &count, nullptr);
    assert(res == SPV_REFLECT_RESULT_SUCCESS);
    
    std::vector<SpvReflectBlockVariable*> blocks(count);
    res = spvReflectEnumeratePushConstantBlocks(&module, &count, blocks.data());
    assert(res == SPV_REFLECT_RESULT_SUCCESS);

    if (blocks.size() > 0)
    {
        auto& push_constants = description.push_constants;
        auto& block = *(blocks[0]);
        if (push_constants.size() == 0)
        {
            description.push_constants.resize(1, VkPushConstantRange{});
            push_constants[0].stageFlags = static_cast<VkShaderStageFlagBits>(module.shader_stage);
            push_constants[0].offset     = block.offset;
            push_constants[0].size       = block.padded_size;
        } 
        else
        {
            push_constants[0].stageFlags |= static_cast<VkShaderStageFlagBits>(module.shader_stage);
        }
        LOG_INFO("push_constant size = %d", push_constants[0].size);
    }

#ifdef REFLECT_VERTEX_ATTRIBS
    if (stage == VK_SHADER_STAGE_VERTEX_BIT)
    {
        u32 var_count = 0;
        res = spvReflectEnumerateInputVariables(&module, &var_count, nullptr);
        if (res != SPV_REFLECT_RESULT_SUCCESS)
        {
            LOG_ERROR("Could not enumerate shader input vars");
        }

        std::vector<SpvReflectInterfaceVariable*> input_vars(var_count);
        res = spvReflectEnumerateInputVariables(&module, &var_count, input_vars.data());
        assert(res == SPV_REFLECT_RESULT_SUCCESS);
        
        LOG_INFO("Found %d vertex inputs", var_count);

        // assuming only 1 binding
        description.binding_descriptions.resize(1);
        auto& binding_description = description.binding_descriptions[0];
        binding_description.binding   = 0;
        binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        binding_description.stride    = 0;	

        auto& attribute_descriptions = description.attribute_descriptions;
        attribute_descriptions.reserve(var_count);
        for (u32 i = 0; i < var_count; ++i)
        {
            const auto& refl_var = *(input_vars[i]);
            if (refl_var.built_in == SPV_INVALID_VALUE)
            {
                LOG_INFO("%s", refl_var.name);
                auto attrib = VkVertexInputAttributeDescription{};
                attrib.location = refl_var.location;
                attrib.binding  = binding_description.binding;
                attrib.format   = static_cast<VkFormat>(refl_var.format);
                attrib.offset   = 0;
                attribute_descriptions.push_back(attrib);
            }
        }

        std::sort(std::begin(attribute_descriptions), std::end(attribute_descriptions),
                [](const VkVertexInputAttributeDescription& a, const VkVertexInputAttributeDescription& b) {
                    return a.location < b.location;
                });

        for (auto& attr : attribute_descriptions)
        {
            u32 size = format_size(attr.format);
            attr.offset = binding_description.stride;
            binding_description.stride += size;
        }

        if (attribute_descriptions.size() == 0)
        {
            description.binding_descriptions.clear();
        }

    }
#endif 
    spvReflectDestroyShaderModule(&module);
}

static VkShaderModule load_shader(VkDevice device, const std::vector<u8>& bin)
{
	if (bin.size() == 0) 
	{
		return VK_NULL_HANDLE;
	}

	VkShaderModuleCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	info.pNext = nullptr;
	info.codeSize = bin.size();
	info.pCode = (const u32*)bin.data();

	VkShaderModule shader;
	VK_CHECK( vkCreateShaderModule(device, &info, nullptr, &shader) );
	return shader;
}

bool build_graphics_pipeline(gpu_context_t& ctx, pipeline_description_t& desc, pipeline_t& pipeline)
{
	bool contains_vertex_shader = false;
	std::vector<VkPipelineShaderStageCreateInfo> shader_stage_infos(desc.shaders.size(), VkPipelineShaderStageCreateInfo{});
	std::vector<VkShaderModule> shader_modules(desc.shaders.size());

	for (size_t i = 0; i < desc.shaders.size(); ++i)
	{
		shader_modules[i] = load_shader(ctx.device, desc.shaders[i].code);
		if (shader_modules[i] == VK_NULL_HANDLE)
		{
			// delete other shader_modules
            for (size_t k = 0; k < i; ++k)
            {
                vkDestroyShaderModule(ctx.device, shader_modules[i], nullptr);
            }
			return false;
		}
		shader_stage_infos[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shader_stage_infos[i].pNext = nullptr;
		shader_stage_infos[i].flags = 0;
		shader_stage_infos[i].stage = desc.shaders[i].stage;
		shader_stage_infos[i].module = shader_modules[i];
		shader_stage_infos[i].pName  = desc.shaders[i].entry.data();

		if (desc.shaders[i].stage == VK_SHADER_STAGE_VERTEX_BIT)
		{
			contains_vertex_shader = true;
		}
	}

	if (!contains_vertex_shader) 
	{
		for (size_t i = 0; i < desc.shaders.size(); ++i) 
		{
			if (shader_modules[i] != VK_NULL_HANDLE)
			{
				vkDestroyShaderModule(ctx.device, shader_modules[i], nullptr);
			}
		}
		LOG_ERROR("Missing vertex shader");
		return false;
	}

	LOG_INFO("Shader modules loaded");

	// Vertex input description
	VkPipelineVertexInputStateCreateInfo vertex_input_info{};
	vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_info.pNext = nullptr;
	vertex_input_info.flags = 0;
	vertex_input_info.vertexBindingDescriptionCount   = static_cast<u32>(desc.binding_descriptions.size());
	vertex_input_info.pVertexBindingDescriptions      = desc.binding_descriptions.data();
	vertex_input_info.vertexAttributeDescriptionCount = static_cast<u32>(desc.attribute_descriptions.size());
	vertex_input_info.pVertexAttributeDescriptions    = desc.attribute_descriptions.data();

    LOG_INFO("stride %d vs %d", desc.binding_descriptions[0].stride, sizeof(v3) + sizeof(v3) + sizeof(v2));
	LOG_INFO("#bindings = %d, #attribs = %d", desc.binding_descriptions.size(), desc.attribute_descriptions.size());

	// Input assembly state
	VkPipelineInputAssemblyStateCreateInfo assembly_info{};
	assembly_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	assembly_info.pNext = nullptr;
	assembly_info.flags = 0;
	assembly_info.topology = desc.topology; 
	assembly_info.primitiveRestartEnable = VK_FALSE; 

	// Skip tesselation
	
	// Viewport state
	VkPipelineViewportStateCreateInfo viewport_info{};
	viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_info.pNext = nullptr;
	viewport_info.flags = 0;
	viewport_info.viewportCount = 1; 
	viewport_info.pViewports = &desc.viewport;
	viewport_info.scissorCount = 1;
	viewport_info.pScissors = &desc.sciccor;

	// Rasterization 
	VkPipelineRasterizationStateCreateInfo rasterizer_info{};
	rasterizer_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer_info.pNext = nullptr;
	rasterizer_info.flags = 0;
	rasterizer_info.depthClampEnable = VK_FALSE;
	rasterizer_info.rasterizerDiscardEnable = VK_FALSE;
	rasterizer_info.polygonMode = desc.polygon_mode;
	rasterizer_info.cullMode = desc.cull_mode;
	rasterizer_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer_info.depthBiasEnable = VK_FALSE;
	rasterizer_info.depthBiasConstantFactor = 0.0f;
	rasterizer_info.depthBiasClamp = 0.0f;
	rasterizer_info.depthBiasSlopeFactor = 0.0f;
	rasterizer_info.lineWidth = 1.0f;

	// Multisampling (DISABLED)
	VkPipelineMultisampleStateCreateInfo ms_info{};
	ms_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	ms_info.pNext = nullptr;
	ms_info.flags = 0;
	ms_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	ms_info.sampleShadingEnable = VK_FALSE;
	ms_info.minSampleShading = 1.0f;
	ms_info.pSampleMask = nullptr;
	ms_info.alphaToCoverageEnable = VK_FALSE;
	ms_info.alphaToOneEnable = VK_FALSE;

	// depth stencil
	VkPipelineDepthStencilStateCreateInfo depth_stencil_info{};
	depth_stencil_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil_info.pNext = nullptr;
	depth_stencil_info.flags = 0;
	depth_stencil_info.depthTestEnable = desc.depth_test ? VK_TRUE : VK_FALSE;
	depth_stencil_info.depthWriteEnable = desc.depth_test ? VK_TRUE : VK_FALSE;
	depth_stencil_info.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	depth_stencil_info.depthBoundsTestEnable = VK_FALSE;
	depth_stencil_info.stencilTestEnable = VK_FALSE;	

	// Color blending
	VkPipelineColorBlendAttachmentState blend_attachment{};
	blend_attachment.blendEnable = desc.color_blending ? VK_TRUE : VK_FALSE;
	blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
	blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
	blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo blend_info{};
	blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend_info.pNext = nullptr;
	blend_info.flags = 0;
	blend_info.logicOpEnable = VK_FALSE;
	blend_info.logicOp = VK_LOGIC_OP_COPY;
	blend_info.attachmentCount =  1;
	blend_info.pAttachments = &blend_attachment;
	blend_info.blendConstants[0] = 0.0f;
	blend_info.blendConstants[1] = 0.0f;
	blend_info.blendConstants[2] = 0.0f;
	blend_info.blendConstants[3] = 0.0f;

	// Skip Dynamic state
    VkPipelineDynamicStateCreateInfo dynamic_info = {};
    dynamic_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_info.dynamicStateCount = static_cast<u32>(desc.dynamic_states.size());
    dynamic_info.pDynamicStates = desc.dynamic_states.data();
    
    // create descriptor set layouts
#ifdef REFLECT_DESCRIPTOR_SETS
    if (pipeline.descriptor_set_layouts.size() == 0)
    {
        std::sort(std::begin(desc.descriptor_sets), std::end(desc.descriptor_sets),
                [](const descriptor_set_data_t& a, const descriptor_set_data_t& b) 
                { return a.set_id < b.set_id; });
        size_t descriptor_count = desc.descriptor_sets.size();
        pipeline.descriptor_set_layouts.resize(descriptor_count);
        for (size_t i = 0; i < descriptor_count; ++i)
        {
            VK_CHECK( vkCreateDescriptorSetLayout(ctx.device, &desc.descriptor_sets[i].create_info, nullptr, &pipeline.descriptor_set_layouts[i]) );     
        }
    }
#endif
	
	// Create Pipeline layout
	VkPipelineLayoutCreateInfo layout_info{};
	layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layout_info.pNext = nullptr;
	layout_info.flags = 0;
	layout_info.setLayoutCount = static_cast<u32>(pipeline.descriptor_set_layouts.size());
	layout_info.pSetLayouts = pipeline.descriptor_set_layouts.data();
	layout_info.pushConstantRangeCount = static_cast<u32>(desc.push_constants.size());
	layout_info.pPushConstantRanges = desc.push_constants.data();
	
	VK_CHECK( vkCreatePipelineLayout(ctx.device, &layout_info, nullptr, &pipeline.layout) );
	LOG_INFO("Pipeline layout created");

	// Create the Graphics pipeline
	VkGraphicsPipelineCreateInfo graphics_info{};
	graphics_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	graphics_info.pNext = nullptr;
	graphics_info.flags = 0;
	graphics_info.stageCount = static_cast<u32>(shader_stage_infos.size());
	graphics_info.pStages = shader_stage_infos.data();
	graphics_info.pVertexInputState = &vertex_input_info;
	graphics_info.pInputAssemblyState = &assembly_info;
	graphics_info.pTessellationState = nullptr;
	graphics_info.pViewportState = &viewport_info;
	graphics_info.pRasterizationState = &rasterizer_info;
	graphics_info.pMultisampleState = &ms_info;
	graphics_info.pDepthStencilState = &depth_stencil_info;
	graphics_info.pColorBlendState = &blend_info;
	graphics_info.pDynamicState = desc.dynamic_states.size() > 0 ? &dynamic_info : nullptr;
	graphics_info.layout = pipeline.layout;
	graphics_info.renderPass = desc.render_pass;
	graphics_info.subpass = 0;
	graphics_info.basePipelineHandle = VK_NULL_HANDLE;
	graphics_info.basePipelineIndex = 0;

    VK_CHECK( vkCreateGraphicsPipelines(ctx.device, VK_NULL_HANDLE, 1, &graphics_info, nullptr, &pipeline.handle) );
	LOG_INFO("Graphics pipeline created");

	for (size_t i = 0; i < desc.shaders.size(); ++i)
	{
		vkDestroyShaderModule(ctx.device, shader_modules[i], nullptr);
	}
	LOG_INFO("Shader modules destroyed");
	return true;
}

void add_shader(rt_pipeline_description_t& description, VkShaderStageFlagBits stage, std::string entry, const char* path)
{
    file_contents_t shader_code;
    if (!read_file(path, &shader_code))
    {
        LOG_ERROR("Could not load shader code");
    }

    shader_t shader;
    shader.entry = entry;
    shader.code = std::vector<u8>(shader_code.contents, shader_code.contents + shader_code.size);
    shader.stage = stage;
    description.shaders.push_back(shader);

    // use reflection to get descriptors, push_constants and inputs/outputs
    SpvReflectShaderModule module;
    SpvReflectResult res;
    res = spvReflectCreateShaderModule(shader_code.size, shader_code.contents, &module);
    if (res != SPV_REFLECT_RESULT_SUCCESS)
    {
        LOG_FATAL("Coud not load spv reflect module");
    }

    u32 count = 0;
    res = spvReflectEnumeratePushConstantBlocks(&module, &count, nullptr);
    assert(res == SPV_REFLECT_RESULT_SUCCESS);
    
    std::vector<SpvReflectBlockVariable*> blocks(count);
    res = spvReflectEnumeratePushConstantBlocks(&module, &count, blocks.data());
    assert(res == SPV_REFLECT_RESULT_SUCCESS);

    if (blocks.size() > 0)
    {
        auto& push_constants = description.push_constants;
        auto& block = *(blocks[0]);
        if (push_constants.size() == 0)
        {
            description.push_constants.resize(1, VkPushConstantRange{});
            push_constants[0].stageFlags = static_cast<VkShaderStageFlagBits>(module.shader_stage);
            push_constants[0].offset     = block.offset;
            push_constants[0].size       = block.padded_size;
        } 
        else
        {
            push_constants[0].stageFlags |= static_cast<VkShaderStageFlagBits>(module.shader_stage);
        }
        LOG_INFO("push_constant size = %d", push_constants[0].size);
    }
}

bool build_raytracing_pipeline(gpu_context_t& ctx, rt_pipeline_description_t& desc, pipeline_t* pipeline)
{
	std::vector<VkPipelineShaderStageCreateInfo> shader_stage_infos(desc.shaders.size());
	std::vector<VkShaderModule> shader_modules(desc.shaders.size());

	for (size_t i = 0; i < desc.shaders.size(); ++i)
	{
		shader_modules[i] = load_shader(ctx.device, desc.shaders[i].code);
		if (shader_modules[i] == VK_NULL_HANDLE)
		{
			// delete other shader_modules
            LOG_ERROR("Failed to load shader module %s, aborting pipeline creation", desc.shaders[i].entry);
            for (size_t i = 0; i < shader_modules.size(); ++i)
            {
                if (shader_modules[i] != VK_NULL_HANDLE)
                {
                    vkDestroyShaderModule(ctx.device, shader_modules[i], nullptr);
                }
            }
			return false;
		}
		shader_stage_infos[i] = {}; 
		shader_stage_infos[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shader_stage_infos[i].pNext = nullptr;
		shader_stage_infos[i].flags = 0;
		shader_stage_infos[i].stage = desc.shaders[i].stage;
		shader_stage_infos[i].module = shader_modules[i];
		shader_stage_infos[i].pName  = desc.shaders[i].entry.data();
	}
	LOG_INFO("Shader modules loaded");

    std::vector<VkRayTracingShaderGroupCreateInfoKHR> shader_group_infos(desc.groups.size());
    for(size_t i = 0; i < desc.groups.size(); ++i)
    {
        shader_group_infos[i] = {};
        shader_group_infos[i].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        shader_group_infos[i].pNext = nullptr;
        shader_group_infos[i].type = desc.groups[i].type;
        shader_group_infos[i].generalShader = desc.groups[i].general;
        shader_group_infos[i].closestHitShader = desc.groups[i].closest_hit;
        shader_group_infos[i].anyHitShader = desc.groups[i].any_hit;
        shader_group_infos[i].intersectionShader = desc.groups[i].intersection;
        shader_group_infos[i].pShaderGroupCaptureReplayHandle = nullptr;
    }

    VkPipelineLayoutCreateInfo layout_info{};
	layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layout_info.pNext = nullptr;
	layout_info.flags = 0;
	layout_info.setLayoutCount = static_cast<u32>(desc.descriptor_set_layouts.size());
	layout_info.pSetLayouts = desc.descriptor_set_layouts.data();
	layout_info.pushConstantRangeCount = static_cast<u32>(desc.push_constants.size());
	layout_info.pPushConstantRanges = desc.push_constants.data();
    
    VK_CHECK( vkCreatePipelineLayout(ctx.device, &layout_info, nullptr, &pipeline->layout) );
    LOG_INFO("Created pipeline layout");

    VkRayTracingPipelineCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    create_info.pNext = nullptr;
    create_info.flags = 0;
    create_info.stageCount = static_cast<u32>(shader_stage_infos.size());
    create_info.pStages = shader_stage_infos.data();
    create_info.groupCount = static_cast<u32>(shader_group_infos.size());
    create_info.pGroups = shader_group_infos.data();
    create_info.maxPipelineRayRecursionDepth = desc.max_recursion_depth;
    create_info.pLibraryInfo = nullptr;
    create_info.pLibraryInterface = nullptr;
    create_info.pDynamicState = nullptr;
    create_info.layout = pipeline->layout;
    create_info.basePipelineHandle = VK_NULL_HANDLE;
    create_info.basePipelineIndex = -1;

    VK_CHECK( vkCreateRayTracingPipelines(ctx.device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &create_info, nullptr, &pipeline->handle) );
    LOG_INFO("Created ray tracing pipeline");

    // cleanup
	for (size_t i = 0; i < shader_modules.size(); ++i)
	{
		vkDestroyShaderModule(ctx.device, shader_modules[i], nullptr);
	}
	LOG_INFO("Shader modules destroyed");
    return true;
}

bool build_shader_binding_table(gpu_context_t& context, rt_pipeline_description_t& description, pipeline_t& pipeline, shader_binding_table_t& sbt)
{
    auto& regions = description.sbt_regions;
    if (regions[RGEN_REGION].size() == 0 || regions[RGEN_REGION].size() > 1)
    {
        LOG_ERROR("Missing or more than 1 rgen region(s) in the shader binding table");
        return false;
    }

    u32 handle_size = context.rt_properties.shaderGroupHandleSize;
    u32 handle_count = static_cast<u32>(description.shaders.size());
    u32 handle_alignment = align(handle_size, context.rt_properties.shaderGroupHandleAlignment);
    u32 base_alignment = context.rt_properties.shaderGroupBaseAlignment;
    LOG_INFO("handle_size = %d, handle_alignment = %d, base_alignment = %d", handle_size, handle_alignment, base_alignment);

    sbt.rgen.deviceAddress = 0;
    sbt.rgen.stride = align(handle_size, base_alignment);
    sbt.rgen.size = sbt.rgen.stride; // stride = size for rgen according to spec

    sbt.hit.deviceAddress = 0;
    sbt.hit.stride = handle_alignment;
    sbt.hit.size = regions[CHIT_REGION].size() * handle_size;

    sbt.miss.deviceAddress = 0;
    sbt.miss.stride = handle_alignment;
    sbt.miss.size = regions[MISS_REGION].size() * handle_size; 

    sbt.call.deviceAddress = 0;
    sbt.call.stride = handle_alignment;
    sbt.call.size = regions[CALL_REGION].size() * handle_size;

    // create buffer
    u64 buffer_size = align(sbt.rgen.size + sbt.hit.size + sbt.miss.size + sbt.call.size, base_alignment);
    LOG_INFO("shader binding table buffer size = %ld", buffer_size);
    LOG_INFO("shader binding table buffer size = %ld", align(sbt.rgen.size + sbt.hit.size, base_alignment) + sbt.miss.size);
    create_buffer(context, buffer_size, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            &sbt.buffer, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    auto buffer_address = get_buffer_device_address(context, sbt.buffer.handle);

    // write handles to buffer
    u32 data_size = handle_count * handle_size;
    std::vector<u8> handles(data_size);
    VK_CHECK( vkGetRayTracingShaderGroupHandles(context.device, pipeline.handle, 0, handle_count, data_size, handles.data()) );

    u8* p_data;
    context.allocator.map_memory(sbt.buffer.allocation, (void**)&p_data);
    u64 offset = 0;
    for (size_t i = 0; i < 4; ++i)
    {
        auto& reg = regions[i];
        u8* ptr = p_data + align(offset, base_alignment); 
        switch (i)
        {
            case RGEN_REGION: sbt.rgen.deviceAddress = buffer_address + offset; break;
            case CHIT_REGION: sbt.hit.deviceAddress  = buffer_address + offset; break;
            case MISS_REGION: sbt.miss.deviceAddress = buffer_address + offset; break;
            case CALL_REGION: sbt.call.deviceAddress = buffer_address + offset; break;
            default: break;
        }
        for (size_t k = 0; k < reg.size(); ++k)
        {
            memcpy(ptr + k * handle_size, handles.data() + reg[k] * handle_size, handle_size);
        }
        offset += align(handle_size * reg.size(), base_alignment);
    }
    LOG_INFO("written to sbt buffer");
    context.allocator.unmap_memory(sbt.buffer.allocation);
    return true;
}

void add_shader(compute_pipeline_description_t& description, std::string entry, const char* path)
{
    file_contents_t shader_code;
    if (!read_file(path, &shader_code))
    {
        LOG_ERROR("Could not load shader code");
    }

    description.shader.entry = entry;
    description.shader.code = std::vector<u8>(shader_code.contents, shader_code.contents + shader_code.size);
    description.shader.stage = VK_SHADER_STAGE_COMPUTE_BIT;

    // use reflection to get descriptors, push_constants and inputs/outputs
    SpvReflectShaderModule module;
    SpvReflectResult res;
    res = spvReflectCreateShaderModule(shader_code.size, shader_code.contents, &module);
    if (res != SPV_REFLECT_RESULT_SUCCESS)
    {
        LOG_FATAL("Coud not load spv reflect module");
    }

    u32 count = 0;
    res = spvReflectEnumeratePushConstantBlocks(&module, &count, nullptr);
    assert(res == SPV_REFLECT_RESULT_SUCCESS);
    
    std::vector<SpvReflectBlockVariable*> blocks(count);
    res = spvReflectEnumeratePushConstantBlocks(&module, &count, blocks.data());
    assert(res == SPV_REFLECT_RESULT_SUCCESS);

    if (blocks.size() > 0)
    {
        auto& push_constants = description.push_constants;
        auto& block = *(blocks[0]);
        if (push_constants.size() == 0)
        {
            description.push_constants.resize(1, VkPushConstantRange{});
            push_constants[0].stageFlags = static_cast<VkShaderStageFlagBits>(module.shader_stage);
            push_constants[0].offset     = block.offset;
            push_constants[0].size       = block.padded_size;
        } 
        else
        {
            push_constants[0].stageFlags |= static_cast<VkShaderStageFlagBits>(module.shader_stage);
        }
        LOG_INFO("push_constant size = %d", push_constants[0].size);
    }
}

bool build_compute_pipeline(gpu_context_t& ctx, compute_pipeline_description_t& description, pipeline_t* pipeline)
{
    auto shader_module = load_shader(ctx.device, description.shader.code);
    if (!shader_module)
    {
        LOG_ERROR("Failed to load shader module");
        return false;
    }

    VkPipelineShaderStageCreateInfo shader_stage = {};
    shader_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stage.stage = description.shader.stage;
    shader_stage.module = shader_module;
    shader_stage.pName = description.shader.entry.data();

    VkPipelineLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount = static_cast<u32>(description.descriptor_set_layouts.size());
    layout_info.pSetLayouts = description.descriptor_set_layouts.data();
    layout_info.pushConstantRangeCount = static_cast<u32>(description.push_constants.size());
    layout_info.pPushConstantRanges = description.push_constants.data();
    VK_CHECK( vkCreatePipelineLayout(ctx.device, &layout_info, nullptr, &pipeline->layout) );

    VkComputePipelineCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    create_info.flags = 0;
    create_info.stage = shader_stage;
    create_info.layout = pipeline->layout;
    create_info.basePipelineHandle = VK_NULL_HANDLE;

    VK_CHECK( vkCreateComputePipelines(ctx.device, VK_NULL_HANDLE, 1, &create_info, nullptr, &pipeline->handle) );
    vkDestroyShaderModule(ctx.device, shader_module, nullptr);
}

void destroy_pipeline(gpu_context_t& ctx, pipeline_t* pipeline)
{
    if (pipeline)
    {
        vkDestroyPipeline(ctx.device, pipeline->handle, nullptr);
        vkDestroyPipelineLayout(ctx.device, pipeline->layout, nullptr);
    }
}

