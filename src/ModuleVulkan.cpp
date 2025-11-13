#include "ModuleVulkan.h"
#include "ModuleWindow.h"
#include "ModuleEditorCamera.h"
#include "FileSystem.h"
#include "vulkan/vulkan.h"
#include "SDL3/SDL_vulkan.h"
#include "meshoptimizer.h"
#include "ImportMesh.h"
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <random>

ModuleVulkan::ModuleVulkan(ModuleWindow* mWin, ModuleEditorCamera* camera) : mWindow(mWin), mCamera(camera)
{
}

ModuleVulkan::~ModuleVulkan()
{
}

static VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData) {

	LOG("Vulkang Validation Layer Message: %s", pCallbackData->pMessage);

	return VK_FALSE;
}

bool ModuleVulkan::Init()
{
	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Vulkan Meshlets";
	appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
	appInfo.pEngineName = "Vulkan Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_2;
	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;
	Uint32 extensionCount = 0;
	const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
	createInfo.enabledExtensionCount = extensionCount;
	createInfo.ppEnabledExtensionNames = extensions;
	if (!CheckVulkanExtensionsSupport(createInfo.ppEnabledExtensionNames, createInfo.enabledExtensionCount))
	{
		LOG("Error: All required extensions not present");
		return false;
	}

#ifndef NDEBUG
	const char** extensionsWithLayers = nullptr;
	const char* validationLayers[] = { "VK_LAYER_KHRONOS_validation" };
	if (CheckVulkanLayersSupport(validationLayers, sizeof(validationLayers) / sizeof(const char*)))
	{
		layersEnabled = true;
		createInfo.enabledLayerCount = sizeof(validationLayers) / sizeof(const char*);
		createInfo.ppEnabledLayerNames = validationLayers;

		//Add the message extension needed to output the validation layer errors
		extensionsWithLayers = new const char* [extensionCount + 1];
		for (int i = 0; i < extensionCount; ++i)
			extensionsWithLayers[i] = extensions[i];
		extensionsWithLayers[extensionCount] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
		createInfo.enabledExtensionCount = extensionCount + 1;
		createInfo.ppEnabledExtensionNames = extensionsWithLayers;

		//To callback my VulkanDebugCallback on the calls to vkCreateInstance and vkDestroyInstance
		VkDebugUtilsMessengerCreateInfoEXT debugMsgCreateInfo;
		debugMsgCreateInfo = {};
		debugMsgCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		debugMsgCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		debugMsgCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		debugMsgCreateInfo.pfnUserCallback = VulkanDebugCallback;
		createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugMsgCreateInfo;
	}
	else
	{
		LOG("Warning: validation layers not enabled");
		createInfo.enabledLayerCount = 0;
	}
#endif

	if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
	{
		LOG("Error creating Vulkan Instance");
		return false;
	}

#ifndef NDEBUG
	if (layersEnabled)
	{
		delete[] extensionsWithLayers;
		VkDebugUtilsMessengerCreateInfoEXT msgCreateInfo{};
		msgCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		msgCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		msgCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		msgCreateInfo.pfnUserCallback = VulkanDebugCallback;
		msgCreateInfo.pUserData = nullptr;
		auto vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
		if (vkCreateDebugUtilsMessengerEXT != nullptr) 
			vkCreateDebugUtilsMessengerEXT(instance, &msgCreateInfo, nullptr, &debugMessenger);
	}
#endif

	if (!SDL_Vulkan_CreateSurface(mWindow->window, instance, nullptr, &surface))
	{
		LOG("Error creating the window surface");
		return false;
	}

	//Select Physical Device
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
	VkPhysicalDevice* physicalDevices = new VkPhysicalDevice[deviceCount];
	vkEnumeratePhysicalDevices(instance, &deviceCount, physicalDevices);
	const char* requiredDeviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_EXT_MESH_SHADER_EXTENSION_NAME, VK_KHR_SPIRV_1_4_EXTENSION_NAME, VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME };
	VkFormat depthFormats[] = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };
	VkPhysicalDeviceFeatures2 deviceFeatures{};
	for (int physicalDeviceIndex = 0; physicalDeviceIndex < deviceCount; ++physicalDeviceIndex)
	{
		VkPhysicalDevice& device = physicalDevices[physicalDeviceIndex];

		//Extension Supports
		if (!CheckDeviceExtensionSupport(device, requiredDeviceExtensions, sizeof(requiredDeviceExtensions) / sizeof(const char*)))
			continue;

		//Mesh shader support
		VkPhysicalDeviceMeshShaderFeaturesEXT meshShadingFeatures{};
		meshShadingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
		VkPhysicalDeviceVulkan11Features onePointOneFeatures{};
		onePointOneFeatures.pNext = &meshShadingFeatures;
		onePointOneFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
		deviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		deviceFeatures.pNext = &onePointOneFeatures;
		vkGetPhysicalDeviceFeatures2(device, &deviceFeatures);
		if (meshShadingFeatures.meshShader == VK_FALSE || meshShadingFeatures.taskShader == VK_FALSE)
		{
			LOG("PhysicalDevice %d does not support required mesh shaders", physicalDeviceIndex);
			continue;
		}
		if (onePointOneFeatures.shaderDrawParameters == VK_FALSE)
		{
			LOG("PhysicalDevice %d does not support draw parameters and the gl_DrawID is required for indirect draw calls", physicalDeviceIndex);
			continue;
		}
		VkPhysicalDeviceMeshShaderPropertiesEXT meshShadingProperties{};
		meshShadingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT;
		VkPhysicalDeviceProperties2 deviceProperties{};
		deviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		deviceProperties.pNext = &meshShadingProperties;
		vkGetPhysicalDeviceProperties2(device, &deviceProperties);
		minStorageBufferOffsetAlignment = deviceProperties.properties.limits.minStorageBufferOffsetAlignment;
		minUniformBufferOffsetAlignment = deviceProperties.properties.limits.minUniformBufferOffsetAlignment;
		meshletMaxOutputVertices = meshShadingProperties.maxMeshOutputVertices;
		meshletMaxOutputPrimitives = meshShadingProperties.maxMeshOutputPrimitives;
		maxPreferredTaskWorkGroupInvocations = meshShadingProperties.maxPreferredTaskWorkGroupInvocations;
		maxPreferredMeshWorkGroupInvocations = meshShadingProperties.maxPreferredMeshWorkGroupInvocations;

		//SwapChain Support
		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
		if (formatCount == 0)
			continue;
		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
		if (presentModeCount == 0)
			continue;

		//check depth buffer formats support
		if (!FindSupportedFormat(depthFormats, sizeof(depthFormats) / sizeof(VkFormat), VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT, depthFormat, &device))
			continue;

		//Check the device is a discrete GPU
		//if (deviceProperties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			//continue;
		//Check the device has the required graphics family queue
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices[physicalDeviceIndex], &queueFamilyCount, nullptr);
		VkQueueFamilyProperties* queueFamilies = new VkQueueFamilyProperties[queueFamilyCount];
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices[physicalDeviceIndex], &queueFamilyCount, queueFamilies);
		graphicsQueueFamilyIndex = 0;
		bool foundQueueFamily = false;
		for (; physicalDeviceIndex < queueFamilyCount; ++graphicsQueueFamilyIndex)
		{
			//TODO: compute queue aparte?
			if (queueFamilies[graphicsQueueFamilyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT && queueFamilies[graphicsQueueFamilyIndex].queueFlags & VK_QUEUE_COMPUTE_BIT)
			{
				//TODO: the graphics and the present queues can be different!!
				VkBool32 presentSupport;
				vkGetPhysicalDeviceSurfaceSupportKHR(device, graphicsQueueFamilyIndex, surface, &presentSupport);
				if (presentSupport)
				{
					foundQueueFamily = true;
					break;
				}
				else
				{
					LOG("Warning: The queue has graphics support but not present support");
				}
			}
		}
		delete[] queueFamilies;
		if (foundQueueFamily)
		{
			physicalDevice = device;
			break;
		}
	}
	if (physicalDevice == VK_NULL_HANDLE)
	{
		//CleanUp();
		LOG("Error: no suitable physical device found");
		return false;
	}

	vkCmdDrawMeshTasksEXT = (PFN_vkCmdDrawMeshTasksEXT)vkGetInstanceProcAddr(instance, "vkCmdDrawMeshTasksEXT");
	vkCmdDrawMeshTasksIndirectEXT = (PFN_vkCmdDrawMeshTasksIndirectEXT)vkGetInstanceProcAddr(instance, "vkCmdDrawMeshTasksIndirectEXT");
	vkCmdDrawMeshTasksIndirectCountEXT = (PFN_vkCmdDrawMeshTasksIndirectCountEXT)vkGetInstanceProcAddr(instance, "vkCmdDrawMeshTasksIndirectCountEXT");
	if (vkCmdDrawMeshTasksEXT == nullptr || vkCmdDrawMeshTasksIndirectCountEXT == nullptr || vkCmdDrawMeshTasksIndirectEXT == nullptr)
	{
		LOG("Error getting the vulkan meshlet draw call funcions");
		return false;
	}

	VkDeviceQueueCreateInfo queueCreateInfo{};
	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo.queueFamilyIndex = graphicsQueueFamilyIndex;
	queueCreateInfo.queueCount = 1;
	float queuePriority = 1.0f;
	queueCreateInfo.pQueuePriorities = &queuePriority;
	VkDeviceCreateInfo deviceCreateInfo{};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
	deviceCreateInfo.queueCreateInfoCount = 1;
	deviceCreateInfo.enabledExtensionCount = sizeof(requiredDeviceExtensions)/sizeof(const char*);
	deviceCreateInfo.ppEnabledExtensionNames = requiredDeviceExtensions;
	deviceCreateInfo.pEnabledFeatures = nullptr;
	deviceCreateInfo.pNext = &deviceFeatures;
	if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS)
	{
		LOG("Error creating logical device");
		return false;
	}
	delete[] physicalDevices;
	vkGetDeviceQueue(device, graphicsQueueFamilyIndex, 0, &graphicsQueue);

	//Swapchain basics setup
	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
	VkSurfaceFormatKHR* formats = new VkSurfaceFormatKHR[formatCount];
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats);
	swapChainSurfaceFormat = formats[0];
	for (int i = 0; i < formatCount; ++i)
	{
		if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB && formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			swapChainSurfaceFormat = formats[i];
			break;
		}
	}
	delete[] formats;
	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
	VkPresentModeKHR* presentModes = new VkPresentModeKHR[presentModeCount];
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes);
	swapChainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
	for (int i = 0; i < presentModeCount; ++i)
	{
		if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			swapChainPresentMode = presentModes[i];
			break;
		}
	}
	delete[] presentModes;

	if (!CreateSwapChain())
	{
		LOG("Error creating the swapChain");
		return false;
	}

	//Render Pass
	VkAttachmentDescription attachments[2]{};
	//color
	attachments[0].format = swapChainSurfaceFormat.format;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	//depth
	attachments[1].format = depthFormat;
	attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentRef{};
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;
	subpass.pDepthStencilAttachment = &depthAttachmentRef;

	VkSubpassDependency dependency[1]{};
	dependency[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency[0].dstSubpass = 0;
	dependency[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependency[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency[0].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependency[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 2;
	renderPassInfo.pAttachments = attachments;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = sizeof(dependency) / sizeof(VkSubpassDependency);
	renderPassInfo.pDependencies = dependency;

	if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
		LOG("Error creating the renderpass object");
		return false;
	}

	if (!CreateFrameBuffers())
		return false;

	char* taskSource = nullptr;
	char* meshSource = nullptr;
	char* fragmentSource = nullptr;
	long taskSourceSize = FileSystem::ReadToBuffer("shaders/task.spv", taskSource, "rb");
	long meshSourceSize = FileSystem::ReadToBuffer("shaders/mesh.spv", meshSource, "rb");
	long fragmentSourceSize = FileSystem::ReadToBuffer("shaders/fragment.spv", fragmentSource, "rb");
	if (!(meshSourceSize && fragmentSourceSize && taskSource))
	{
		LOG("Error loading the shaders from a file");
		return false;
	}
	VkShaderModule taskModule;
	VkShaderModule meshModule;
	VkShaderModule fragmentModule;
	VkShaderModuleCreateInfo shaderModuleCreateInfo{};
	shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleCreateInfo.codeSize = taskSourceSize;
	shaderModuleCreateInfo.pCode = reinterpret_cast<uint32_t*>(taskSource);
	VkResult taskResult = vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr, &taskModule);
	shaderModuleCreateInfo.codeSize = meshSourceSize;
	shaderModuleCreateInfo.pCode = reinterpret_cast<uint32_t*>(meshSource);
	VkResult meshResult = vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr, &meshModule);
	shaderModuleCreateInfo.codeSize = fragmentSourceSize;
	shaderModuleCreateInfo.pCode = reinterpret_cast<uint32_t*>(fragmentSource);
	VkResult fragmentResult = vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr, &fragmentModule);
	if (taskResult != VK_SUCCESS || meshResult != VK_SUCCESS || fragmentResult != VK_SUCCESS)
	{
		LOG("Error crating the shader Modules");
		return false;
	}
	delete[] taskSource;
	delete[] meshSource;
	delete[] fragmentSource;

	VkSpecializationMapEntry taskMapEntry{};
	taskMapEntry.constantID = 1;
	taskMapEntry.offset = 0;
	taskMapEntry.size = sizeof(maxPreferredTaskWorkGroupInvocations);
	VkSpecializationInfo taskSpecializationInfo{};
	taskSpecializationInfo.dataSize = sizeof(maxPreferredTaskWorkGroupInvocations);
	taskSpecializationInfo.pData = &maxPreferredTaskWorkGroupInvocations;
	taskSpecializationInfo.mapEntryCount = 1;
	taskSpecializationInfo.pMapEntries = &taskMapEntry;
	VkSpecializationMapEntry meshMapEntry[1]{};
	meshMapEntry[0].constantID = 0;
	meshMapEntry[0].offset = 0;
	meshMapEntry[0].size = sizeof(maxPreferredMeshWorkGroupInvocations);
	//meshMapEntry[1].constantID = 1;
	//meshMapEntry[1].offset = 0;
	//meshMapEntry[1].size = sizeof(meshletMaxOutputVertices);
	//meshMapEntry[2].constantID = 2;
	//meshMapEntry[2].offset = 0;
	//meshMapEntry[2].size = sizeof(meshletMaxOutputPrimitives);
	uint32_t meshletData[] = { maxPreferredMeshWorkGroupInvocations, maxPreferredMeshWorkGroupInvocations, maxPreferredMeshWorkGroupInvocations };
	VkSpecializationInfo meshSpecializationInfo{};
	meshSpecializationInfo.dataSize = sizeof(maxPreferredMeshWorkGroupInvocations);
	meshSpecializationInfo.pData = meshletData;
	meshSpecializationInfo.mapEntryCount = sizeof(meshMapEntry) / sizeof(VkSpecializationMapEntry);
	meshSpecializationInfo.pMapEntries = meshMapEntry;
	VkPipelineShaderStageCreateInfo shaderStagesInfo[3]{};
	shaderStagesInfo[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStagesInfo[0].stage = VK_SHADER_STAGE_TASK_BIT_EXT;
	shaderStagesInfo[0].module = taskModule;
	shaderStagesInfo[0].pName = "main";
	shaderStagesInfo[0].pSpecializationInfo = &taskSpecializationInfo;
	shaderStagesInfo[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStagesInfo[1].stage = VK_SHADER_STAGE_MESH_BIT_EXT;
	shaderStagesInfo[1].pSpecializationInfo = &meshSpecializationInfo;
	shaderStagesInfo[1].module = meshModule;
	shaderStagesInfo[1].pName = "main";
	shaderStagesInfo[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStagesInfo[2].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shaderStagesInfo[2].module = fragmentModule;
	shaderStagesInfo[2].pName = "main";

	VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(sizeof(dynamicStates) / sizeof(VkDynamicState));
	dynamicState.pDynamicStates = dynamicStates;

	//VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	//vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	//vertexInputInfo.vertexBindingDescriptionCount = 0;
	//vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
	//vertexInputInfo.vertexAttributeDescriptionCount = 0;
	//vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional
	//
	//VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	//inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	//inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	//inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)swapChainExtent.width;
	viewport.height = (float)swapChainExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = swapChainExtent;

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f; // Optional
	rasterizer.depthBiasClamp = 0.0f; // Optional
	rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

	//VkPipelineDepthStencilStateCreateInfo

	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional
	//additive blending
	//colorBlendAttachment.blendEnable = VK_TRUE;
	//colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	//colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	//colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	//colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	//colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	//colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f; // Optional
	colorBlending.blendConstants[1] = 0.0f; // Optional
	colorBlending.blendConstants[2] = 0.0f; // Optional
	colorBlending.blendConstants[3] = 0.0f; // Optional

	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.minDepthBounds = 0.0f; // Optional
	depthStencil.maxDepthBounds = 1.0f; // Optional
	depthStencil.stencilTestEnable = VK_FALSE;
	depthStencil.front = {}; // Optional
	depthStencil.back = {}; // Optional

	VkDescriptorSetLayoutBinding layoutBindings[8]{};
	layoutBindings[0].binding = 0;
	layoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	layoutBindings[0].descriptorCount = 1;
	layoutBindings[0].stageFlags = VK_SHADER_STAGE_TASK_BIT_EXT;
	layoutBindings[0].pImmutableSamplers = nullptr; // Optional

	layoutBindings[1].binding = 1;
	layoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	layoutBindings[1].descriptorCount = 1;
	layoutBindings[1].stageFlags = VK_SHADER_STAGE_MESH_BIT_EXT;
	layoutBindings[1].pImmutableSamplers = nullptr; // Optional

	layoutBindings[2].binding = 2;
	layoutBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	layoutBindings[2].descriptorCount = 1;
	layoutBindings[2].stageFlags = VK_SHADER_STAGE_MESH_BIT_EXT;
	layoutBindings[2].pImmutableSamplers = nullptr; // Optional

	layoutBindings[3].binding = 3;
	layoutBindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	layoutBindings[3].descriptorCount = 1;
	layoutBindings[3].stageFlags = VK_SHADER_STAGE_MESH_BIT_EXT;
	layoutBindings[3].pImmutableSamplers = nullptr; // Optional

	layoutBindings[4].binding = 4;
	layoutBindings[4].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	layoutBindings[4].descriptorCount = 1;
	layoutBindings[4].stageFlags = VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT;
	layoutBindings[4].pImmutableSamplers = nullptr; // Optional

	layoutBindings[5].binding = 5;
	layoutBindings[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	layoutBindings[5].descriptorCount = 1;
	layoutBindings[5].stageFlags = VK_SHADER_STAGE_MESH_BIT_EXT;
	layoutBindings[5].pImmutableSamplers = nullptr; // Optional

	layoutBindings[6].binding = 6;
	layoutBindings[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	layoutBindings[6].descriptorCount = 1;
	layoutBindings[6].stageFlags = VK_SHADER_STAGE_TASK_BIT_EXT;
	layoutBindings[6].pImmutableSamplers = nullptr; // Optional

	layoutBindings[7].binding = 7;
	layoutBindings[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	layoutBindings[7].descriptorCount = 1;
	layoutBindings[7].stageFlags = VK_SHADER_STAGE_TASK_BIT_EXT;
	layoutBindings[7].pImmutableSamplers = nullptr; // Optional

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = sizeof(layoutBindings) / sizeof(VkDescriptorSetLayoutBinding);
	layoutInfo.pBindings = layoutBindings;
	VkDescriptorSetLayout descriptorSetLayout;
	if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
		LOG("Error creating the layout bindings");
		return false;
	}

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1; // Optional
	pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout; // Optional
	pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
	pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

	if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
		LOG("Error creating the pipeline layout");
		return false;
	}
	
	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	
	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = sizeof(shaderStagesInfo) / sizeof(VkPipelineShaderStageCreateInfo);
	pipelineInfo.pStages = shaderStagesInfo;
	pipelineInfo.pVertexInputState = nullptr;
	pipelineInfo.pInputAssemblyState = nullptr;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = pipelineLayout;
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
	pipelineInfo.basePipelineIndex = -1; // Optional
	
	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
		LOG("Error creating the graphics pipeline");
		return false;
	}
	
	vkDestroyShaderModule(device, taskModule, nullptr);
	vkDestroyShaderModule(device, meshModule, nullptr);
	vkDestroyShaderModule(device, fragmentModule, nullptr);

	char* cullSource = nullptr;
	long cullSourceSize = FileSystem::ReadToBuffer("shaders/cull.spv", cullSource, "rb");
	if (cullSourceSize == 0)
	{
		LOG("Error loading the shaders from a file");
		return false;
	}
	VkShaderModuleCreateInfo cullModuleCreateInfo{};
	cullModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	cullModuleCreateInfo.codeSize = cullSourceSize;
	cullModuleCreateInfo.pCode = reinterpret_cast<uint32_t*>(cullSource);
	VkShaderModule cullModule;
	if (vkCreateShaderModule(device, &cullModuleCreateInfo, nullptr, &cullModule) != VK_SUCCESS)
	{
		LOG("Error loading the compute cull shader module");
		return false;
	}
	VkPipelineShaderStageCreateInfo cullStageInfo{};
	cullStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	cullStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	cullStageInfo.module = cullModule;
	cullStageInfo.pName = "main";

	VkDescriptorSetLayoutBinding cullDescriptorSetLayoutBindings[7]{};
	cullDescriptorSetLayoutBindings[0].binding = 0;
	cullDescriptorSetLayoutBindings[0].descriptorCount = 1;
	cullDescriptorSetLayoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	cullDescriptorSetLayoutBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	cullDescriptorSetLayoutBindings[1].binding = 1;
	cullDescriptorSetLayoutBindings[1].descriptorCount = 1;
	cullDescriptorSetLayoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	cullDescriptorSetLayoutBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	cullDescriptorSetLayoutBindings[2].binding = 2;
	cullDescriptorSetLayoutBindings[2].descriptorCount = 1;
	cullDescriptorSetLayoutBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	cullDescriptorSetLayoutBindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	cullDescriptorSetLayoutBindings[3].binding = 3;
	cullDescriptorSetLayoutBindings[3].descriptorCount = 1;
	cullDescriptorSetLayoutBindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	cullDescriptorSetLayoutBindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	cullDescriptorSetLayoutBindings[4].binding = 4;
	cullDescriptorSetLayoutBindings[4].descriptorCount = 1;
	cullDescriptorSetLayoutBindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	cullDescriptorSetLayoutBindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	cullDescriptorSetLayoutBindings[5].binding = 5;
	cullDescriptorSetLayoutBindings[5].descriptorCount = 1;
	cullDescriptorSetLayoutBindings[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	cullDescriptorSetLayoutBindings[5].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	cullDescriptorSetLayoutBindings[6].binding = 6;
	cullDescriptorSetLayoutBindings[6].descriptorCount = 1;
	cullDescriptorSetLayoutBindings[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	cullDescriptorSetLayoutBindings[6].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	VkDescriptorSetLayoutCreateInfo cullDescriptorSetLayoutInfo{};
	cullDescriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	cullDescriptorSetLayoutInfo.bindingCount = sizeof(cullDescriptorSetLayoutBindings) / sizeof(VkDescriptorSetLayoutBinding);
	cullDescriptorSetLayoutInfo.pBindings = cullDescriptorSetLayoutBindings;
	VkDescriptorSetLayout cullSetLayout;
	vkCreateDescriptorSetLayout(device, &cullDescriptorSetLayoutInfo, nullptr, &cullSetLayout);
	VkPipelineLayoutCreateInfo cullPipelineLayoutInfo{};
	cullPipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	cullPipelineLayoutInfo.setLayoutCount = 1;
	cullPipelineLayoutInfo.pSetLayouts = &cullSetLayout;
	vkCreatePipelineLayout(device, &cullPipelineLayoutInfo, nullptr, &cullPipelineLayout);
	VkComputePipelineCreateInfo computePipelineInfo{};
	computePipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipelineInfo.basePipelineIndex = -1;
	computePipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	computePipelineInfo.layout = cullPipelineLayout;
	computePipelineInfo.stage = cullStageInfo;
	if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipelineInfo, nullptr, &computePipeline) != VK_SUCCESS)
	{
		LOG("Error creating the compute pipeline");
		return false;
	}
	vkDestroyShaderModule(device, cullModule, nullptr);
	delete[] cullSource;

	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = graphicsQueueFamilyIndex;
	if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
		LOG("failed to create command pool!");
		return false;
	}
	
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
	if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers) != VK_SUCCESS) {
		LOG("failed to allocate command buffers!");
		return false;
	}
	
	VkSemaphoreCreateInfo semaphoreCreateInfo{};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		VkFenceCreateInfo fenceCreateInfo{};
		fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		if (vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS || vkCreateFence(device, &fenceCreateInfo, nullptr, &frameFences[i]))
		{
			LOG("Error creating fences and semaphores");
			return false;
		}
	}
	renderFinishedSemaphores = new VkSemaphore[swapChainImageCount];
	for (int i = 0; i < swapChainImageCount; ++i)
	{
		if (vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS)
		{
			LOG("Error creating swap chain semaphores");
			return false;
		}
	}

	//Import the gltf model
	Mesh mesh;
	if (!ImporterMesh::ImportFirst("assets/Duck/Duck.gltf", mesh))
	{
		LOG("Error loading the model");
		return false;
	}
	GenerateMeshlet(mesh, meshletMesh);
	modelAABB.Generate(meshletMesh.mesh);

	const size_t transformsSize = sizeof(float) * 19;
	const size_t frustumPlaneSize = sizeof(float) * 4 * 6 + sizeof(uint32_t);
	const size_t modelMatricesSize = sizeof(float) * 16 * NUM_MODELS;
	const size_t OBBsSize = sizeof(float) * 4 * 8 * NUM_MODELS; //one for padding
	const size_t parameterSize = sizeof(uint32_t);
	if (!CreateBuffer((transformsSize + GetInbetweenAlignmentSpace(transformsSize, minUniformBufferOffsetAlignment)) * MAX_FRAMES_IN_FLIGHT, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT , VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, transformsBuffer, transformsBufferMemory) ||
		!CreateBuffer((frustumPlaneSize + GetInbetweenAlignmentSpace(frustumPlaneSize, minUniformBufferOffsetAlignment)) * MAX_FRAMES_IN_FLIGHT, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, frustumPlanesBuffer, frustumPlanesBufferMemory) ||
		!CreateBuffer((modelMatricesSize + GetInbetweenAlignmentSpace(modelMatricesSize, minStorageBufferOffsetAlignment)) * MAX_FRAMES_IN_FLIGHT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, modelMatricesBuffer, modelMatricesBufferMemory) ||
		!CreateBuffer((OBBsSize + GetInbetweenAlignmentSpace(OBBsSize, minStorageBufferOffsetAlignment)) * MAX_FRAMES_IN_FLIGHT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, OBBsBuffer, OBBsBufferMemory) ||
		!CreateBuffer((parameterSize + GetInbetweenAlignmentSpace(parameterSize, minStorageBufferOffsetAlignment)) * MAX_FRAMES_IN_FLIGHT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, parameterBuffer, parameterBufferMemory))
	{
		LOG("Error creating the uniform and persistent buffers");
		return false;
	}
	//map persistent buffers
	vkMapMemory(device, transformsBufferMemory, 0, VK_WHOLE_SIZE, 0, &transformsBufferPtr[0]);
	vkMapMemory(device, frustumPlanesBufferMemory, 0, VK_WHOLE_SIZE, 0, &frustumPlanesBufferPtr[0]);
	vkMapMemory(device, modelMatricesBufferMemory, 0, VK_WHOLE_SIZE, 0, &modelMatricesBufferPtr[0]);
	vkMapMemory(device, OBBsBufferMemory, 0, VK_WHOLE_SIZE, 0, &OBBsBufferPtr[0]);
	vkMapMemory(device, parameterBufferMemory, 0, VK_WHOLE_SIZE, 0, &parameterBufferPtr[0]);
	for (int i = 1; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		transformsBufferPtr[i] = static_cast<char*>(transformsBufferPtr[0]) + (transformsSize + GetInbetweenAlignmentSpace(transformsSize, minUniformBufferOffsetAlignment)) * i;
		frustumPlanesBufferPtr[i] = static_cast<char*>(frustumPlanesBufferPtr[0]) + (frustumPlaneSize + GetInbetweenAlignmentSpace(frustumPlaneSize, minUniformBufferOffsetAlignment)) * i;
		modelMatricesBufferPtr[i] = static_cast<char*>(modelMatricesBufferPtr[0]) + (modelMatricesSize + GetInbetweenAlignmentSpace(modelMatricesSize, minStorageBufferOffsetAlignment)) * i;
		OBBsBufferPtr[i] = static_cast<char*>(OBBsBufferPtr[0]) + (OBBsSize + GetInbetweenAlignmentSpace(OBBsSize, minStorageBufferOffsetAlignment)) * i;
		parameterBufferPtr[i] = static_cast<char*>(parameterBufferPtr[0]) + (parameterSize + GetInbetweenAlignmentSpace(parameterSize, minStorageBufferOffsetAlignment)) * i;
	}

	//initialize uniform buffers
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		glm::mat4 model(1.0f);//glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		memcpy(transformsBufferPtr[i], &model, sizeof(float) * 16);
		const uint32_t numCommands = NUM_MODELS;
		memcpy(static_cast<float*>(frustumPlanesBufferPtr[i]) + 6 * 4, &numCommands, sizeof(numCommands));
	}

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	size_t stagingBufferSize = meshletMesh.meshletCount * sizeof(meshopt_Meshlet) +
		meshletMesh.meshletCount * sizeof(float) * 8 + //meshopt_Bounds
		meshletMesh.GetMeshletsVerticeCount() * sizeof(unsigned int) +
		meshletMesh.GetMeshletsTriangleCount() * sizeof(unsigned int) +
		meshletMesh.mesh.numVertices * sizeof(Vertex) +
		sizeof(uint32_t) * NUM_MODELS;
	if (!CreateBuffer(stagingBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory))
	{
		LOG("Error creating the staging buffer");
		return false;
	}
	void* stagingBufferPtr;
	vkMapMemory(device, stagingBufferMemory, 0, stagingBufferSize, 0, &stagingBufferPtr);
	unsigned int offset = 0;
	memcpy(stagingBufferPtr, meshletMesh.meshlets, meshletMesh.meshletCount * sizeof(meshopt_Meshlet));
	offset += meshletMesh.meshletCount * sizeof(meshopt_Meshlet);
	for (int i = 0; i < meshletMesh.meshletCount; ++i)
	{
		memcpy(static_cast<char*>(stagingBufferPtr) + offset, meshletMesh.meshletBounds[i].cone_apex, sizeof(meshletMesh.meshletBounds->cone_apex));
		offset += sizeof(float) * 3;
		memcpy(static_cast<char*>(stagingBufferPtr) + offset, &meshletMesh.meshletBounds[i].cone_cutoff, sizeof(meshletMesh.meshletBounds->cone_cutoff));
		offset += sizeof(float);
		memcpy(static_cast<char*>(stagingBufferPtr) + offset, meshletMesh.meshletBounds[i].cone_axis, sizeof(meshletMesh.meshletBounds->cone_axis));
		offset += sizeof(float) * 4;
	}
	memcpy(static_cast<char*>(stagingBufferPtr) + offset, meshletMesh.meshletVertices, meshletMesh.GetMeshletsVerticeCount() * sizeof(unsigned int));
	offset += meshletMesh.GetMeshletsVerticeCount() * sizeof(unsigned int);
	for (int i = 0; i < meshletMesh.GetMeshletsTriangleCount(); ++i)
		reinterpret_cast<unsigned int*>(static_cast<char*>(stagingBufferPtr) + offset)[i] = meshletMesh.meshletTriangles[i];
	//memcpy(static_cast<char*>(stagingBufferPtr) + offset, meshletMesh.meshletTriangles, meshletMesh.GetMeshletsTriangleCount() * sizeof(unsigned int));
	offset += meshletMesh.GetMeshletsTriangleCount() * sizeof(unsigned int);
	memcpy(static_cast<char*>(stagingBufferPtr) + offset, meshletMesh.mesh.vertices, meshletMesh.mesh.numVertices * sizeof(Vertex));
	offset += meshletMesh.mesh.numVertices * sizeof(Vertex);
	for(int i = 0; i < NUM_MODELS; ++i)
		*reinterpret_cast<uint32_t*>(static_cast<char*>(stagingBufferPtr) + offset + sizeof(uint32_t) * i) = meshletMesh.meshletCount;
	vkUnmapMemory(device, stagingBufferMemory);

	if (!CreateBuffer(meshletMesh.meshletCount * sizeof(meshopt_Meshlet), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, meshletBuffer, meshletBufferMemory) ||
		!CreateBuffer(meshletMesh.meshletCount * sizeof(float) * 8, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, meshletCullInfoBuffer, meshletCullInfoBufferMemory) ||
		!CreateBuffer(meshletMesh.GetMeshletsVerticeCount() * sizeof(unsigned int), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, meshletVerticesBuffer, meshletVerticesBufferMemory) ||
		!CreateBuffer(meshletMesh.GetMeshletsTriangleCount() * sizeof(unsigned int), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, meshletTrianglesBuffer, meshletTrianglesBufferMemory) ||
		!CreateBuffer(meshletMesh.mesh.numVertices * sizeof(Vertex), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory) ||
		!CreateBuffer(sizeof(uint32_t) * NUM_MODELS, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, numMeshletsBuffer, numMeshletsBufferMemory) ||
		!CreateBuffer(sizeof(uint32_t) * 3 * NUM_MODELS, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, dispatchIndirectBuffer, dispatchIndirectBufferMemory) ||
		!CreateBuffer(sizeof(uint32_t) * NUM_MODELS, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, modelIDsBuffer, modelIDsBufferMemory))
	{
		LOG("Error creating the device buffers");
		return false;
	}

	VkCommandPool tmpCommandPool;
	VkCommandPoolCreateInfo tmpCommandPoolInfo{};
	tmpCommandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	tmpCommandPoolInfo.queueFamilyIndex = graphicsQueueFamilyIndex;
	tmpCommandPoolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
	if (vkCreateCommandPool(device, &tmpCommandPoolInfo, nullptr, &tmpCommandPool))
	{
		LOG("Error creating the temporary command pool");
		return false;
	}
	VkCommandBufferAllocateInfo cmdAllocateInfo{};
	cmdAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdAllocateInfo.commandBufferCount = 1;
	cmdAllocateInfo.commandPool = tmpCommandPool;
	cmdAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	VkCommandBuffer tmpCmdBuffer;
	if (vkAllocateCommandBuffers(device, &cmdAllocateInfo, &tmpCmdBuffer))
	{
		LOG("Error allocating temporary comand buffer");
		return false;
	}

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = 0; // Optional
	beginInfo.pInheritanceInfo = nullptr; // Optional

	if (vkBeginCommandBuffer(tmpCmdBuffer, &beginInfo) != VK_SUCCESS) {
		LOG("Error with begin the copy buffer command buffer recording")
			return false;
	}

	offset = 0;
	VkBufferCopy bufferCopyRegion{};
	bufferCopyRegion.size = meshletMesh.meshletCount * sizeof(meshopt_Meshlet);
	bufferCopyRegion.dstOffset = 0;
	bufferCopyRegion.srcOffset = 0;
	vkCmdCopyBuffer(tmpCmdBuffer, stagingBuffer, meshletBuffer, 1, &bufferCopyRegion);
	offset += bufferCopyRegion.size;
	bufferCopyRegion.size = meshletMesh.meshletCount * sizeof(float) * 8;
	bufferCopyRegion.dstOffset = 0;
	bufferCopyRegion.srcOffset = offset;
	vkCmdCopyBuffer(tmpCmdBuffer, stagingBuffer, meshletCullInfoBuffer, 1, &bufferCopyRegion);
	offset += bufferCopyRegion.size;
	bufferCopyRegion.size = meshletMesh.GetMeshletsVerticeCount() * sizeof(uint32_t);
	bufferCopyRegion.srcOffset = offset;
	vkCmdCopyBuffer(tmpCmdBuffer, stagingBuffer, meshletVerticesBuffer, 1, &bufferCopyRegion);
	offset += bufferCopyRegion.size;
	bufferCopyRegion.size = meshletMesh.GetMeshletsTriangleCount() * sizeof(uint32_t);
	bufferCopyRegion.srcOffset = offset;
	vkCmdCopyBuffer(tmpCmdBuffer, stagingBuffer, meshletTrianglesBuffer, 1, &bufferCopyRegion);
	offset += bufferCopyRegion.size;
	bufferCopyRegion.size = meshletMesh.mesh.numVertices * sizeof(Vertex);
	bufferCopyRegion.srcOffset = offset;
	vkCmdCopyBuffer(tmpCmdBuffer, stagingBuffer, vertexBuffer, 1, &bufferCopyRegion);
	offset += bufferCopyRegion.size;
	bufferCopyRegion.size = sizeof(uint32_t) * NUM_MODELS;
	bufferCopyRegion.srcOffset = offset;
	vkCmdCopyBuffer(tmpCmdBuffer, stagingBuffer, numMeshletsBuffer, 1, &bufferCopyRegion);

	vkEndCommandBuffer(tmpCmdBuffer);
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &tmpCmdBuffer;
	vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(graphicsQueue);
	//vkFreeCommandBuffers(device, tmpCommandPool, 1, &tmpCmdBuffer);
	vkDestroyCommandPool(device, tmpCommandPool, nullptr);
	vkFreeMemory(device, stagingBufferMemory, nullptr);
	vkDestroyBuffer(device, stagingBuffer, nullptr);


	VkDescriptorPoolSize poolSize[4]{};
	//graphics descriptors
	poolSize[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSize[0].descriptorCount = 1 * MAX_FRAMES_IN_FLIGHT;
	poolSize[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	poolSize[1].descriptorCount = 6 * MAX_FRAMES_IN_FLIGHT;
	//cull descriptors
	poolSize[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSize[2].descriptorCount = 1 * MAX_FRAMES_IN_FLIGHT;
	poolSize[3].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	poolSize[3].descriptorCount = 7 * MAX_FRAMES_IN_FLIGHT;
	VkDescriptorPoolCreateInfo dPoolInfo{};
	dPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	dPoolInfo.poolSizeCount = sizeof(poolSize) / sizeof(VkDescriptorPoolSize);
	dPoolInfo.pPoolSizes = poolSize;
	dPoolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT) * 2;
	if (vkCreateDescriptorPool(device, &dPoolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
		LOG("Error creating the descriptor pool");
		return false;
	}

	VkDescriptorSetLayout dSetLayouts[MAX_FRAMES_IN_FLIGHT * 2];
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		//Graphics
		dSetLayouts[i] = descriptorSetLayout;
		//Compute
		dSetLayouts[i + MAX_FRAMES_IN_FLIGHT] = cullSetLayout;
	}
	VkDescriptorSetAllocateInfo dSetAllocInfo{};
	dSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	dSetAllocInfo.descriptorPool = descriptorPool;
	dSetAllocInfo.descriptorSetCount = sizeof(dSetLayouts) / sizeof(VkDescriptorSetLayout);
	dSetAllocInfo.pSetLayouts = dSetLayouts;
	descriptorSets = new VkDescriptorSet[dSetAllocInfo.descriptorSetCount];
	if (vkAllocateDescriptorSets(device, &dSetAllocInfo, descriptorSets) != VK_SUCCESS) {
		LOG("Error allocating descriptor sets");
		return false;
	}
	//graphics
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		VkDescriptorBufferInfo uBufferInfo{};
		uBufferInfo.buffer = transformsBuffer;
		uBufferInfo.offset = (transformsSize + GetInbetweenAlignmentSpace(transformsSize, minUniformBufferOffsetAlignment)) * i;
		uBufferInfo.range = transformsSize;

		VkDescriptorBufferInfo ssBufferInfo[7]{};
		ssBufferInfo[0].buffer = meshletVerticesBuffer;
		ssBufferInfo[0].offset = 0;
		ssBufferInfo[0].range = VK_WHOLE_SIZE;
		ssBufferInfo[1].buffer = meshletTrianglesBuffer;
		ssBufferInfo[1].offset = 0;
		ssBufferInfo[1].range = VK_WHOLE_SIZE;
		ssBufferInfo[2].buffer = vertexBuffer;
		ssBufferInfo[2].offset = 0;
		ssBufferInfo[2].range = VK_WHOLE_SIZE;
		ssBufferInfo[3].buffer = meshletBuffer;
		ssBufferInfo[3].offset = 0;
		ssBufferInfo[3].range = VK_WHOLE_SIZE;
		ssBufferInfo[4].buffer = modelMatricesBuffer;
		ssBufferInfo[4].offset = (modelMatricesSize + GetInbetweenAlignmentSpace(modelMatricesSize, minStorageBufferOffsetAlignment)) * i;
		ssBufferInfo[4].range = modelMatricesSize;
		ssBufferInfo[5].buffer = modelIDsBuffer;
		ssBufferInfo[5].offset = 0;
		ssBufferInfo[5].range = VK_WHOLE_SIZE;
		ssBufferInfo[6].buffer = meshletCullInfoBuffer;
		ssBufferInfo[6].offset = 0;
		ssBufferInfo[6].range = VK_WHOLE_SIZE;

		VkWriteDescriptorSet descriptorWrite[5]{};
		descriptorWrite[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite[0].dstSet = descriptorSets[i];
		descriptorWrite[0].dstBinding = 1;
		descriptorWrite[0].dstArrayElement = 0;
		descriptorWrite[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptorWrite[0].descriptorCount = 3;
		descriptorWrite[0].pBufferInfo = ssBufferInfo;
		descriptorWrite[0].pImageInfo = nullptr; // Optional
		descriptorWrite[0].pTexelBufferView = nullptr; // Optional

		descriptorWrite[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite[1].dstSet = descriptorSets[i];
		descriptorWrite[1].dstBinding = 4;
		descriptorWrite[1].dstArrayElement = 0;
		descriptorWrite[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrite[1].descriptorCount = 1;
		descriptorWrite[1].pBufferInfo = &uBufferInfo;
		descriptorWrite[1].pImageInfo = nullptr; // Optional
		descriptorWrite[1].pTexelBufferView = nullptr; // Optional

		descriptorWrite[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite[2].dstSet = descriptorSets[i];
		descriptorWrite[2].dstBinding = 0;
		descriptorWrite[2].dstArrayElement = 0;
		descriptorWrite[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptorWrite[2].descriptorCount = 1;
		descriptorWrite[2].pBufferInfo = &ssBufferInfo[3];
		descriptorWrite[2].pImageInfo = nullptr; // Optional
		descriptorWrite[2].pTexelBufferView = nullptr; // Optional

		descriptorWrite[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite[3].dstSet = descriptorSets[i];
		descriptorWrite[3].dstBinding = 5;
		descriptorWrite[3].dstArrayElement = 0;
		descriptorWrite[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptorWrite[3].descriptorCount = 1;
		descriptorWrite[3].pBufferInfo = &ssBufferInfo[4];
		descriptorWrite[3].pImageInfo = nullptr; // Optional
		descriptorWrite[3].pTexelBufferView = nullptr; // Optional

		descriptorWrite[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite[4].dstSet = descriptorSets[i];
		descriptorWrite[4].dstBinding = 6;
		descriptorWrite[4].dstArrayElement = 0;
		descriptorWrite[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptorWrite[4].descriptorCount = 2;
		descriptorWrite[4].pBufferInfo = &ssBufferInfo[5];
		descriptorWrite[4].pImageInfo = nullptr; // Optional
		descriptorWrite[4].pTexelBufferView = nullptr; // Optional

		vkUpdateDescriptorSets(device, sizeof(descriptorWrite) / sizeof(VkWriteDescriptorSet), descriptorWrite, 0, nullptr);
	}
	//compute
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		VkDescriptorBufferInfo uBufferInfo[1]{};
		uBufferInfo[0].buffer = frustumPlanesBuffer;
		uBufferInfo[0].offset = (frustumPlaneSize + GetInbetweenAlignmentSpace(frustumPlaneSize, minUniformBufferOffsetAlignment)) * i;
		uBufferInfo[0].range = frustumPlaneSize;
	
		VkDescriptorBufferInfo ssBufferInfo[6]{};
		ssBufferInfo[0].buffer = numMeshletsBuffer;
		ssBufferInfo[0].offset = 0;
		ssBufferInfo[0].range = VK_WHOLE_SIZE;
		ssBufferInfo[1].buffer = dispatchIndirectBuffer;
		ssBufferInfo[1].offset = 0;
		ssBufferInfo[1].range = VK_WHOLE_SIZE;
		ssBufferInfo[2].buffer = parameterBuffer;
		ssBufferInfo[2].offset = (parameterSize + GetInbetweenAlignmentSpace(parameterSize, minStorageBufferOffsetAlignment)) * i;
		ssBufferInfo[2].range = parameterSize;
		ssBufferInfo[3].buffer = OBBsBuffer;
		ssBufferInfo[3].offset = (OBBsSize + GetInbetweenAlignmentSpace(OBBsSize, minStorageBufferOffsetAlignment)) * i;
		ssBufferInfo[3].range = OBBsSize;
		ssBufferInfo[4].buffer = modelMatricesBuffer;
		ssBufferInfo[4].offset = (modelMatricesSize + GetInbetweenAlignmentSpace(modelMatricesSize, minStorageBufferOffsetAlignment)) * i;
		ssBufferInfo[4].range = modelMatricesSize;
		ssBufferInfo[5].buffer = modelIDsBuffer;
		ssBufferInfo[5].offset = 0;
		ssBufferInfo[5].range = VK_WHOLE_SIZE;
	
		VkWriteDescriptorSet descriptorWrite[2]{};
		descriptorWrite[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite[0].dstSet = descriptorSets[i + MAX_FRAMES_IN_FLIGHT];
		descriptorWrite[0].dstBinding = 0;
		descriptorWrite[0].dstArrayElement = 0;
		descriptorWrite[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrite[0].descriptorCount = sizeof(uBufferInfo) / sizeof(VkDescriptorBufferInfo);
		descriptorWrite[0].pBufferInfo = uBufferInfo;
		descriptorWrite[0].pImageInfo = nullptr; // Optional
		descriptorWrite[0].pTexelBufferView = nullptr; // Optional
	
		descriptorWrite[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite[1].dstSet = descriptorSets[i + MAX_FRAMES_IN_FLIGHT];
		descriptorWrite[1].dstBinding = 1;
		descriptorWrite[1].dstArrayElement = 0;
		descriptorWrite[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptorWrite[1].descriptorCount = sizeof(ssBufferInfo) / sizeof(VkDescriptorBufferInfo);
		descriptorWrite[1].pBufferInfo = ssBufferInfo;
		descriptorWrite[1].pImageInfo = nullptr; // Optional
		descriptorWrite[1].pTexelBufferView = nullptr; // Optional
	
		vkUpdateDescriptorSets(device, sizeof(descriptorWrite) / sizeof(VkWriteDescriptorSet), descriptorWrite, 0, nullptr);
	}

	modelMatrices = new glm::mat4[NUM_MODELS];
	int randomNumber1 = 0;
	int randomNumber2 = 1000;
	int randomNumber3 = 200;
	int randomNumber4 = 45;
	int randomNumber5 = 200;
	int randomNumber6 = 4;
	int randomNumber7 = 70;
	for (int i = 0; i < NUM_MODELS; ++i)
	{
		glm::mat4 model(1.0f);//glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		srand(randomNumber1);
		randomNumber1 = rand() % 10001;
		float randomNum1 = (static_cast<float>(randomNumber1) / 10001.f) * 2.f - 1.f;
		srand(randomNumber2);
		randomNumber2 = rand() % 10001;
		float randomNum2 = (static_cast<float>(randomNumber2) / 10001.f) * 2.f - 1.f;
		srand(randomNumber3);
		randomNumber3 = rand() % 10001;
		float randomNum3 = (static_cast<float>(randomNumber3) / 10001.f) * 2.f - 1.f;
		srand(randomNumber4);
		randomNumber4 = rand() % 360;
		srand(randomNumber5);
		randomNumber5 = rand() % 10001;
		float randomNum5 = (static_cast<float>(randomNumber5) / 10001.f) * 2.f - 1.f;
		srand(randomNumber6);
		randomNumber6 = rand() % 10001;
		float randomNum6 = (static_cast<float>(randomNumber6) / 10001.f) * 2.f - 1.f;
		srand(randomNumber7);
		randomNumber7 = rand() % 10001;
		float randomNum7 = (static_cast<float>(randomNumber7) / 10001.f) * 2.f - 1.f;
		model = glm::rotate(model, glm::radians(static_cast<float>(randomNumber4)), glm::vec3(randomNum5, randomNum6, randomNum7));
		modelMatrices[i] = glm::translate(model, glm::vec3(6000.f * randomNum1, 6000.f * randomNum2, 6000.f * randomNum3));

		//modelMatrices[i] = glm::mat4(1.0f);
		//if(i == 1)
		//	modelMatrices[i] = glm::translate(modelMatrices[i], glm::vec3(400, 0, 0));
	}

	return true;
}

UpdateStatus ModuleVulkan::PostUpdate(float dt)
{
	vkWaitForFences(device, 1, &frameFences[currentFrame], VK_TRUE, UINT64_MAX);
	//parameter buffer
	*static_cast<uint32_t*>(parameterBufferPtr[currentFrame]) = 0;
	SetCameraInfo(mCamera->GetProj() * mCamera->GetView(), mCamera->GetPosition());
	//MODEL MATRICES
	for (int i = 0; i < NUM_MODELS; ++i)
		memcpy(static_cast<float*>(modelMatricesBufferPtr[currentFrame]) + 16 * i, &modelMatrices[i], sizeof(float) * 16);
	// Bounding boxes
	glm::vec3 AABBPoints[8];
	modelAABB.GetPoints(AABBPoints);
	for (int j = 0; j < NUM_MODELS; ++j)
	{
		for (int i = 0; i < 8; ++i)
			memcpy(static_cast<float*>(OBBsBufferPtr[currentFrame]) + (4 * 8 * j) + 4 * i, &AABBPoints[i], sizeof(glm::vec3));
	}
	// frustum planes + numCommands(NUM_MODEL)
	glm::vec4 planes[6];
	mCamera->GetFrustumPlanes(planes);
	memcpy(frustumPlanesBufferPtr[currentFrame], planes, sizeof(planes));
	const uint32_t numModels = NUM_MODELS;
	memcpy(static_cast<float*>(frustumPlanesBufferPtr[currentFrame]) + 6 * 4, &numModels, sizeof(numModels));
	VkResult result = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &swapChainImageIndex);
	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		//check window minimized (TODO): handle it :)
		VkSurfaceCapabilitiesKHR capabilities;
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);
		if (capabilities.currentExtent.width != 0 && capabilities.currentExtent.height != 0)
		{
			vkDeviceWaitIdle(device);
			DestroySwapChain();
			DestroyFrameBuffers();
			CreateSwapChain();
			CreateFrameBuffers();
			mCamera->ChangeAspectRatio(static_cast<float>(capabilities.currentExtent.width) / static_cast<float>(capabilities.currentExtent.height));
		}
		return UpdateStatus::UPDATE_CONTINUE;
	}
	//Reset the fence just if we know that we are going to submit work
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		LOG("Runtime error aquiring the next image to present");
		return UpdateStatus::UPDATE_ERROR;
	}
	vkResetFences(device, 1, &frameFences[currentFrame]);
	vkResetCommandBuffer(commandBuffers[currentFrame], 0);
	RecordCommandBuffer(commandBuffers[currentFrame], swapChainImageIndex, meshletMesh.meshletCount);
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &imageAvailableSemaphores[currentFrame];
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffers[currentFrame];
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &renderFinishedSemaphores[swapChainImageIndex];
	if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, frameFences[currentFrame]) != VK_SUCCESS) {
		LOG("failed to submit draw command buffer!");
		return UpdateStatus::UPDATE_ERROR;
	}
	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &renderFinishedSemaphores[swapChainImageIndex];
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapChain;
	presentInfo.pImageIndices = &swapChainImageIndex;
	presentInfo.pResults = nullptr; // Optional
	vkQueuePresentKHR(graphicsQueue, &presentInfo);

	currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	return UpdateStatus::UPDATE_CONTINUE;
}

bool ModuleVulkan::CleanUp()
{
	if (device == VK_NULL_HANDLE)
		return true;
	vkDeviceWaitIdle(device);
	vkDestroyDescriptorPool(device, descriptorPool, nullptr);
	delete[] descriptorSets;
	vkDestroyCommandPool(device, commandPool, nullptr);
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
		vkDestroyFence(device, frameFences[i], nullptr);
	}
	for (int i = 0; i < swapChainImageCount; ++i)
	{
		vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
	}
	vkDestroyRenderPass(device, renderPass, nullptr);
	DestroySwapChain();
	DestroyFrameBuffers();
	delete[] swapChainImages;
	delete[] swapChainImageViews;
	delete[] renderFinishedSemaphores;
	delete[] swapChainFramebuffers;
	vkDestroyPipeline(device, graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
	vkDestroyPipeline(device, computePipeline, nullptr);
	vkDestroyPipelineLayout(device, cullPipelineLayout, nullptr);
	vkDestroySurfaceKHR(instance, surface, nullptr);
	vkDestroyDevice(device, nullptr);
#ifndef NDEBUG
	if (layersEnabled)
	{
		auto vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if (vkDestroyDebugUtilsMessengerEXT != nullptr)
			vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
	}
#endif
	vkDestroyInstance(instance, nullptr);
	delete[] modelMatrices;
	return true;
}

bool ModuleVulkan::CreateSwapChain()
{
	VkSurfaceCapabilitiesKHR capabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);
	//TODO: do not render while the window is minimized
	//while (SDL_GetWindowFlags(mWindow->window) & SDL_WINDOW_MINIMIZED)
	//{
	//	SDL_WaitEvent(NULL);
	//}
	if(capabilities.currentExtent.width == 0 && capabilities.currentExtent.height == 0)
	{
		LOG("Window minimized");
	}
	if (capabilities.currentExtent.width == UINT32_MAX)
	{
		swapChainExtent = capabilities.currentExtent;
	}
	else
	{
		int width, height;
		SDL_GetWindowSizeInPixels(mWindow->window, &width, &height);
		swapChainExtent.width = (width < capabilities.minImageExtent.width) ? width : capabilities.minImageExtent.width;
		swapChainExtent.height = (height < capabilities.minImageExtent.height) ? height : capabilities.minImageExtent.height;
	}
	swapChainImageCount = capabilities.minImageCount + 1;
	if (capabilities.maxImageCount > 0 && swapChainImageCount > capabilities.maxImageCount)
		swapChainImageCount = capabilities.maxImageCount;

	VkSwapchainCreateInfoKHR swapChainCreateInfo{};
	swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainCreateInfo.surface = surface;
	swapChainCreateInfo.minImageCount = swapChainImageCount;
	swapChainCreateInfo.imageFormat = swapChainSurfaceFormat.format;
	swapChainCreateInfo.imageColorSpace = swapChainSurfaceFormat.colorSpace;
	swapChainCreateInfo.imageExtent = swapChainExtent;
	swapChainCreateInfo.imageArrayLayers = 1;
	swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	//if (indices.graphicsFamily != indices.presentFamily) {
	//	createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
	//	createInfo.queueFamilyIndexCount = 2;
	//	createInfo.pQueueFamilyIndices = queueFamilyIndices;
	//}
	//else {
	swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapChainCreateInfo.queueFamilyIndexCount = 0; // Optional
	swapChainCreateInfo.pQueueFamilyIndices = nullptr; // Optional
	//}
	swapChainCreateInfo.preTransform = capabilities.currentTransform;
	swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapChainCreateInfo.presentMode = swapChainPresentMode;
	swapChainCreateInfo.clipped = VK_TRUE;
	swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;
	if (vkCreateSwapchainKHR(device, &swapChainCreateInfo, nullptr, &swapChain) != VK_SUCCESS) {
		LOG("Error creating the swapChain");
		return false;
	}
	vkGetSwapchainImagesKHR(device, swapChain, &swapChainImageCount, nullptr);
	if(swapChainImages == nullptr)
		swapChainImages = new VkImage[swapChainImageCount];
	vkGetSwapchainImagesKHR(device, swapChain, &swapChainImageCount, swapChainImages);
	if(swapChainImageViews == nullptr)
		swapChainImageViews = new VkImageView[swapChainImageCount];
	for (int i = 0; i < swapChainImageCount; ++i)
	{
		VkImageViewCreateInfo imageViewCreateInfo = {};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.image = swapChainImages[i];
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.format = swapChainSurfaceFormat.format;
		imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewCreateInfo.subresourceRange.levelCount = 1;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		imageViewCreateInfo.subresourceRange.layerCount = 1;
		if (vkCreateImageView(device, &imageViewCreateInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
			LOG("Error creating swapchain image views");
			return false;
		}
	}
	return true;
}

bool ModuleVulkan::CreateFrameBuffers()
{
	//Depth Buffer Creation
	if (!CreateImage(swapChainExtent.width, swapChainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageMemory))
	{
		LOG("Error creating the depht buffer");
		return false;
	}

	VkImageViewCreateInfo imageViewCreateInfo = {};
	imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCreateInfo.image = depthImage;
	imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewCreateInfo.format = depthFormat;
	imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	imageViewCreateInfo.subresourceRange.levelCount = 1;
	imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	imageViewCreateInfo.subresourceRange.layerCount = 1;
	if (vkCreateImageView(device, &imageViewCreateInfo, nullptr, &depthImageView) != VK_SUCCESS) {
		LOG("Error creating depth image view");
		return false;
	}

	if (swapChainFramebuffers == nullptr)
		swapChainFramebuffers = new VkFramebuffer[swapChainImageCount];
	for (int i = 0; i < swapChainImageCount; ++i)
	{
		VkImageView views[] = { swapChainImageViews[i], depthImageView };
		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderPass;
		framebufferInfo.attachmentCount = 2;
		framebufferInfo.pAttachments = views;
		framebufferInfo.width = swapChainExtent.width;
		framebufferInfo.height = swapChainExtent.height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
			LOG("Error creating framebuffers");
			return false;
		}
	}
	return true;
}

void ModuleVulkan::DestroySwapChain()
{
	vkDestroySwapchainKHR(device, swapChain, nullptr);
}

void ModuleVulkan::DestroyFrameBuffers()
{
	for (int i = 0; i < swapChainImageCount; ++i)
		vkDestroyFramebuffer(device, swapChainFramebuffers[i], nullptr);
	for (int i = 0; i < swapChainImageCount; ++i)
		vkDestroyImageView(device, swapChainImageViews[i], nullptr);
	vkDestroyImage(device, depthImage, nullptr);
	vkFreeMemory(device, depthImageMemory, nullptr);
}

void ModuleVulkan::RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t numMeshlets)
{
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = 0; // Optional
	beginInfo.pInheritanceInfo = nullptr; // Optional

	if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
		LOG("Error with begin the command buffer recording")
			return;
	}

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, cullPipelineLayout, 0, 1, &descriptorSets[currentFrame + MAX_FRAMES_IN_FLIGHT], 0, nullptr);
	vkCmdDispatch(commandBuffer, NUM_MODELS, 1, 1);
	VkMemoryBarrier memBarrier{};
	memBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	memBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	memBarrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT;
	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_TASK_SHADER_BIT_EXT, 0, 1, &memBarrier, 0, nullptr, 0, nullptr);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
	
	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = renderPass;
	renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = swapChainExtent;
	VkClearValue clearColor[2];
	clearColor[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
	clearColor[1].depthStencil.depth = 1.0f;
	clearColor[1].depthStencil.stencil = 0.0f;
	renderPassInfo.clearValueCount = 2;
	renderPassInfo.pClearValues = clearColor;
	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(swapChainExtent.width);
	viewport.height = static_cast<float>(swapChainExtent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = swapChainExtent;
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);

	//vkCmdDrawMeshTasksEXT(commandBuffer, numMeshlets, 1, 1);
	//NOTE: Draw without the indirect count(uncomment the line below and comment 2 lines below)
	//vkCmdDrawMeshTasksIndirectEXT(commandBuffer, dispatchIndirectBuffer, 0, NUM_MODELS, sizeof(uint32_t) * 3);
	vkCmdDrawMeshTasksIndirectCountEXT(commandBuffer, dispatchIndirectBuffer, 0, parameterBuffer, (sizeof(uint32_t) + GetInbetweenAlignmentSpace(sizeof(uint32_t), minStorageBufferOffsetAlignment))*currentFrame, NUM_MODELS, sizeof(uint32_t) * 3);

	vkCmdEndRenderPass(commandBuffer);

	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
		LOG("Error recording the command buffer");
	}
}

void ModuleVulkan::SetModelMatrix(const glm::mat4& model)
{
	memcpy(transformsBufferPtr[currentFrame], &model, sizeof(float) * 16);
}

void ModuleVulkan::SetCameraInfo(const glm::mat4& viewProj, const glm::vec3& cameraPos)
{
	memcpy(static_cast<char*>(transformsBufferPtr[currentFrame]), &viewProj, sizeof(float) * 16);
	memcpy(static_cast<char*>(transformsBufferPtr[currentFrame]) + sizeof(float) * 16, &cameraPos, sizeof(cameraPos));
}

bool ModuleVulkan::CheckVulkanExtensionsSupport(const char* const* ppEnabledExtensionNames, const unsigned int enabledExtensionCount)
{
	Uint32 extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
	VkExtensionProperties* extensionProperties = new VkExtensionProperties[extensionCount];
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensionProperties);
	bool found = true;
	for (int j = 0; j < enabledExtensionCount; ++j)
	{
		found = false;
		for (int i = 0; i < extensionCount; ++i)
		{
			if (strcmp(extensionProperties[i].extensionName, ppEnabledExtensionNames[j]) == 0)
			{
				LOG("VkExtension %s found", ppEnabledExtensionNames[j]);
				found = true;
				break;
			}
		}
		if (!found)
		{
			LOG("Error: VkExtension %s not present", ppEnabledExtensionNames[j]);
			break;
		}
	}
	delete[] extensionProperties;
	return found;
}

bool ModuleVulkan::CheckVulkanLayersSupport(const char* const* validationLayerNames, const unsigned int numLayers)
{
	unsigned int layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
	VkLayerProperties* availableLayers = new VkLayerProperties[layerCount];
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers);
	bool found = true;
	for (int j = 0; j < numLayers; ++j)
	{
		found = false;
		for (int i = 0; i < layerCount; ++i)
		{
			if (strcmp(availableLayers[i].layerName, validationLayerNames[j]) == 0)
			{
				LOG("VkLayer %s found", validationLayerNames[j]);
				found = true;
				break;
			}
		}
		if (!found)
		{
			LOG("Warning: VkLayer %s not present", validationLayerNames[j]);
			break;
		}
	}
	delete[] availableLayers;
	return found;
}

