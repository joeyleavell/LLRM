#include "Ruby.h"

#include "llrm.h"
#include "GLFW/glfw3.h"
#include "Utill.h"
#include "ShaderManager.h"

namespace Ruby
{
	RubyContext GContext;

#if LLRM_VULKAN
	RenderingAPI GAPI = RenderingAPI::Vulkan;
#endif

	RubyContext CreateContext(const ContextParams& Params)
	{  
		RubyContext NewContext{};
		NewContext.CompiledShaders = Params.CompiledShaders;
		NewContext.ShadersRoot = Params.ShadersRoot;

		if(NewContext.CompiledShaders.empty())
		{
			NewContext.CompiledShaders = GetDefaultCompiledShadersPath(GAPI);
		}

		if (NewContext.ShadersRoot.empty())
		{
			NewContext.ShadersRoot = GetDefaultShadersPath();
		}
		  
		GContext = NewContext;

		if (!glfwInit())
			return {}; // Error

		NewContext.LLContext = llrm::CreateContext();

		Ruby::InitShaderCompilation();

		return NewContext;
	}

	void DestroyContext(const RubyContext& Context)
	{
		Ruby::FinishShaderCompilation();
	}

	void SetContext(const RubyContext& Context)
	{
		GContext = Context;
	}

	FrameBuffer CreateFrameBuffer(uint32_t Width, uint32_t Height, bool DepthAttachment)
	{
		FrameBuffer NewBuffer{};
		NewBuffer.ColorAttachment = llrm::CreateTexture(llrm::AttachmentFormat::B8G8R8A8_SRGB, Width, Height, llrm::TEXTURE_USAGE_RT);

		if(DepthAttachment)
		{
			NewBuffer.DepthAttachment = llrm::CreateTexture(llrm::AttachmentFormat::D24_UNORM_S8_UINT, Width, Height, llrm::TEXTURE_USAGE_RT);
		}

		// Create render graph
		llrm::RenderGraphCreateInfo Info{};
		Info.Attachments.push_back({
			llrm::AttachmentUsage::ColorAttachment,
			llrm::AttachmentUsage::ColorAttachment,
			llrm::AttachmentFormat::B8G8R8A8_SRGB
		});
		Info.Passes = { {{0}} };

		if(DepthAttachment)
		{
			Info.Attachments.push_back({
				llrm::AttachmentUsage::DepthStencilAttachment,
				llrm::AttachmentUsage::DepthStencilAttachment,
				llrm::AttachmentFormat::D24_UNORM_S8_UINT
			});
		}

		NewBuffer.Graph = llrm::CreateRenderGraph(Info);

		NewBuffer.Buffer = llrm::CreateFrameBuffer({
			Width, Height,
			{NewBuffer.ColorAttachment},
			NewBuffer.Graph
		});

		return NewBuffer;
	}

	void DestroyFrameBuffer()
	{

	}

	void RenderScene(const Scene& Scene, const FrameBuffer& Target)
	{

	}

}
