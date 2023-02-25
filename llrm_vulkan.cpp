#include <algorithm>
#include <cassert>
#include <functional>
#include <iostream>

#include "llrm_vulkan.h"
#include <unordered_set>
#include <vector>

#ifdef LLRM_VULKAN

// LLRM uses glfw for now, move away from this to become independent from the windowing framework.
#include "GLFW/glfw3.h"

// Defined in the windows header
#undef max
#undef min

#if LLRM_VULKAN_VALIDATION
	constexpr bool bEnableValidation = true;
#else
	constexpr bool bEnableValidation = false;
#endif

#if LLRM_VULKAN_MOLTENVK
	constexpr bool bUsingMoltenVK = true;
#else
	constexpr bool bUsingMoltenVK = false;
#endif

// //////////////////////////////////////////
// Vulkan backend helper functions
// //////////////////////////////////////////

void GetInstanceExtensions(std::vector<const char*>& OutExt)
{
	// Get GLFW extensions
	uint32_t GLFWExtensionCount = 0;
	const char** GLFWExtensions = glfwGetRequiredInstanceExtensions(&GLFWExtensionCount);

	if (GLFWExtensions)
	{
		for (uint32_t Ext = 0; Ext < GLFWExtensionCount; Ext++)
		{
			OutExt.emplace_back(GLFWExtensions[Ext]);
		}
	}

	// Add debug utils if we're using validation layers
	if constexpr (bEnableValidation)
	{
		OutExt.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	if constexpr (bUsingMoltenVK)
	{
		// MoltenVK needs this extension
		OutExt.emplace_back("VK_KHR_get_physical_device_properties2");
	}

	// TODO: Add other required extensions
}

bool CheckSupportedInstanceExtensions(const std::vector<const char*>& RequiredExtensions)
{
	// Get number of supported extensions
	uint32_t SupportedExtensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &SupportedExtensionCount, nullptr);

	// Get the extensions
	std::vector<VkExtensionProperties> SupportedExtensions(SupportedExtensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &SupportedExtensionCount, SupportedExtensions.data());

	//GLog->info("Supported Vulkan extensions: ");
	std::unordered_set<std::string> SupportedExtenstionStrings;
	for (const auto& SupportedExt : SupportedExtensions)
	{
		//GLog->info(std::string("\t") + SupportedExt.extensionName);
		SupportedExtenstionStrings.insert(SupportedExt.extensionName);
	}

	bool bAllExtensionsSupported = true;
	for (const auto& RequiredExt : RequiredExtensions)
	{
		if (SupportedExtenstionStrings.find(RequiredExt) == SupportedExtenstionStrings.end())
		{
			// Extension not found
			//GLog->error(std::string("Required instance extension not supported: ") + RequiredExt);
			bAllExtensionsSupported = false;
		}
	}

	return bAllExtensionsSupported;
}


bool CheckSupportedValidationLayers(const std::vector<const char*> RequiredValidationLayers)
{
	uint32_t LayerCount = 0;
	vkEnumerateInstanceLayerProperties(&LayerCount, nullptr);

	std::vector<VkLayerProperties> AvailableLayers(LayerCount);
	vkEnumerateInstanceLayerProperties(&LayerCount, AvailableLayers.data());

	std::unordered_set<std::string> AvailableLayerStrings;
	//GLog->info("Supported Vulkan validation layers: ");
	for (const auto& AvailableLayer : AvailableLayers)
	{
		//GLog->info(std::string("\t") + AvailableLayer.layerName);
		AvailableLayerStrings.insert(AvailableLayer.layerName);
	}

	bool bAllValidationLayersSupported = true;
	for (const auto& RequiredLayer : RequiredValidationLayers)
	{
		if (AvailableLayerStrings.find(RequiredLayer) == AvailableLayerStrings.end())
		{
			//GLog->error(std::string("Required validation layer not supported: ") + RequiredLayer);
			bAllValidationLayersSupported = false;
		}
	}

	return bAllValidationLayersSupported;
}

/**
 * Calculates a suitability score for a particular device.
 */
uint32_t CalcPhysicalDeviceScore(const VkPhysicalDevice& PhysicalDevice)
{
	uint32_t DeviceScore = 0;

	VkPhysicalDeviceProperties DeviceProperties;
	vkGetPhysicalDeviceProperties(PhysicalDevice, &DeviceProperties);

	VkPhysicalDeviceFeatures DeviceFeatures;
	vkGetPhysicalDeviceFeatures(PhysicalDevice, &DeviceFeatures);

	// Discrete GPUs get precedence
	if (DeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
	{
		DeviceScore += 1000;
	}

	return DeviceScore;
}

bool CheckSupportedPhysicalDeviceExtensions(const VkPhysicalDevice& PhysicalDevice, const std::vector<const char*> RequiredDeviceExtensions)
{
	uint32_t ExtensionCount;
	vkEnumerateDeviceExtensionProperties(PhysicalDevice, nullptr, &ExtensionCount, nullptr);

	std::vector<VkExtensionProperties> AvailableExtensions(ExtensionCount);
	vkEnumerateDeviceExtensionProperties(PhysicalDevice, nullptr, &ExtensionCount, AvailableExtensions.data());

	// Get the name of the device for debugging
	VkPhysicalDeviceProperties DeviceProperties;
	vkGetPhysicalDeviceProperties(PhysicalDevice, &DeviceProperties);

	// Put extensions in a set
	//GLog->info(std::string("Available extensions for ") + DeviceProperties.deviceName);

	std::unordered_set<std::string> AvailableExtensionStrings;
	for (const auto& SupportedExtension : AvailableExtensions)
	{
		//GLog->info(std::string("\t") + SupportedExtension.extensionName);
		AvailableExtensionStrings.insert(SupportedExtension.extensionName);
	}

	// Ensure all extensions are supported
	for (const auto& RequiredExtension : RequiredDeviceExtensions)
	{
		if (AvailableExtensionStrings.find(RequiredExtension) == AvailableExtensionStrings.end())
		{
			//GLog->error(std::string("Required device extension not supported: ") + RequiredExtension);
			return false;
		}
	}

	return true;
}

bool PickPhysicalDevice(
	const VkInstance& Instance,
	std::vector<const char*> RequiredDeviceExtensions,
	VkPhysicalDevice& OutDevice,
	uint32_t& OutGraphicsQueueFamilyIndex,
	uint32_t& OutPresentQueueFamilyIndex
)
{
	uint32_t DeviceCount = 0;
	vkEnumeratePhysicalDevices(Instance, &DeviceCount, nullptr);

	if (DeviceCount == 0)
	{
		//GLog->critical("No GPUs found with Vulkan support");
		return false;
	}

	std::vector<VkPhysicalDevice> PhysicalDevices(DeviceCount);
	vkEnumeratePhysicalDevices(Instance, &DeviceCount, PhysicalDevices.data());

	int32_t MaxDeviceIndex = -1;
	uint32_t MaxDeviceGraphicsQueueFam = 0;
	uint32_t MaxDevicePresentQueueFam = 0;
	uint32_t MaxDeviceScore = 0;
	for (uint32_t CurDeviceIndex = 0; CurDeviceIndex < PhysicalDevices.size(); CurDeviceIndex++)
	{
		const auto& PhysicalDevice = PhysicalDevices[CurDeviceIndex];

		// Determine if graphics card has a queue family that supports the graphics queue bit
		int32_t GraphicsQueueFam = -1;
		int32_t PresentQueueFam = -1;
		bool bSupportsExtensions = CheckSupportedPhysicalDeviceExtensions(PhysicalDevice, RequiredDeviceExtensions); // Determine if device supports required extensions
		bool bSupportsSurface = true;
		{
			uint32_t QueueFamilyCount = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &QueueFamilyCount, nullptr);

			std::vector<VkQueueFamilyProperties> QueueFamProperties(QueueFamilyCount);
			vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &QueueFamilyCount, QueueFamProperties.data());
			for (uint32_t QueueFamIndex = 0; QueueFamIndex < QueueFamilyCount; QueueFamIndex++)
			{
				// Check if this queue family supports graphics rendering. Save result if so.
				const auto& QueueFamProps = QueueFamProperties[QueueFamIndex];
				if (QueueFamProps.queueFlags & VK_QUEUE_GRAPHICS_BIT)
				{
					GraphicsQueueFam = static_cast<int32_t>(QueueFamIndex);
				}

				// Check if this queue family supports presentation to the created surface. If so, save this result.
				if(glfwGetPhysicalDevicePresentationSupport(Instance, PhysicalDevice, QueueFamIndex) == GLFW_TRUE)
				{
					PresentQueueFam = static_cast<int32_t>(QueueFamIndex);
				}
			}
		}

		bool bSupportsFeatures = true;
		{
			VkPhysicalDeviceFeatures PDFeatures{};
			vkGetPhysicalDeviceFeatures(PhysicalDevice, &PDFeatures);

			if (PDFeatures.independentBlend != VK_TRUE)
			{
				bSupportsFeatures = false;
			}
		}

		// Check that the device meets all of the requirements before considering it for scoring
		if (bSupportsFeatures && bSupportsExtensions && bSupportsSurface && GraphicsQueueFam >= 0 && PresentQueueFam >= 0)
		{
			uint32_t DeviceScore = CalcPhysicalDeviceScore(PhysicalDevice);
			if (DeviceScore >= MaxDeviceScore)
			{
				MaxDeviceScore = DeviceScore;
				MaxDeviceGraphicsQueueFam = GraphicsQueueFam;
				MaxDevicePresentQueueFam = PresentQueueFam;
				MaxDeviceIndex = CurDeviceIndex;
			}
		}

	}

	if (MaxDeviceIndex >= 0)
	{
		OutDevice = PhysicalDevices[MaxDeviceIndex];
		OutGraphicsQueueFamilyIndex = MaxDeviceGraphicsQueueFam;
		OutPresentQueueFamilyIndex = MaxDevicePresentQueueFam;
		return true;
	}
	else
	{
		return false;
	}
}

// Vulkan debug callback
VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT MessageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT MessageType,
	const VkDebugUtilsMessengerCallbackDataEXT* CallbackData,
	void* UserData)
{
	bool bWarning = MessageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
	bool bError = MessageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

	std::string TraceString = "";

#if GENERATE_STACKTRACES
	if (bWarning || bError)
	{
		auto Stack = boost::stacktrace::stacktrace();
		for (boost::stacktrace::frame Frame : Stack)
		{
			TraceString += "\t" + Frame.source_file() + ":" + std::to_string(Frame.source_line()) + '\n';
		}
	}
#endif

	// TODO: Provide API for message callbacks
	if (bWarning)
	{
		std::cout << "[vkWarning]: " << CallbackData->pMessage << std::endl;
		//GLog->warn(std::string(" ") +  + "\n" + TraceString);
	}
	else if (bError)
	{
		std::cout << "[vkError]: " << CallbackData->pMessage << std::endl;

		//GLog->error(std::string("[vkError]: ") + CallbackData->pMessage + "\n" + TraceString);
	}

	return VK_FALSE;
}


inline int32_t FindMemoryType(VkPhysicalDevice PhysicalDevice, uint32_t TypeFilter, VkMemoryPropertyFlags Properties)
{
	VkPhysicalDeviceMemoryProperties MemProperties;
	vkGetPhysicalDeviceMemoryProperties(PhysicalDevice, &MemProperties);

	for (uint32_t MemTypeIndex = 0; MemTypeIndex < MemProperties.memoryTypeCount; MemTypeIndex++)
	{
		bool bTypeSupported = TypeFilter & (1 << MemTypeIndex);
		bool bFlagsSupported = MemProperties.memoryTypes[MemTypeIndex].propertyFlags & Properties;
		if (bTypeSupported && bFlagsSupported)
		{
			return static_cast<int32_t>(MemTypeIndex);
		}
	}

	return -1;
}

/*inline void CreateDsc(VkDevice Device)
{
	std::vector<VkDescriptorSetLayoutBinding> Bindings;

	VkDescriptorSetLayoutBinding UniformBuffer{};
	UniformBuffer.binding = 0;
	UniformBuffer.descriptorCount = 1;
	UniformBuffer.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	UniformBuffer.pImmutableSamplers = nullptr;
	UniformBuffer.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	Bindings = {
		UniformBuffer
	};

	VkDescriptorSetLayoutCreateInfo CreateInfo{};
	CreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	CreateInfo.bindingCount = static_cast<uint32_t>(Bindings.size());
	CreateInfo.pBindings = Bindings.data();

	VkDescriptorSetLayout Layout;
	if (vkCreateDescriptorSetLayout(Device, &CreateInfo, nullptr, &Layout) != VK_SUCCESS)
	{
		return;
	}
}*/

namespace llrm
{
	VulkanContext GVulkanContext;

