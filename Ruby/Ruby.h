#pragma once

#include <string>
#include "llrm.h"
#include "Ruby.h"
#include "Ruby.h"

namespace Ruby
{
	enum class RenderingAPI
	{
		Vulkan
	};

	struct SceneResources
	{

	};

	struct RubyContext
	{
		std::string ShadersRoot;
		std::string CompiledShaders;

		llrm::Context LLContext{};

		std::unordered_map<uint32_t, SceneResources> mResources;
	};

	struct SwapChain
	{
		GLFWwindow* mWnd = nullptr;
		llrm::Surface mSurface;
		llrm::SwapChain mSwap;
		llrm::RenderGraph mGraph;
		std::vector<llrm::FrameBuffer> mFrameBuffers;
		std::vector<llrm::CommandBuffer> mCmdBuffers;
	};

	struct ContextParams
	{
		std::string ShadersRoot;
		std::string CompiledShaders;
	};

	RubyContext CreateContext(const ContextParams& Params);
	void DestroyContext(const RubyContext& Context);
	void SetContext(const RubyContext& Context);
	SwapChain CreateSwapChain(GLFWwindow* Wnd);
	void DestroySwapChain(const SwapChain& Swap);

	int32_t BeginFrame(SwapChain Swap);
	void EndFrame();

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

	class Scene
	{
	public:

		uint32_t mId;

		Scene(uint32_t Id)
		{
			mId = Id;
		}

	};

	void RenderScene(const Scene& Scene, const SwapChain& Target);

}