bool ModuleVulkan::CheckDeviceExtensionSupport(VkPhysicalDevice device, const char* const* ppEnabledExtensionNames, const unsigned int enabledExtensionCount)
{
	Uint32 extensionCount = 0;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
	VkExtensionProperties* extensionProperties = new VkExtensionProperties[extensionCount];
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensionProperties);
	bool found = true;
	for (int j = 0; j < enabledExtensionCount; ++j)
	{
		found = false;
		for (int i = 0; i < extensionCount; ++i)
		{
			if (strcmp(extensionProperties[i].extensionName, ppEnabledExtensionNames[j]) == 0)
			{
				LOG("VkExtension %s found in physical device", ppEnabledExtensionNames[j]);
				found = true;
				break;
			}
		}
		if (!found)
		{
			LOG("Error: VkExtension %s not present in physical device", ppEnabledExtensionNames[j]);
			break;
		}
	}
	delete[] extensionProperties;
	return found;
}

uint32_t ModuleVulkan::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
			return i;

	LOG("Failed to find suitable memory type!");
	return UINT32_MAX;
}

bool ModuleVulkan::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
{
	VkBufferCreateInfo bufferCreateInfo{};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = size;
	bufferCreateInfo.usage = usage;//VK_SHADER_STAGE_FRAGMENT_BIT;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if (vkCreateBuffer(device, &bufferCreateInfo, nullptr, &buffer))
	{
		LOG("Error creating the buffer");
		return false;
	}

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(device, buffer, &memRequirements);
	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);//VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) 
	{
		LOG("failed to allocate buffer memory!");
		vkDestroyBuffer(device, buffer, nullptr);
		return false;
	}
	if (vkBindBufferMemory(device, buffer, bufferMemory, 0))
	{
		vkDestroyBuffer(device, buffer, nullptr);
		vkFreeMemory(device, bufferMemory, nullptr);
		LOG("Error binding the new buffer to the buffermemory");
		return false;
	}

	return true;
}