	llrm::Context CreateContext()
	{
		VulkanContext* VkContext = new ::VulkanContext;

		// Check that all needed instance extenstions are supported
		GetInstanceExtensions(VkContext->InstanceExtensions);
		if (!CheckSupportedInstanceExtensions(VkContext->InstanceExtensions))
		{
			delete VkContext;
			return nullptr;
		}

		// Check that all needed validation layers are supported
		if constexpr (bEnableValidation)
		{
			VkContext->ValidationLayers = { "VK_LAYER_KHRONOS_validation" };
			if (!CheckSupportedValidationLayers(VkContext->ValidationLayers))
			{
				delete VkContext;
				return nullptr;
			}
		}

		VkDebugUtilsMessengerCreateInfoEXT DbgCreateInfo{};
		DbgCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		DbgCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		DbgCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		DbgCreateInfo.pfnUserCallback = &VulkanDebugCallback;
		DbgCreateInfo.pUserData = nullptr; // Optional

		VkApplicationInfo AppInfo{};
		AppInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		AppInfo.pApplicationName = "NewEngine";
		AppInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		AppInfo.pEngineName = "NewEngine";
		AppInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		AppInfo.apiVersion = VK_API_VERSION_1_2;

		VkInstanceCreateInfo CreateInfo{};
		CreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		CreateInfo.pApplicationInfo = &AppInfo;
		CreateInfo.enabledExtensionCount = static_cast<uint32_t>(VkContext->InstanceExtensions.size());
		CreateInfo.ppEnabledExtensionNames = VkContext->InstanceExtensions.data();
		CreateInfo.enabledLayerCount = static_cast<uint32_t>(VkContext->ValidationLayers.size());
		CreateInfo.ppEnabledLayerNames = VkContext->ValidationLayers.data();

		// Setup debug messenger for instance creation if validation is enabled
		if constexpr (bEnableValidation)
		{
			CreateInfo.pNext = reinterpret_cast<void*>(&DbgCreateInfo);
		}

		if (vkCreateInstance(&CreateInfo, nullptr, &VkContext->Instance) != VK_SUCCESS)
		{
			delete VkContext;
			return nullptr;
		}

		// Setup debug messenger if validation layers are enabled
		if constexpr (bEnableValidation)
		{
			if (auto DebugFunction = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(VkContext->Instance, "vkCreateDebugUtilsMessengerEXT"))
			{
				if (DebugFunction(VkContext->Instance, &DbgCreateInfo, nullptr, &VkContext->DebugMessenger) != VK_SUCCESS)
				{
					//GLog->error("Failed to create debug messenger");
					delete VkContext;
					return nullptr;
				}
			}
			else
			{
				//GLog->error("Failed to load Vulkan debug procedure address");
				delete VkContext;
				return nullptr;
			}
		}


		std::vector<const char*> RequiredDeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

#ifdef HOST_PLATFORM_OSX
		// MoltenVK needs this extension
		RequiredDeviceExtensions.emplace_back("VK_KHR_portability_subset");
#endif

		// Find the best physical device for our application
		if (!PickPhysicalDevice(
			VkContext->Instance,
			RequiredDeviceExtensions,
			VkContext->PhysicalDevice,
			VkContext->GraphicsQueueFamIndex, VkContext->PresentQueueFamIndex)
		)
		{
			//GLog->critical("No eligable GPUs found. GPU must have queue families supporting graphics and presentation and have the required extensions.");
			return nullptr;
		}

		VkPhysicalDeviceProperties DeviceProperties;
		vkGetPhysicalDeviceProperties(VkContext->PhysicalDevice, &DeviceProperties);
		//GLog->info(std::string("Using physical device: ") + DeviceProperties.deviceName);

		// Only create as many queues as we need
		std::unordered_set QueueFamilySet = { VkContext->GraphicsQueueFamIndex, VkContext->PresentQueueFamIndex };
		std::vector<VkDeviceQueueCreateInfo> QueueCreateInfos;
		for (uint32_t UniqueQueueFamily : QueueFamilySet)
		{
			float QueuePriority = 1.0f;
			VkDeviceQueueCreateInfo UniqueQueueCreateInfo{};
			UniqueQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			UniqueQueueCreateInfo.queueFamilyIndex = UniqueQueueFamily;
			UniqueQueueCreateInfo.queueCount = 1;
			UniqueQueueCreateInfo.pQueuePriorities = &QueuePriority;

			QueueCreateInfos.push_back(UniqueQueueCreateInfo);
		}

		VkPhysicalDeviceFeatures UsedDeviceFeatures{};
		UsedDeviceFeatures.independentBlend = VK_TRUE;

		VkDeviceCreateInfo DeviceCreateInfo{};
		DeviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		DeviceCreateInfo.pQueueCreateInfos = QueueCreateInfos.data();
		DeviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(QueueCreateInfos.size());
		DeviceCreateInfo.pEnabledFeatures = &UsedDeviceFeatures;
		DeviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(VkContext->ValidationLayers.size());
		DeviceCreateInfo.ppEnabledLayerNames = VkContext->ValidationLayers.data();
		DeviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(RequiredDeviceExtensions.size());
		DeviceCreateInfo.ppEnabledExtensionNames = RequiredDeviceExtensions.data();

		if (vkCreateDevice(VkContext->PhysicalDevice, &DeviceCreateInfo, nullptr, &VkContext->Device) != VK_SUCCESS)
		{
			return nullptr;
		}

		// Retrieve the graphics and presentation queues
		vkGetDeviceQueue(VkContext->Device, VkContext->GraphicsQueueFamIndex, 0, &VkContext->GraphicsQueue);
		vkGetDeviceQueue(VkContext->Device, VkContext->PresentQueueFamIndex, 0, &VkContext->PresentQueue);

		// Create the primary command pool
		VkCommandPoolCreateInfo CmdPoolCreateInfo{};
		CmdPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		CmdPoolCreateInfo.queueFamilyIndex = VkContext->GraphicsQueueFamIndex;
		CmdPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

		if (vkCreateCommandPool(VkContext->Device, &CmdPoolCreateInfo, nullptr, &VkContext->MainCommandPool) != VK_SUCCESS)
		{
			//GLog->critical("Failed to create primary command pool");
			return nullptr;
		}

		// Give ImGui an oversized descriptor pool
		VkDescriptorPoolSize PoolSizes[] =
		{
			{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
		};

		// Create primary descriptor pool
		VkDescriptorPoolCreateInfo DscPoolCreateInfo{};
		DscPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		DscPoolCreateInfo.maxSets = 1000;
		DscPoolCreateInfo.poolSizeCount = static_cast<uint32_t>(std::size(PoolSizes));
		DscPoolCreateInfo.pPoolSizes = PoolSizes;
		DscPoolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

		if (vkCreateDescriptorPool(VkContext->Device, &DscPoolCreateInfo, nullptr, &VkContext->MainDscPool) != VK_SUCCESS)
		{
			//GLog->critical("Failed to create primary descriptor pool");
			return nullptr;
		}

		GVulkanContext = *VkContext;
		return VkContext;
	}

	void DestroyContext(Context Context)
	{
		VulkanContext* VkContext = static_cast<VulkanContext*>(Context);

		if constexpr (bEnableValidation)
		{
			// TODO: Add back resource allocation tracking. C++23 stacktraces?
			// Look for resources that haven't been freed with backtrace
			/*for (auto Resource : AllocatedTraces)
			{
				std::string TraceString = "";
				for (boost::stacktrace::frame Frame : Resource.second)
				{
					TraceString += Frame.source_file() + ":" + std::to_string(Frame.source_line()) + '\n';
				}
				GLog->warn("Allocated vulkan resource was not freed: \n" + TraceString);
			}*/
		}

		// Cleanup primary descriptor pool
		vkDestroyDescriptorPool(VkContext->Device, VkContext->MainDscPool, nullptr);

		// Cleanup primary command pool
		vkDestroyCommandPool(VkContext->Device, VkContext->MainCommandPool, nullptr);

		// Cleanup logical device
		vkDestroyDevice(VkContext->Device, nullptr);

		// Destroy debug messenger if validation is enabled
		if constexpr (bEnableValidation)
		{
			if (auto DestroyDbgFunc = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(VkContext->Instance, "vkDestroyDebugUtilsMessengerEXT"))
			{
				DestroyDbgFunc(VkContext->Instance, VkContext->DebugMessenger, nullptr);
			}
		}

		// Destroy vulkan 
		vkDestroyInstance(VkContext->Instance, nullptr);
		delete VkContext;
	}

	void SetContext(Context Context)
	{
		GVulkanContext = *static_cast<VulkanContext*>(Context);
	}

	Surface CreateSurface(GLFWwindow* Window)
	{
		VulkanSurface* NewSurface = new VulkanSurface;

		if (glfwCreateWindowSurface(GVulkanContext.Instance, Window, nullptr, &NewSurface->VkSurface) != VK_SUCCESS)
		{
			//GLog->error("Failed to create Vulkan surface");
			return nullptr;
		}

		//Target->RenderAPISurface = NewSurface;

		RECORD_RESOURCE_ALLOC(NewSurface)

		return NewSurface;
	}

	void DestroySurface(Surface Surface)
	{
		VulkanSurface* VkSurface = static_cast<VulkanSurface*>(Surface);

		vkDeviceWaitIdle(GVulkanContext.Device);

		// Cleanup primary surface
		vkDestroySurfaceKHR(GVulkanContext.Instance, VkSurface->VkSurface, nullptr);

		REMOVE_RESOURCE_ALLOC(Surface);

		delete VkSurface;
	}

	void VkCmdBuffer(CommandBuffer Cmd, std::function<void(VkCommandBuffer&)> Inner)
	{
		VulkanCommandBuffer* VkCmd = static_cast<VulkanCommandBuffer*>(Cmd);
		Inner(VkCmd->CmdBuffer);
	};

	VkFormat AttachmentFormatToVkFormat(AttachmentFormat Format)
	{
		//	assert((Format == AttachmentFormat::MatchBackBuffer && ReferencingSwap) || (Format != AttachmentFormat::MatchBackBuffer && !ReferencingSwap()));

		switch (Format)
		{
		case AttachmentFormat::B8G8R8A8_SRGB:
			return VK_FORMAT_B8G8R8A8_SRGB;
		case AttachmentFormat::B8G8R8A8_UNORM:
			return VK_FORMAT_B8G8R8A8_UNORM;
		case AttachmentFormat::R32_UINT:
			return VK_FORMAT_R32_UINT;
		case AttachmentFormat::R32_FLOAT:
			return VK_FORMAT_R32_SFLOAT;
		case AttachmentFormat::R32_SINT:
			return VK_FORMAT_R32_SINT;
		case AttachmentFormat::R8_UINT:
			return VK_FORMAT_R8_UINT;
		case AttachmentFormat::D24_UNORM_S8_UINT:
			VkFormat DepthStencilFormat = VK_FORMAT_D24_UNORM_S8_UINT;
			VkFormatProperties FormatProps;
			vkGetPhysicalDeviceFormatProperties(GVulkanContext.PhysicalDevice, DepthStencilFormat, &FormatProps);
			VkFormatFeatureFlags RequiredFlags = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;

			// Assume optimal tiling, create assertion for our required flags
			if ((FormatProps.optimalTilingFeatures & RequiredFlags) != RequiredFlags)
			{
				//GLog->error("Physical device does not support depth/stencil attachment");
				std::abort();
			}

			return DepthStencilFormat;
		}

		//GLog->error("Attachment not found");
		return VK_FORMAT_B8G8R8A8_SRGB;
	}

	AttachmentFormat VkFormatToAttachmentFormat(VkFormat Format)
	{

		switch (Format)
		{
		case VK_FORMAT_B8G8R8A8_SRGB:
			return AttachmentFormat::B8G8R8A8_SRGB;
		case VK_FORMAT_B8G8R8A8_UNORM:
			return AttachmentFormat::B8G8R8A8_UNORM;

		}

		//GLog->error("Attachment not found");
		return AttachmentFormat::B8G8R8A8_UNORM;
	}

	VkImageLayout AttachmentUsageToVkLayout(AttachmentUsage Usage)
	{
		switch (Usage)
		{
		case AttachmentUsage::ColorAttachment:
			return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		case AttachmentUsage::DepthStencilAttachment:
			return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		case AttachmentUsage::Presentation:
			return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		case AttachmentUsage::TransferDestination:
			return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		case AttachmentUsage::TransferSource:
			return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		case AttachmentUsage::ShaderRead:
			return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		case AttachmentUsage::Undefined:
			return VK_IMAGE_LAYOUT_UNDEFINED;
		default:
			return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}
	}

	VkBlendOp BlendOpToVkBlend(BlendOperation Operator)
	{
		switch (Operator)
		{
		case BlendOperation::Add:
			return VK_BLEND_OP_ADD;
		case BlendOperation::Max:
			return VK_BLEND_OP_MAX;
		case BlendOperation::Min:
			return VK_BLEND_OP_MIN;
		}

		return VK_BLEND_OP_ADD;
	}

	VkFilter FilterToVkFilter(FilterType Filter)
	{
		switch (Filter)
		{
		case FilterType::LINEAR:
			return VK_FILTER_LINEAR;
		case FilterType::NEAREST:
			return VK_FILTER_NEAREST;
		}

		return VK_FILTER_LINEAR;
	}

	VkBlendFactor BlendFactorToVkFactor(BlendFactor Factor)
	{
		switch (Factor)
		{
		case BlendFactor::One:
			return VK_BLEND_FACTOR_ONE;
		case BlendFactor::Zero:
			return VK_BLEND_FACTOR_ZERO;
		case BlendFactor::SrcAlpha:
			return VK_BLEND_FACTOR_SRC_ALPHA;
		case BlendFactor::SrcColor:
			return VK_BLEND_FACTOR_SRC_COLOR;
		case BlendFactor::DstAlpha:
			return VK_BLEND_FACTOR_DST_ALPHA;
		case BlendFactor::DstColor:
			return VK_BLEND_FACTOR_DST_COLOR;
		case BlendFactor::OneMinusSrcColor:
			return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
		case BlendFactor::OneMinusSrcAlpha:
			return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		case BlendFactor::OneMinusDstColor:
			return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
		case BlendFactor::OneMinusDstAlpha:
			return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
		}

		//GLog->error("Blend factor not found");
		return VK_BLEND_FACTOR_ONE;
	}

	bool CreateFrameBufferResource(VulkanFrameBuffer* DstFbo, VkRenderPass Pass, uint32_t Width, uint32_t Height)
	{
		std::vector<VkImageView> Attachments;
		for (VulkanTexture* Attach : DstFbo->AllAttachments)
			Attachments.push_back(Attach->TextureImageView);

		// Finally, create the framebuffer
		VkFramebufferCreateInfo FrameBufferCreateInfo{};
		FrameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		FrameBufferCreateInfo.renderPass = Pass;
		FrameBufferCreateInfo.attachmentCount = static_cast<uint32_t>(Attachments.size());
		FrameBufferCreateInfo.pAttachments = Attachments.data();
		FrameBufferCreateInfo.width = Width;
		FrameBufferCreateInfo.height = Height;
		FrameBufferCreateInfo.layers = 1;

		if (vkCreateFramebuffer(GVulkanContext.Device, &FrameBufferCreateInfo, nullptr, &DstFbo->VulkanFbo) != VK_SUCCESS)
		{
			//GLog->critical("Failed to create vulkan framebuffer resource");
			return false;
		}

		return true;
	}

	uint32_t GetCurrentViewportHeight(CommandBuffer Buf)
	{
		VulkanCommandBuffer* VkCmd = static_cast<VulkanCommandBuffer*>(Buf);

		uint32_t ViewportHeight = 0;
		if (VkCmd->CurrentSwapChain)
			ViewportHeight = VkCmd->CurrentSwapChain->SwapChainExtent.height;
		else if (VkCmd->CurrentFbo)
			ViewportHeight = VkCmd->CurrentFbo->AttachmentHeight;
		//else
		//	GLog->error("Invalid swap chain");

		return ViewportHeight;
	}

	bool CreateBuffer(uint64_t Size, VkBufferUsageFlags BufferUsage, VkMemoryPropertyFlags MemPropertyFlags, VkBuffer& OutBuffer, VkDeviceMemory& OutBufferMemory)
	{
		VkBufferCreateInfo VboCreateInfo{};
		VboCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		VboCreateInfo.size = Size;
		VboCreateInfo.usage = BufferUsage;
		VboCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (vkCreateBuffer(GVulkanContext.Device, &VboCreateInfo, nullptr, &OutBuffer) != VK_SUCCESS)
		{
			//GLog->critical("Failed to create vulkan buffer");
			return false;
		}

		VkMemoryRequirements BufferMemRequirements{};
		vkGetBufferMemoryRequirements(GVulkanContext.Device, OutBuffer, &BufferMemRequirements);

		VkMemoryAllocateInfo AllocInfo{};
		AllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		AllocInfo.allocationSize = BufferMemRequirements.size;
		AllocInfo.memoryTypeIndex = FindMemoryType(GVulkanContext.PhysicalDevice, BufferMemRequirements.memoryTypeBits, MemPropertyFlags);

		if (vkAllocateMemory(GVulkanContext.Device, &AllocInfo, nullptr, &OutBufferMemory) != VK_SUCCESS)
		{
			//GLog->critical("Failed to allocate vulkan memory");
			return false;
		}

		// Bind the memory
		if (vkBindBufferMemory(GVulkanContext.Device, OutBuffer, OutBufferMemory, 0) != VK_SUCCESS)
		{
			//GLog->info("Failed to bind memory to vulkan buffer");
			return false;
		}

		return true;
	}

	void Reset(CommandBuffer Buf)
	{
		VkCmdBuffer(Buf, [&](VkCommandBuffer& CmdBuffer)
		{
			vkResetCommandBuffer(CmdBuffer, 0);
		});
	}

	void Begin(CommandBuffer Buf)
	{
		VkCmdBuffer(Buf, [&](VkCommandBuffer& CmdBuffer)
		{
			VulkanCommandBuffer* VkCmd = static_cast<VulkanCommandBuffer*>(Buf);

			VkCommandBufferBeginInfo BeginInfo{};
			BeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			BeginInfo.pInheritanceInfo = nullptr;
			BeginInfo.flags = VkCmd->bOneTimeUse ? VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT : 0;

			if (vkBeginCommandBuffer(CmdBuffer, &BeginInfo) != VK_SUCCESS)
			{
				//GLog->critical("Failed to begin recording vulkan command buffer");
			}
		});
	}

	void TransitionCmd(VkCommandBuffer Buf, VkImage Img, VkImageAspectFlags AspectFlags, AttachmentUsage Old, AttachmentUsage New)
	{
		VkImageMemoryBarrier ImageMemBarrier{};
		ImageMemBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		ImageMemBarrier.oldLayout = AttachmentUsageToVkLayout(Old);
		ImageMemBarrier.newLayout = AttachmentUsageToVkLayout(New);
		ImageMemBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; // Don't currently support queue family transfer
		ImageMemBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		ImageMemBarrier.image = Img;
		ImageMemBarrier.subresourceRange.aspectMask = AspectFlags;
		ImageMemBarrier.subresourceRange.baseMipLevel = 0;
		ImageMemBarrier.subresourceRange.levelCount = 1;
		ImageMemBarrier.subresourceRange.baseArrayLayer = 0;
		ImageMemBarrier.subresourceRange.layerCount = 1;

		VkPipelineStageFlags SourceStage;
		VkPipelineStageFlags DstStage;

		// Here's the hard part. We have to specify possible masks/stages for image layout transitions, so below are the most common ones.

		if (Old == AttachmentUsage::Undefined)
		{
			ImageMemBarrier.srcAccessMask = 0;
			SourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		}
		else if (Old == AttachmentUsage::ShaderRead)
		{
			ImageMemBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			SourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else if (Old == AttachmentUsage::TransferDestination)
		{
			ImageMemBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			SourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (Old == AttachmentUsage::TransferSource)
		{
			ImageMemBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			SourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else
		{
			//GLog->critical("Vulkan image layout transition not supported");
			return;
		}

		if (New == AttachmentUsage::ColorAttachment)
		{
			ImageMemBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			DstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		}
		else if (New == AttachmentUsage::DepthStencilAttachment)
		{
			ImageMemBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			DstStage = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		}
		else if (New == AttachmentUsage::ShaderRead)
		{
			ImageMemBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			DstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else if (New == AttachmentUsage::TransferDestination)
		{
			ImageMemBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			DstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (New == AttachmentUsage::TransferSource)
		{
			ImageMemBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			DstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else
		{
			//GLog->critical("Vulkan image layout transition not supported");
			return;
		}

		vkCmdPipelineBarrier
		(
			Buf,
			SourceStage, DstStage,
			0,
			0, nullptr,
			0, nullptr,
			1,
			&ImageMemBarrier
		);
	}

	void TransitionTexture(CommandBuffer Buf, Texture Image, AttachmentUsage Old,
		AttachmentUsage New)
	{
		VulkanTexture* VkTexture = static_cast<VulkanTexture*>(Image);

		VkCmdBuffer(Buf, [&](VkCommandBuffer& CmdBuffer)
		{
			VkImageAspectFlags Flags = 0;
			if (IsColorFormat(VkTexture->TextureFormat))
				Flags |= VK_IMAGE_ASPECT_COLOR_BIT;
			if (IsDepthFormat(VkTexture->TextureFormat))
				Flags |= VK_IMAGE_ASPECT_DEPTH_BIT;
			if (IsStencilFormat(VkTexture->TextureFormat))
				Flags |= VK_IMAGE_ASPECT_STENCIL_BIT;

			TransitionCmd(CmdBuffer, VkTexture->TextureImage, Flags, Old, New);
		});
	}

	void End(CommandBuffer Buf)
	{
		VkCmdBuffer(Buf, [&](VkCommandBuffer& CmdBuffer)
		{
			vkEndCommandBuffer(CmdBuffer);
		});
	}

	void GetVkClearValues(std::vector<ClearValue> ClearValues, std::vector<VkClearValue>& OutVkValues)
	{
		for (uint32_t ClearValueIndex = 0; ClearValueIndex < ClearValues.size(); ClearValueIndex++)
		{
			ClearValue& CV = ClearValues[ClearValueIndex];
			VkClearValue ClearValue{};
			if (CV.Clear == ClearType::Float)
			{
				float ClearFloats[4] = { CV.FloatClearValue[0], CV.FloatClearValue[1], CV.FloatClearValue[2], CV.FloatClearValue[3]};
				memcpy(ClearValue.color.float32, ClearFloats, sizeof(ClearFloats));
			}
			else if (CV.Clear == ClearType::SInt)
				memcpy(ClearValue.color.int32, CV.IntClearValue, sizeof(CV.IntClearValue));
			else if (CV.Clear == ClearType::UInt)
				memcpy(ClearValue.color.uint32, CV.UIntClearValue, sizeof(CV.UIntClearValue));
			else if (CV.Clear == ClearType::DepthStencil)
				ClearValue.depthStencil = { CV.FloatClearValue[0], CV.UIntClearValue[0]};

			OutVkValues[ClearValueIndex] = ClearValue;
		}
	}
	
	void BeginRenderGraph(CommandBuffer Buf, RenderGraph Graph, FrameBuffer Target, std::vector<ClearValue> ClearValues)
	{
		VulkanRenderGraph* VkRg = static_cast<VulkanRenderGraph*>(Graph);
		VulkanFrameBuffer* VkFbo = static_cast<VulkanFrameBuffer*>(Target);
		VulkanCommandBuffer* VkCmd = static_cast<VulkanCommandBuffer*>(Buf);

		VkCmd->CurrentFbo = VkFbo;

		VkCmdBuffer(Buf, [&](VkCommandBuffer& CmdBuffer)
		{
			VkRenderPassBeginInfo RpBeginInfo{};
			RpBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			RpBeginInfo.renderPass = VkRg->RenderPass;
			RpBeginInfo.framebuffer = VkFbo->VulkanFbo; // This means the cmd buffer needs to be re-created when the swap chain gets recreated
			RpBeginInfo.renderArea.offset = { 0, 0 };
			RpBeginInfo.renderArea.extent = { VkFbo->AttachmentWidth, VkFbo->AttachmentHeight };

			std::vector<VkClearValue> VkValues(ClearValues.size());
			GetVkClearValues(ClearValues, VkValues);

			RpBeginInfo.clearValueCount = static_cast<uint32_t>(VkValues.size());
			RpBeginInfo.pClearValues = VkValues.data();

			vkCmdBeginRenderPass(CmdBuffer, &RpBeginInfo, VK_SUBPASS_CONTENTS_INLINE); // This would need to be changed for secondary command buffers
		});
	}

	void EndRenderGraph(CommandBuffer Buf)
	{
		VulkanCommandBuffer* VkCmd = static_cast<VulkanCommandBuffer*>(Buf);

		VkCmd->CurrentSwapChain = nullptr;
		VkCmd->CurrentFbo = nullptr;

		VkCmdBuffer(Buf, [&](VkCommandBuffer& CmdBuffer)
		{
			vkCmdEndRenderPass(CmdBuffer);
		});
	}

	void BindPipeline(CommandBuffer Buf, Pipeline PipelineObject)
	{
		VulkanCommandBuffer* VkCmd = static_cast<VulkanCommandBuffer*>(Buf);
		VulkanPipeline* VkPipeline = reinterpret_cast<VulkanPipeline*>(PipelineObject);

		// Descriptor sets need to know about the pipeline layout, so store this here
		VkCmd->BoundPipeline = VkPipeline;

		VkCmdBuffer(Buf, [&](VkCommandBuffer& CmdBuffer)
		{
			vkCmdBindPipeline(CmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, VkPipeline->Pipeline);
		});
	}

	void BindResources(CommandBuffer Buf, ResourceSet Resources)
	{
		VulkanCommandBuffer* VkCmd = static_cast<VulkanCommandBuffer*>(Buf);
		VulkanResourceSet* VkRes = reinterpret_cast<VulkanResourceSet*>(Resources);

		uint32_t CurrentFrame = GVulkanContext.CurrentSwapChain->AcquiredImageIndex;
		VkDescriptorSet& Set = VkRes->DescriptorSets[CurrentFrame];

		vkCmdBindDescriptorSets
		(
			VkCmd->CmdBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			VkCmd->BoundPipeline->PipelineLayout,
			0, 1, &Set,
			0, nullptr
		);
	}

	void DrawVertexBuffer(CommandBuffer Buf, VertexBuffer Vbo, uint32_t VertexCount)
	{
		VulkanVertexBuffer* VulkanVbo = static_cast<VulkanVertexBuffer*>(Vbo);

		VkCmdBuffer(Buf, [&](VkCommandBuffer& CmdBuffer)
		{
			VkBuffer VertexBuffers[] = {
				VulkanVbo->DeviceVertexBuffer
			};
			VkDeviceSize Offsets[] = {
				0
			};

			// Bind the vertex buffer
			vkCmdBindVertexBuffers(CmdBuffer, 0, 1, VertexBuffers, Offsets);

			// Draw vertex buffer
			vkCmdDraw(CmdBuffer, VertexCount, 1, 0, 0);
		});
	}

	void DrawVertexBufferIndexed(CommandBuffer Buf, VertexBuffer Vbo, IndexBuffer Ibo, uint32_t IndexCount)
	{
		VulkanVertexBuffer* VulkanVbo = static_cast<VulkanVertexBuffer*>(Vbo);
		VulkanIndexBuffer* VulkanIbo = static_cast<VulkanIndexBuffer*>(Ibo);

		VkCmdBuffer(Buf, [&](VkCommandBuffer& CmdBuffer)
		{
			VkBuffer VertexBuffers[] = {
				VulkanVbo->DeviceVertexBuffer
			};
			VkDeviceSize Offsets[] = {
				0
			};

			// Bind the vertex buffer
			vkCmdBindVertexBuffers(CmdBuffer, 0, 1, VertexBuffers, Offsets);

			// Bind the index buffer (force uint32)
			vkCmdBindIndexBuffer(CmdBuffer, VulkanIbo->DeviceIndexBuffer, 0, VK_INDEX_TYPE_UINT32);

			// Draw indexed vertex buffer
			vkCmdDrawIndexed(CmdBuffer, IndexCount, 1, 0, 0, 0);
		});
	}

	void SetViewport(CommandBuffer Buf, uint32_t X, uint32_t Y, uint32_t W, uint32_t H)
	{
		VkCmdBuffer(Buf, [&](VkCommandBuffer& CmdBuffer)
		{
			VkViewport Viewport;
			Viewport.x = static_cast<float>(X);
			Viewport.y = static_cast<float>(GetCurrentViewportHeight(Buf) - Y);
			Viewport.width = static_cast<float>(W);
			Viewport.height = static_cast<float>(-1.0f * H); // Flip Vulkan viewport
			Viewport.minDepth = 0.0f;
			Viewport.maxDepth = 1.0f;

			vkCmdSetViewport(CmdBuffer, 0, 1, &Viewport);
		});
	}

	void SetScissor(CommandBuffer Buf, uint32_t X, uint32_t Y, uint32_t W, uint32_t H)
	{
		VkCmdBuffer(Buf, [&](VkCommandBuffer& CmdBuffer)
		{
			uint32_t ScissorY = std::max(GetCurrentViewportHeight(Buf) - (Y + H), static_cast<uint32_t>(0));

			VkRect2D Scissor;
			Scissor.offset.x = static_cast<int32_t>(X);
			Scissor.offset.y = ScissorY; // Flip on Y
			Scissor.extent.width = static_cast<uint32_t>(W);
			Scissor.extent.height = static_cast<uint32_t>(H);

			vkCmdSetScissor(CmdBuffer, 0, 1, &Scissor);
		});
	}

	bool CreateShaderModule(const std::vector<uint32_t>& SpvCode, VkShaderModule& OutShaderModule)
	{
		VkShaderModuleCreateInfo CreateInfo{};
		CreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		CreateInfo.codeSize = SpvCode.size() * sizeof(uint32_t);
		CreateInfo.pCode = SpvCode.data();

		VkShaderModule ShaderModule;
		if (vkCreateShaderModule(GVulkanContext.Device, &CreateInfo, nullptr, &ShaderModule) != VK_SUCCESS)
		{
			return false;
		}

		OutShaderModule = ShaderModule;

		return true;
	}

	ShaderProgram CreateRasterProgram(const std::vector<uint32_t>& VertShader, const std::vector<uint32_t>& FragShader)
	{
		VulkanShader* Result = new VulkanShader;

		std::vector<VkShaderModule> ShaderModules;

		// Create vertex shader
		if (!CreateShaderModule(VertShader, Result->VertexModule))
		{
			//GLog->critical("Failed to create shader module");

			delete Result;
			return nullptr;
		}
		else
		{
			ShaderModules.push_back(Result->VertexModule);
			Result->bHasVertexShader = true;
		}

		// Create fragment shader
		if (!CreateShaderModule(FragShader, Result->FragmentModule))
		{
			//GLog->critical("Failed to create shader module");

			delete Result;
			return nullptr;
		}
		else
		{
			ShaderModules.push_back(Result->FragmentModule);
			Result->bHasFragmentShader = true;
		}

		RECORD_RESOURCE_ALLOC(Result);
		return Result;
	}

	/*ShaderProgram CreateShader(const ShaderCreateInfo* ProgramData)
	{
		// Get the Spv data
		HlslToSpvResult SpvResult;
		if (!HlslToSpv(ProgramData, SpvResult))
		{
			GLog->error("Failed to produce spv for Vulkan shader module");
			return nullptr;
		}

		std::vector<VkPipelineShaderStageCreateInfo> PipelineShaderStages;
		std::vector<VkShaderModule> ShaderModules;

		VulkanShader* Result = new VulkanShader;

		// Create vertex shader
		if (ProgramData->VertexShaderVirtual)
		{
			if (!CreateShaderModule(SpvResult.VertexSpv, Result->VertexModule))
			{
				//GLog->critical("Failed to create shader module");

				delete Result;
				return nullptr;
			}
			else
			{
				ShaderModules.push_back(Result->VertexModule);
				Result->bHasVertexShader = true;
			}
		}

		// Create fragment shader
		if (ProgramData->FragmentShaderVirtual)
		{
			if (!CreateShaderModule(SpvResult.FragmentSpv, Result->FragmentModule))
			{
				//GLog->critical("Failed to create shader module");

				delete Result;
				return nullptr;
			}
			else
			{
				ShaderModules.push_back(Result->FragmentModule);
				Result->bHasFragmentShader = true;
			}
		}

		RECORD_RESOURCE_ALLOC(Result);

		return Result;
	}*/

	/**
	 * Queries the physical device for the optimal swap chain settings.
	 *
	 */
	void QuerySwapChainOptimalSettings(
		int32_t DesiredSwapChainWidth,
		int32_t DesiredSwapChainHeight,
		const VkSurfaceKHR& TargetSurface,
		VkExtent2D& OutOptimalSwapChainExtent,
		VkSurfaceFormatKHR& OutOptimalSwapChainSurfaceFormat,
		VkPresentModeKHR& OutOptimalSwapChainPresentMode,
		VkSurfaceCapabilitiesKHR& OutSurfaceCapabilities,
		uint32_t& OutImageCount
	)
	{
		std::vector<VkSurfaceFormatKHR> SupportedFormats;
		std::vector<VkPresentModeKHR> SupportedPresentModes;
		uint32_t SurfaceFormatCount;
		uint32_t PresentModeCount;

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(GVulkanContext.PhysicalDevice, TargetSurface, &OutSurfaceCapabilities); // Get surface capabilities
		vkGetPhysicalDeviceSurfaceFormatsKHR(GVulkanContext.PhysicalDevice, TargetSurface, &SurfaceFormatCount, nullptr); // Get count of surface formats
		vkGetPhysicalDeviceSurfacePresentModesKHR(GVulkanContext.PhysicalDevice, TargetSurface, &PresentModeCount, nullptr); // Get count of present formats

		if (SurfaceFormatCount > 0 && PresentModeCount > 0)
		{
			// Get the surface formats and present modes
			SupportedFormats.resize(SurfaceFormatCount);
			SupportedPresentModes.resize(PresentModeCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(GVulkanContext.PhysicalDevice, TargetSurface, &SurfaceFormatCount, SupportedFormats.data());
			vkGetPhysicalDeviceSurfacePresentModesKHR(GVulkanContext.PhysicalDevice, TargetSurface, &PresentModeCount, SupportedPresentModes.data());

			// Find the optimal extent
			{
				if (OutSurfaceCapabilities.currentExtent.width != UINT32_MAX)
				{
					// The optimal extent is already specified, so we need to match it.
					OutOptimalSwapChainExtent = OutSurfaceCapabilities.currentExtent;
				}
				else
				{
					OutOptimalSwapChainExtent.width = std::clamp(static_cast<uint32_t>(DesiredSwapChainWidth), OutSurfaceCapabilities.minImageExtent.width, OutSurfaceCapabilities.maxImageExtent.width);
					OutOptimalSwapChainExtent.height = std::clamp(static_cast<uint32_t>(DesiredSwapChainHeight), OutSurfaceCapabilities.minImageExtent.height, OutSurfaceCapabilities.maxImageExtent.height);
				}
			}

			// Give ourselves one more image than the min image count. Make sure this doesn't exceed the max image count (zero means no max)
			OutImageCount = OutSurfaceCapabilities.minImageCount + 1;
			if (OutSurfaceCapabilities.maxImageCount > 0 && OutImageCount > OutSurfaceCapabilities.maxImageCount)
				OutImageCount = OutImageCount > OutSurfaceCapabilities.maxImageCount;

			// Find the optimal surface format
			{
				// Rank desired vulkan swap chain formats, lower number means higher priority
				static std::vector<std::tuple<VkFormat, VkColorSpaceKHR, uint8_t>> FormatRanks = {
					{VK_FORMAT_R16G16B16A16_SFLOAT, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, 1},
					{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, 0}
				};

				// This is technically slow at O(N^2), but only happens once and this simplifies the code since we don't have to write a hash specialization.
				static auto FindFormatRank = [](VkFormat Format, VkColorSpaceKHR ColorSpace) -> int16_t
				{
					for (auto Tup : FormatRanks)
						if (std::get<0>(Tup) == Format && std::get<1>(Tup) == ColorSpace)
							return std::get<2>(Tup);
					return -1;
				};

				// Default to first available surface format
				OutOptimalSwapChainSurfaceFormat = SupportedFormats[0];
				for (const auto& SupportedFormat : SupportedFormats)
				{
					auto SupportedFormatRank = FindFormatRank(SupportedFormat.format, SupportedFormat.colorSpace);
					auto CurFormatRank = FindFormatRank(OutOptimalSwapChainSurfaceFormat.format, OutOptimalSwapChainSurfaceFormat.colorSpace);

					if (SupportedFormatRank >= 0 && (CurFormatRank < 0 || SupportedFormatRank < CurFormatRank))
						OutOptimalSwapChainSurfaceFormat = SupportedFormat;
				}
			}

			// Find the optimal presentation mode
			// TODO: Do more research here. Do we want to prioritize mailbox mode? That seems like it may use more energy than FIFO.
			{
				static std::unordered_map<VkPresentModeKHR, uint32_t> PresentModeOrder = {
					{VK_PRESENT_MODE_FIFO_KHR, 1},
					{VK_PRESENT_MODE_MAILBOX_KHR, 0}
				};

				// Default to present mode FIFO since that is the only one guaranteed to be present
				OutOptimalSwapChainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
				uint32_t BestOrder = std::numeric_limits<uint32_t>::max();

				// Present mailbox present mode
				for (const auto& SupportedPresentMode : SupportedPresentModes)
				{
					if (PresentModeOrder.contains(SupportedPresentMode))
					{
						uint32_t Order = PresentModeOrder[SupportedPresentMode];
						if (Order < BestOrder)
						{
							OutOptimalSwapChainPresentMode = SupportedPresentMode;
							BestOrder = Order;
						}
					}
				}
			}

		}
	}

	bool CreateVkSwapChain(VulkanSwapChain* Dst, Surface TargetSurface, int32_t DesiredWidth, int32_t DesiredHeight)
	{
		VulkanSurface* VkSurface = static_cast<VulkanSurface*>(TargetSurface);

		// Query optimal swap chain settings for current surface
		VkExtent2D OptimalExtent;
		VkSurfaceFormatKHR OptimalFormat;
		VkPresentModeKHR OptimalPresentMode;
		VkSurfaceCapabilitiesKHR SurfaceCapabilities;
		uint32_t OptimalImageCount;
		QuerySwapChainOptimalSettings(DesiredWidth, DesiredHeight, VkSurface->VkSurface,
			OptimalExtent, OptimalFormat, OptimalPresentMode, SurfaceCapabilities, OptimalImageCount);

		// This needs to be declared in case the queue families are different
		uint32_t QueueFamilyIndices[] = { GVulkanContext.GraphicsQueueFamIndex, GVulkanContext.PresentQueueFamIndex };

		VkSwapchainCreateInfoKHR CreateInfo{};
		CreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		CreateInfo.surface = VkSurface->VkSurface;
		CreateInfo.minImageCount = OptimalImageCount;
		CreateInfo.imageFormat = OptimalFormat.format;
		CreateInfo.imageColorSpace = OptimalFormat.colorSpace;
		CreateInfo.imageExtent = OptimalExtent;
		CreateInfo.imageArrayLayers = 1; // TODO: This would be 2 for VR
		CreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // TODO: This will need to be changed in some instances (e.g. post processing)
		CreateInfo.preTransform = SurfaceCapabilities.currentTransform; // I don't think we ever want to use this
		CreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // This is how you blend with other windows
		CreateInfo.presentMode = OptimalPresentMode;
		CreateInfo.clipped = VK_TRUE; // This would need to be false if we cared about clipped pixels (obscured by a window for example)
		CreateInfo.oldSwapchain = VK_NULL_HANDLE;

		// Make things simple: Use concurrent sharing if queue families are different
		if (GVulkanContext.GraphicsQueueFamIndex != GVulkanContext.PresentQueueFamIndex)
		{
			CreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			CreateInfo.queueFamilyIndexCount = 2;
			CreateInfo.pQueueFamilyIndices = QueueFamilyIndices;
		}
		else
		{
			CreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			CreateInfo.queueFamilyIndexCount = 0;
			CreateInfo.pQueueFamilyIndices = nullptr;
		}

		// Also store the optimal settings for later reference
		Dst->ImageFormat = VkFormatToAttachmentFormat(OptimalFormat.format);
		Dst->PresentMode = OptimalPresentMode;
		Dst->SwapChainExtent = OptimalExtent;

		if (vkCreateSwapchainKHR(GVulkanContext.Device, &CreateInfo, nullptr, &Dst->SwapChain) != VK_SUCCESS)
		{
			//GLog->critical("Failed to create a Vulkan swap chain");
			return false;
		}

		return true;
	}

	bool CreateVkSwapChainImageViews(VulkanSwapChain* Dst)
	{
		// Retrieve the swap chain images
		uint32_t ImageCount{};
		vkGetSwapchainImagesKHR(GVulkanContext.Device, Dst->SwapChain, &ImageCount, nullptr);

		// Allocate space for images
		std::vector<VkImage> Images(ImageCount);

		// Get the actual swap chain images
		vkGetSwapchainImagesKHR(GVulkanContext.Device, Dst->SwapChain, &ImageCount, Images.data());

		Dst->Images.resize(ImageCount);

		for (uint32_t ImageIndex = 0; ImageIndex < ImageCount; ImageIndex++)
		{
			Dst->Images[ImageIndex].TextureImage = Images[ImageIndex];
			Dst->Images[ImageIndex].TextureFormat = Dst->ImageFormat;

			VkImageViewCreateInfo ImageViewCreateInfo{};
			ImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			ImageViewCreateInfo.image = Images[ImageIndex];
			ImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			ImageViewCreateInfo.format = AttachmentFormatToVkFormat(Dst->ImageFormat);
			ImageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			ImageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			ImageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			ImageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			ImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			ImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
			ImageViewCreateInfo.subresourceRange.levelCount = 1;
			ImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
			ImageViewCreateInfo.subresourceRange.layerCount = 1;

			if (vkCreateImageView(GVulkanContext.Device, &ImageViewCreateInfo, nullptr, &Dst->Images[ImageIndex].TextureImageView) != VK_SUCCESS)
			{
				//GLog->error("Failed to create image view for swap chain");

				// Error in creation
				return false;
			}
		}

		return true;
	}

	void DestroyVkSwapChain(VulkanSwapChain* VkSwap)
	{
		vkDestroySwapchainKHR(GVulkanContext.Device, VkSwap->SwapChain, nullptr);
	}

	void DestroyVkSwapChainImageViews(VulkanSwapChain* VkSwap)
	{
		// Clean up framebuffers and image views
		for (uint32_t SwapChainImage = 0; SwapChainImage < VkSwap->Images.size(); SwapChainImage++)
		{
			vkDestroyImageView(GVulkanContext.Device, VkSwap->Images[SwapChainImage].TextureImageView, nullptr);
		}
	}

	SwapChain CreateSwapChain(Surface TargetSurface, int32_t DesiredWidth,
		int32_t DesiredHeight)
	{
		VulkanSwapChain* Result = new VulkanSwapChain;

		// Create swap chain resources
		if (!CreateVkSwapChain(Result, TargetSurface, DesiredWidth, DesiredHeight))
		{
			return nullptr;
		}

		if (!CreateVkSwapChainImageViews(Result))
		{
			return nullptr;
		}

		// Create n frames in flight for this swap chain. This only needs to be done at creation, hence there isn't another function for it.
		for (uint32_t FrameInFlight = 0; FrameInFlight < MAX_FRAMES_IN_FLIGHT; FrameInFlight++)
		{
			VkSemaphore ImageAvailableSem, PresentSem;
			VkFence InFlightFence;

			// Create semaphore for frame in flight
			VkSemaphoreCreateInfo SemCreate{};
			SemCreate.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
			if (vkCreateSemaphore(GVulkanContext.Device, &SemCreate, nullptr, &ImageAvailableSem) != VK_SUCCESS ||
				vkCreateSemaphore(GVulkanContext.Device, &SemCreate, nullptr, &PresentSem) != VK_SUCCESS)
			{
				//GLog->critical("Failed to create semaphores for swap chain");

				delete Result;
				return nullptr;
			}

			VkFenceCreateInfo FenceCreate{};
			FenceCreate.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			FenceCreate.flags = VK_FENCE_CREATE_SIGNALED_BIT;
			if (vkCreateFence(GVulkanContext.Device, &FenceCreate, nullptr, &InFlightFence) != VK_SUCCESS)
			{
				//GLog->critical("Failed to create fence for swap chain");
				return nullptr;
			}

			Result->FramesInFlight.emplace_back(ImageAvailableSem, PresentSem, InFlightFence);
		}

		// Resize image fences to as many swap chain images that we have. Also, start them all at the null handle.
		Result->ImageFences.resize(Result->Images.size(), VK_NULL_HANDLE);

		RECORD_RESOURCE_ALLOC(Result)

		return Result;
	}

	void DestroySwapChain(SwapChain Swap)
	{
		vkDeviceWaitIdle(GVulkanContext.Device);

		VulkanSwapChain* VkSwap = static_cast<VulkanSwapChain*>(Swap);

		// Destroy volatile swap chain resources
		DestroyVkSwapChainImageViews(VkSwap);
		DestroyVkSwapChain(VkSwap);

		// Destroy frames in flight
		for (const auto& FrameInFlight : VkSwap->FramesInFlight)
		{
			vkDestroySemaphore(GVulkanContext.Device, FrameInFlight.ImageAvailableSemaphore, nullptr);
			vkDestroySemaphore(GVulkanContext.Device, FrameInFlight.RenderingFinishedSemaphore, nullptr);
			vkDestroyFence(GVulkanContext.Device, FrameInFlight.InFlightFence, nullptr);
		}

		REMOVE_RESOURCE_ALLOC(VkSwap)

		// Free swap chain memory
		delete VkSwap;
	}

	void SubmitCommandBuffer(CommandBuffer Buffer, bool bWait, Fence WaitFence)
	{
		VulkanCommandBuffer* VkCmd = static_cast<VulkanCommandBuffer*>(Buffer);

		VkSubmitInfo SubmitInfo{};
		SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		SubmitInfo.commandBufferCount = 1;
		SubmitInfo.pCommandBuffers = &VkCmd->CmdBuffer;

		if (vkQueueSubmit(GVulkanContext.GraphicsQueue, 1, &SubmitInfo, WaitFence ? static_cast<VkFence>(WaitFence) : VK_NULL_HANDLE) != VK_SUCCESS)
		{
			//GLog->critical("Failed to submit vulkan command buffers to graphics queue");
		}

		if (bWait)
		{
			vkQueueWaitIdle(GVulkanContext.GraphicsQueue);
		}
	}

	int32_t BeginFrame(GLFWwindow* Window, llrm::SwapChain Swap, llrm::Surface Target)
	{
		VulkanSwapChain* VkSwap = static_cast<VulkanSwapChain*>(Swap);

		GVulkanContext.CurrentSwapChain = VkSwap;
		GVulkanContext.CurrentSurface = Target;
		GVulkanContext.CurrentWindow = Window;

		VkSwap->bInsideFrame = true;

		int32_t Width, Height;
		glfwGetFramebufferSize(Window, &Width, &Height);

		if (Width == 0 || Height == 0)
		{
			vkDeviceWaitIdle(GVulkanContext.Device);
			return -1;
		}

		// Acquire image, this is the swapchain image index that we will be rendering command buffers for + presenting to this frame.
		VkResult ImageAcquireResult = vkAcquireNextImageKHR(GVulkanContext.Device, VkSwap->SwapChain, UINT64_MAX,
			VkSwap->FramesInFlight[VkSwap->CurrentFrame].ImageAvailableSemaphore, VK_NULL_HANDLE, &VkSwap->AcquiredImageIndex);

		// Vulkan swap chain needs to re-created immediately
		if (ImageAcquireResult == VK_ERROR_OUT_OF_DATE_KHR)
		{
			RecreateSwapChain(Swap, Target, Width, Height);
		}

		// Previous frame using this image
		if (VkSwap->ImageFences[VkSwap->AcquiredImageIndex] != VK_NULL_HANDLE)
		{
			// Wait for the previous frame to complete execution of vkQueueSubmit
			// This is necessary because it's possible the command buffer submitted is still being used on the GPU, 
			vkWaitForFences(GVulkanContext.Device, 1, &VkSwap->ImageFences[VkSwap->AcquiredImageIndex], VK_TRUE, UINT64_MAX);
		}

		// Mark new image as being used by this "frame in flight"
		VkSwap->ImageFences[VkSwap->AcquiredImageIndex] = VkSwap->FramesInFlight[VkSwap->CurrentFrame].InFlightFence;

		return VkSwap->AcquiredImageIndex;
	}

	void EndFrame(const std::vector<CommandBuffer>& Buffers)
	{
		GVulkanContext.CurrentSwapChain->bInsideFrame = false;

		int32_t Width, Height;
		glfwGetFramebufferSize(GVulkanContext.CurrentWindow, &Width, &Height);

		// Detect minimization
		if (Width == 0 || Height == 0)
		{
			vkDeviceWaitIdle(GVulkanContext.Device);
			return;
		}

		// Reset the fence that we're waiting on
		vkResetFences(GVulkanContext.Device, 1, &GVulkanContext.CurrentSwapChain->FramesInFlight[GVulkanContext.CurrentSwapChain->CurrentFrame].InFlightFence);

		std::vector<VkCommandBuffer> VkBuffers(Buffers.size());
		for (uint32_t Buffer = 0; Buffer < Buffers.size(); Buffer++)
			VkBuffers[Buffer] = (static_cast<VulkanCommandBuffer*>(Buffers[Buffer]))->CmdBuffer;

		// Submit our command buffers for this frame
		VkPipelineStageFlags WaitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		VkSubmitInfo QueueSubmit{};
		QueueSubmit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		QueueSubmit.waitSemaphoreCount = 1;
		QueueSubmit.pWaitSemaphores = &GVulkanContext.CurrentSwapChain->FramesInFlight[GVulkanContext.CurrentSwapChain->CurrentFrame].ImageAvailableSemaphore;
		QueueSubmit.pWaitDstStageMask = WaitStages;
		QueueSubmit.commandBufferCount = static_cast<uint32_t>(VkBuffers.size());
		QueueSubmit.pCommandBuffers = VkBuffers.data();
		QueueSubmit.signalSemaphoreCount = 1;
		QueueSubmit.pSignalSemaphores = &GVulkanContext.CurrentSwapChain->FramesInFlight[GVulkanContext.CurrentSwapChain->CurrentFrame].RenderingFinishedSemaphore; // Signal when rendering is finished

		// Notify the in flight fence once the execution of this vkQueueSubmit is complete
		if (vkQueueSubmit(GVulkanContext.GraphicsQueue, static_cast<uint32_t>(VkBuffers.size()), &QueueSubmit, GVulkanContext.CurrentSwapChain->FramesInFlight[GVulkanContext.CurrentSwapChain->CurrentFrame].InFlightFence) != VK_SUCCESS)
		{
			//GLog->critical("Failed to submit vulkan command buffers to graphics queue");
		}

		// Present the rendered images
		VkPresentInfoKHR PresentInfo{};
		PresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		PresentInfo.waitSemaphoreCount = 1;
		PresentInfo.pWaitSemaphores = &GVulkanContext.CurrentSwapChain->FramesInFlight[GVulkanContext.CurrentSwapChain->CurrentFrame].RenderingFinishedSemaphore; // Wait for rendering to complete before presenting
		PresentInfo.swapchainCount = 1;
		PresentInfo.pSwapchains = &GVulkanContext.CurrentSwapChain->SwapChain;
		PresentInfo.pImageIndices = &GVulkanContext.CurrentSwapChain->AcquiredImageIndex;
		PresentInfo.pResults = nullptr;

		// Present the rendered image
		VkResult PresentResult = vkQueuePresentKHR(GVulkanContext.GraphicsQueue, &PresentInfo);

		if (PresentResult == VK_ERROR_OUT_OF_DATE_KHR || PresentResult == VK_SUBOPTIMAL_KHR)
		{
			RecreateSwapChain(GVulkanContext.CurrentSwapChain, GVulkanContext.CurrentSurface, Width, Height);
		}

		// Advance the current frame
		GVulkanContext.CurrentSwapChain->CurrentFrame = (GVulkanContext.CurrentSwapChain->CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

		GVulkanContext.CurrentSwapChain = nullptr;
		GVulkanContext.CurrentSurface = nullptr;
		GVulkanContext.CurrentWindow = nullptr;
	}

	void GetSwapChainSize(SwapChain Swap, uint32_t& Width, uint32_t& Height)
	{
		VulkanSwapChain* VkSwap = static_cast<VulkanSwapChain*>(Swap);
		Width = VkSwap->SwapChainExtent.width;
		Height = VkSwap->SwapChainExtent.height;
	}

	uint32_t GetSwapChainImageCount(SwapChain Swap)
	{
		VulkanSwapChain* VkSwap = static_cast<VulkanSwapChain*>(Swap);
		return VkSwap->Images.size();
	}

	Texture GetSwapChainImage(SwapChain Swap, uint32_t Index)
	{
		VulkanSwapChain* VkSwap = static_cast<VulkanSwapChain*>(Swap);
		return &VkSwap->Images[Index];
	}

	void RecreateSwapChain(SwapChain Swap, Surface Target, int32_t DesiredWidth, int32_t DesiredHeight)
	{
		VulkanSwapChain* VkSwap = static_cast<VulkanSwapChain*>(Swap);
		vkDeviceWaitIdle(GVulkanContext.Device);

		// Re-create the needed resources
		DestroyVkSwapChainImageViews(VkSwap);
		DestroyVkSwapChain(VkSwap);

		CreateVkSwapChain(VkSwap, Target, DesiredWidth, DesiredHeight);
		CreateVkSwapChainImageViews(VkSwap);
	}

	void GetFrameBufferSize(FrameBuffer Fbo, uint32_t& Width, uint32_t& Height)
	{
		VulkanFrameBuffer* VkFbo = static_cast<VulkanFrameBuffer*>(Fbo);

		Width = VkFbo->AttachmentWidth;
		Height = VkFbo->AttachmentHeight;
	}

	void UpdateUniformBuffer(ResourceSet Resources, SwapChain Target, uint32_t BufferIndex, void* Data, uint64_t DataSize)
	{
		VulkanSwapChain* VkSwap = static_cast<VulkanSwapChain*>(Target);
		VulkanResourceSet* VkRes = static_cast<VulkanResourceSet*>(Resources);

		if (!VkSwap->bInsideFrame)
		{
			//GLog->critical("Must update constant buffer within the limits of a swap chain frame");
			return;
		}

		VkDeviceMemory& Mem = VkRes->ConstantBuffers[BufferIndex].Memory[VkSwap->AcquiredImageIndex];

		// Data is guaranteed available since this frame is guaranteed to have previous operations complete by cpu fence in BeginFrame
		void* MappedData;
		vkMapMemory(GVulkanContext.Device, Mem, 0, DataSize, 0, &MappedData);
		{
			std::memcpy(MappedData, Data, DataSize);
		}
		vkUnmapMemory(GVulkanContext.Device, Mem); // Memory is host-coherent, so no flush necessary

		// Update descriptor set memory
		const auto& ConstBuf = VkRes->ConstantBuffers[BufferIndex];
		uint32_t CurrentImage = VkSwap->AcquiredImageIndex;

		VkDescriptorBufferInfo BufInfo{};
		BufInfo.buffer = ConstBuf.Buffers[CurrentImage];
		BufInfo.offset = 0;
		BufInfo.range = DataSize;

		VkWriteDescriptorSet BufferWrite{};
		BufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		BufferWrite.dstSet = VkRes->DescriptorSets[CurrentImage];
		BufferWrite.dstBinding = ConstBuf.Binding;
		BufferWrite.dstArrayElement = 0; // TODO: support multiple array elements
		BufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		BufferWrite.descriptorCount = 1;
		BufferWrite.pBufferInfo = &BufInfo;
		BufferWrite.pImageInfo = nullptr;
		BufferWrite.pTexelBufferView = nullptr;

		vkUpdateDescriptorSets(GVulkanContext.Device, 1, &BufferWrite, 0, nullptr);
	}

	// TODO: Reuse code among UpdateTextureResource and UpdateAttachmentResource
	void UpdateTextureResource(ResourceSet Resources, SwapChain Target, Texture* Images, uint32_t ImageCount, uint32_t Binding)
	{
		VulkanSwapChain* VkSwap = static_cast<VulkanSwapChain*>(Target);
		VulkanResourceSet* VkRes = static_cast<VulkanResourceSet*>(Resources);

		VkWriteDescriptorSet* Writes = new VkWriteDescriptorSet[ImageCount];
		VkDescriptorImageInfo* ImageInfos = new VkDescriptorImageInfo[ImageCount];
		uint32_t CurrentImage = VkSwap->AcquiredImageIndex;

		for (uint32_t ArrayImage = 0; ArrayImage < ImageCount; ArrayImage++)
		{
			VulkanTexture* VkTex = static_cast<VulkanTexture*>(Images[ArrayImage]);

			VkDescriptorImageInfo& ImageInfo = ImageInfos[ArrayImage];
			VkWriteDescriptorSet& ImageWrite = Writes[ArrayImage];

			ImageInfo = {};
			ImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			ImageInfo.imageView = VkTex->TextureImageView;
			ImageInfo.sampler = VkTex->TextureSampler;

			ImageWrite = {};
			ImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			ImageWrite.dstSet = VkRes->DescriptorSets[CurrentImage];
			ImageWrite.dstBinding = Binding;
			ImageWrite.dstArrayElement = ArrayImage;
			ImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			ImageWrite.descriptorCount = 1;
			ImageWrite.pBufferInfo = nullptr;
			ImageWrite.pImageInfo = &ImageInfo;
			ImageWrite.pTexelBufferView = nullptr;
		}

		vkUpdateDescriptorSets(GVulkanContext.Device, ImageCount, Writes, 0, nullptr);

		delete[] Writes;
		delete[] ImageInfos;
	}

	void SynchronizedUploadBufferData(VkFence StagingFence, uint64_t Size, const void* Data,
		VkCommandBuffer StagingCommandBuffer,
		VkDeviceMemory StagingMemory, VkBuffer StagingBuffer,
		VkBuffer DeviceBuffer,
		VkAccessFlagBits SrcAccess, VkAccessFlagBits DstAccess,
		VkPipelineStageFlagBits SrcStage, VkPipelineStageFlagBits DstStage)
	{

		// Wait for previous staging transfer operation to complete
		vkWaitForFences(GVulkanContext.Device, 1, &StagingFence, VK_TRUE, UINT64_MAX);
		vkResetFences(GVulkanContext.Device, 1, &StagingFence);

		// Now we can safely write to host-visible staging buffer memory
		void* MappedData = nullptr;
		vkMapMemory(GVulkanContext.Device, StagingMemory, 0, Size, 0, &MappedData);
		{
			std::memcpy(MappedData, Data, Size);
		}
		vkUnmapMemory(GVulkanContext.Device, StagingMemory);

		VkCommandBufferBeginInfo BeginInfo{};
		BeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		BeginInfo.pInheritanceInfo = nullptr;
		BeginInfo.flags = 0;

		vkResetCommandBuffer(StagingCommandBuffer, 0);
		vkBeginCommandBuffer(StagingCommandBuffer, &BeginInfo);
		{
			VkBufferMemoryBarrier PreBarrier{};
			PreBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			PreBarrier.buffer = DeviceBuffer;
			PreBarrier.size = Size;
			PreBarrier.offset = 0;
			PreBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			PreBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			PreBarrier.srcAccessMask = SrcAccess;
			PreBarrier.dstAccessMask = DstAccess;

			// After this barrier we're good to write to the device buffer
			vkCmdPipelineBarrier(StagingCommandBuffer,
				SrcStage, DstStage,
				0,
				0, nullptr,
				1, &PreBarrier,
				0, nullptr
			);

			VkBufferCopy CopyRegion{};
			CopyRegion.srcOffset = 0;
			CopyRegion.dstOffset = 0;
			CopyRegion.size = Size;
			vkCmdCopyBuffer(StagingCommandBuffer, StagingBuffer, DeviceBuffer, 1, &CopyRegion);

			VkBufferMemoryBarrier PostBarrier{};
			PostBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			PostBarrier.buffer = DeviceBuffer;
			PostBarrier.size = Size;
			PostBarrier.offset = 0;
			PostBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			PostBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			PostBarrier.srcAccessMask = DstAccess;
			PostBarrier.dstAccessMask = SrcAccess;

			// After this barrier, the vertex shader is good to read from the device buffer
			vkCmdPipelineBarrier(StagingCommandBuffer,
				DstStage, SrcStage,
				0,
				0, nullptr,
				1, &PostBarrier,
				0, nullptr
			);

		}
		vkEndCommandBuffer(StagingCommandBuffer);

		// TODO: Is there a performance benefit to submitting to a separate transfer queue?
		VkSubmitInfo QueueSubmit{};
		QueueSubmit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		QueueSubmit.commandBufferCount = 1;
		QueueSubmit.pCommandBuffers = &StagingCommandBuffer;

		// Pass in a fence. This will prevent us from creating a RAW (read-after-write) hazard in the pipeline.
		vkQueueSubmit(GVulkanContext.GraphicsQueue, 1, &QueueSubmit, StagingFence);
	}

	void UploadVertexBufferData(VertexBuffer Buffer, const void* Data, uint64_t Size)
	{
		VulkanVertexBuffer* VulkanVbo = static_cast<VulkanVertexBuffer*>(Buffer);

		SynchronizedUploadBufferData(VulkanVbo->VertexStagingCompleteFence, Size, Data,
			VulkanVbo->VertexStagingCommandBuffer,
			VulkanVbo->StagingVertexBufferMemory, VulkanVbo->StagingVertexBuffer,
			VulkanVbo->DeviceVertexBuffer,
			VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

	}

	void UploadIndexBufferData(IndexBuffer Buffer, const uint32_t* Data, uint64_t Size)
	{
		VulkanIndexBuffer* VulkanIbo = static_cast<VulkanIndexBuffer*>(Buffer);

		SynchronizedUploadBufferData(VulkanIbo->IndexStagingCompleteFence, Size, Data,
			VulkanIbo->IndexStagingCommandBuffer,
			VulkanIbo->StagingIndexBufferMemory, VulkanIbo->StagingIndexBuffer,
			VulkanIbo->DeviceIndexBuffer,
			VK_ACCESS_INDEX_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	}

	void ResizeVertexBuffer(VertexBuffer Buffer, uint64_t NewSize)
	{
		// Delete old resources
		VulkanVertexBuffer* VulkanVbo = static_cast<VulkanVertexBuffer*>(Buffer);

		// Force queue idle
		vkQueueWaitIdle(GVulkanContext.GraphicsQueue);

		vkFreeMemory(GVulkanContext.Device, VulkanVbo->DeviceVertexBufferMemory, nullptr);
		vkFreeMemory(GVulkanContext.Device, VulkanVbo->StagingVertexBufferMemory, nullptr);
		vkDestroyBuffer(GVulkanContext.Device, VulkanVbo->DeviceVertexBuffer, nullptr);
		vkDestroyBuffer(GVulkanContext.Device, VulkanVbo->StagingVertexBuffer, nullptr);

		// Re-allocate buffer resources using the same flags that were used to create it
		CreateBuffer(NewSize,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			VulkanVbo->StagingVertexBuffer, VulkanVbo->StagingVertexBufferMemory
		);

		CreateBuffer(NewSize,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			VulkanVbo->DeviceVertexBuffer, VulkanVbo->DeviceVertexBufferMemory
		);
	}

	void ResizeIndexBuffer(IndexBuffer Buffer, uint64_t NewSize)
	{
		// Delete old resources
		VulkanIndexBuffer* VulkanIbo = static_cast<VulkanIndexBuffer*>(Buffer);

		// Force queue idle
		vkQueueWaitIdle(GVulkanContext.GraphicsQueue);

		vkFreeMemory(GVulkanContext.Device, VulkanIbo->DeviceIndexBufferMemory, nullptr);
		vkFreeMemory(GVulkanContext.Device, VulkanIbo->StagingIndexBufferMemory, nullptr);
		vkDestroyBuffer(GVulkanContext.Device, VulkanIbo->DeviceIndexBuffer, nullptr);
		vkDestroyBuffer(GVulkanContext.Device, VulkanIbo->StagingIndexBuffer, nullptr);

		CreateBuffer(NewSize,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			VulkanIbo->StagingIndexBuffer, VulkanIbo->StagingIndexBufferMemory
		);

		// Create device index buffer. Because we will be copying the staging buffer to the device buffer, we need to make it eligible for transfer.
		CreateBuffer(NewSize,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			VulkanIbo->DeviceIndexBuffer, VulkanIbo->DeviceIndexBufferMemory
		);
	}

	void ReadTexture(Texture Tex, uint32_t Attachment, void* Dst, uint64_t BufferSize, AttachmentUsage PreviousUsage)
	{
		vkDeviceWaitIdle(GVulkanContext.Device);

		VulkanTexture* VkTex = static_cast<VulkanTexture*>(Tex); 

		VkBuffer StagingBuffer;
		VkDeviceMemory StagingBufferMemory;
		CreateBuffer
		(
			BufferSize,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			StagingBuffer, StagingBufferMemory
		);

		// Transition image to transfer src from its previous usage
		ImmediateSubmitAndWait([&](CommandBuffer Dst)
		{
			TransitionTexture(Dst, Tex, PreviousUsage, AttachmentUsage::TransferSource);
		});

		ImmediateSubmitAndWait([&](CommandBuffer Dst)
		{
			VkCmdBuffer(Dst, [&](VkCommandBuffer Buf)
			{
				VkBufferImageCopy ImageCopy{};
				ImageCopy.bufferOffset = 0;
				ImageCopy.bufferRowLength = 0;
				ImageCopy.bufferImageHeight = 0;

				ImageCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				ImageCopy.imageSubresource.mipLevel = 0;
				ImageCopy.imageSubresource.baseArrayLayer = 0;
				ImageCopy.imageSubresource.layerCount = 1;

				ImageCopy.imageOffset = { 0, 0, 0 };
				ImageCopy.imageExtent = { VkTex->Width, VkTex->Height, 1 };

				// Copy image data to buffer
				vkCmdCopyImageToBuffer(Buf,
					VkTex->TextureImage,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					StagingBuffer,
					1,
					&ImageCopy
				);
			});
		});

		void* MappedData;
		vkMapMemory(GVulkanContext.Device, StagingBufferMemory, 0, BufferSize, 0, &MappedData);
		{
			std::memcpy(Dst, MappedData, BufferSize);
		}
		vkUnmapMemory(GVulkanContext.Device, StagingBufferMemory);

		// Transition image back
		ImmediateSubmitAndWait([&](CommandBuffer Dst)
		{
			TransitionTexture(Dst, Tex, AttachmentUsage::TransferSource, PreviousUsage);
		});

		// TODO: optimize this, we should have a staging buffer set aside already and a flag TEXTURE_USAGE_CPU_READ
		vkFreeMemory(GVulkanContext.Device, StagingBufferMemory, nullptr);
		vkDestroyBuffer(GVulkanContext.Device, StagingBuffer, nullptr);
	}

	AttachmentFormat GetTextureFormat(Texture Tex)
	{
		VulkanTexture* VkTex = static_cast<VulkanTexture*>(Tex);
		return VkTex->TextureFormat;
	}

	VkShaderStageFlags ShaderStageToVkStage(ShaderStage Stage)
	{
		switch (Stage)
		{
		case ShaderStage::Vertex:
			return VK_SHADER_STAGE_VERTEX_BIT;
		case ShaderStage::Fragment:
			return VK_SHADER_STAGE_FRAGMENT_BIT;
		}

		return VK_SHADER_STAGE_VERTEX_BIT;
	}

	ResourceLayout CreateResourceLayout(const ResourceLayoutCreateInfo& CreateInfo)
	{
		VulkanResourceLayout* Result = new VulkanResourceLayout;

		std::vector<VkDescriptorSetLayoutBinding> LayoutBindings;
		for (uint32_t LayoutBindingIndex = 0; LayoutBindingIndex < CreateInfo.ConstantBuffers.size(); LayoutBindingIndex++)
		{
			VkDescriptorSetLayoutBinding LayoutBinding{};
			LayoutBinding.binding = CreateInfo.ConstantBuffers[LayoutBindingIndex].Binding;
			LayoutBinding.descriptorCount = CreateInfo.ConstantBuffers[LayoutBindingIndex].Count;
			LayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			LayoutBinding.pImmutableSamplers = nullptr;
			LayoutBinding.stageFlags = ShaderStageToVkStage(CreateInfo.ConstantBuffers[LayoutBindingIndex].StageUsedAt);

			LayoutBindings.push_back(LayoutBinding);

			// Persist constant buffers
			Result->ConstantBuffers.push_back(CreateInfo.ConstantBuffers[LayoutBindingIndex]);
		}

		for (uint32_t TexBindingIndex = 0; TexBindingIndex < CreateInfo.Textures.size(); TexBindingIndex++)
		{
			VkDescriptorSetLayoutBinding LayoutBinding{};
			LayoutBinding.binding = CreateInfo.Textures[TexBindingIndex].Binding;
			LayoutBinding.descriptorCount = CreateInfo.Textures[TexBindingIndex].Count;
			LayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			LayoutBinding.pImmutableSamplers = nullptr;
			LayoutBinding.stageFlags = ShaderStageToVkStage(CreateInfo.Textures[TexBindingIndex].StageUsedAt);

			LayoutBindings.push_back(LayoutBinding);

			// Persist constant buffers
			Result->TextureBindings.push_back(CreateInfo.Textures[TexBindingIndex]);
		}

		VkDescriptorSetLayoutCreateInfo LayoutCreateInfo{};
		LayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		LayoutCreateInfo.bindingCount = static_cast<uint32_t>(LayoutBindings.size());
		LayoutCreateInfo.pBindings = LayoutBindings.data();

		if (vkCreateDescriptorSetLayout(GVulkanContext.Device, &LayoutCreateInfo, nullptr, &Result->VkLayout) != VK_SUCCESS)
		{
			//GLog->critical("Failed to create Vulkan descriptor set layout");
			return nullptr;
		}

		RECORD_RESOURCE_ALLOC(Result)

		return Result;
	}

	void DestroyResourceLayout(ResourceLayout Layout)
	{
		VulkanResourceLayout* VkLayout = static_cast<VulkanResourceLayout*>(Layout);
		vkDestroyDescriptorSetLayout(GVulkanContext.Device, VkLayout->VkLayout, nullptr);

		REMOVE_RESOURCE_ALLOC(VkLayout)

		delete VkLayout;
	}

	void DestroyResourceSet(ResourceSet Resources)
	{
		vkDeviceWaitIdle(GVulkanContext.Device);

		VulkanResourceSet* VkRes = static_cast<VulkanResourceSet*>(Resources);
		for (const auto& ConstBuf : VkRes->ConstantBuffers)
		{
			for (const auto& Memory : ConstBuf.Memory)
				vkFreeMemory(GVulkanContext.Device, Memory, nullptr);

			for (const auto& Buf : ConstBuf.Buffers)
				vkDestroyBuffer(GVulkanContext.Device, Buf, nullptr);

		}

		vkFreeDescriptorSets(GVulkanContext.Device, GVulkanContext.MainDscPool, static_cast<uint32_t>(VkRes->DescriptorSets.size()), VkRes->DescriptorSets.data());

		REMOVE_RESOURCE_ALLOC(VkRes)

		delete VkRes;
	}

	void CreatePipelineShaderStage(VkShaderStageFlagBits Stage, VkShaderModule Module, VkPipelineShaderStageCreateInfo& OutCreateInfo)
	{
		// Setup pipeline stage create info
		VkPipelineShaderStageCreateInfo NewInfo{};
		NewInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		NewInfo.stage = Stage;
		NewInfo.module = Module;
		NewInfo.pName = "main";

		OutCreateInfo = NewInfo;
	}

	VkFormat EngineFormatToVkFormat(VertexAttributeFormat Format)
	{
		switch (Format)
		{
		case VertexAttributeFormat::Float:
			return VK_FORMAT_R32_SFLOAT;
		case VertexAttributeFormat::Float2:
			return VK_FORMAT_R32G32_SFLOAT;
		case VertexAttributeFormat::Float3:
			return VK_FORMAT_R32G32B32_SFLOAT;
		case VertexAttributeFormat::Float4:
			return VK_FORMAT_R32G32B32A32_SFLOAT;
		case VertexAttributeFormat::Int32:
			return VK_FORMAT_R32_SINT;
		}

		return VK_FORMAT_R32_SFLOAT;
	}

	VkPrimitiveTopology EngineTopToVkTop(PipelineRenderPrimitive Primitive)
	{
		switch (Primitive)
		{
		case PipelineRenderPrimitive::TRIANGLES:
			return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		case PipelineRenderPrimitive::LINES:
			return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
		}

		return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	}

	Pipeline CreatePipeline(const PipelineState& CreateInfo)
	{
		VulkanShader* VkShader = static_cast<VulkanShader*>(CreateInfo.Shader);
		VulkanRenderGraph* VkRenderGraph = static_cast<VulkanRenderGraph*>(CreateInfo.CompatibleGraph);
		if (!VkRenderGraph)
		{
			//GLog->critical("Must specify a valid Vulkan RenderGraph when creating a pipeline");
			return nullptr;
		}

		VulkanPipeline* Result = new VulkanPipeline;

		std::vector<VkPipelineShaderStageCreateInfo> ShaderCreateInfos;
		std::vector<VkVertexInputAttributeDescription> VertexAttributes;
		std::vector<VkVertexInputBindingDescription> VertexBindings;

		if (VkShader->bHasVertexShader)
		{
			VkPipelineShaderStageCreateInfo VertInfo{};
			CreatePipelineShaderStage(VK_SHADER_STAGE_VERTEX_BIT, VkShader->VertexModule, VertInfo);
			ShaderCreateInfos.push_back(VertInfo);
		}

		if (VkShader->bHasFragmentShader)
		{
			VkPipelineShaderStageCreateInfo FragInfo{};
			CreatePipelineShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, VkShader->FragmentModule, FragInfo);
			ShaderCreateInfos.push_back(FragInfo);
		}

		// Only use a single vertex binding for now
		VkVertexInputBindingDescription BindingDescription{};
		BindingDescription.binding = 0;
		BindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; // Would probably need to change this for instanced rendering
		BindingDescription.stride = CreateInfo.VertexBufferStride;
		VertexBindings.push_back(BindingDescription);

		uint32_t AttribIndex = 0;
		for(auto& Attrib : CreateInfo.VertexAttributes)
		{
			VkVertexInputAttributeDescription AttributeDescription{};
			AttributeDescription.format = EngineFormatToVkFormat(Attrib.first);
			AttributeDescription.binding = 0; // Always use one binding for vertex buffers
			AttributeDescription.location = AttribIndex++;
			AttributeDescription.offset = Attrib.second;

			VertexAttributes.push_back(AttributeDescription);
		}

		// Create VkPipelineVertexInputStageCreateInfo
		VkPipelineVertexInputStateCreateInfo VertexStateCreateInfo{};
		VertexStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		VertexStateCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(VertexAttributes.size());
		VertexStateCreateInfo.pVertexAttributeDescriptions = VertexAttributes.data();
		VertexStateCreateInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(VertexBindings.size());
		VertexStateCreateInfo.pVertexBindingDescriptions = VertexBindings.data();

		// Create input assembly
		VkPipelineInputAssemblyStateCreateInfo InputAssembly{};
		InputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		InputAssembly.topology = EngineTopToVkTop(CreateInfo.Primitive);
		InputAssembly.primitiveRestartEnable = VK_FALSE; // Used for strip topologies

		// VkViewport and VkScissor will always be dynamic to prevent needing to re-create the pipeline on swap chain re-creation
		VkViewport Viewport{};
		Viewport.x = 0.0f;
		Viewport.y = 0.0f;
		Viewport.width = 1.0f;
		Viewport.height = 1.0f;
		Viewport.minDepth = 0.0f;
		Viewport.maxDepth = 0.0f;

		VkRect2D Scissor{};
		Scissor.offset = { 0, 0 };
		Scissor.extent = { 1, 1 };

		VkPipelineViewportStateCreateInfo ViewportStateCreateInfo{};
		ViewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		ViewportStateCreateInfo.viewportCount = 1;
		ViewportStateCreateInfo.pViewports = &Viewport;
		ViewportStateCreateInfo.scissorCount = 1;
		ViewportStateCreateInfo.pScissors = &Scissor;

		VkPipelineRasterizationStateCreateInfo Rasterizer{};
		Rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		Rasterizer.depthClampEnable = VK_FALSE;
		Rasterizer.rasterizerDiscardEnable = VK_FALSE;
		Rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		Rasterizer.lineWidth = 1.0f;
		Rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
		Rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // Reverse this from the usual CLOCKWISE because we flip the viewport
		Rasterizer.depthBiasEnable = VK_FALSE;
		Rasterizer.depthBiasConstantFactor = 0.0f;
		Rasterizer.depthBiasClamp = 0.0f;
		Rasterizer.depthBiasSlopeFactor = 0.0f;

		VkPipelineDepthStencilStateCreateInfo DepthStencilCreateInfo{};
		DepthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		DepthStencilCreateInfo.depthTestEnable = VK_FALSE;
		DepthStencilCreateInfo.depthWriteEnable = VK_FALSE;
		DepthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;
		DepthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;
		DepthStencilCreateInfo.minDepthBounds = 0.0f; // Optional
		DepthStencilCreateInfo.maxDepthBounds = 1.0f; // Optional
		DepthStencilCreateInfo.stencilTestEnable = VK_FALSE;
		DepthStencilCreateInfo.front = {}; // Optional
		DepthStencilCreateInfo.back = {}; // Optional
		if (CreateInfo.DepthStencil.bEnableDepthTest)
		{
			DepthStencilCreateInfo.depthTestEnable = VK_TRUE;
			DepthStencilCreateInfo.depthWriteEnable = VK_TRUE;
		}

		VkPipelineMultisampleStateCreateInfo Multisampling{};
		Multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		Multisampling.sampleShadingEnable = VK_FALSE;
		Multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		Multisampling.minSampleShading = 1.0f; // Optional
		Multisampling.pSampleMask = nullptr; // Optional
		Multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
		Multisampling.alphaToOneEnable = VK_FALSE; // Optional
		Multisampling.pSampleMask = nullptr;

		std::vector<VkPipelineColorBlendAttachmentState> ColorBlendAttachments;
		for(const PipelineBlendSettings& Settings : CreateInfo.BlendSettings)
		{
			VkPipelineColorBlendAttachmentState ColorBlendAttachment{};
			ColorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			ColorBlendAttachment.blendEnable = Settings.bBlendingEnabled ? VK_TRUE : VK_FALSE;
			ColorBlendAttachment.srcColorBlendFactor = BlendFactorToVkFactor(Settings.SrcColorFactor);
			ColorBlendAttachment.dstColorBlendFactor = BlendFactorToVkFactor(Settings.DstColorFactor);
			ColorBlendAttachment.colorBlendOp = BlendOpToVkBlend(Settings.ColorOp);
			ColorBlendAttachment.srcAlphaBlendFactor = BlendFactorToVkFactor(Settings.SrcAlphaFactor);
			ColorBlendAttachment.dstAlphaBlendFactor = BlendFactorToVkFactor(Settings.DstAlphaFactor);
			ColorBlendAttachment.alphaBlendOp = BlendOpToVkBlend(Settings.AlphaOp);

			ColorBlendAttachments.push_back(ColorBlendAttachment);
		}

		VkPipelineColorBlendStateCreateInfo ColorBlending{};
		ColorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		ColorBlending.logicOpEnable = VK_FALSE;
		ColorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
		ColorBlending.attachmentCount = static_cast<uint32_t>(ColorBlendAttachments.size());
		ColorBlending.pAttachments = ColorBlendAttachments.data();
		ColorBlending.blendConstants[0] = 0.0f; // Optional
		ColorBlending.blendConstants[1] = 0.0f; // Optional
		ColorBlending.blendConstants[2] = 0.0f; // Optional
		ColorBlending.blendConstants[3] = 0.0f; // Optional

		VkDynamicState DynamicStates[] = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};

		VkPipelineDynamicStateCreateInfo DynamicState{};
		DynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		DynamicState.dynamicStateCount = 2;
		DynamicState.pDynamicStates = DynamicStates;

		// Create pipeline layout
		VulkanResourceLayout* VkLayout = static_cast<VulkanResourceLayout*>(CreateInfo.Layout);

		VkPipelineLayoutCreateInfo PipelineLayoutInfo{};
		PipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		PipelineLayoutInfo.setLayoutCount = CreateInfo.Layout ? 1 : 0;
		PipelineLayoutInfo.pSetLayouts = &VkLayout->VkLayout;
		PipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
		PipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

		if (vkCreatePipelineLayout(GVulkanContext.Device, &PipelineLayoutInfo, nullptr, &Result->PipelineLayout) != VK_SUCCESS)
		{
			//GLog->critical("Failed to create pipeline layout");

			delete Result;
			return nullptr;
		}

		// Create the pipeline object
		VkGraphicsPipelineCreateInfo PipelineCreateInfo{};
		PipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		PipelineCreateInfo.stageCount = static_cast<uint32_t>(ShaderCreateInfos.size());
		PipelineCreateInfo.pStages = ShaderCreateInfos.data();
		PipelineCreateInfo.pVertexInputState = &VertexStateCreateInfo;
		PipelineCreateInfo.pInputAssemblyState = &InputAssembly;
		PipelineCreateInfo.pViewportState = &ViewportStateCreateInfo;
		PipelineCreateInfo.pRasterizationState = &Rasterizer;
		PipelineCreateInfo.pMultisampleState = &Multisampling;
		PipelineCreateInfo.pDepthStencilState = &DepthStencilCreateInfo;
		PipelineCreateInfo.pColorBlendState = &ColorBlending;
		PipelineCreateInfo.pDynamicState = &DynamicState;
		PipelineCreateInfo.layout = Result->PipelineLayout;
		PipelineCreateInfo.renderPass = VkRenderGraph->RenderPass;
		PipelineCreateInfo.subpass = CreateInfo.PassIndex;
		PipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
		PipelineCreateInfo.basePipelineIndex = -1;

		// TODO: use a global pipeline cache
		if (vkCreateGraphicsPipelines(GVulkanContext.Device, VK_NULL_HANDLE, 1, &PipelineCreateInfo, nullptr, &Result->Pipeline) != VK_SUCCESS)
		{
			//GLog->critical("Failed to create a Vulkan graphics pipeline");

			delete Result;
			return nullptr;
		}

		RECORD_RESOURCE_ALLOC(Result)

		return Result;
	}

	RenderGraph CreateRenderGraph(const RenderGraphCreateInfo& CreateInfo)
	{
		VulkanRenderGraph* Result = new VulkanRenderGraph;

		struct VkSubPassInfo
		{
			// Preserve the vector so it doesn't get invalidated
			std::vector<VkAttachmentReference> ColorAttachmentRefs;
			VkAttachmentReference DepthStencilAttachmentRef;
		};

		std::vector<VkSubPassInfo> SubPassInfos(CreateInfo.Passes.size());
		std::vector<VkAttachmentDescription> AttachmentDescriptions;
		std::vector<VkSubpassDescription> VkPassDescriptions;

		bool HasDepthAttachment = false;

		// Create attachment descriptions
		for (uint32_t AttachmentIndex = 0; AttachmentIndex < CreateInfo.Attachments.size(); AttachmentIndex++)
		{
			const RenderGraphAttachmentDescription& Description = CreateInfo.Attachments[AttachmentIndex];

			VkAttachmentDescription VkAttachmentDesc{};
			VkAttachmentDesc.format = AttachmentFormatToVkFormat(Description.Format);
			VkAttachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT; // Todo: msaa
			VkAttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			VkAttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			VkAttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			VkAttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			VkAttachmentDesc.initialLayout = AttachmentUsageToVkLayout(Description.InitialUsage);
			VkAttachmentDesc.finalLayout = AttachmentUsageToVkLayout(Description.FinalUsage);

			AttachmentDescriptions.push_back(VkAttachmentDesc);

			if (IsDepthFormat(Description.Format))
				HasDepthAttachment = true;
		}

		// Create subpasses
		for (uint32_t SubpassIndex = 0; SubpassIndex < CreateInfo.Passes.size(); SubpassIndex++)
		{
			const RenderPassInfo& PassInfo = CreateInfo.Passes[SubpassIndex];
			VkSubPassInfo& SubpassInfo = SubPassInfos[SubpassIndex];

			VkSubpassDescription VkDescription{};

			VkDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // Always bind to graphics, unless Vulkan eventually supports compute subpasses

			// Load attachment refs
			bool UsesDepthStencil = false;
			for(int32_t AttachRefIndex : PassInfo.OutputAttachments)
			{
				VkAttachmentReference AttachRef{};
				AttachRef.attachment = AttachRefIndex;

				if (IsColorFormat(CreateInfo.Attachments[AttachRefIndex].Format))
				{
					AttachRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
					SubpassInfo.ColorAttachmentRefs.push_back(AttachRef);
				}
				else // Depth
				{
					AttachRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
					SubpassInfo.DepthStencilAttachmentRef = AttachRef;
					UsesDepthStencil = true;
				}
			}

			VkDescription.colorAttachmentCount = static_cast<uint32_t>(SubpassInfo.ColorAttachmentRefs.size());
			VkDescription.pColorAttachments = SubpassInfo.ColorAttachmentRefs.data();
			VkDescription.inputAttachmentCount = 0; // TODO: eventually support these
			VkDescription.pInputAttachments = nullptr;
			VkDescription.pDepthStencilAttachment = UsesDepthStencil ? &SubpassInfo.DepthStencilAttachmentRef : nullptr;
			VkDescription.pPreserveAttachments = nullptr;
			VkDescription.preserveAttachmentCount = 0;

			VkPassDescriptions.push_back(VkDescription);
		}

		// Define explit external subpass dependencies
		VkSubpassDependency PreviousDep{};
		PreviousDep.srcSubpass = VK_SUBPASS_EXTERNAL;
		PreviousDep.dstSubpass = 0;
		PreviousDep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // Wait for the previous color attachment to be written
		PreviousDep.srcAccessMask = 0;
		PreviousDep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // Transition image at the color attachment output stage
		PreviousDep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; // Ensure we can write to color attachment

		VkSubpassDependency PostDep{};
		PostDep.srcSubpass = 0;
		PostDep.dstSubpass = VK_SUBPASS_EXTERNAL;
		PostDep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // We wrote to the FBO
		PostDep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		PostDep.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; // Next render pass needs this fbo to be made available
		PostDep.dstAccessMask = VK_ACCESS_SHADER_READ_BIT; // Ensure next pass can read from fbo

		// Add depth buffer dependencies
		if (HasDepthAttachment)
		{
			PreviousDep.srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			PreviousDep.dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			PreviousDep.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

			PostDep.srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			PostDep.srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			PostDep.dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			PostDep.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
		}

		std::vector<VkSubpassDependency> Deps = { PreviousDep, PostDep };

		// Create the render pass
		VkRenderPassCreateInfo RenderPassCreateInfo{};
		RenderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		RenderPassCreateInfo.attachmentCount = static_cast<uint32_t>(AttachmentDescriptions.size());
		RenderPassCreateInfo.pAttachments = AttachmentDescriptions.data();
		RenderPassCreateInfo.subpassCount = static_cast<uint32_t>(VkPassDescriptions.size());
		RenderPassCreateInfo.pSubpasses = VkPassDescriptions.data();
		RenderPassCreateInfo.dependencyCount = static_cast<uint32_t>(Deps.size());
		RenderPassCreateInfo.pDependencies = Deps.data();

		if (vkCreateRenderPass(GVulkanContext.Device, &RenderPassCreateInfo, nullptr, &Result->RenderPass) != VK_SUCCESS)
		{
			//GLog->critical("Failed to create Vulkan render pass");

			delete Result;
			return nullptr;
		}

		RECORD_RESOURCE_ALLOC(Result)

		return Result;
	}

	CommandBuffer CreateCommandBuffer(bool bOneTimeUse)
	{
		// Create a single command buffer
		VkCommandBufferAllocateInfo CmdBufAllocInfo{};
		CmdBufAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		CmdBufAllocInfo.commandPool = GVulkanContext.MainCommandPool;
		CmdBufAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; // TODO: support secondary command buffers
		CmdBufAllocInfo.commandBufferCount = 1;

		VulkanCommandBuffer* NewCmdBuf = new VulkanCommandBuffer;
		NewCmdBuf->bOneTimeUse = bOneTimeUse;

		if (vkAllocateCommandBuffers(GVulkanContext.Device, &CmdBufAllocInfo, &NewCmdBuf->CmdBuffer) != VK_SUCCESS)
		{
			//GLog->critical("Failed to allocate command buffer");

			delete NewCmdBuf;
			return nullptr;
		}

		RECORD_RESOURCE_ALLOC(NewCmdBuf)

		return NewCmdBuf;
	}

	ResourceSet CreateResourceSet(const ResourceSetCreateInfo& CreateInfo)
	{
		VulkanSwapChain* VkSwap = static_cast<VulkanSwapChain*>(CreateInfo.TargetSwap);
		VulkanResourceLayout* VkLayout = static_cast<VulkanResourceLayout*>(CreateInfo.Layout);

		VulkanResourceSet* Result = new VulkanResourceSet;

		uint32_t ImageCount = VkSwap->Images.size();

		// Allocate buffers
		for (const auto& ConstBuf : VkLayout->ConstantBuffers)
		{
			ConstantBufferStorage BufStorage;
			BufStorage.Binding = ConstBuf.Binding;

			for (uint32_t Image = 0; Image < ImageCount; Image++)
			{
				VkBuffer NewBuffer;
				VkDeviceMemory NewMemory;
				bool bSuccess = CreateBuffer(ConstBuf.BufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
					NewBuffer, NewMemory);

				if (!bSuccess)
				{
					//GLog->critical("Failed to allocate uniform buffer for resource set");
					return nullptr;
				}

				BufStorage.Buffers.push_back(NewBuffer);
				BufStorage.Memory.push_back(NewMemory);
			}

			Result->ConstantBuffers.push_back(BufStorage);
		}

		for (uint32_t Image = 0; Image < ImageCount; Image++)
		{
			// Allocate descriptor sets
			VkDescriptorSetAllocateInfo SetAllocInfo{};
			SetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			SetAllocInfo.descriptorPool = GVulkanContext.MainDscPool;
			SetAllocInfo.descriptorSetCount = 1;
			SetAllocInfo.pSetLayouts = &VkLayout->VkLayout;

			VkDescriptorSet NewSet;
			if (vkAllocateDescriptorSets(GVulkanContext.Device, &SetAllocInfo, &NewSet) != VK_SUCCESS)
			{
				//GLog->critical("Failed to create vulkan descriptor set");
				return nullptr;
			}

			Result->DescriptorSets.push_back(NewSet);
		}

		RECORD_RESOURCE_ALLOC(Result);

		return Result;
	}

	Texture CreateTexture(AttachmentFormat Format, uint32_t Width, uint32_t Height, uint64_t Flags, uint64_t ImageSize, void* Data)
	{
		VulkanTexture* Result = new VulkanTexture;

		Result->TextureFormat = Format;
		Result->Width = Width;
		Result->Height = Height;
		Result->TextureFlags = Flags;

		// Create staging buffer and write initial data to it
		if(Flags & TEXTURE_USAGE_WRITE)
		{
			CreateBuffer
			(
				ImageSize,
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				Result->StagingBuffer, Result->StagingBufferMemory
			);

			void* MappedData;
			vkMapMemory(GVulkanContext.Device, Result->StagingBufferMemory, 0, ImageSize, 0, &MappedData);
			{
				std::memcpy(MappedData, Data, ImageSize);
			}
			vkUnmapMemory(GVulkanContext.Device, Result->StagingBufferMemory);
		}

		VkImageCreateInfo ImageCreate{};
		ImageCreate.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		ImageCreate.imageType = VK_IMAGE_TYPE_2D;
		ImageCreate.extent.width = Width;
		ImageCreate.extent.height = Height;
		ImageCreate.extent.depth = 1;
		ImageCreate.mipLevels = 1;
		ImageCreate.arrayLayers = 1;
		ImageCreate.format = AttachmentFormatToVkFormat(Result->TextureFormat);
		ImageCreate.tiling = VK_IMAGE_TILING_OPTIMAL;
		ImageCreate.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		ImageCreate.samples = VK_SAMPLE_COUNT_1_BIT;
		ImageCreate.flags = 0;
		ImageCreate.usage = 0;

		// Set usage based on flags
		if (Flags & TEXTURE_USAGE_WRITE)
			ImageCreate.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		if (Flags & TEXTURE_USAGE_SAMPLE)
			ImageCreate.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
		if (Flags & TEXTURE_USAGE_READ)
			ImageCreate.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

		if(Flags & TEXTURE_USAGE_RT)
		{
			if(IsColorFormat(Format))
				ImageCreate.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			else
				ImageCreate.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		}

		if (vkCreateImage(GVulkanContext.Device, &ImageCreate, nullptr, &Result->TextureImage) != VK_SUCCESS)
		{
			//GLog->critical("Failed to create vulkan image");
			return nullptr;
		}

		VkMemoryRequirements MemReq{};
		vkGetImageMemoryRequirements(GVulkanContext.Device, Result->TextureImage, &MemReq);

		VkMemoryAllocateInfo AllocInfo{};
		AllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		AllocInfo.allocationSize = MemReq.size;
		AllocInfo.memoryTypeIndex = FindMemoryType(GVulkanContext.PhysicalDevice, MemReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		if (vkAllocateMemory(GVulkanContext.Device, &AllocInfo, nullptr, &Result->TextureMemory) != VK_SUCCESS)
		{
			//GLog->critical("Failed to create memory for vulkan image");
			return nullptr;
		}

		vkBindImageMemory(GVulkanContext.Device, Result->TextureImage, Result->TextureMemory, 0);

		// Write texture data
		if(Flags & TEXTURE_USAGE_WRITE)
		{
			ImmediateSubmitAndWait([&](CommandBuffer Buf)
			{

				TransitionTexture(Buf, Result, AttachmentUsage::Undefined, AttachmentUsage::TransferDestination);
			});

			ImmediateSubmitAndWait([&](CommandBuffer Buf)
			{
				VkCmdBuffer(Buf, [&](VkCommandBuffer Buf)
				{
					VkBufferImageCopy ImageCopy{};
					ImageCopy.bufferOffset = 0;
					ImageCopy.bufferRowLength = 0;
					ImageCopy.bufferImageHeight = 0;

					ImageCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					ImageCopy.imageSubresource.mipLevel = 0;
					ImageCopy.imageSubresource.baseArrayLayer = 0;
					ImageCopy.imageSubresource.layerCount = 1;

					ImageCopy.imageOffset = { 0, 0, 0 };
					ImageCopy.imageExtent = { Width, Height, 1 };

					// Copy buffer data to image
					vkCmdCopyBufferToImage
					(
						Buf,
						Result->StagingBuffer,
						Result->TextureImage,
						VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						1,
						&ImageCopy
					);
				});
			});

			// Transition image to be shader read optimal
			ImmediateSubmitAndWait([&](CommandBuffer Buf)
			{
				TransitionTexture(Buf, Result, AttachmentUsage::TransferDestination, AttachmentUsage::ShaderRead);
			});
		}
		else if(Flags & TEXTURE_USAGE_RT) // Can't be an RT and a CPU write at the same time
		{
			// Transition image to be an attachment
			ImmediateSubmitAndWait([&](CommandBuffer Buf)
			{
				if(IsColorFormat(Format))
					TransitionTexture(Buf, Result, AttachmentUsage::Undefined, AttachmentUsage::ColorAttachment);
				else
					TransitionTexture(Buf, Result, AttachmentUsage::Undefined, AttachmentUsage::DepthStencilAttachment);
			});
		}

		// Create image view
		VkImageViewCreateInfo ViewInfo{};
		ViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		ViewInfo.image = Result->TextureImage;
		ViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		ViewInfo.format = AttachmentFormatToVkFormat(Format);
		ViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		ViewInfo.subresourceRange.baseMipLevel = 0;
		ViewInfo.subresourceRange.levelCount = 1;
		ViewInfo.subresourceRange.baseArrayLayer = 0;
		ViewInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(GVulkanContext.Device, &ViewInfo, nullptr, &Result->TextureImageView) != VK_SUCCESS)
		{
			//GLog->critical("Failed to create vulkan image view");
			return nullptr;
		}

		if (Flags & TEXTURE_USAGE_SAMPLE) // Can't be an RT and a CPU write at the same time
		{

			VkSamplerCreateInfo SamplerInfo{};
			SamplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			SamplerInfo.magFilter = VK_FILTER_NEAREST; // TODO: make an option
			SamplerInfo.minFilter = VK_FILTER_NEAREST; // TODO: make an option
			SamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			SamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			SamplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			SamplerInfo.anisotropyEnable = VK_FALSE; // todo: enable anisotropy
			SamplerInfo.maxAnisotropy = 0;
			SamplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
			SamplerInfo.unnormalizedCoordinates = VK_FALSE;
			SamplerInfo.compareEnable = VK_FALSE;
			SamplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
			SamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			SamplerInfo.mipLodBias = 0.0f;
			SamplerInfo.minLod = 0.0f;
			SamplerInfo.maxLod = 0.0f;

			if (vkCreateSampler(GVulkanContext.Device, &SamplerInfo, nullptr, &Result->TextureSampler) != VK_SUCCESS)
			{
				//GLog->critical("Failed to create vulkan sampler");
				return nullptr;
			}
		}

		RECORD_RESOURCE_ALLOC(Result)

		return Result;
	}

	FrameBuffer CreateFrameBuffer(const FrameBufferCreateInfo& CreateInfo)
	{
		VulkanRenderGraph* VkRenderGraph = static_cast<VulkanRenderGraph*>(CreateInfo.Target);

		VulkanFrameBuffer* Result = new VulkanFrameBuffer;
		Result->AttachmentWidth = CreateInfo.Width;
		Result->AttachmentHeight = CreateInfo.Height;
		Result->CreatedFor = VkRenderGraph->RenderPass;

		for(Texture Attach : CreateInfo.Attachments)
		{
			Result->AllAttachments.push_back(static_cast<VulkanTexture*>(Attach));
		}

		if (!CreateFrameBufferResource(Result, VkRenderGraph->RenderPass, CreateInfo.Width, CreateInfo.Height))
		{
			return nullptr;
		}

		RECORD_RESOURCE_ALLOC(Result)

		return Result;
	}

	VertexBuffer CreateVertexBuffer(uint64_t Size, const void* Data)
	{
		VulkanVertexBuffer* VulkanVbo = new VulkanVertexBuffer;

		// Create staging buffer. This needs the transfer source bit since we will be transferring it to the device memory.
		CreateBuffer(Size,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			VulkanVbo->StagingVertexBuffer, VulkanVbo->StagingVertexBufferMemory
		);

		// Create device buffer. Because we will be copying the staging buffer to the device buffer, we need to make it eligible for transfer.
		CreateBuffer(Size,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			VulkanVbo->DeviceVertexBuffer, VulkanVbo->DeviceVertexBufferMemory
		);

		// Create fence to synchronize staging buffer access
		VkFenceCreateInfo FenceCreate{};
		FenceCreate.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		FenceCreate.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		if (vkCreateFence(GVulkanContext.Device, &FenceCreate, nullptr, &VulkanVbo->VertexStagingCompleteFence) != VK_SUCCESS)
		{
			//GLog->critical("Failed to create staging fence");
			return nullptr;
		}

		// Create command buffer used to transfer staging buffer to device buffer
		VkCommandBufferAllocateInfo AllocInfo{};
		AllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		AllocInfo.commandBufferCount = 1;
		AllocInfo.commandPool = GVulkanContext.MainCommandPool;
		AllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

		if (vkAllocateCommandBuffers(GVulkanContext.Device, &AllocInfo, &VulkanVbo->VertexStagingCommandBuffer) != VK_SUCCESS)
		{
			//GLog->critical("Failed to create staging command buffer");
			return nullptr;
		}

		if(Data)
		{
			UploadVertexBufferData(VulkanVbo, Data, Size);
		}

		RECORD_RESOURCE_ALLOC(VulkanVbo)

		return VulkanVbo;
	}

	IndexBuffer CreateIndexBuffer(uint64_t Size, const void* Data)
	{
		VulkanIndexBuffer* VulkanIbo = new VulkanIndexBuffer;

		CreateBuffer(Size,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			VulkanIbo->StagingIndexBuffer, VulkanIbo->StagingIndexBufferMemory
		);

		// Create device index buffer. Because we will be copying the staging buffer to the device buffer, we need to make it eligible for transfer.
		CreateBuffer(Size,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			VulkanIbo->DeviceIndexBuffer, VulkanIbo->DeviceIndexBufferMemory
		);

		// Create fence to synchronize staging buffer access
		VkFenceCreateInfo FenceCreate{};
		FenceCreate.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		FenceCreate.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		if (vkCreateFence(GVulkanContext.Device, &FenceCreate, nullptr, &VulkanIbo->IndexStagingCompleteFence) != VK_SUCCESS)
		{
			//GLog->critical("Failed to create staging fence");
			return nullptr;
		}

		// Create command buffer used to transfer staging buffer to device buffer
		VkCommandBufferAllocateInfo AllocInfo{};
		AllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		AllocInfo.commandBufferCount = 1;
		AllocInfo.commandPool = GVulkanContext.MainCommandPool;
		AllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

		if(vkAllocateCommandBuffers(GVulkanContext.Device, &AllocInfo, &VulkanIbo->IndexStagingCommandBuffer) != VK_SUCCESS)
		{
			//GLog->critical("Failed to create staging command buffer");
			return nullptr;
		}

		RECORD_RESOURCE_ALLOC(VulkanIbo)
		return VulkanIbo;
	}

	void DestroyVertexBuffer(VertexBuffer VertexBuffer)
	{
		VulkanVertexBuffer* VulkanVbo = static_cast<VulkanVertexBuffer*>(VertexBuffer);
		REMOVE_RESOURCE_ALLOC(VulkanVbo)

		vkDeviceWaitIdle(GVulkanContext.Device);

		vkFreeMemory(GVulkanContext.Device, VulkanVbo->DeviceVertexBufferMemory, nullptr);
		vkFreeMemory(GVulkanContext.Device, VulkanVbo->StagingVertexBufferMemory, nullptr);
		vkDestroyBuffer(GVulkanContext.Device, VulkanVbo->StagingVertexBuffer, nullptr);
		vkDestroyBuffer(GVulkanContext.Device, VulkanVbo->DeviceVertexBuffer, nullptr);
		vkDestroyFence(GVulkanContext.Device, VulkanVbo->VertexStagingCompleteFence, nullptr);
		vkFreeCommandBuffers(GVulkanContext.Device, GVulkanContext.MainCommandPool, 1, &VulkanVbo->VertexStagingCommandBuffer);

		delete VulkanVbo;
	}

	void DestroyIndexBuffer(IndexBuffer IndexBuffer)
	{
		VulkanIndexBuffer* VulkanIbo = static_cast<VulkanIndexBuffer*>(IndexBuffer);
		REMOVE_RESOURCE_ALLOC(VulkanVbo)

		vkDeviceWaitIdle(GVulkanContext.Device);

		vkFreeMemory(GVulkanContext.Device, VulkanIbo->DeviceIndexBufferMemory, nullptr);
		vkFreeMemory(GVulkanContext.Device, VulkanIbo->StagingIndexBufferMemory, nullptr);
		vkDestroyBuffer(GVulkanContext.Device, VulkanIbo->StagingIndexBuffer, nullptr);
		vkDestroyBuffer(GVulkanContext.Device, VulkanIbo->DeviceIndexBuffer, nullptr);
		vkDestroyFence(GVulkanContext.Device, VulkanIbo->IndexStagingCompleteFence, nullptr);
		vkFreeCommandBuffers(GVulkanContext.Device, GVulkanContext.MainCommandPool, 1, &VulkanIbo->IndexStagingCommandBuffer);

		delete VulkanIbo;
	}

	void DestroyFrameBuffer(FrameBuffer FrameBuffer)
	{
		VulkanFrameBuffer* VkFbo = static_cast<VulkanFrameBuffer*>(FrameBuffer);
		REMOVE_RESOURCE_ALLOC(VkFbo)

		vkDeviceWaitIdle(GVulkanContext.Device);

		vkDestroyFramebuffer(GVulkanContext.Device, VkFbo->VulkanFbo, nullptr);

		delete VkFbo;
	}

	void DestroyCommandBuffer(CommandBuffer CmdBuffer)
	{
		VulkanCommandBuffer* VkCmdBuffer = static_cast<VulkanCommandBuffer*>(CmdBuffer);
		REMOVE_RESOURCE_ALLOC(VkCmdBuffer)

		vkQueueWaitIdle(GVulkanContext.GraphicsQueue);
		vkFreeCommandBuffers(GVulkanContext.Device, GVulkanContext.MainCommandPool, 1, &VkCmdBuffer->CmdBuffer);

		delete VkCmdBuffer;
	}

	void DestroyRenderGraph(RenderGraph Graph)
	{
		VulkanRenderGraph* VkRenderGraph = static_cast<VulkanRenderGraph*>(Graph);

		vkQueueWaitIdle(GVulkanContext.GraphicsQueue);
		vkDestroyRenderPass(GVulkanContext.Device, VkRenderGraph->RenderPass, nullptr);

		REMOVE_RESOURCE_ALLOC(VkRenderGraph)

		delete VkRenderGraph;
	}

	void DestroyPipeline(Pipeline Pipeline)
	{
		VulkanPipeline* VkPipeline = static_cast<VulkanPipeline*>(Pipeline);

		vkQueueWaitIdle(GVulkanContext.GraphicsQueue);

		// Destroy pipeline layout
		vkDestroyPipelineLayout(GVulkanContext.Device, VkPipeline->PipelineLayout, nullptr);

		// Destroy pipeline
		vkDestroyPipeline(GVulkanContext.Device, VkPipeline->Pipeline, nullptr);

		REMOVE_RESOURCE_ALLOC(VkPipeline)

		delete VkPipeline;
	}

	void DestroyProgram(ShaderProgram Shader)
	{
		VulkanShader* VkShader = static_cast<VulkanShader*>(Shader);

		vkQueueWaitIdle(GVulkanContext.GraphicsQueue);

		if (VkShader->bHasVertexShader)
			vkDestroyShaderModule(GVulkanContext.Device, VkShader->VertexModule, nullptr);
		if (VkShader->bHasFragmentShader)
			vkDestroyShaderModule(GVulkanContext.Device, VkShader->FragmentModule, nullptr);

		REMOVE_RESOURCE_ALLOC(VkShader)

		delete VkShader;
	}

	void DestroyTexture(Texture Image)
	{
		vkDeviceWaitIdle(GVulkanContext.Device);
		VulkanTexture* VkTex = static_cast<VulkanTexture*>(Image);

		vkFreeMemory(GVulkanContext.Device, VkTex->TextureMemory, nullptr);
		vkDestroyImage(GVulkanContext.Device, VkTex->TextureImage, nullptr);
		vkDestroyImageView(GVulkanContext.Device, VkTex->TextureImageView, nullptr);

		if(VkTex->TextureFlags & TEXTURE_USAGE_WRITE)
		{
			vkFreeMemory(GVulkanContext.Device, VkTex->StagingBufferMemory, nullptr);
			vkDestroyBuffer(GVulkanContext.Device, VkTex->StagingBuffer, nullptr);
		}

		if (VkTex->TextureFlags & TEXTURE_USAGE_SAMPLE)
		{
			vkDestroySampler(GVulkanContext.Device, VkTex->TextureSampler, nullptr);
		}

		REMOVE_RESOURCE_ALLOC(VkTex)

		delete VkTex;
	}

}

#endif