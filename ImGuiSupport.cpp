#include "ImGuiSupport.h"

#ifdef LLRM_BUILD_IMGUI

#include "llrm_vulkan.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"

struct VulkanCommandBuffer;

#ifdef LLRM_VULKAN
void RecordImGuiCmds(llrm::CommandBuffer Dst)
{
    VulkanCommandBuffer* VkCmd = static_cast<VulkanCommandBuffer*>(Dst);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), VkCmd->CmdBuffer);
}
#endif

void InitImGuiVulkan(GLFWwindow* Wnd, llrm::Context Context, llrm::RenderGraph Graph, llrm::SwapChain Swap)
{
    VulkanContext* VkContext = static_cast<VulkanContext*>(Context);
    VulkanSwapChain* VkSwap = static_cast<VulkanSwapChain*>(Swap);
    VulkanRenderGraph* VkGraph = static_cast<VulkanRenderGraph*>(Graph);

    ImGui_ImplGlfw_InitForVulkan(Wnd, true);
    ImGui_ImplVulkan_InitInfo InitInfo = {};
    InitInfo.Instance = VkContext->Instance;
    InitInfo.PhysicalDevice = VkContext->PhysicalDevice;
    InitInfo.Device = VkContext->Device;
    InitInfo.QueueFamily = VkContext->GraphicsQueueFamIndex;
    InitInfo.Queue = VkContext->GraphicsQueue;
    InitInfo.PipelineCache = nullptr;
    InitInfo.DescriptorPool = VkContext->MainDscPool;
    InitInfo.Allocator = nullptr;
    InitInfo.MinImageCount = VkSwap->Images.size();
    InitInfo.ImageCount = VkSwap->Images.size();
    InitInfo.CheckVkResultFn = nullptr;
    InitInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    ImGui_ImplVulkan_Init(&InitInfo, VkGraph->RenderPass);

    // Upload ImGui fonts
    llrm::ImmediateSubmitAndWait([&](llrm::CommandBuffer Cmd)
    {
        VulkanCommandBuffer* VkCmd = static_cast<VulkanCommandBuffer*>(Cmd);
        ImGui_ImplVulkan_CreateFontsTexture(VkCmd->CmdBuffer);
    });

    // Clear font resources
    ImGui_ImplVulkan_DestroyFontUploadObjects();

}

void BeginImGuiFrame()
{
#ifdef LLRM_VULKAN
	ImGui_ImplVulkan_NewFrame();
#endif

    // imgui new frame
    ImGui_ImplGlfw_NewFrame();

    // New ImGui frame
    ImGui::NewFrame();
}

void EndImGuiFrame()
{
    ImGui::Render();
}

void UpdateImGuiViewports()
{
    // Update and Render additional Platform Windows
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}

ImGuiContext* InitImGui(GLFWwindow* Wnd, llrm::Context Context, llrm::SwapChain Swap, llrm::RenderGraph Graph, ImGuiFeatures Features)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    // Modify ImGUI flags
    ImGuiIO& IO = ImGui::GetIO();
    if (Features.EnableSRGB)
        IO.ConfigFlags |= ImGuiConfigFlags_IsSRGB;
    if (Features.EnableDocking)
        IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    if (Features.EnableViewports)
        IO.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    IO.ConfigViewportsNoDecoration = false;
    IO.ConfigViewportsNoAutoMerge = true;
    IO.ConfigWindowsMoveFromTitleBarOnly = true;

    // Set style
    ImGui::StyleColorsDark();

    // Initialize backend
#ifdef LLRM_VULKAN
    InitImGuiVulkan(Wnd, Context, Graph, Swap);
#endif

    // Setup Platform/Renderer bindings

    return ImGui::GetCurrentContext();
}

void ShutdownImGui(llrm::Context Context)
{
#ifdef LLRM_VULKAN
    VulkanContext* VkContext = static_cast<VulkanContext*>(Context);
	vkDeviceWaitIdle(VkContext->Device);
    ImGui_ImplVulkan_Shutdown();
#endif

    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    ImGui::SetCurrentContext(nullptr);
}

/*ImTextureID CreateImTextureFboColorAttachmentVulkan(FrameBuffer Fbo, uint32_t ColorAttachmentIndex)
{
    VulkanFrameBuffer* VkFbo = static_cast<VulkanFrameBuffer*>(Fbo);

    return ImGui_ImplVulkan_AddTexture(VkFbo->ColorAttachmentSamplers[ColorAttachmentIndex], VkFbo->ColorAttachmentImageViews[ColorAttachmentIndex], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

ImTextureID CreateImTextureVulkan(Texture Image)
{
    VulkanTexture* VkImage = static_cast<VulkanTexture*>(Image);

    return ImGui_ImplVulkan_AddTexture(VkImage->TextureSampler, VkImage->TextureImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

ImTextureID CreateImTextureFboColorAttachment(FrameBuffer Fbo, uint32_t ColorAttachmentIndex)
{
    if (GRenderAPIType == DynamicRenderAPI::Vulkan)
    {
        return CreateImTextureFboColorAttachmentVulkan(Fbo, ColorAttachmentIndex);
    }

    return nullptr;
}

ImTextureID CreateImTexture(Texture Image)
{
    if (GRenderAPIType == DynamicRenderAPI::Vulkan)
    {
        return CreateImTextureVulkan(Image);
    }

    return nullptr;
}

void FreeImGuiTexture(ImTextureID Image)
{
    if (GRenderAPIType == DynamicRenderAPI::Vulkan)
    {
        vkQueueWaitIdle(GVulkanInfo.GraphicsQueue);

        VkDescriptorSet AsSet = reinterpret_cast<VkDescriptorSet>(Image);
        vkFreeDescriptorSets(GVulkanInfo.Device, GVulkanInfo.MainDscPool, 1, &AsSet);
    }
}
*/

#endif