#pragma once

#include <string>
#include <vector>
//#include "Platform.h"
#include "vulkan/vulkan.h"
#include <functional>
//#include "spdlog/spdlog.h"
#undef max // Vulkan includes windows headers now, which define this

#define MAX_FRAMES_IN_FLIGHT 3

struct GLFWwindow;

const uint32_t MAX_SUBPASSES = 8;

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
    VkBuffer StagingVertexBuffer;
    VkBuffer DeviceIndexBuffer;
    VkBuffer StagingIndexBuffer;

    VkDeviceMemory DeviceVertexBufferMemory;
    VkDeviceMemory StagingVertexBufferMemory;
    VkDeviceMemory DeviceIndexBufferMemory;
    VkDeviceMemory StagingIndexBufferMemory;

    VkCommandBuffer VertexStagingCommandBuffer;
    VkCommandBuffer IndexStagingCommandBuffer;

	VkFence VertexStagingCompleteFence;
    VkFence IndexStagingCompleteFence;

    bool bHasIndexBuffer = false;
};

struct VulkanFrameBuffer
{
    uint32_t AttachmentWidth;
    uint32_t AttachmentHeight;

    std::vector<VkImageView>    AllAttachments;

    // Color attachment info
    std::vector<VkImage>        ColorAttachmentImages;
    std::vector<VkImageView>    ColorAttachmentImageViews;
    std::vector<VkDeviceMemory> ColorAttachmentMemory;
    std::vector<VkSampler>      ColorAttachmentSamplers;

    // Depth/stencil info
    bool                        bHasDepthStencilAttachment = false;
    VkImage                     DepthStencilImage;
    VkImageView                 DepthStencilImageView;
    VkDeviceMemory              DepthStencilMemory;

    VkFramebuffer VulkanFbo;

    // Store some of the initial creation info for re-creating the framebuffers
    std::vector<FramebufferAttachmentDescription> ColorAttachmentDescriptions;
    FramebufferAttachmentDescription              DepthStencilAttachmentDesc;
	VkRenderPass                                  CreatedFor;
};

struct VulkanCommandBuffer
{
    VulkanSwapChain* CurrentSwapChain{};
    VulkanFrameBuffer* CurrentFbo{};
    std::vector<VkCommandBuffer> CmdBuffers;
    bool bDynamic = false; // If bTargetSwapChain is true, whether this command buffer will be re-recorded every frame (true) or very in-frequently recorded
    bool bOneTimeUse = false; // Whether this command buffer is intended to only be used once
    bool bTargetSwapChain = false; // Whether this command buffer targets the swap chain (i.e. references a frame with vkBeginRenderPass)

    VulkanPipeline* BoundPipeline = nullptr;
};

struct VulkanResourceLayout
{
    VkDescriptorSetLayout VkLayout;
    std::vector<ConstantBufferDescription> ConstantBuffers;
    std::vector<TextureDescription> TextureBindings;

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

struct VulkanTexture
{
    VkBuffer StagingBuffer;
    VkDeviceMemory StagingBufferMemory;

    VkDeviceMemory TextureMemory;
    VkImage TextureImage;

    VkImageView TextureImageView;
    VkSampler TextureSampler;
};

class VulkanRenderingInterface : public RenderingInterface
{
public:

	VulkanRenderingInterface() = default;
	virtual ~VulkanRenderingInterface() = default;

	bool InitializeRenderAPI() override;
	bool InitializeForSurface(Surface Target) override;
	void ShutdownRenderAPI() override;
	ShaderProgram CreateShader(const ShaderCreateInfo* ProgramData) override;
	SwapChain CreateSwapChain(Surface TargetSurface, int32_t DesiredWidth, int32_t DesiredHeight) override;
	Surface CreateSurfaceForWindow(Window* Target) override;
    ResourceLayout CreateResourceLayout(const ResourceLayoutCreateInfo* CreateInfo) override;
	Pipeline CreatePipeline(const PipelineCreateInfo* CreateInfo) override;
	RenderGraph CreateRenderGraph(const RenderGraphCreateInfo* CreateInfo) override;
    FrameBuffer CreateFrameBuffer(FrameBufferCreateInfo* CreateInfo) override;
    VertexBuffer CreateVertexBuffer(VertexBufferCreateInfo* CreateInfo) override;
    CommandBuffer CreateSwapChainCommandBuffer(SwapChain Target, bool bDynamic) override;
    CommandBuffer CreateCommandBuffer(bool bOneTimeUse = false) override;
    ResourceSet CreateResourceSet(ResourceSetCreateInfo* CreateInfo) override;
    Texture CreateTexture(uint64_t ImageSize, TextureFormat Format, uint32_t Width, uint32_t Height, void* Data) override;

    void DestroyVertexBuffer(VertexBuffer VertexBuffer) override;
    void DestroyFrameBuffer(FrameBuffer FrameBuffer) override;
	void DestroyCommandBuffer(CommandBuffer CmdBuffer) override;
	void DestroyRenderGraph(RenderGraph Graph) override;
    void DestroyResourceLayout(ResourceLayout Layout) override;
    void DestroyResourceSet(ResourceSet Resources) override;
	void DestroyPipeline(Pipeline Pipeline) override;
	void DestroyShader(ShaderProgram Shader) override;
    void DestroyTexture(Texture Image) override;
	void DestroySurface(Surface Surface) override;
	void DestroySwapChain(SwapChain Swap) override;

