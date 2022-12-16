#include "VulkanInterface.h"
#include <unordered_set>
#include "ShaderCompile.h"
#include "Window.h"
#include "spdlog/spdlog.h"
#include "GLFW/glfw3.h"
#include <algorithm>
#include "RenderHelper.h"
#include "boost/stacktrace.hpp"

GlobalVulkanInfo GVulkanInfo;

std::function<void(CommandBuffer Cmd, std::function<void(VkCommandBuffer&, int32_t)>)> ForEachCmdBuffer =
[](CommandBuffer Cmd, std::function<void(VkCommandBuffer&, int32_t ImageIndex)> Inner)
{
	VulkanCommandBuffer* VkCmd = static_cast<VulkanCommandBuffer*>(Cmd);
    if(VkCmd->bTargetSwapChain && VkCmd->bDynamic)
    {
        Inner(VkCmd->CmdBuffers[GVulkanInfo.CurrentSwapChain->AcquiredImageIndex], GVulkanInfo.CurrentSwapChain->AcquiredImageIndex);
    }
    else
    {
        for(int32_t CmdBufferIndex = 0; CmdBufferIndex < VkCmd->CmdBuffers.size(); CmdBufferIndex++)
        {
            Inner(VkCmd->CmdBuffers[CmdBufferIndex], CmdBufferIndex);
        }
    }
};

VkFormat AttachmentFormatToVkFormat(const SwapChain ReferencingSwap, AttachmentFormat Format)
{
//	assert((Format == AttachmentFormat::MatchBackBuffer && ReferencingSwap) || (Format != AttachmentFormat::MatchBackBuffer && !ReferencingSwap()));

	switch (Format)
	{
	case AttachmentFormat::B8G8R8A8_SRGB:
		assert(!ReferencingSwap);
		return VK_FORMAT_B8G8R8A8_SRGB;
	case AttachmentFormat::R32_UINT:
		assert(!ReferencingSwap);
		return VK_FORMAT_R32_UINT;
	case AttachmentFormat::R32_FLOAT:
		assert(!ReferencingSwap);
		return VK_FORMAT_R32_SFLOAT;
	case AttachmentFormat::R32_SINT:
		assert(!ReferencingSwap);
		return VK_FORMAT_R32_SINT;
	case AttachmentFormat::R8_UINT:
		assert(!ReferencingSwap);
		return VK_FORMAT_R8_UINT;
	case AttachmentFormat::MatchBackBuffer:
		assert(ReferencingSwap);
		if (const VulkanSwapChain* VkSwap = static_cast<const VulkanSwapChain*>(ReferencingSwap))
			return VkSwap->ImageFormat;
	case AttachmentFormat::DepthStencil:
		VkFormat DepthStencilFormat = VK_FORMAT_D24_UNORM_S8_UINT;
		VkFormatProperties FormatProps;
		vkGetPhysicalDeviceFormatProperties(GVulkanInfo.PhysicalDevice, DepthStencilFormat, &FormatProps);
		VkFormatFeatureFlags RequiredFlags = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;

		// Assume optimal tiling, create assertion for our required flags
		if((FormatProps.optimalTilingFeatures & RequiredFlags) != RequiredFlags)
		{
			GLog->error("Physical device does not support depth/stencil attachment");
			std::abort();
		}

		return DepthStencilFormat;
	}

	GLog->error("Attachment not found");
	return VK_FORMAT_B8G8R8A8_SRGB;
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
	switch(Operator)
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
	switch(Factor)
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

	GLog->error("Blend factor not found");
	return VK_BLEND_FACTOR_ONE;
}

bool CreateFrameBufferImages(VulkanFrameBuffer* DstFbo, std::vector<FramebufferAttachmentDescription> Attachments, uint32_t Width, uint32_t Height)
{

	// Create color attachments
	for (uint32_t ColorAttachmentIndex = 0; ColorAttachmentIndex < Attachments.size(); ColorAttachmentIndex++)
	{
		const FramebufferAttachmentDescription& AttachDesc = Attachments[ColorAttachmentIndex];

		// Use this function to ensure this and RenderGraph get the same color attachment format
		VkFormat ColorAttachmentFormat = AttachmentFormatToVkFormat(nullptr, AttachDesc.Format);

		// Create image image memory
		VkImageCreateInfo ColorAttachCreateInfo{};
		ColorAttachCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		ColorAttachCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		ColorAttachCreateInfo.extent.width = Width;
		ColorAttachCreateInfo.extent.height = Height;
		ColorAttachCreateInfo.extent.depth = 1;
		ColorAttachCreateInfo.mipLevels = 1;
		ColorAttachCreateInfo.arrayLayers = 1;
		ColorAttachCreateInfo.format = ColorAttachmentFormat;
		ColorAttachCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		ColorAttachCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		// This will both be written to and sampled by from the shader
		// This will also allow us to read back the framebuffer attachment to the cpu
		ColorAttachCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		ColorAttachCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		ColorAttachCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		ColorAttachCreateInfo.flags = 0;

		VkImage ResultImage;
		if (vkCreateImage(GVulkanInfo.Device, &ColorAttachCreateInfo, nullptr, &ResultImage) != VK_SUCCESS)
		{
			GLog->critical("Failed to create color attachment image for Vulkan framebuffer");
			return false;
		}


		DstFbo->ColorAttachmentImages.push_back(ResultImage);

		// Create device memory
		VkMemoryRequirements ImageMemRequirements;
		vkGetImageMemoryRequirements(GVulkanInfo.Device, ResultImage, &ImageMemRequirements);

		// This memory will never be accessed by the host, so keep it device local
		int32_t MemTypeIndex = FindMemoryType(ImageMemRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		if (MemTypeIndex < 0)
		{
			GLog->critical("Failed to find memory type while creating device memory for image");
			return false;
		}

		VkMemoryAllocateInfo ImageAllocInfo{};
		ImageAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		ImageAllocInfo.allocationSize = ImageMemRequirements.size;
		ImageAllocInfo.memoryTypeIndex = MemTypeIndex;

		VkDeviceMemory ColorAttachMemory;
		if (vkAllocateMemory(GVulkanInfo.Device, &ImageAllocInfo, nullptr, &ColorAttachMemory) != VK_SUCCESS)
		{
			GLog->critical("Failed to create device memory for framebuffer color attachment");
			return false;
		}

		DstFbo->ColorAttachmentMemory.push_back(ColorAttachMemory);

		// Bind the memory to the image
		vkBindImageMemory(GVulkanInfo.Device, ResultImage, ColorAttachMemory, 0);

		// Create an image view for the newly created image
		VkImageViewCreateInfo ImageViewCreateInfo{};
		ImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		ImageViewCreateInfo.image = ResultImage;
		ImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		ImageViewCreateInfo.format = ColorAttachmentFormat;
		ImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		ImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		ImageViewCreateInfo.subresourceRange.levelCount = 1;
		ImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		ImageViewCreateInfo.subresourceRange.layerCount = 1;

		VkImageView ColorAttachmentImageView;
		if (vkCreateImageView(GVulkanInfo.Device, &ImageViewCreateInfo, nullptr, &ColorAttachmentImageView) != VK_SUCCESS)
		{
			GLog->critical("Failed to create image view for vulkan framebuffer color attachment");
			return false;
		}

		DstFbo->ColorAttachmentImageViews.push_back(ColorAttachmentImageView);
		DstFbo->AllAttachments.push_back(ColorAttachmentImageView);

		// Immediately transition image to correct layout
		ImmediateSubmitAndWait([=](CommandBuffer Cmd)
		{
			GRenderAPI->TransitionFrameBufferColorAttachment(Cmd, DstFbo, ColorAttachmentIndex, AttachmentUsage::Undefined, AttachDesc.InitialImageUsage);
		});
	}

	// Create optional depth stencil attachment
	if(DstFbo->bHasDepthStencilAttachment)
	{
		const FramebufferAttachmentDescription& AttachDesc = DstFbo->DepthStencilAttachmentDesc;

		// Use this function to ensure this and RenderGraph get the same color attachment format
		VkFormat DepthStencilAtachmentFormat = AttachmentFormatToVkFormat(nullptr, AttachmentFormat::DepthStencil);

		// Create image memory
		VkImageCreateInfo DepthStencilCreateInfo{};
		DepthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		DepthStencilCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		DepthStencilCreateInfo.extent.width = Width;
		DepthStencilCreateInfo.extent.height = Height;
		DepthStencilCreateInfo.extent.depth = 1;
		DepthStencilCreateInfo.mipLevels = 1;
		DepthStencilCreateInfo.arrayLayers = 1;
		DepthStencilCreateInfo.format = DepthStencilAtachmentFormat;
		DepthStencilCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		DepthStencilCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		DepthStencilCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT; // This will possibly both be written to and sampled by from the shader
		DepthStencilCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		DepthStencilCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		DepthStencilCreateInfo.flags = 0;

		if (vkCreateImage(GVulkanInfo.Device, &DepthStencilCreateInfo, nullptr, &DstFbo->DepthStencilImage) != VK_SUCCESS)
		{
			GLog->critical("Failed to create color attachment image for Vulkan framebuffer");
			return false;
		}

		// Create device memory
		VkMemoryRequirements ImageMemRequirements;
		vkGetImageMemoryRequirements(GVulkanInfo.Device, DstFbo->DepthStencilImage, &ImageMemRequirements);

		// This memory will never be accessed by the host, so keep it device local
		int32_t MemTypeIndex = FindMemoryType(ImageMemRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		if (MemTypeIndex < 0)
		{
			GLog->critical("Failed to find memory type while creating device memory for image");
			return false;
		}

		VkMemoryAllocateInfo ImageAllocInfo{};
		ImageAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		ImageAllocInfo.allocationSize = ImageMemRequirements.size;
		ImageAllocInfo.memoryTypeIndex = MemTypeIndex;

		if (vkAllocateMemory(GVulkanInfo.Device, &ImageAllocInfo, nullptr, &DstFbo->DepthStencilMemory) != VK_SUCCESS)
		{
			GLog->critical("Failed to create device memory for framebuffer color attachment");
			return false;
		}

		// Bind the memory to the image
		vkBindImageMemory(GVulkanInfo.Device, DstFbo->DepthStencilImage, DstFbo->DepthStencilMemory, 0);

		// Create an image view for the newly created image
		VkImageViewCreateInfo ImageViewCreateInfo{};
		ImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		ImageViewCreateInfo.image = DstFbo->DepthStencilImage;
		ImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		ImageViewCreateInfo.format = DepthStencilAtachmentFormat;
		ImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		ImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		ImageViewCreateInfo.subresourceRange.levelCount = 1;
		ImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		ImageViewCreateInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(GVulkanInfo.Device, &ImageViewCreateInfo, nullptr, &DstFbo->DepthStencilImageView) != VK_SUCCESS)
		{
			GLog->critical("Failed to create image view for vulkan framebuffer color attachment");
			return false;
		}

		DstFbo->AllAttachments.push_back(DstFbo->DepthStencilImageView);

		// Immediately transition image to correct layout
		ImmediateSubmitAndWait([=](CommandBuffer Cmd)
		{
			GRenderAPI->TransitionFrameBufferDepthStencilAttachment(Cmd, DstFbo, AttachmentUsage::Undefined, AttachDesc.InitialImageUsage);
		});
	}

	return true;
}

bool CreateFrameBufferSamplers(VulkanFrameBuffer* DstFbo, uint32_t ColorAttachmentCount)
{
	// Create framebuffer samplers
	for (uint32_t ColorAttachmentIndex = 0; ColorAttachmentIndex < ColorAttachmentCount; ColorAttachmentIndex++)
	{
		VkFilter SampFilter = FilterToVkFilter(DstFbo->ColorAttachmentDescriptions[ColorAttachmentIndex].SamplerFilter);

		VkSamplerCreateInfo SampleCreateInfo{};
		SampleCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		SampleCreateInfo.magFilter = SampFilter;
		SampleCreateInfo.minFilter = SampFilter;
		SampleCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		SampleCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		SampleCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		SampleCreateInfo.anisotropyEnable = VK_FALSE;
		SampleCreateInfo.maxAnisotropy = 0;
		SampleCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		SampleCreateInfo.unnormalizedCoordinates = VK_FALSE;
		SampleCreateInfo.compareEnable = VK_FALSE;
		SampleCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		SampleCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		SampleCreateInfo.mipLodBias = 0.0f;
		SampleCreateInfo.minLod = 0.0f;
		SampleCreateInfo.maxLod = 0.0f;

		VkSampler ResultSampler;
		if (vkCreateSampler(GVulkanInfo.Device, &SampleCreateInfo, nullptr, &ResultSampler) != VK_SUCCESS)
		{
			GLog->critical("Failed to create sampler when creating vulkan framebuffer");
			return false;
		}

		DstFbo->ColorAttachmentSamplers.push_back(ResultSampler);
	}

	return true;
}

bool CreateFrameBufferResource(VulkanFrameBuffer* DstFbo, VkRenderPass Pass, uint32_t Width, uint32_t Height)
{
	// Finally, create the framebuffer
	VkFramebufferCreateInfo FrameBufferCreateInfo{};
	FrameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	FrameBufferCreateInfo.renderPass = Pass;
	FrameBufferCreateInfo.attachmentCount = static_cast<uint32_t>(DstFbo->AllAttachments.size());
	FrameBufferCreateInfo.pAttachments = DstFbo->AllAttachments.data();
	FrameBufferCreateInfo.width = Width;
	FrameBufferCreateInfo.height = Height;
	FrameBufferCreateInfo.layers = 1;

	if (vkCreateFramebuffer(GVulkanInfo.Device, &FrameBufferCreateInfo, nullptr, &DstFbo->VulkanFbo) != VK_SUCCESS)
	{
		GLog->critical("Failed to create vulkan framebuffer resource");
		return false;
	}

	return true;
}

void DestroyFramebufferImages(VulkanFrameBuffer* VkFbo)
{
	std::for_each(VkFbo->ColorAttachmentImageViews.begin(), VkFbo->ColorAttachmentImageViews.end(), [](const VkImageView& ImageView)
	{
		vkDestroyImageView(GVulkanInfo.Device, ImageView, nullptr);
	});

	std::for_each(VkFbo->ColorAttachmentMemory.begin(), VkFbo->ColorAttachmentMemory.end(), [](const VkDeviceMemory& ImageMemory)
	{
		vkFreeMemory(GVulkanInfo.Device, ImageMemory, nullptr);
	});

	std::for_each(VkFbo->ColorAttachmentImages.begin(), VkFbo->ColorAttachmentImages.end(), [](const VkImage& Image)
	{
		vkDestroyImage(GVulkanInfo.Device, Image, nullptr);
	});

	if(VkFbo->bHasDepthStencilAttachment)
	{
		vkDestroyImage(GVulkanInfo.Device, VkFbo->DepthStencilImage, nullptr);
		vkFreeMemory(GVulkanInfo.Device, VkFbo->DepthStencilMemory, nullptr);
		vkDestroyImageView(GVulkanInfo.Device, VkFbo->DepthStencilImageView, nullptr);
	}

	VkFbo->ColorAttachmentImages.clear();
	VkFbo->ColorAttachmentImageViews.clear();
	VkFbo->ColorAttachmentMemory.clear();
	VkFbo->AllAttachments.clear();
}

void DestroyFramebufferSamplers(VulkanFrameBuffer* VkFbo)
{
	std::for_each(VkFbo->ColorAttachmentSamplers.begin(), VkFbo->ColorAttachmentSamplers.end(), [](const VkSampler& Sampler)
	{
		vkDestroySampler(GVulkanInfo.Device, Sampler, nullptr);
	});

	VkFbo->ColorAttachmentSamplers.clear();
}

uint32_t GetCurrentViewportHeight(CommandBuffer Buf)
{
	VulkanCommandBuffer* VkCmd = static_cast<VulkanCommandBuffer*>(Buf);

	uint32_t ViewportHeight = 0;
	if (VkCmd->CurrentSwapChain)
		ViewportHeight = VkCmd->CurrentSwapChain->SwapChainExtent.height;
	else if (VkCmd->CurrentFbo)
		ViewportHeight = VkCmd->CurrentFbo->AttachmentHeight;
	else
		GLog->error("Invalid swap chain");

	return ViewportHeight;
}

bool CreateBuffer(uint64_t Size, VkBufferUsageFlags BufferUsage, VkMemoryPropertyFlags MemPropertyFlags, VkBuffer& OutBuffer, VkDeviceMemory& OutBufferMemory)
{
	VkBufferCreateInfo VboCreateInfo{};
	VboCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	VboCreateInfo.size = Size;
	VboCreateInfo.usage = BufferUsage;
	VboCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateBuffer(GVulkanInfo.Device, &VboCreateInfo, nullptr, &OutBuffer) != VK_SUCCESS)
	{
		GLog->critical("Failed to create vulkan buffer");
		return false;
	}

	VkMemoryRequirements BufferMemRequirements{};
	vkGetBufferMemoryRequirements(GVulkanInfo.Device, OutBuffer, &BufferMemRequirements);

	VkMemoryAllocateInfo AllocInfo{};
	AllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	AllocInfo.allocationSize = BufferMemRequirements.size;
	AllocInfo.memoryTypeIndex = FindMemoryType(BufferMemRequirements.memoryTypeBits, MemPropertyFlags);

	if (vkAllocateMemory(GVulkanInfo.Device, &AllocInfo, nullptr, &OutBufferMemory) != VK_SUCCESS)
	{
		GLog->critical("Failed to allocate vulkan memory");
		return false;
	}

	// Bind the memory
	if (vkBindBufferMemory(GVulkanInfo.Device, OutBuffer, OutBufferMemory, 0) != VK_SUCCESS)
	{
		GLog->info("Failed to bind memory to vulkan buffer");
		return false;
	}

	return true;
}