bool ModuleVulkan::CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) {
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = format;
	imageInfo.tiling = tiling;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = usage;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
		LOG("Failed to create image!");
		return false;
	}

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(device, image, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
		LOG("Failed to allocate image memory!");
		return false;
	}

	vkBindImageMemory(device, image, imageMemory, 0);
	return true;
}

void ModuleVulkan::GenerateMeshlet(Mesh& mesh, MeshletMesh& meshletMesh) const
{
	meshletMesh.maxMeshlets = meshopt_buildMeshletsBound(mesh.numIndices, meshletMaxOutputVertices, meshletMaxOutputPrimitives);
	meshopt_Meshlet* meshlets = new meshopt_Meshlet[meshletMesh.maxMeshlets];
	unsigned int* meshletVertices = new unsigned int[mesh.numIndices];
	unsigned char* meshletTriangles = new unsigned char[mesh.numIndices];
	meshletMesh.meshletCount = meshopt_buildMeshlets(meshlets, meshletVertices, meshletTriangles, mesh.indices, mesh.numIndices, &mesh.vertices->position[0], mesh.numVertices, sizeof(Vertex), meshletMaxOutputVertices, meshletMaxOutputPrimitives, 0.0f);
	for (int i = 0; i < meshletMesh.meshletCount; ++i)
		meshopt_optimizeMeshlet(&meshletVertices[meshlets[i].vertex_offset], &meshletTriangles[meshlets[i].triangle_offset], meshlets[i].triangle_count, meshlets[i].vertex_count);
	meshletMesh.meshlets = new meshopt_Meshlet[meshletMesh.meshletCount];
	memcpy(meshletMesh.meshlets, meshlets, sizeof(meshopt_Meshlet) * meshletMesh.meshletCount);
	delete[] meshlets;
	const meshopt_Meshlet& last = meshletMesh.meshlets[meshletMesh.meshletCount - 1];
	unsigned int trimedSize = last.vertex_offset + last.vertex_count;
	meshletMesh.meshletVertices = new unsigned int[trimedSize];
	memcpy(meshletMesh.meshletVertices, meshletVertices, sizeof(unsigned int) * trimedSize);
	delete[] meshletVertices;
	trimedSize = (last.triangle_offset + last.triangle_count) * sizeof(unsigned char) * 3;
	meshletMesh.meshletTriangles = new unsigned char[trimedSize];
	memcpy(meshletMesh.meshletTriangles, meshletTriangles, trimedSize);
	delete[] meshletTriangles;
	memcpy(&meshletMesh.mesh, &mesh, sizeof(Mesh));
	mesh.indices = nullptr;
	mesh.vertices = nullptr;
	mesh.numIndices = 0;
	mesh.numVertices = 0;

	meshletMesh.meshletBounds = new meshopt_Bounds[meshletMesh.meshletCount];
	for (int i = 0; i < meshletMesh.meshletCount; ++i)
		meshletMesh.meshletBounds[i] = meshopt_computeMeshletBounds(&meshletMesh.meshletVertices[meshletMesh.meshlets[i].vertex_offset], &meshletMesh.meshletTriangles[meshletMesh.meshlets[i].triangle_offset], meshletMesh.meshlets[i].triangle_count, reinterpret_cast<float*>(meshletMesh.mesh.vertices), meshletMesh.mesh.numVertices, sizeof(Vertex));
}

