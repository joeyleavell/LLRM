#include <iostream>

#include "llrm.h"
#include <unordered_set>
#include <vector>

#ifdef LLRM_VULKAN

// LLRM uses glfw for now, move away from this to become independent from the windowing framework.
#include "vulkan/vulkan.h"
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

// Helper to record stack traces of allocated resources to track down resource that need to be freed
#ifdef VULKAN_VALIDATION
#define RECORD_RESOURCE_ALLOC(Res)	AllocatedTraces.insert(std::make_pair(Res, boost::stacktrace::stacktrace()));
#define REMOVE_RESOURCE_ALLOC(Res)	AllocatedTraces.erase(Res);
	std::unordered_map<void*, boost::stacktrace::stacktrace> AllocatedTraces;
#else
#define RECORD_RESOURCE_ALLOC(Res) // Do nothing
#define REMOVE_RESOURCE_ALLOC(Res) // Do nothing
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
				if(glfwGetPhysicalDevicePresentationSupport(Instance, PhysicalDevice, QueueFamIndex))
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

// Frame in flight
struct VulkanFrame
{
	VkSemaphore ImageAvailableSemaphore;
	VkSemaphore RenderingFinishedSemaphore;
	VkFence InFlightFence;

	VulkanFrame(VkSemaphore ImageAvailableSem, VkSemaphore RenderingFinishedSem, VkFence FlightFence)
	{
		this->ImageAvailableSemaphore = ImageAvailableSem;
		this->RenderingFinishedSemaphore = RenderingFinishedSem;
		this->InFlightFence = FlightFence;
	}
};

struct VulkanSwapChain
{
	VkSwapchainKHR SwapChain;
	VkFormat ImageFormat;
	VkPresentModeKHR PresentMode;
	VkExtent2D SwapChainExtent;
	uint32_t ImageCount;
	VkImage* Images;
	std::vector<VkImageView> ImageViews;

	VkRenderPass MainPass;

	int32_t CurrentFrame = 0;

	std::vector<VkFramebuffer> FrameBuffers;

	std::vector<VkCommandBuffer> BuffersToSubmit;

	/**
	 * Keep track of frames in flight for this swap chain.
	 */
	std::vector<VulkanFrame> FramesInFlight;

	std::vector<VkFence> ImageFences;

	// The image index acquired for the current frame
	uint32_t AcquiredImageIndex;

	bool bInsideFrame = false;

};

struct VulkanSurface
{
	VkSurfaceKHR VkSurface;
};

struct VulkanContext
{
	/**
	 * The global Vulkan instance.
	 */
	VkInstance Instance;

	/**
	 * The physical device selected for all Vulkan operations. MultiGPU operations are currently not supported, so there will only ever be a single physical device.
	 */
	VkPhysicalDevice PhysicalDevice;

	/**
	 * The logical device created for a particular physical device.
	 */
	VkDevice Device;

	/**
	 * The primary pool for allocating command buffers.
	 */
	VkCommandPool MainCommandPool;

	/**
	 * The primary pool for allocating descriptor sets.
	 */
	VkDescriptorPool MainDscPool;

	/**
	 * The queue family index of the graphics queue.
	 */
	uint32_t GraphicsQueueFamIndex;

	/**
	 * The queue family index of the graphics queue.
	 */
	uint32_t PresentQueueFamIndex;

	/**
	 * The primary graphics queue for submitting command buffers.
	 */
	VkQueue GraphicsQueue;

	/**
	 * The queue for presenting rendered images.
	 */
	VkQueue PresentQueue;

	VulkanSwapChain* CurrentSwapChain{};

	/**
	 * Extensions that are used by the Vulkan instance.
	 */
	std::vector<const char*> InstanceExtensions;

	/**
	 * Extensions that are used by the Vulkan device.
	 */
	std::vector<const char*> DeviceExtensions;

	/**
	 * Validation layers that are used by the Vulkan application.
	 */
	std::vector<const char*> ValidationLayers;

	/**
	 * The debug messenger for the instance, if validation is enabled.
	 */
	VkDebugUtilsMessengerEXT DebugMessenger;

};

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


namespace llrm
{
	VulkanContext GVulkanContext;

	llrm::Context CreateContext(GLFWwindow* Window)
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

}

#endif