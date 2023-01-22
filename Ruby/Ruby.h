#pragma once

#include <string>
#include "llrm.h"

namespace Ruby
{
	enum class RenderingAPI
	{
		Vulkan
	};

	struct RubyContext
	{
		std::string ShadersRoot;
		std::string CompiledShaders;

		llrm::Context LLContext{};
	};

	struct ContextParams
	{
		std::string ShadersRoot;
		std::string CompiledShaders;
	};

	RubyContext CreateContext(const ContextParams& Params);
	void DestroyContext(const RubyContext& Context);
	void SetContext(const RubyContext& Context);

	extern RubyContext GContext;

	struct Mesh
	{
		
	};

	struct FrameBuffer
	{
		llrm::Texture ColorAttachment;
		llrm::Texture DepthAttachment;
		llrm::FrameBuffer Buffer;
		llrm::RenderGraph Graph{};
	};

	FrameBuffer CreateFrameBuffer(uint32_t Width, uint32_t Height, bool DepthAttachment);
	void DestroyFrameBuffer();

	struct Window
	{
		GLFWwindow* Wnd{};
		llrm::SwapChain Swap{};
		llrm::Surface Surface{};
	};

	struct SceneResources
	{
		
	};

	class Scene
	{
	public:

		SceneResources& GetResources() { return Resources; }

	private:
		SceneResources Resources;
	};

	void RenderScene(const Scene& Scene, const FrameBuffer& Target);

}