    void SubmitSwapCommandBuffer(SwapChain Target, CommandBuffer Buffer) override;
	void BeginFrame(SwapChain Swap, Surface Target, int32_t FrameWidth, int32_t FrameHeight) override;
	void EndFrame(SwapChain Swap, Surface Target, int32_t FrameWidth, int32_t FrameHeight) override;
	void GetSwapChainSize(SwapChain Swap, uint32_t& Width, uint32_t& Height) override;
    void RecreateSwapChain(SwapChain Swap, Surface Target, int32_t DesiredWidth, int32_t DesiredHeight) override;

    void GetFrameBufferSize(FrameBuffer Fbo, uint32_t& Width, uint32_t& Height) override;
    void ResizeFrameBuffer(FrameBuffer Fbo, uint32_t NewWidth, uint32_t NewHeight) override;

    void UpdateUniformBuffer(ResourceSet Resources, SwapChain Target, uint32_t BufferIndex, void* Data, uint64_t DataSize) override;
    void UpdateTextureResource(ResourceSet Resources, SwapChain Target, Texture* Images, uint32_t ImageCount, uint32_t Binding) override;
    void UpdateAttachmentResource(ResourceSet Resources, SwapChain Target, FrameBuffer SrcBufer, uint32_t AttachmentIndex, uint32_t Binding) override;
    void UpdateAttachmentResources(ResourceSet Resources, SwapChain Target, FrameBuffer* Buffers, uint32_t BufferCount, uint32_t AttachmentIndex, uint32_t Binding) override;

    void UploadVertexBufferData(VertexBuffer Buffer, void* Data, uint64_t Size) override;
	void UploadIndexBufferData(VertexBuffer Buffer, uint32_t* Data, uint64_t Size) override;
	void ResizeVertexBuffer(VertexBuffer Buffer, uint64_t NewSize) override;
    void ResizeIndexBuffer(VertexBuffer Buffer, uint64_t NewSize) override;

    void ReadFramebufferAttachment(FrameBuffer SrcBuffer, uint32_t Attachment, void* Dst, uint64_t BufferSize) override;

    void SubmitCommandBuffer(CommandBuffer Buffer, bool bWait, Fence WaitFence) override;
    void Reset(CommandBuffer Buf) override;
    void Begin(CommandBuffer Buf) override;
    void TransitionFrameBufferColorAttachment(CommandBuffer Buf, FrameBuffer Source, uint32_t AttachmentIndex, AttachmentUsage Old, AttachmentUsage New) override;
    void TransitionFrameBufferDepthStencilAttachment(CommandBuffer Buf, FrameBuffer Source, AttachmentUsage Old, AttachmentUsage New) override;
	void TransitionTexture(CommandBuffer Buf, Texture Image, AttachmentUsage Old, AttachmentUsage New) override;
	void End(CommandBuffer Buf) override;
    void BeginRenderGraph(CommandBuffer Buf, SwapChain Target, RenderGraphInfo Info = {}) override;
    void BeginRenderGraph(CommandBuffer Buf, RenderGraph Graph, FrameBuffer Target, RenderGraphInfo Info = {}) override;
    void EndRenderGraph(CommandBuffer Buf) override;
    void BindPipeline(CommandBuffer Buf, Pipeline PipelineObject) override;
    void BindResources(CommandBuffer Buf, ResourceSet Resources) override;
	void DrawVertexBuffer(CommandBuffer Buf, VertexBuffer Vbo, uint32_t VertexCount) override;
    void DrawVertexBufferIndexed(CommandBuffer Buf, VertexBuffer Vbo, uint32_t IndexCount) override;
	void SetViewport(CommandBuffer Buf, uint32_t X, uint32_t Y, uint32_t W, uint32_t H) override;
    void SetScissor(CommandBuffer Buf, uint32_t X, uint32_t Y, uint32_t W, uint32_t H) override;

};

/**
 * Global Vulkan construct
 */
extern RUNTIME_MODULE GlobalVulkanInfo GVulkanInfo;

inline int32_t FindMemoryType(uint32_t TypeFilter, VkMemoryPropertyFlags Properties)
{
    VkPhysicalDeviceMemoryProperties MemProperties;
    vkGetPhysicalDeviceMemoryProperties(GVulkanInfo.PhysicalDevice, &MemProperties);

    for(uint32_t MemTypeIndex = 0; MemTypeIndex < MemProperties.memoryTypeCount; MemTypeIndex++)
    {
        bool bTypeSupported = TypeFilter & (1 << MemTypeIndex);
        bool bFlagsSupported = MemProperties.memoryTypes[MemTypeIndex].propertyFlags & Properties;
	    if(bTypeSupported && bFlagsSupported)
	    {
            return static_cast<int32_t>(MemTypeIndex);
	    }
    }

    return -1;
}

inline void CreateDsc()
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
    if(vkCreateDescriptorSetLayout(GVulkanInfo.Device, &CreateInfo, nullptr, &Layout) != VK_SUCCESS)
    {
        return;
    }


}
