#pragma once

#include "llrm.h"
#include "vulkan/vulkan.h"


// Helper to record stack traces of allocated resources to track down resource that need to be freed
#ifdef VULKAN_VALIDATION
#define RECORD_RESOURCE_ALLOC(Res)	AllocatedTraces.insert(std::make_pair(Res, boost::stacktrace::stacktrace()));
#define REMOVE_RESOURCE_ALLOC(Res)	AllocatedTraces.erase(Res);
std::unordered_map<void*, boost::stacktrace::stacktrace> AllocatedTraces;
#else
#define RECORD_RESOURCE_ALLOC(Res) // Do nothing
#define REMOVE_RESOURCE_ALLOC(Res) // Do nothing
#endif

#define MAX_FRAMES_IN_FLIGHT 3

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

struct VulkanTexture
{
	// This staging buffer is used if the texture can be uploaded to from the CPU
	VkBuffer StagingBuffer;
	VkDeviceMemory StagingBufferMemory;

	VkDeviceMemory TextureMemory;
	VkImage TextureImage;

	VkImageView TextureImageView;
	VkSampler TextureSampler;

	uint64_t TextureFlags{};

	llrm::AttachmentFormat TextureFormat{};
	uint32_t Width = 0, Height = 0;
};

struct VulkanSwapChain
{
	VkSwapchainKHR SwapChain;
	llrm::AttachmentFormat ImageFormat;
	VkPresentModeKHR PresentMode;
	VkExtent2D SwapChainExtent;

	std::vector<VulkanTexture> Images;

	int32_t CurrentFrame = 0;

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

	// Resources for current frame
	VulkanSwapChain* CurrentSwapChain{};
	GLFWwindow* CurrentWindow{};
	llrm::Surface CurrentSurface{};

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

struct VulkanShader
{
	bool bHasVertexShader;
	bool bHasFragmentShader;
	VkShaderModule VertexModule;
	VkShaderModule FragmentModule;
};

struct VulkanPipeline
{
	VkPipelineLayout PipelineLayout;
	VkPipeline Pipeline;
};

struct VulkanRenderGraph
{
	VkRenderPass RenderPass;
};

struct VulkanVertexBuffer
{
	VkBuffer DeviceVertexBuffer;
	VkDeviceMemory DeviceVertexBufferMemory;

	VkBuffer StagingVertexBuffer;
	VkDeviceMemory StagingVertexBufferMemory;

	VkCommandBuffer VertexStagingCommandBuffer;
	VkFence VertexStagingCompleteFence;
};

struct VulkanIndexBuffer
{
	VkBuffer DeviceIndexBuffer;
	VkDeviceMemory DeviceIndexBufferMemory;

	VkBuffer StagingIndexBuffer;
	VkDeviceMemory StagingIndexBufferMemory;

	VkCommandBuffer IndexStagingCommandBuffer;
	VkFence IndexStagingCompleteFence;
};

struct VulkanFrameBuffer
{
	uint32_t AttachmentWidth;
	uint32_t AttachmentHeight;

	std::vector<VulkanTexture*>    AllAttachments;

	VkFramebuffer VulkanFbo;

	// Store some of the initial creation info for re-creating the framebuffers
	llrm::FramebufferAttachmentDescription              DepthStencilAttachmentDesc;
	VkRenderPass                                  CreatedFor;
};

struct VulkanCommandBuffer
{
	VulkanSwapChain* CurrentSwapChain{};
	VulkanFrameBuffer* CurrentFbo{};
	VkCommandBuffer CmdBuffer;
	bool bDynamic = false; // If bTargetSwapChain is true, whether this command buffer will be re-recorded every frame (true) or very in-frequently recorded
	bool bOneTimeUse = false; // Whether this command buffer is intended to only be used once
	bool bTargetSwapChain = false; // Whether this command buffer targets the swap chain (i.e. references a frame with vkBeginRenderPass)

	VulkanPipeline* BoundPipeline = nullptr;
};

struct VulkanResourceLayout
{
	VkDescriptorSetLayout VkLayout;
	std::vector<llrm::ConstantBufferDescription> ConstantBuffers;
	std::vector<llrm::TextureDescription> TextureBindings;
};

struct ConstantBufferStorage
{
	uint32_t Binding;
	std::vector<VkBuffer> Buffers;
	std::vector<VkDeviceMemory> Memory;
};

struct VulkanResourceSet
{
	std::vector<ConstantBufferStorage> ConstantBuffers;
	std::vector<VkDescriptorSet> DescriptorSets;
};