unsigned int MeshletMesh::GetMeshletsVerticeCount()
{
	const meshopt_Meshlet& last = meshlets[meshletCount - 1];
	return last.vertex_offset + last.vertex_count;
}

unsigned int MeshletMesh::GetMeshletsTriangleCount()
{
	const meshopt_Meshlet& last = meshlets[meshletCount - 1];
	return (last.triangle_offset + last.triangle_count) * sizeof(unsigned char) * 3;
}

bool ModuleVulkan::FindSupportedFormat(const VkFormat* candidates, size_t numCandidates, VkImageTiling tiling, VkFormatFeatureFlags features, VkFormat& out, VkPhysicalDevice* pDevice)
{
	for (int i = 0; i < numCandidates; ++i)
	{
		VkFormatProperties props;
		if(pDevice != nullptr)
			vkGetPhysicalDeviceFormatProperties(*pDevice, candidates[i], &props);
		else
			vkGetPhysicalDeviceFormatProperties(physicalDevice, candidates[i], &props);

		if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
			out = candidates[i];
			return true;
		}
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
			out = candidates[i];
			return true;
		}
	}

	LOG("Failed to find supported format!");
	return false;
}

void AABB::Generate(const Mesh& mesh)
{
	assert("mesh does not have vertices" && mesh.numVertices != 0);
	minPoint = glm::vec3(mesh.vertices[0].position[0], mesh.vertices[0].position[1], mesh.vertices[0].position[2]);
	maxPoint = minPoint;
	for (int i = 0; i < mesh.numVertices; ++i)
	{
		const glm::vec3 pos = { mesh.vertices[i].position[0], mesh.vertices[i].position[1], mesh.vertices[i].position[2] };
		minPoint = glm::min(pos, minPoint);
		maxPoint = glm::max(pos, maxPoint);
	}
}

void AABB::GetPoints(glm::vec3(&points)[8]) const
{

	points[0] = maxPoint;
	points[1] = glm::vec3(maxPoint.x, minPoint.y, maxPoint.z);
	points[2] = glm::vec3(maxPoint.x, minPoint.y, minPoint.z);
	points[3] = glm::vec3(maxPoint.x, maxPoint.y, minPoint.z);
	points[4] = minPoint;
	points[5] = glm::vec3(minPoint.x, maxPoint.y, maxPoint.z);
	points[6] = glm::vec3(minPoint.x, minPoint.y, maxPoint.z);
	points[7] = glm::vec3(minPoint.x, maxPoint.y, minPoint.z);
}