void VulkanRenderingInterface::Reset(CommandBuffer Buf)
{
    ForEachCmdBuffer(Buf, [&](VkCommandBuffer& CmdBuffer, int32_t ImageIndex)
	{
		vkResetCommandBuffer(CmdBuffer, 0);
	});
}

void VulkanRenderingInterface::Begin(CommandBuffer Buf)
{
    ForEachCmdBuffer(Buf, [&](VkCommandBuffer& CmdBuffer, int32_t ImageIndex)
	{
		VulkanCommandBuffer* VkCmd = static_cast<VulkanCommandBuffer*>(Buf);

		VkCommandBufferBeginInfo BeginInfo{};
		BeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		BeginInfo.pInheritanceInfo = nullptr;
		BeginInfo.flags = VkCmd->bOneTimeUse ? VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT : 0;

		if (vkBeginCommandBuffer(CmdBuffer, &BeginInfo) != VK_SUCCESS)
		{
			GLog->critical("Failed to begin recording vulkan command buffer");
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
	else if(Old == AttachmentUsage::TransferDestination)
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
		GLog->critical("Vulkan image layout transition not supported");
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
		GLog->critical("Vulkan image layout transition not supported");
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

void VulkanRenderingInterface::TransitionFrameBufferColorAttachment(CommandBuffer Buf, FrameBuffer Source, uint32_t AttachmentIndex,
	AttachmentUsage Old, AttachmentUsage New)
{
	VulkanFrameBuffer* VkFbo = static_cast<VulkanFrameBuffer*>(Source);

	ForEachCmdBuffer(Buf, [Old, New, VkFbo, AttachmentIndex](VkCommandBuffer& CmdBuffer, int32_t ImageIndex)
	{
		TransitionCmd(CmdBuffer, VkFbo->ColorAttachmentImages[AttachmentIndex], VK_IMAGE_ASPECT_COLOR_BIT, Old, New);
	});
}

void VulkanRenderingInterface::TransitionFrameBufferDepthStencilAttachment(CommandBuffer Buf, FrameBuffer Source,
	AttachmentUsage Old, AttachmentUsage New)
{
	VulkanFrameBuffer* VkFbo = static_cast<VulkanFrameBuffer*>(Source);

	ForEachCmdBuffer(Buf, [Old, New, VkFbo](VkCommandBuffer& CmdBuffer, int32_t ImageIndex)
	{
		TransitionCmd(CmdBuffer, VkFbo->DepthStencilImage, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, Old, New);
	});
}

void VulkanRenderingInterface::TransitionTexture(CommandBuffer Buf, Texture Image, AttachmentUsage Old,
                                                 AttachmentUsage New)
{
	VulkanTexture* VkTexture = static_cast<VulkanTexture*>(Image);

	ForEachCmdBuffer(Buf, [&](VkCommandBuffer& CmdBuffer, int32_t ImageIndex)
	{
		TransitionCmd(CmdBuffer, VkTexture->TextureImage, VK_IMAGE_ASPECT_COLOR_BIT, Old, New);
	});
}

void VulkanRenderingInterface::End(CommandBuffer Buf)
{
    ForEachCmdBuffer(Buf, [&](VkCommandBuffer& CmdBuffer, int32_t ImageIndex)
	{
		vkEndCommandBuffer(CmdBuffer);
	});
}

void GetVkClearValues(ClearValue* ClearValues, uint32_t ValueCount, std::vector<VkClearValue>& OutVkValues)
{
	for (uint32_t ClearValueIndex = 0; ClearValueIndex < ValueCount; ClearValueIndex++)
	{
		ClearValue& CV = ClearValues[ClearValueIndex];
		VkClearValue ClearValue{};
		if(CV.Clear == ClearType::Float)
		{
			float ClearFloats[4] = { CV.FloatClearValue.x, CV.FloatClearValue.y, CV.FloatClearValue.z, CV.FloatClearValue.w };
			memcpy(ClearValue.color.float32, ClearFloats, sizeof(ClearFloats));
		}
		else if (CV.Clear == ClearType::SInt)
			memcpy(ClearValue.color.int32, CV.IntClearValue, sizeof(CV.IntClearValue));
		else if (CV.Clear == ClearType::UInt)
			memcpy(ClearValue.color.uint32, CV.UIntClearValue, sizeof(CV.UIntClearValue));
		else if (CV.Clear == ClearType::DepthStencil)
			ClearValue.depthStencil = { CV.Depth, CV.Stencil };

		OutVkValues[ClearValueIndex] = ClearValue;
	}
}

void VulkanRenderingInterface::BeginRenderGraph(CommandBuffer Buf, SwapChain Target, RenderGraphInfo Info)
{
	VulkanSwapChain* VkSwap = static_cast<VulkanSwapChain*>(Target);
	VulkanCommandBuffer* VkCmd = static_cast<VulkanCommandBuffer*>(Buf);

    VkCmd->CurrentSwapChain = VkSwap;

    ForEachCmdBuffer(Buf, [&](VkCommandBuffer& CmdBuffer, int32_t ImageIndex)
	{
		VkRenderPassBeginInfo RpBeginInfo{};
		RpBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		RpBeginInfo.renderPass = VkSwap->MainPass;
		RpBeginInfo.framebuffer = VkSwap->FrameBuffers[ImageIndex]; // This means the cmd buffer needs to be re-created when the swap chain gets recreated
		RpBeginInfo.renderArea.offset = { 0, 0 };
		RpBeginInfo.renderArea.extent = VkSwap->SwapChainExtent;

		std::vector<VkClearValue> VkValues(Info.ClearValueCount);
		GetVkClearValues(Info.ClearValues, Info.ClearValueCount, VkValues);

		RpBeginInfo.clearValueCount = static_cast<uint32_t>(VkValues.size());
		RpBeginInfo.pClearValues = VkValues.data();

		vkCmdBeginRenderPass(CmdBuffer, &RpBeginInfo, VK_SUBPASS_CONTENTS_INLINE); // This would need to be changed for secondary command buffers
	});
}

void VulkanRenderingInterface::BeginRenderGraph(CommandBuffer Buf, RenderGraph Graph, FrameBuffer Target, RenderGraphInfo Info)
{
	VulkanRenderGraph* VkRg = static_cast<VulkanRenderGraph*>(Graph);
	VulkanFrameBuffer* VkFbo = static_cast<VulkanFrameBuffer*>(Target);
	VulkanCommandBuffer* VkCmd = static_cast<VulkanCommandBuffer*>(Buf);

	VkCmd->CurrentFbo = VkFbo;

	ForEachCmdBuffer(Buf, [&](VkCommandBuffer& CmdBuffer, int32_t ImageIndex)
	{
		VkRenderPassBeginInfo RpBeginInfo{};
		RpBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		RpBeginInfo.renderPass = VkRg->RenderPass;
		RpBeginInfo.framebuffer = VkFbo->VulkanFbo; // This means the cmd buffer needs to be re-created when the swap chain gets recreated
		RpBeginInfo.renderArea.offset = { 0, 0 };
		RpBeginInfo.renderArea.extent = { VkFbo->AttachmentWidth, VkFbo->AttachmentHeight };

		std::vector<VkClearValue> VkValues(Info.ClearValueCount);
		GetVkClearValues(Info.ClearValues, Info.ClearValueCount, VkValues);

		RpBeginInfo.clearValueCount = static_cast<uint32_t>(VkValues.size());
		RpBeginInfo.pClearValues = VkValues.data();

		vkCmdBeginRenderPass(CmdBuffer, &RpBeginInfo, VK_SUBPASS_CONTENTS_INLINE); // This would need to be changed for secondary command buffers
	});
}

void VulkanRenderingInterface::EndRenderGraph(CommandBuffer Buf)
{
	VulkanCommandBuffer* VkCmd = static_cast<VulkanCommandBuffer*>(Buf);

	VkCmd->CurrentSwapChain = nullptr;
	VkCmd->CurrentFbo = nullptr;

    ForEachCmdBuffer(Buf, [&](VkCommandBuffer& CmdBuffer, int32_t ImageIndex)
    {
		vkCmdEndRenderPass(CmdBuffer);
	});
}

void VulkanRenderingInterface::BindPipeline(CommandBuffer Buf, Pipeline PipelineObject)
{
	VulkanCommandBuffer* VkCmd = static_cast<VulkanCommandBuffer*>(Buf);
	VulkanPipeline* VkPipeline = reinterpret_cast<VulkanPipeline*>(PipelineObject);

	// Descriptor sets need to know about the pipeline layout, so store this here
	VkCmd->BoundPipeline = VkPipeline;

    ForEachCmdBuffer(Buf, [&](VkCommandBuffer& CmdBuffer, int32_t ImageIndex)
	{
		vkCmdBindPipeline(CmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, VkPipeline->Pipeline);
	});
}

void VulkanRenderingInterface::BindResources(CommandBuffer Buf, ResourceSet Resources)
{
	VulkanCommandBuffer* VkCmd = static_cast<VulkanCommandBuffer*>(Buf);
	VulkanResourceSet* VkRes = reinterpret_cast<VulkanResourceSet*>(Resources);

	ForEachCmdBuffer(Buf, [&](VkCommandBuffer& CmdBuffer, int32_t ImageIndex)
	{
		VkDescriptorSet& Set = VkRes->DescriptorSets[ImageIndex];

		vkCmdBindDescriptorSets
		(
			CmdBuffer, 
			VK_PIPELINE_BIND_POINT_GRAPHICS, 
			VkCmd->BoundPipeline->PipelineLayout, 
			0, 1, &Set, 
			0, nullptr
		);

	});

}

void VulkanRenderingInterface::DrawVertexBuffer(CommandBuffer Buf, VertexBuffer Vbo, uint32_t VertexCount)
{
	VulkanVertexBuffer* VulkanVbo = static_cast<VulkanVertexBuffer*>(Vbo);

	ForEachCmdBuffer(Buf, [&](VkCommandBuffer& CmdBuffer, int32_t ImageIndex)
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

void VulkanRenderingInterface::DrawVertexBufferIndexed(CommandBuffer Buf, VertexBuffer Vbo, uint32_t IndexCount)
{
	VulkanVertexBuffer* VulkanVbo = static_cast<VulkanVertexBuffer*>(Vbo);

	if(!VulkanVbo->bHasIndexBuffer)
	{
		GLog->error("Vulkan vertex buffer was not created with an index buffer, can't draw indexed vertex buffer");
		return;
	}

	ForEachCmdBuffer(Buf, [&](VkCommandBuffer& CmdBuffer, int32_t ImageIndex)
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
		vkCmdBindIndexBuffer(CmdBuffer, VulkanVbo->DeviceIndexBuffer, 0, VK_INDEX_TYPE_UINT32);

		// Draw indexed vertex buffer
		vkCmdDrawIndexed(CmdBuffer, IndexCount, 1, 0, 0, 0);
	});
}

void VulkanRenderingInterface::SetViewport(CommandBuffer Buf, uint32_t X, uint32_t Y, uint32_t W, uint32_t H)
{
    ForEachCmdBuffer(Buf, [&](VkCommandBuffer& CmdBuffer, int32_t ImageIndex)
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

void VulkanRenderingInterface::SetScissor(CommandBuffer Buf, uint32_t X, uint32_t Y, uint32_t W, uint32_t H)
{
    ForEachCmdBuffer(Buf, [&](VkCommandBuffer& CmdBuffer, int32_t ImageIndex)
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

bool VulkanRenderingInterface::InitializeRenderAPI()
{

}

bool VulkanRenderingInterface::InitializeForSurface(Surface Target)
{

}

void VulkanRenderingInterface::ShutdownRenderAPI()
{
 
}

bool CreateShaderModule(std::vector<uint32_t> SpvCode, VkShaderModule& OutShaderModule)
{
	VkShaderModuleCreateInfo CreateInfo{};
	CreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	CreateInfo.codeSize = SpvCode.size() * sizeof(uint32_t);
	CreateInfo.pCode = SpvCode.data();

	VkShaderModule ShaderModule;
	if(vkCreateShaderModule(GVulkanInfo.Device, &CreateInfo, nullptr, &ShaderModule) != VK_SUCCESS)
	{
		return false;
	}

	OutShaderModule = ShaderModule;

	return true;
}

ShaderProgram VulkanRenderingInterface::CreateShader(const ShaderCreateInfo* ProgramData)
{
	// Get the Spv data
	HlslToSpvResult SpvResult;
	if(!HlslToSpv(ProgramData, SpvResult))
	{
		GLog->error("Failed to produce spv for Vulkan shader module");
		return nullptr;
	}

	std::vector<VkPipelineShaderStageCreateInfo> PipelineShaderStages;
	std::vector<VkShaderModule> ShaderModules;

	VulkanShader* Result = new VulkanShader;

	// Create vertex shader
	if(ProgramData->VertexShaderVirtual)
	{
		if(!CreateShaderModule(SpvResult.VertexSpv, Result->VertexModule))
		{
			GLog->critical("Failed to create shader module");

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
	if(ProgramData->FragmentShaderVirtual)
	{
		if (!CreateShaderModule(SpvResult.FragmentSpv, Result->FragmentModule))
		{
			GLog->critical("Failed to create shader module");

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
}

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

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(GVulkanInfo.PhysicalDevice, TargetSurface, &OutSurfaceCapabilities); // Get surface capabilities
	vkGetPhysicalDeviceSurfaceFormatsKHR(GVulkanInfo.PhysicalDevice, TargetSurface, &SurfaceFormatCount, nullptr); // Get count of surface formats
	vkGetPhysicalDeviceSurfacePresentModesKHR(GVulkanInfo.PhysicalDevice, TargetSurface, &PresentModeCount, nullptr); // Get count of present formats

	if (SurfaceFormatCount > 0 && PresentModeCount > 0)
	{
		// Get the surface formats and present modes
		SupportedFormats.resize(SurfaceFormatCount);
		SupportedPresentModes.resize(PresentModeCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(GVulkanInfo.PhysicalDevice, TargetSurface, &SurfaceFormatCount, SupportedFormats.data());
		vkGetPhysicalDeviceSurfacePresentModesKHR(GVulkanInfo.PhysicalDevice, TargetSurface, &PresentModeCount, SupportedPresentModes.data());

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
                for(auto Tup : FormatRanks)
                    if(std::get<0>(Tup) == Format && std::get<1>(Tup) == ColorSpace)
                        return std::get<2>(Tup);
                return -1;
            };
            
			// Default to first available surface format
			OutOptimalSwapChainSurfaceFormat = SupportedFormats[0];
			for (const auto& SupportedFormat : SupportedFormats)
			{
                auto SupportedFormatRank = FindFormatRank(SupportedFormat.format, SupportedFormat.colorSpace);
                auto CurFormatRank = FindFormatRank(OutOptimalSwapChainSurfaceFormat.format, OutOptimalSwapChainSurfaceFormat.colorSpace);

                if(SupportedFormatRank >= 0 && (CurFormatRank < 0 || SupportedFormatRank < CurFormatRank))
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
					if(Order < BestOrder)
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
    uint32_t QueueFamilyIndices[] = { GVulkanInfo.GraphicsQueueFamIndex, GVulkanInfo.PresentQueueFamIndex };

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
    if (GVulkanInfo.GraphicsQueueFamIndex != GVulkanInfo.PresentQueueFamIndex)
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
    Dst->ImageFormat = OptimalFormat.format;
    Dst->PresentMode = OptimalPresentMode;
    Dst->SwapChainExtent = OptimalExtent;

    if (vkCreateSwapchainKHR(GVulkanInfo.Device, &CreateInfo, nullptr, &Dst->SwapChain) != VK_SUCCESS)
    {
        GLog->critical("Failed to create a Vulkan swap chain");
        return false;
    }
    
    return true;
}
    
bool CreateVkSwapChainImageViews(VulkanSwapChain* Dst)
{
    // Retrieve the swap chain images
    vkGetSwapchainImagesKHR(GVulkanInfo.Device, Dst->SwapChain, &Dst->ImageCount, nullptr);

    // Allocate space for images
    Dst->Images = new VkImage[Dst->ImageCount];

    // Get the actual swap chain images
    vkGetSwapchainImagesKHR(GVulkanInfo.Device, Dst->SwapChain, &Dst->ImageCount, Dst->Images);

    // Create image views for swap chain
    Dst->ImageViews.resize(Dst->ImageCount);
    Dst->FrameBuffers.resize(Dst->ImageCount);

    for (uint32_t ImageIndex = 0; ImageIndex < Dst->ImageCount; ImageIndex++)
    {
        VkImageViewCreateInfo ImageViewCreateInfo{};
        ImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ImageViewCreateInfo.image = Dst->Images[ImageIndex];
        ImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ImageViewCreateInfo.format = Dst->ImageFormat;
        ImageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        ImageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        ImageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        ImageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        ImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        ImageViewCreateInfo.subresourceRange.levelCount = 1;
        ImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        ImageViewCreateInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(GVulkanInfo.Device, &ImageViewCreateInfo, nullptr, &Dst->ImageViews[ImageIndex]) != VK_SUCCESS)
        {
            GLog->error("Failed to create image view for swap chain");

            // Error in creation
            return false;
        }
    }
    
    return true;
}
    
bool CreateVkSwapChainRenderPass(VulkanSwapChain* Dst)
{
    // Create default render pass
    VkAttachmentDescription ColorAttachDesc{};
    ColorAttachDesc.format = Dst->ImageFormat;
    ColorAttachDesc.samples = VK_SAMPLE_COUNT_1_BIT;
    ColorAttachDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    ColorAttachDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    ColorAttachDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    ColorAttachDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    ColorAttachDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ColorAttachDesc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference ColorAttachRef{};
    ColorAttachRef.attachment = 0;
    ColorAttachRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // TODO: Add a depth attachment if needed

    VkSubpassDescription MainPass{};
    MainPass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    MainPass.colorAttachmentCount = 1;
    MainPass.pColorAttachments = &ColorAttachRef;

    VkRenderPassCreateInfo RpCreateInfo{};
    RpCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    RpCreateInfo.attachmentCount = 1;
    RpCreateInfo.pAttachments = &ColorAttachDesc;
    RpCreateInfo.subpassCount = 1;
    RpCreateInfo.pSubpasses = &MainPass;

    // Create main render pass
    if(vkCreateRenderPass(GVulkanInfo.Device, &RpCreateInfo, nullptr, &Dst->MainPass) != VK_SUCCESS)
    {
        GLog->critical("Failed to create main render pass");
        return false;
    }
    
    return true;
}
    
bool CreateVkSwapChainFrameBuffers(VulkanSwapChain* Dst)
{
    // Create framebuffers for the render pass and image views
    for(uint32_t SwapFrameBuffer = 0; SwapFrameBuffer < Dst->ImageCount; SwapFrameBuffer++)
    {
        VkFramebufferCreateInfo FramebufferCreateInfo{};
        FramebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        FramebufferCreateInfo.renderPass = Dst->MainPass;
        FramebufferCreateInfo.attachmentCount = 1;
        FramebufferCreateInfo.pAttachments = &Dst->ImageViews[SwapFrameBuffer]; // Attachment is the nth image view
        FramebufferCreateInfo.width = Dst->SwapChainExtent.width;
        FramebufferCreateInfo.height = Dst->SwapChainExtent.height;
        FramebufferCreateInfo.layers = 1;

        if(vkCreateFramebuffer(GVulkanInfo.Device, &FramebufferCreateInfo, nullptr, &Dst->FrameBuffers[SwapFrameBuffer]) != VK_SUCCESS)
        {
            GLog->critical("Failed to create framebuffer for swap chain");
            return false;
        }
    }
    
    return true;
}

void DestroyVkSwapChain(VulkanSwapChain* VkSwap)
{
    vkDestroySwapchainKHR(GVulkanInfo.Device, VkSwap->SwapChain, nullptr);
}

void DestroyVkSwapChainImageViews(VulkanSwapChain* VkSwap)
{
    // Clean up the images
    delete[] VkSwap->Images;

    // Clean up framebuffers and image views
    for(uint32_t SwapChainImage = 0; SwapChainImage < VkSwap->ImageCount; SwapChainImage++)
    {
        vkDestroyImageView(GVulkanInfo.Device, VkSwap->ImageViews[SwapChainImage], nullptr);
    }
}

void DestroyVkSwapChainFramebuffers(VulkanSwapChain* VkSwap)
{
    // Clean up framebuffers and image views
    for(uint32_t SwapChainImage = 0; SwapChainImage < VkSwap->ImageCount; SwapChainImage++)
    {
        vkDestroyFramebuffer(GVulkanInfo.Device, VkSwap->FrameBuffers[SwapChainImage], nullptr);
    }
}

void DestroyVkSwapChainRenderPass(VulkanSwapChain* VkSwap)
{
    // Destroy render pass
    vkDestroyRenderPass(GVulkanInfo.Device, VkSwap->MainPass, nullptr);
}

SwapChain VulkanRenderingInterface::CreateSwapChain(Surface TargetSurface, int32_t DesiredWidth,
	int32_t DesiredHeight)
{
    VulkanSwapChain* Result = new VulkanSwapChain;

    // Create swap chain resources
    if(!CreateVkSwapChain(Result, TargetSurface, DesiredWidth, DesiredHeight))
    {
        return nullptr;
    }
    
    if(!CreateVkSwapChainImageViews(Result))
    {
        return nullptr;
    }
    
    if(!CreateVkSwapChainRenderPass(Result))
    {
        return nullptr;
    }
    
    if(!CreateVkSwapChainFrameBuffers(Result))
    {
        return nullptr;
    }

    // Create n frames in flight for this swap chain. This only needs to be done at creation, hence there isn't another function for it.
    for(uint32_t FrameInFlight = 0; FrameInFlight < MAX_FRAMES_IN_FLIGHT; FrameInFlight++)
    {
        VkSemaphore ImageAvailableSem, PresentSem;
        VkFence InFlightFence;

        // Create semaphore for frame in flight
        VkSemaphoreCreateInfo SemCreate{};
        SemCreate.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        if(vkCreateSemaphore(GVulkanInfo.Device, &SemCreate, nullptr, &ImageAvailableSem) != VK_SUCCESS ||
            vkCreateSemaphore(GVulkanInfo.Device, &SemCreate, nullptr, &PresentSem) != VK_SUCCESS)
        {
            GLog->critical("Failed to create semaphores for swap chain");

            delete Result;
            return nullptr;
        }

        VkFenceCreateInfo FenceCreate{};
        FenceCreate.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        FenceCreate.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        if(vkCreateFence(GVulkanInfo.Device, &FenceCreate, nullptr, &InFlightFence) != VK_SUCCESS)
        {
            GLog->critical("Failed to create fence for swap chain");
            return nullptr;
        }

        Result->FramesInFlight.emplace_back(ImageAvailableSem, PresentSem, InFlightFence);
    }

    // Resize image fences to as many swap chain images that we have. Also, start them all at the null handle.
    Result->ImageFences.resize(Result->ImageCount, VK_NULL_HANDLE);

	RECORD_RESOURCE_ALLOC(Result)

    return Result;
}

void VulkanRenderingInterface::DestroySwapChain(SwapChain Swap)
{
    vkDeviceWaitIdle(GVulkanInfo.Device);

	VulkanSwapChain* VkSwap = static_cast<VulkanSwapChain*>(Swap);

    // Destroy volatile swap chain resources
    DestroyVkSwapChainFramebuffers(VkSwap);
    DestroyVkSwapChainRenderPass(VkSwap);
    DestroyVkSwapChainImageViews(VkSwap);
    DestroyVkSwapChain(VkSwap);

	// Destroy frames in flight
	for(const auto& FrameInFlight : VkSwap->FramesInFlight)
	{
		vkDestroySemaphore(GVulkanInfo.Device, FrameInFlight.ImageAvailableSemaphore, nullptr);
		vkDestroySemaphore(GVulkanInfo.Device, FrameInFlight.RenderingFinishedSemaphore, nullptr);
		vkDestroyFence(GVulkanInfo.Device, FrameInFlight.InFlightFence, nullptr);
	}

	REMOVE_RESOURCE_ALLOC(VkSwap)

	// Free swap chain memory
	delete VkSwap;
}

void VulkanRenderingInterface::SubmitSwapCommandBuffer(SwapChain Target, CommandBuffer Buffer)
{
	VulkanSwapChain* VkSwap = static_cast<VulkanSwapChain*>(Target);
	VulkanCommandBuffer* VkCmd = static_cast<VulkanCommandBuffer*>(Buffer);

	// Submit the buffer for the current acquired image
	VkSwap->BuffersToSubmit.push_back(VkCmd->CmdBuffers[VkSwap->AcquiredImageIndex]);
}

void VulkanRenderingInterface::SubmitCommandBuffer(CommandBuffer Buffer, bool bWait, Fence WaitFence)
{
	VulkanCommandBuffer* VkCmd = static_cast<VulkanCommandBuffer*>(Buffer);

	VkSubmitInfo SubmitInfo{};
	SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	SubmitInfo.commandBufferCount = 1;
	SubmitInfo.pCommandBuffers = &VkCmd->CmdBuffers[0];

	if (vkQueueSubmit(GVulkanInfo.GraphicsQueue, 1, &SubmitInfo, WaitFence ? static_cast<VkFence>(WaitFence) : VK_NULL_HANDLE) != VK_SUCCESS)
	{
		GLog->critical("Failed to submit vulkan command buffers to graphics queue");
	}

	if(bWait)
	{
		vkQueueWaitIdle(GVulkanInfo.GraphicsQueue);
	}
}

void VulkanRenderingInterface::BeginFrame(SwapChain Swap, Surface Target, int32_t FrameWidth, int32_t FrameHeight)
{
	VulkanSwapChain* VkSwap = static_cast<VulkanSwapChain*>(Swap);

    GVulkanInfo.CurrentSwapChain = VkSwap;

	VkSwap->bInsideFrame = true;

	if (FrameWidth == 0 || FrameHeight == 0)
	{
		vkDeviceWaitIdle(GVulkanInfo.Device);
		return;
	}
    
	// Acquire image
	VkResult ImageAcquireResult = vkAcquireNextImageKHR(GVulkanInfo.Device, VkSwap->SwapChain, UINT64_MAX,
		VkSwap->FramesInFlight[VkSwap->CurrentFrame].ImageAvailableSemaphore, VK_NULL_HANDLE, &VkSwap->AcquiredImageIndex);
    
    // Vulkan swap chain needs to re-created immediately
    if(ImageAcquireResult == VK_ERROR_OUT_OF_DATE_KHR)
    {
        RecreateSwapChain(Swap, Target, FrameWidth, FrameHeight);
    }

	// Previous frame using this image
	if(VkSwap->ImageFences[VkSwap->AcquiredImageIndex] != VK_NULL_HANDLE)
	{
		// Wait for the previous frame to complete execution of vkQueueSubmit
		vkWaitForFences(GVulkanInfo.Device, 1, &VkSwap->ImageFences[VkSwap->AcquiredImageIndex], VK_TRUE, UINT64_MAX);
	}

	// Mark new image as being used by this "frame in flight"
	VkSwap->ImageFences[VkSwap->AcquiredImageIndex] = VkSwap->FramesInFlight[VkSwap->CurrentFrame].InFlightFence;
} 

void VulkanRenderingInterface::EndFrame(SwapChain Swap, Surface Target, int32_t FrameWidth, int32_t FrameHeight)
{
	VulkanSwapChain* VkSwap = static_cast<VulkanSwapChain*>(Swap);
    GVulkanInfo.CurrentSwapChain = nullptr;

	VkSwap->bInsideFrame = false;

	if (FrameWidth == 0 || FrameHeight == 0)
	{
		vkDeviceWaitIdle(GVulkanInfo.Device);
		VkSwap->BuffersToSubmit.clear();
		return;
	}
	
	// Reset the fence that we're waiting on
	vkResetFences(GVulkanInfo.Device, 1, &VkSwap->FramesInFlight[VkSwap->CurrentFrame].InFlightFence);

	// Submit our command buffers for this frame
	VkPipelineStageFlags WaitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	VkSubmitInfo QueueSubmit{};
	QueueSubmit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	QueueSubmit.waitSemaphoreCount = 1;
	QueueSubmit.pWaitSemaphores = &VkSwap->FramesInFlight[VkSwap->CurrentFrame].ImageAvailableSemaphore;
	QueueSubmit.pWaitDstStageMask = WaitStages;
	QueueSubmit.commandBufferCount = static_cast<uint32_t>(VkSwap->BuffersToSubmit.size());
	QueueSubmit.pCommandBuffers = VkSwap->BuffersToSubmit.data();
	QueueSubmit.signalSemaphoreCount = 1;
	QueueSubmit.pSignalSemaphores = &VkSwap->FramesInFlight[VkSwap->CurrentFrame].RenderingFinishedSemaphore; // Signal when rendering is finished

	// Notify the in flight fence once the execution of this vkQueueSubmit is complete
	if(vkQueueSubmit(GVulkanInfo.GraphicsQueue, static_cast<uint32_t>(VkSwap->BuffersToSubmit.size()), &QueueSubmit, VkSwap->FramesInFlight[VkSwap->CurrentFrame].InFlightFence) != VK_SUCCESS)
	{
		GLog->critical("Failed to submit vulkan command buffers to graphics queue");
	}

	// Present the rendered images
	VkPresentInfoKHR PresentInfo{};
	PresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	PresentInfo.waitSemaphoreCount = 1;
	PresentInfo.pWaitSemaphores = &VkSwap->FramesInFlight[VkSwap->CurrentFrame].RenderingFinishedSemaphore; // Wait for rendering to complete before presenting
	PresentInfo.swapchainCount = 1;
	PresentInfo.pSwapchains = &VkSwap->SwapChain;
	PresentInfo.pImageIndices = &VkSwap->AcquiredImageIndex;
	PresentInfo.pResults = nullptr;

	// Present the rendered image
	VkResult PresentResult = vkQueuePresentKHR(GVulkanInfo.GraphicsQueue, &PresentInfo);
    
    if(PresentResult == VK_ERROR_OUT_OF_DATE_KHR || PresentResult == VK_SUBOPTIMAL_KHR)
    {
        RecreateSwapChain(Swap, Target, FrameWidth, FrameHeight);
    }

	// Advance the current frame
	VkSwap->CurrentFrame = (VkSwap->CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

	// Reset the submitted command buffers
	VkSwap->BuffersToSubmit.clear();
}

void VulkanRenderingInterface::GetSwapChainSize(SwapChain Swap, uint32_t& Width, uint32_t& Height)
{
	VulkanSwapChain* VkSwap = static_cast<VulkanSwapChain*>(Swap);
	Width = VkSwap->SwapChainExtent.width;
	Height = VkSwap->SwapChainExtent.height;
}

void VulkanRenderingInterface::RecreateSwapChain(SwapChain Swap, Surface Target, int32_t DesiredWidth, int32_t DesiredHeight)
{
	VulkanSwapChain* VkSwap = static_cast<VulkanSwapChain*>(Swap);
    vkDeviceWaitIdle(GVulkanInfo.Device);
    
    // Re-create the needed resources
    DestroyVkSwapChainFramebuffers(VkSwap);
    DestroyVkSwapChainRenderPass(VkSwap);
    DestroyVkSwapChainImageViews(VkSwap);
    DestroyVkSwapChain(VkSwap);
    
    CreateVkSwapChain(VkSwap, Target, DesiredWidth, DesiredHeight);
    CreateVkSwapChainImageViews(VkSwap);
    CreateVkSwapChainRenderPass(VkSwap);
    CreateVkSwapChainFrameBuffers(VkSwap);
}

void VulkanRenderingInterface::GetFrameBufferSize(FrameBuffer Fbo, uint32_t& Width, uint32_t& Height)
{
	VulkanFrameBuffer* VkFbo = static_cast<VulkanFrameBuffer*>(Fbo);

	Width = VkFbo->AttachmentWidth;
	Height = VkFbo->AttachmentHeight;
}



void VulkanRenderingInterface::ResizeFrameBuffer(FrameBuffer Fbo, uint32_t NewWidth, uint32_t NewHeight)
{
	VulkanFrameBuffer* VkFbo = static_cast<VulkanFrameBuffer*>(Fbo);

	// Wait on queue
	vkDeviceWaitIdle(GVulkanInfo.Device);

	// Destroy old images
	DestroyFramebufferImages(VkFbo);
	vkDestroyFramebuffer(GVulkanInfo.Device, VkFbo->VulkanFbo, nullptr);

	// Re-create images and framebuffer
	if(!CreateFrameBufferImages(VkFbo, VkFbo->ColorAttachmentDescriptions, NewWidth, NewHeight))
	{
		return;
	}

	if(!CreateFrameBufferResource(VkFbo, VkFbo->CreatedFor, NewWidth, NewHeight))
	{
		return;
	}

	// Set the new width/height
	VkFbo->AttachmentWidth = NewWidth;
	VkFbo->AttachmentHeight = NewHeight;
}

void VulkanRenderingInterface::UpdateUniformBuffer(ResourceSet Resources, SwapChain Target, uint32_t BufferIndex,
	void* Data, uint64_t DataSize)
{
	VulkanSwapChain* VkSwap = static_cast<VulkanSwapChain*>(Target);
	VulkanResourceSet* VkRes = static_cast<VulkanResourceSet*>(Resources);

	if(!VkSwap->bInsideFrame)
	{
		GLog->critical("Must update constant buffer within the limits of a swap chain frame");
		return;
	}

	VkDeviceMemory& Mem = VkRes->ConstantBuffers[BufferIndex].Memory[VkSwap->AcquiredImageIndex];

	// Data is guaranteed available since this frame is guaranteed to have previous operations complete by cpu fence in BeginFrame
	void* MappedData;
	vkMapMemory(GVulkanInfo.Device, Mem, 0, DataSize, 0, &MappedData);
	{
		std::memcpy(MappedData, Data, DataSize);
	}
	vkUnmapMemory(GVulkanInfo.Device, Mem); // Memory is host-coherent, so no flush necessary

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

	vkUpdateDescriptorSets(GVulkanInfo.Device, 1, &BufferWrite, 0, nullptr);
}

// TODO: Reuse code among UpdateTextureResource and UpdateAttachmentResource
void VulkanRenderingInterface::UpdateTextureResource(ResourceSet Resources, SwapChain Target, Texture* Images, uint32_t ImageCount, uint32_t Binding)
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

	vkUpdateDescriptorSets(GVulkanInfo.Device, ImageCount, Writes, 0, nullptr);

	delete[] Writes;
	delete[] ImageInfos;
}

void VulkanRenderingInterface::UpdateAttachmentResource(ResourceSet Resources, SwapChain Target, FrameBuffer SrcBuffer,
	uint32_t AttachmentIndex, uint32_t Binding)
{
	VulkanSwapChain* VkSwap = static_cast<VulkanSwapChain*>(Target);
	VulkanResourceSet* VkRes = static_cast<VulkanResourceSet*>(Resources);
	uint32_t CurrentImage = VkSwap->AcquiredImageIndex;

	VulkanFrameBuffer* VkFbo = static_cast<VulkanFrameBuffer*>(SrcBuffer);

	VkDescriptorImageInfo ImageInfo = {};
	ImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	ImageInfo.imageView = VkFbo->ColorAttachmentImageViews[AttachmentIndex];
	ImageInfo.sampler = VkFbo->ColorAttachmentSamplers[AttachmentIndex];

	VkWriteDescriptorSet ImageWrite = {};
	ImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	ImageWrite.dstSet = VkRes->DescriptorSets[CurrentImage];
	ImageWrite.dstBinding = Binding;
	ImageWrite.dstArrayElement = 0;
	ImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	ImageWrite.descriptorCount = 1;
	ImageWrite.pBufferInfo = nullptr;
	ImageWrite.pImageInfo = &ImageInfo;
	ImageWrite.pTexelBufferView = nullptr;

	vkUpdateDescriptorSets(GVulkanInfo.Device, 1, &ImageWrite, 0, nullptr);
}

void VulkanRenderingInterface::UpdateAttachmentResources(ResourceSet Resources, SwapChain Target, FrameBuffer* Buffers,
	uint32_t BufferCount, uint32_t AttachmentIndex, uint32_t Binding)
{
	VulkanSwapChain* VkSwap = static_cast<VulkanSwapChain*>(Target);
	VulkanResourceSet* VkRes = static_cast<VulkanResourceSet*>(Resources);

	VkWriteDescriptorSet* Writes = new VkWriteDescriptorSet[BufferCount];
	VkDescriptorImageInfo* ImageInfos = new VkDescriptorImageInfo[BufferCount];
	uint32_t CurrentImage = VkSwap->AcquiredImageIndex;

	for (uint32_t ArrayImage = 0; ArrayImage < BufferCount; ArrayImage++)
	{
		VulkanFrameBuffer* VkFbo = static_cast<VulkanFrameBuffer*>(Buffers[ArrayImage]);

		VkDescriptorImageInfo& ImageInfo = ImageInfos[ArrayImage];
		VkWriteDescriptorSet& ImageWrite = Writes[ArrayImage];

		ImageInfo = {};
		ImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		ImageInfo.imageView = VkFbo->ColorAttachmentImageViews[AttachmentIndex];
		ImageInfo.sampler = VkFbo->ColorAttachmentSamplers[AttachmentIndex];

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

	vkUpdateDescriptorSets(GVulkanInfo.Device, BufferCount, Writes, 0, nullptr);

	delete[] Writes;
	delete[] ImageInfos;
}

void SynchronizedUploadBufferData(VkFence StagingFence, uint64_t Size, void* Data,
                                  VkCommandBuffer StagingCommandBuffer,
                                  VkDeviceMemory StagingMemory, VkBuffer StagingBuffer,
                                  VkBuffer DeviceBuffer,
                                  VkAccessFlagBits SrcAccess, VkAccessFlagBits DstAccess,
                                  VkPipelineStageFlagBits SrcStage, VkPipelineStageFlagBits DstStage)
{

	// Wait for previous staging transfer operation to complete
	vkWaitForFences(GVulkanInfo.Device, 1, &StagingFence, VK_TRUE, UINT64_MAX);
	vkResetFences(GVulkanInfo.Device, 1, &StagingFence);

	// Now we can safely write to host-visible staging buffer memory
	void* MappedData = nullptr;
	vkMapMemory(GVulkanInfo.Device, StagingMemory, 0, Size, 0, &MappedData);
	{
		std::memcpy(MappedData, Data, Size);
	}
	vkUnmapMemory(GVulkanInfo.Device, StagingMemory);

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
	vkQueueSubmit(GVulkanInfo.GraphicsQueue, 1, &QueueSubmit, StagingFence);

}

void VulkanRenderingInterface::UploadVertexBufferData(VertexBuffer Buffer, void* Data, uint64_t Size)
{
	VulkanVertexBuffer* VulkanVbo = static_cast<VulkanVertexBuffer*>(Buffer);

	SynchronizedUploadBufferData(VulkanVbo->VertexStagingCompleteFence, Size, Data,
		VulkanVbo->VertexStagingCommandBuffer,
		VulkanVbo->StagingVertexBufferMemory, VulkanVbo->StagingVertexBuffer,
		VulkanVbo->DeviceVertexBuffer,
		VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

}

void VulkanRenderingInterface::UploadIndexBufferData(VertexBuffer Buffer, uint32_t* Data, uint64_t Size)
{
	VulkanVertexBuffer* VulkanVbo = static_cast<VulkanVertexBuffer*>(Buffer);

	if(!VulkanVbo->bHasIndexBuffer)
	{
		GLog->error("Vulkan vertex buffer was not created with an index buffer, cant upload index buffer data");
		return;
	}
	
	SynchronizedUploadBufferData(VulkanVbo->IndexStagingCompleteFence, Size, Data,
		VulkanVbo->IndexStagingCommandBuffer,
		VulkanVbo->StagingIndexBufferMemory, VulkanVbo->StagingIndexBuffer,
		VulkanVbo->DeviceIndexBuffer,
		VK_ACCESS_INDEX_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
}

void VulkanRenderingInterface::ResizeVertexBuffer(VertexBuffer Buffer, uint64_t NewSize)
{
	// Delete old resources
	VulkanVertexBuffer* VulkanVbo = static_cast<VulkanVertexBuffer*>(Buffer);

	// Force queue idle
	vkQueueWaitIdle(GVulkanInfo.GraphicsQueue);

	vkFreeMemory(GVulkanInfo.Device, VulkanVbo->DeviceVertexBufferMemory, nullptr);
	vkFreeMemory(GVulkanInfo.Device, VulkanVbo->StagingVertexBufferMemory, nullptr);
	vkDestroyBuffer(GVulkanInfo.Device, VulkanVbo->DeviceVertexBuffer, nullptr);
	vkDestroyBuffer(GVulkanInfo.Device, VulkanVbo->StagingVertexBuffer, nullptr);

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

void VulkanRenderingInterface::ResizeIndexBuffer(VertexBuffer Buffer, uint64_t NewSize)
{
	// Delete old resources
	VulkanVertexBuffer* VulkanVbo = static_cast<VulkanVertexBuffer*>(Buffer);

	if(!VulkanVbo->bHasIndexBuffer)
	{
		GLog->error("Vulkan vertex buffer was not created with an index buffer, can't resize index buffer");
		return;
	}

	// Force queue idle
	vkQueueWaitIdle(GVulkanInfo.GraphicsQueue);

	vkFreeMemory(GVulkanInfo.Device, VulkanVbo->DeviceIndexBufferMemory, nullptr);
	vkFreeMemory(GVulkanInfo.Device, VulkanVbo->StagingIndexBufferMemory, nullptr);
	vkDestroyBuffer(GVulkanInfo.Device, VulkanVbo->DeviceIndexBuffer, nullptr);
	vkDestroyBuffer(GVulkanInfo.Device, VulkanVbo->StagingIndexBuffer, nullptr);

	CreateBuffer(NewSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
		VulkanVbo->StagingIndexBuffer, VulkanVbo->StagingIndexBufferMemory
	);

	// Create device index buffer. Because we will be copying the staging buffer to the device buffer, we need to make it eligible for transfer.
	CreateBuffer(NewSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		VulkanVbo->DeviceIndexBuffer, VulkanVbo->DeviceIndexBufferMemory
	);
}

void VulkanRenderingInterface::ReadFramebufferAttachment(FrameBuffer SrcBuffer, uint32_t Attachment, void* Dst, uint64_t BufferSize)
{
	vkDeviceWaitIdle(GVulkanInfo.Device);
	VulkanFrameBuffer* VkFbo = static_cast<VulkanFrameBuffer*>(SrcBuffer);

	VkBuffer StagingBuffer;
	VkDeviceMemory StagingBufferMemory;
	CreateBuffer
	(
		BufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		StagingBuffer, StagingBufferMemory
	);

	// Transition image to transfer src from shader read
	ImmediateSubmitAndWait([&](CommandBuffer Dst)
	{
		GRenderAPI->TransitionFrameBufferColorAttachment(Dst, SrcBuffer, Attachment, AttachmentUsage::ShaderRead, AttachmentUsage::TransferSource);
	});

	ImmediateSubmitAndWait([&](CommandBuffer Dst)
	{
		ForEachCmdBuffer(Dst, [&](VkCommandBuffer Buf, int32_t ImageIndex)
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
			ImageCopy.imageExtent = { VkFbo->AttachmentWidth, VkFbo->AttachmentHeight, 1 };

			// Copy image data to buffer
			vkCmdCopyImageToBuffer(Buf,
				VkFbo->ColorAttachmentImages[Attachment],
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				StagingBuffer,
				1,
				&ImageCopy
			);
		});
	});

	void* MappedData;
	vkMapMemory(GVulkanInfo.Device, StagingBufferMemory, 0, BufferSize, 0, &MappedData);
	{
		std::memcpy(Dst, MappedData, BufferSize);
	}
	vkUnmapMemory(GVulkanInfo.Device, StagingBufferMemory);

	// Transition image back
	ImmediateSubmitAndWait([&](CommandBuffer Dst)
	{
		GRenderAPI->TransitionFrameBufferColorAttachment(Dst, SrcBuffer, Attachment, AttachmentUsage::TransferSource, AttachmentUsage::ShaderRead);
	});

	vkFreeMemory(GVulkanInfo.Device, StagingBufferMemory, nullptr);
	vkDestroyBuffer(GVulkanInfo.Device, StagingBuffer, nullptr);
}

VkShaderStageFlags ShaderStageToVkStage(ShaderStage Stage)
{
	switch(Stage)
	{
	case ShaderStage::Vertex:
		return VK_SHADER_STAGE_VERTEX_BIT;
	case ShaderStage::Fragment:
		return VK_SHADER_STAGE_FRAGMENT_BIT;
	}

	return VK_SHADER_STAGE_VERTEX_BIT;
}

ResourceLayout VulkanRenderingInterface::CreateResourceLayout(const ResourceLayoutCreateInfo* CreateInfo)
{
	VulkanResourceLayout* Result = new VulkanResourceLayout;

	std::vector<VkDescriptorSetLayoutBinding> LayoutBindings;
	for(uint32_t LayoutBindingIndex = 0; LayoutBindingIndex < CreateInfo->ConstantBufferCount; LayoutBindingIndex++)
	{
		VkDescriptorSetLayoutBinding LayoutBinding{};
		LayoutBinding.binding = CreateInfo->ConstantBuffers[LayoutBindingIndex].Binding;
		LayoutBinding.descriptorCount = CreateInfo->ConstantBuffers[LayoutBindingIndex].Count;
		LayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		LayoutBinding.pImmutableSamplers = nullptr;
		LayoutBinding.stageFlags = ShaderStageToVkStage(CreateInfo->ConstantBuffers[LayoutBindingIndex].StageUsedAt);

		LayoutBindings.push_back(LayoutBinding);

		// Persist constant buffers
		Result->ConstantBuffers.push_back(CreateInfo->ConstantBuffers[LayoutBindingIndex]);
	}

	for (uint32_t TexBindingIndex = 0; TexBindingIndex < CreateInfo->TextureCount; TexBindingIndex++)
	{
		VkDescriptorSetLayoutBinding LayoutBinding{};
		LayoutBinding.binding = CreateInfo->Textures[TexBindingIndex].Binding;
		LayoutBinding.descriptorCount = CreateInfo->Textures[TexBindingIndex].Count;
		LayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		LayoutBinding.pImmutableSamplers = nullptr;
		LayoutBinding.stageFlags = ShaderStageToVkStage(CreateInfo->Textures[TexBindingIndex].StageUsedAt);

		LayoutBindings.push_back(LayoutBinding);

		// Persist constant buffers
		Result->TextureBindings.push_back(CreateInfo->Textures[TexBindingIndex]);
	}

	VkDescriptorSetLayoutCreateInfo LayoutCreateInfo{};
	LayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	LayoutCreateInfo.bindingCount = static_cast<uint32_t>(LayoutBindings.size());
	LayoutCreateInfo.pBindings = LayoutBindings.data();

	if(vkCreateDescriptorSetLayout(GVulkanInfo.Device, &LayoutCreateInfo, nullptr, &Result->VkLayout) != VK_SUCCESS)
	{
		GLog->critical("Failed to create Vulkan descriptor set layout");
		return nullptr;
	}

	RECORD_RESOURCE_ALLOC(Result)

	return Result;
}

void VulkanRenderingInterface::DestroyResourceLayout(ResourceLayout Layout)
{
	VulkanResourceLayout* VkLayout = static_cast<VulkanResourceLayout*>(Layout);
	vkDestroyDescriptorSetLayout(GVulkanInfo.Device, VkLayout->VkLayout, nullptr);

	REMOVE_RESOURCE_ALLOC(VkLayout)

	delete VkLayout;
}

void VulkanRenderingInterface::DestroyResourceSet(ResourceSet Resources)
{
	vkDeviceWaitIdle(GVulkanInfo.Device);

	VulkanResourceSet* VkRes = static_cast<VulkanResourceSet*>(Resources);
	for(const auto& ConstBuf : VkRes->ConstantBuffers)
	{
		for (const auto& Memory : ConstBuf.Memory)
			vkFreeMemory(GVulkanInfo.Device, Memory, nullptr);

		for (const auto& Buf : ConstBuf.Buffers)
			vkDestroyBuffer(GVulkanInfo.Device, Buf, nullptr);

	}

	vkFreeDescriptorSets(GVulkanInfo.Device, GVulkanInfo.MainDscPool, static_cast<uint32_t>(VkRes->DescriptorSets.size()), VkRes->DescriptorSets.data());

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
	switch(Format)
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

Pipeline VulkanRenderingInterface::CreatePipeline(const PipelineCreateInfo* CreateInfo)
{
	VulkanShader* VkShader = static_cast<VulkanShader*>(CreateInfo->Shader);
	VulkanRenderGraph* VkRenderGraph = static_cast<VulkanRenderGraph*>(CreateInfo->CompatibleGraph);
	VulkanSwapChain* VkSwap = static_cast<VulkanSwapChain*>(CreateInfo->CompatibleSwapChain);
	if(!VkSwap && !VkRenderGraph)
	{
		GLog->critical("Must specify either a valid Vulkan RenderGraph or SwapChain when creating a pipeline");
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
	BindingDescription.stride = CreateInfo->VertexBufferStride;
	VertexBindings.push_back(BindingDescription);

	for (uint32_t VertexIndex = 0; VertexIndex < CreateInfo->VertexAttributeCount; VertexIndex++)
	{
		VertexAttribute Attrib = CreateInfo->VertexAttributes[VertexIndex];

		VkVertexInputAttributeDescription AttributeDescription{};
		AttributeDescription.format = EngineFormatToVkFormat(Attrib.Format);
		AttributeDescription.binding = 0; // Always use one binding for vertex buffers
		AttributeDescription.location = VertexIndex;
		AttributeDescription.offset = Attrib.Offset;

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
	InputAssembly.topology = EngineTopToVkTop(CreateInfo->Primitive);
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
	Scissor.extent = { 1, 1};

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
	if(CreateInfo->DepthStencil.bEnableDepthTest)
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
	for(uint32_t ColorBlendAttachmentIndex = 0; ColorBlendAttachmentIndex < CreateInfo->BlendSettingCount; ColorBlendAttachmentIndex++)
	{
		PipelineBlendSettings& Settings = CreateInfo->BlendSettings[ColorBlendAttachmentIndex];

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
	VulkanResourceLayout* VkLayout = static_cast<VulkanResourceLayout*>(CreateInfo->Layout);

	VkPipelineLayoutCreateInfo PipelineLayoutInfo{};
	PipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	PipelineLayoutInfo.setLayoutCount = CreateInfo->Layout ? 1 : 0;
	PipelineLayoutInfo.pSetLayouts = &VkLayout->VkLayout;
	PipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
	PipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

	if (vkCreatePipelineLayout(GVulkanInfo.Device, &PipelineLayoutInfo, nullptr, &Result->PipelineLayout) != VK_SUCCESS) 
	{
		GLog->critical("Failed to create pipeline layout");

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
	PipelineCreateInfo.renderPass = VkRenderGraph ? VkRenderGraph->RenderPass : VkSwap->MainPass;
	PipelineCreateInfo.subpass = CreateInfo->PassIndex;
	PipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	PipelineCreateInfo.basePipelineIndex = -1;

	// TODO: use a global pipeline cache
	if(vkCreateGraphicsPipelines(GVulkanInfo.Device, VK_NULL_HANDLE, 1, &PipelineCreateInfo, nullptr, &Result->Pipeline) != VK_SUCCESS)
	{
		GLog->critical("Failed to create a Vulkan graphics pipeline");

		delete Result;
		return nullptr;
	}

	RECORD_RESOURCE_ALLOC(Result)

	return Result;
}

RenderGraph VulkanRenderingInterface::CreateRenderGraph(const RenderGraphCreateInfo* CreateInfo)
{
	VulkanRenderGraph* Result = new VulkanRenderGraph;

	struct VkSubPassInfo
	{
		// Preserve the vector so it doesn't get invalidated
		std::vector<VkAttachmentReference> ColorAttachmentRefs;
		VkAttachmentReference DepthStencilAttachmentRef;
	};

	VkSubPassInfo SubPassInfos[MAX_SUBPASSES];
	std::vector<VkAttachmentDescription> AttachmentDescriptions;
	std::vector<VkSubpassDescription> VkPassDescriptions;

	// Create color attachment descriptions
	for(uint32_t AttachmentIndex = 0; AttachmentIndex < CreateInfo->ColorAttachmentCount; AttachmentIndex++)
	{
		const RenderGraphAttachmentDescription& Description = CreateInfo->ColorAttachmentDescriptions[AttachmentIndex];

		VkAttachmentDescription VkAttachmentDesc{};
		VkAttachmentDesc.format = AttachmentFormatToVkFormat(CreateInfo->SourceSwap, Description.Format);
		VkAttachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT; // Todo: msaa
		VkAttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		VkAttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		VkAttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		VkAttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		VkAttachmentDesc.initialLayout = AttachmentUsageToVkLayout(Description.InitialUsage);
		VkAttachmentDesc.finalLayout = AttachmentUsageToVkLayout(Description.FinalUsage);

		AttachmentDescriptions.push_back(VkAttachmentDesc);
	}

	// Create depth attachment description, if provided
	if(CreateInfo->bHasDepthStencilAttachment)
	{
		VkAttachmentDescription VkDepthStencilAttachmentDesc{};
		VkDepthStencilAttachmentDesc.format = AttachmentFormatToVkFormat(CreateInfo->SourceSwap, AttachmentFormat::DepthStencil);
		VkDepthStencilAttachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
		VkDepthStencilAttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		VkDepthStencilAttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		VkDepthStencilAttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		VkDepthStencilAttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
		VkDepthStencilAttachmentDesc.initialLayout = AttachmentUsageToVkLayout(CreateInfo->DepthStencilAttachmentDescription.InitialUsage);
		VkDepthStencilAttachmentDesc.finalLayout = AttachmentUsageToVkLayout(CreateInfo->DepthStencilAttachmentDescription.FinalUsage);

		AttachmentDescriptions.push_back(VkDepthStencilAttachmentDesc);
	}

	// Create subpasses
	for(uint32_t SubpassIndex = 0; SubpassIndex < CreateInfo->PassCount; SubpassIndex++)
	{
		const RenderPassInfo& PassInfo = CreateInfo->Passes[SubpassIndex];

		VkSubpassDescription VkDescription{};
		VkSubPassInfo& SubpassInfo = SubPassInfos[SubpassIndex];

		VkDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // Always bind to graphics, unless Vulkan eventually supports compute subpasses

		// Load attachment refs
		for(uint32_t OutAttachRefIndex = 0; OutAttachRefIndex < PassInfo.OutputColorAttachmentCount; OutAttachRefIndex++)
		{
			int32_t AttachReference = PassInfo.OutputColorAttachments[OutAttachRefIndex];

			VkAttachmentReference AttachRef{};
			AttachRef.attachment = AttachReference;
			AttachRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // Not sure we'd ever want this to change?

			SubpassInfo.ColorAttachmentRefs.push_back(AttachRef);
		}

		if(PassInfo.bUsesDepthStencilAttachment)
		{
			VkAttachmentReference AttachRef{};
			AttachRef.attachment = CreateInfo->ColorAttachmentCount; // Depth attachment is always last attachment
			AttachRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			SubpassInfo.DepthStencilAttachmentRef = AttachRef;
		}

		VkDescription.colorAttachmentCount = static_cast<uint32_t>(SubpassInfo.ColorAttachmentRefs.size());
		VkDescription.pColorAttachments = SubpassInfo.ColorAttachmentRefs.data();
		VkDescription.inputAttachmentCount = 0; // TODO: eventually support these
		VkDescription.pInputAttachments = nullptr;
		VkDescription.pDepthStencilAttachment = PassInfo.bUsesDepthStencilAttachment ? &SubpassInfo.DepthStencilAttachmentRef : nullptr;
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
	if (CreateInfo->bHasDepthStencilAttachment)
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

	if(vkCreateRenderPass(GVulkanInfo.Device, &RenderPassCreateInfo, nullptr, &Result->RenderPass) != VK_SUCCESS)
	{
		GLog->critical("Failed to create Vulkan render pass");

		delete Result;
		return nullptr;
	}

	RECORD_RESOURCE_ALLOC(Result)

	return Result;
}

CommandBuffer VulkanRenderingInterface::CreateSwapChainCommandBuffer(SwapChain Target, bool bDynamic)
{
	VulkanSwapChain* VkSwap = static_cast<VulkanSwapChain*>(Target);

	// Create a single command buffer
	VkCommandBufferAllocateInfo CmdBufAllocInfo{};
	CmdBufAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	CmdBufAllocInfo.commandPool = GVulkanInfo.MainCommandPool;
	CmdBufAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; // TODO: support secondary command buffers
	CmdBufAllocInfo.commandBufferCount = VkSwap->ImageCount;

	VulkanCommandBuffer* NewCmdBuf = new VulkanCommandBuffer;
	NewCmdBuf->CmdBuffers.resize(VkSwap->ImageCount);
	NewCmdBuf->bDynamic = bDynamic;
	NewCmdBuf->bTargetSwapChain = true;

	if (vkAllocateCommandBuffers(GVulkanInfo.Device, &CmdBufAllocInfo, NewCmdBuf->CmdBuffers.data()) != VK_SUCCESS)
	{
		GLog->critical("Failed to allocate command buffer");

		delete NewCmdBuf;
		return nullptr;
	}

	RECORD_RESOURCE_ALLOC(NewCmdBuf)

	return NewCmdBuf;
}

CommandBuffer VulkanRenderingInterface::CreateCommandBuffer(bool bOneTimeUse)
{
	// Create a single command buffer
	VkCommandBufferAllocateInfo CmdBufAllocInfo{};
	CmdBufAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	CmdBufAllocInfo.commandPool = GVulkanInfo.MainCommandPool;
	CmdBufAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; // TODO: support secondary command buffers
	CmdBufAllocInfo.commandBufferCount = 1;

	VulkanCommandBuffer* NewCmdBuf = new VulkanCommandBuffer;
	NewCmdBuf->CmdBuffers.resize(1); // Only create a single command buffer since we're not targeting the swap chain
	NewCmdBuf->bOneTimeUse = bOneTimeUse;

	if (vkAllocateCommandBuffers(GVulkanInfo.Device, &CmdBufAllocInfo, NewCmdBuf->CmdBuffers.data()) != VK_SUCCESS)
	{
		GLog->critical("Failed to allocate command buffer");

		delete NewCmdBuf;
		return nullptr;
	}

	RECORD_RESOURCE_ALLOC(NewCmdBuf)

	return NewCmdBuf;
}

ResourceSet VulkanRenderingInterface::CreateResourceSet(ResourceSetCreateInfo* CreateInfo)
{
	VulkanSwapChain* VkSwap = static_cast<VulkanSwapChain*>(CreateInfo->TargetSwap);
	VulkanResourceLayout* VkLayout = static_cast<VulkanResourceLayout*>(CreateInfo->Layout);

	VulkanResourceSet* Result = new VulkanResourceSet;

	uint32_t ImageCount = VkSwap->ImageCount;

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
				GLog->critical("Failed to allocate uniform buffer for resource set");
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
		SetAllocInfo.descriptorPool = GVulkanInfo.MainDscPool;
		SetAllocInfo.descriptorSetCount = 1;
		SetAllocInfo.pSetLayouts = &VkLayout->VkLayout;

		VkDescriptorSet NewSet;
		if (vkAllocateDescriptorSets(GVulkanInfo.Device, &SetAllocInfo, &NewSet) != VK_SUCCESS)
		{
			GLog->critical("Failed to create vulkan descriptor set");
			return nullptr;
		}

		Result->DescriptorSets.push_back(NewSet);
	}

	RECORD_RESOURCE_ALLOC(Result);

	return Result;
}

VkFormat TexFormatToVkFormat(TextureFormat Format)
{
	switch(Format)
	{
	case TextureFormat::UINT32_R8G8B8A8:
		return VK_FORMAT_B8G8R8A8_SRGB;
	}

	return VK_FORMAT_R8G8B8A8_SRGB;
}

Texture VulkanRenderingInterface::CreateTexture(uint64_t ImageSize, TextureFormat Format, uint32_t Width, uint32_t Height, void* Data)
{
	VulkanTexture* Result = new VulkanTexture;

	VkFormat ImageFormat = VK_FORMAT_R8G8B8A8_SRGB;// TexFormatToVkFormat(Format);

	// Create staging buffer
	CreateBuffer
	(
		ImageSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		Result->StagingBuffer, Result->StagingBufferMemory
	);

	void* MappedData;
	vkMapMemory(GVulkanInfo.Device, Result->StagingBufferMemory, 0, ImageSize, 0, &MappedData);
	{
		std::memcpy(MappedData, Data, ImageSize);
	}
	vkUnmapMemory(GVulkanInfo.Device, Result->StagingBufferMemory);

	VkImageCreateInfo ImageCreate{};
	ImageCreate.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	ImageCreate.imageType = VK_IMAGE_TYPE_2D;
	ImageCreate.extent.width = Width;
	ImageCreate.extent.height = Height;
	ImageCreate.extent.depth = 1;
	ImageCreate.mipLevels = 1;
	ImageCreate.arrayLayers = 1;
	ImageCreate.format = ImageFormat;
	ImageCreate.tiling = VK_IMAGE_TILING_OPTIMAL;
	ImageCreate.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	ImageCreate.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	ImageCreate.samples = VK_SAMPLE_COUNT_1_BIT;
	ImageCreate.flags = 0;

	if(vkCreateImage(GVulkanInfo.Device, &ImageCreate, nullptr, &Result->TextureImage) != VK_SUCCESS)
	{
		GLog->critical("Failed to create vulkan image");
		return nullptr;
	}

	VkMemoryRequirements MemReq{};
	vkGetImageMemoryRequirements(GVulkanInfo.Device, Result->TextureImage, &MemReq);

	VkMemoryAllocateInfo AllocInfo{};
	AllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	AllocInfo.allocationSize = MemReq.size;
	AllocInfo.memoryTypeIndex = FindMemoryType(MemReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	if(vkAllocateMemory(GVulkanInfo.Device, &AllocInfo, nullptr, &Result->TextureMemory) != VK_SUCCESS)
	{
		GLog->critical("Failed to create memory for vulkan image");
		return nullptr;
	}

	vkBindImageMemory(GVulkanInfo.Device, Result->TextureImage, Result->TextureMemory, 0);

	// Transition image to upload data to
	ImmediateSubmitAndWait([&](CommandBuffer Buf)
	{
		GRenderAPI->TransitionTexture(Buf, Result, AttachmentUsage::Undefined, AttachmentUsage::TransferDestination);
	});

	ImmediateSubmitAndWait([&](CommandBuffer Buf)
	{
		ForEachCmdBuffer(Buf, [&](VkCommandBuffer Buf, int32_t ImageIndex)
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
			ImageCopy.imageExtent = {Width, Height, 1};

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
		GRenderAPI->TransitionTexture(Buf, Result, AttachmentUsage::TransferDestination, AttachmentUsage::ShaderRead);
	});

	// Create image view
	VkImageViewCreateInfo ViewInfo{};
	ViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	ViewInfo.image = Result->TextureImage;
	ViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	ViewInfo.format = ImageFormat;
	ViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	ViewInfo.subresourceRange.baseMipLevel = 0;
	ViewInfo.subresourceRange.levelCount = 1;
	ViewInfo.subresourceRange.baseArrayLayer = 0;
	ViewInfo.subresourceRange.layerCount = 1;

	if(vkCreateImageView(GVulkanInfo.Device, &ViewInfo, nullptr, &Result->TextureImageView) != VK_SUCCESS)
	{
		GLog->critical("Failed to create vulkan image view");
		return nullptr;
	}

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

	if(vkCreateSampler(GVulkanInfo.Device, &SamplerInfo, nullptr, &Result->TextureSampler) != VK_SUCCESS)
	{
		GLog->critical("Failed to create vulkan sampler");
		return nullptr;
	}

	RECORD_RESOURCE_ALLOC(Result)

	return Result;
}

FrameBuffer VulkanRenderingInterface::CreateFrameBuffer(FrameBufferCreateInfo* CreateInfo)
{
	VulkanRenderGraph* VkRenderGraph = static_cast<VulkanRenderGraph*>(CreateInfo->TargetGraph);

	VulkanFrameBuffer* Result = new VulkanFrameBuffer;
	Result->AttachmentWidth = CreateInfo->Width;
	Result->AttachmentHeight = CreateInfo->Height;
	Result->CreatedFor = VkRenderGraph->RenderPass;

	for (uint32_t ColorAttach = 0; ColorAttach < CreateInfo->ColorAttachmentCount; ColorAttach++)
		Result->ColorAttachmentDescriptions.push_back(CreateInfo->ColorAttachmentDescriptions[ColorAttach]);

	if (CreateInfo->bHasDepthStencilAttachment)
	{
		Result->bHasDepthStencilAttachment = true;
		Result->DepthStencilAttachmentDesc = CreateInfo->DepthStencilDescription;
	}

	if (!CreateFrameBufferImages(Result, Result->ColorAttachmentDescriptions, CreateInfo->Width, CreateInfo->Height))
	{
		return nullptr;
	}

	if (!CreateFrameBufferSamplers(Result, static_cast<uint32_t>(Result->ColorAttachmentImages.size())))
	{
		return nullptr;
	}

	if (!CreateFrameBufferResource(Result, VkRenderGraph->RenderPass, CreateInfo->Width, CreateInfo->Height))
	{
		return nullptr;
	}

	RECORD_RESOURCE_ALLOC(Result)

	return Result;
}

VertexBuffer VulkanRenderingInterface::CreateVertexBuffer(VertexBufferCreateInfo* CreateInfo)
{
	VulkanVertexBuffer* VulkanVbo = new VulkanVertexBuffer;
	VulkanVbo->bHasIndexBuffer = CreateInfo->bCreateIndexBuffer;

	// Create staging buffer. This needs the transfer source bit since we will be transferring it to the device memory.
	CreateBuffer(CreateInfo->VertexBufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
		VulkanVbo->StagingVertexBuffer, VulkanVbo->StagingVertexBufferMemory
	);

	// Create device buffer. Because we will be copying the staging buffer to the device buffer, we need to make it eligible for transfer.
	CreateBuffer(CreateInfo->VertexBufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		VulkanVbo->DeviceVertexBuffer, VulkanVbo->DeviceVertexBufferMemory
	);

	if(CreateInfo->bCreateIndexBuffer)
	{
		CreateBuffer(CreateInfo->IndexBufferSize,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			VulkanVbo->StagingIndexBuffer, VulkanVbo->StagingIndexBufferMemory
		);

		// Create device index buffer. Because we will be copying the staging buffer to the device buffer, we need to make it eligible for transfer.
		CreateBuffer(CreateInfo->IndexBufferSize,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			VulkanVbo->DeviceIndexBuffer, VulkanVbo->DeviceIndexBufferMemory
		);
	}

	// Create fence to synchronize staging buffer access
	VkFenceCreateInfo FenceCreate{};
	FenceCreate.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	FenceCreate.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	if(vkCreateFence(GVulkanInfo.Device, &FenceCreate, nullptr, &VulkanVbo->VertexStagingCompleteFence) != VK_SUCCESS)
	{
		GLog->critical("Failed to create staging fence");
		return nullptr;
	}

	if(CreateInfo->bCreateIndexBuffer && vkCreateFence(GVulkanInfo.Device, &FenceCreate, nullptr, &VulkanVbo->IndexStagingCompleteFence) != VK_SUCCESS)
	{
		GLog->critical("Failed to create staging fence");
		return nullptr;
	}

	// Create command buffer used to transfer staging buffer to device buffer
	VkCommandBufferAllocateInfo AllocInfo{};
	AllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	AllocInfo.commandBufferCount = 1;
	AllocInfo.commandPool = GVulkanInfo.MainCommandPool;
	AllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	if(vkAllocateCommandBuffers(GVulkanInfo.Device, &AllocInfo, &VulkanVbo->VertexStagingCommandBuffer) != VK_SUCCESS)
	{
		GLog->critical("Failed to create staging command buffer");
		return nullptr;
	}

	if(CreateInfo->bCreateIndexBuffer && vkAllocateCommandBuffers(GVulkanInfo.Device, &AllocInfo, &VulkanVbo->IndexStagingCommandBuffer) != VK_SUCCESS)
	{
		GLog->critical("Failed to create staging command buffer");
		return nullptr;
	}

	RECORD_RESOURCE_ALLOC(VulkanVbo)

	return VulkanVbo;
}

void VulkanRenderingInterface::DestroyVertexBuffer(VertexBuffer VertexBuffer)
{
	VulkanVertexBuffer* VulkanVbo = static_cast<VulkanVertexBuffer*>(VertexBuffer);
	REMOVE_RESOURCE_ALLOC(VulkanVbo)

	vkDeviceWaitIdle(GVulkanInfo.Device);

	if(VulkanVbo->bHasIndexBuffer)
	{
		vkFreeMemory(GVulkanInfo.Device, VulkanVbo->StagingVertexBufferMemory, nullptr);
		vkFreeMemory(GVulkanInfo.Device, VulkanVbo->StagingIndexBufferMemory, nullptr);
		vkDestroyBuffer(GVulkanInfo.Device, VulkanVbo->StagingIndexBuffer, nullptr);
		vkDestroyBuffer(GVulkanInfo.Device, VulkanVbo->DeviceIndexBuffer, nullptr);
		vkDestroyFence(GVulkanInfo.Device, VulkanVbo->IndexStagingCompleteFence, nullptr);
		vkFreeCommandBuffers(GVulkanInfo.Device, GVulkanInfo.MainCommandPool, 1, &VulkanVbo->IndexStagingCommandBuffer);
	}

	vkFreeMemory(GVulkanInfo.Device, VulkanVbo->DeviceVertexBufferMemory, nullptr);
	vkFreeMemory(GVulkanInfo.Device, VulkanVbo->DeviceIndexBufferMemory, nullptr);
	vkDestroyBuffer(GVulkanInfo.Device, VulkanVbo->StagingVertexBuffer, nullptr);
	vkDestroyBuffer(GVulkanInfo.Device, VulkanVbo->DeviceVertexBuffer, nullptr);
	vkDestroyFence(GVulkanInfo.Device, VulkanVbo->VertexStagingCompleteFence, nullptr);
	vkFreeCommandBuffers(GVulkanInfo.Device, GVulkanInfo.MainCommandPool, 1, &VulkanVbo->VertexStagingCommandBuffer);

	delete VulkanVbo;
}

void VulkanRenderingInterface::DestroyFrameBuffer(FrameBuffer FrameBuffer)
{
	VulkanFrameBuffer* VkFbo = static_cast<VulkanFrameBuffer*>(FrameBuffer);
	REMOVE_RESOURCE_ALLOC(VkFbo)

	vkDeviceWaitIdle(GVulkanInfo.Device);

	DestroyFramebufferImages(VkFbo);
	DestroyFramebufferSamplers(VkFbo);

	vkDestroyFramebuffer(GVulkanInfo.Device, VkFbo->VulkanFbo, nullptr);

	delete VkFbo;
}

void VulkanRenderingInterface::DestroyCommandBuffer(CommandBuffer CmdBuffer)
{
	VulkanCommandBuffer* VkCmdBuffer = static_cast<VulkanCommandBuffer*>(CmdBuffer);
	REMOVE_RESOURCE_ALLOC(VkCmdBuffer)

	vkQueueWaitIdle(GVulkanInfo.GraphicsQueue);
	vkFreeCommandBuffers(GVulkanInfo.Device, GVulkanInfo.MainCommandPool, static_cast<uint32_t>(VkCmdBuffer->CmdBuffers.size()), VkCmdBuffer->CmdBuffers.data());

	delete VkCmdBuffer;
}

void VulkanRenderingInterface::DestroyRenderGraph(RenderGraph Graph)
{
	VulkanRenderGraph* VkRenderGraph = static_cast<VulkanRenderGraph*>(Graph);

    vkQueueWaitIdle(GVulkanInfo.GraphicsQueue);
	vkDestroyRenderPass(GVulkanInfo.Device, VkRenderGraph->RenderPass, nullptr);

	REMOVE_RESOURCE_ALLOC(VkRenderGraph)

	delete VkRenderGraph;
}

void VulkanRenderingInterface::DestroyPipeline(Pipeline Pipeline)
{
	VulkanPipeline* VkPipeline = static_cast<VulkanPipeline*>(Pipeline);

    vkQueueWaitIdle(GVulkanInfo.GraphicsQueue);

	// Destroy pipeline layout
	vkDestroyPipelineLayout(GVulkanInfo.Device, VkPipeline->PipelineLayout, nullptr);

	// Destroy pipeline
	vkDestroyPipeline(GVulkanInfo.Device, VkPipeline->Pipeline, nullptr);

	REMOVE_RESOURCE_ALLOC(VkPipeline)

	delete VkPipeline;
}

void VulkanRenderingInterface::DestroyShader(ShaderProgram Shader)
{
	VulkanShader* VkShader = static_cast<VulkanShader*>(Shader);

    vkQueueWaitIdle(GVulkanInfo.GraphicsQueue);

	if(VkShader->bHasVertexShader)
		vkDestroyShaderModule(GVulkanInfo.Device, VkShader->VertexModule, nullptr);
	if (VkShader->bHasFragmentShader)
		vkDestroyShaderModule(GVulkanInfo.Device, VkShader->FragmentModule, nullptr);

	REMOVE_RESOURCE_ALLOC(VkShader)

	delete VkShader;
}

void VulkanRenderingInterface::DestroyTexture(Texture Image)
{
	vkDeviceWaitIdle(GVulkanInfo.Device);
	VulkanTexture* VkTex = static_cast<VulkanTexture*>(Image);

	vkFreeMemory(GVulkanInfo.Device, VkTex->TextureMemory, nullptr);
	vkFreeMemory(GVulkanInfo.Device, VkTex->StagingBufferMemory, nullptr);

	vkDestroyImage(GVulkanInfo.Device, VkTex->TextureImage, nullptr);
	vkDestroyBuffer(GVulkanInfo.Device, VkTex->StagingBuffer, nullptr);

	vkDestroyImageView(GVulkanInfo.Device, VkTex->TextureImageView, nullptr);
	vkDestroySampler(GVulkanInfo.Device, VkTex->TextureSampler, nullptr);

	REMOVE_RESOURCE_ALLOC(VkTex)

	delete VkTex;
}
