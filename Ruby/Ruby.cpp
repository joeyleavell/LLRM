#include "Ruby.h"

#include "llrm.h"
#include "GLFW/glfw3.h"
#include "Utill.h"
#include "ShaderManager.h"
#include "glm/vec2.hpp"

namespace Ruby
{
	RubyContext GContext;

#if LLRM_VULKAN
	RenderingAPI GAPI = RenderingAPI::Vulkan;
#endif


	void UpdateSwapChain(llrm::SwapChain Swap, llrm::RenderGraph Graph, std::vector<llrm::FrameBuffer>& Fbos, std::vector<llrm::CommandBuffer>& CmdBuffers)
	{
		uint32_t Width, Height;
		llrm::GetSwapChainSize(Swap, Width, Height);

		CmdBuffers.resize(llrm::GetSwapChainImageCount(Swap), nullptr);
		Fbos.resize(CmdBuffers.size(), nullptr);

		for (uint32_t Image = 0; Image < Fbos.size(); Image++)
		{
			if (Fbos[Image])
				llrm::DestroyFrameBuffer(Fbos[Image]);

			Fbos[Image] = llrm::CreateFrameBuffer({
				Width, Height,
				{llrm::GetSwapChainImage(Swap, Image)},
				Graph
				});

			if (!CmdBuffers[Image])
				CmdBuffers[Image] = llrm::CreateCommandBuffer();
		}
	}

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

		NewContext.mDeferredGeometryRl = llrm::CreateResourceLayout({{}, {}});

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

	SwapChain CreateSwapChain(GLFWwindow* Wnd)
	{
		SwapChain Swap{};

		// Create surface and set mWnd
		{
			Swap.mSurface = llrm::CreateSurface(Wnd);
			Swap.mWnd = Wnd;
		}

		// Create llrm::SwapChain
		{
			int Width, Height;
			glfwGetFramebufferSize(Wnd, &Width, &Height);
			Swap.mSwap = llrm::CreateSwapChain(Swap.mSurface, Width, Height);
		}

		// Create render graph for window
		{
			llrm::RenderGraphCreateInfo Info{};
			Info.Attachments.push_back({
				llrm::AttachmentUsage::Undefined,
				llrm::AttachmentUsage::Presentation,
				llrm::GetTextureFormat(llrm::GetSwapChainImage(Swap.mSwap, 0)),
			});
			Info.Passes = { {{0}} };

			Swap.mGraph = llrm::CreateRenderGraph(Info);
		}

		// Create final pipeline stage
		{
			Swap.mPipeline = llrm::CreatePipeline({
				LoadRasterShader("DeferredGeometry", "DeferredGeometry"),
				Swap.mGraph,
				GContext.mDeferredGeometryRl,
				sizeof(MeshVertex),
				{{llrm::VertexAttributeFormat::Float3, offsetof(MeshVertex, Position)}},
				llrm::PipelineRenderPrimitive::TRIANGLES,
				{{false}},
				{false},
				0
			});
		}

		// Create command buffers and frame buffers
		UpdateSwapChain(Swap.mSwap, Swap.mGraph, Swap.mFrameBuffers, Swap.mCmdBuffers);

		return Swap;
	}

	void DestroySwapChain(const SwapChain& Swap)
	{
		llrm::DestroySwapChain(Swap.mSwap);
		llrm::DestroySurface(Swap.mSurface);
	}

	Mesh CreateMesh(const Tesselation& Tesselation)
	{
		Mesh Result;

		Result.mId = GContext.mNextMeshId;
		Result.mVbo = llrm::CreateVertexBuffer(Tesselation.mVerts.size() * sizeof(MeshVertex), Tesselation.mVerts.data());
		Result.mIbo = llrm::CreateIndexBuffer(Tesselation.mIndicies.size() * sizeof(uint32_t), Tesselation.mIndicies.data());
		Result.mIndexCount = Tesselation.mIndicies.size();

		GContext.mNextMeshId++;
		GContext.mMeshes.emplace(Result.mId, Result);

		return Result;
	}

	void DestroyMesh(const Mesh& Mesh)
	{
		llrm::DestroyVertexBuffer(Mesh.mVbo);
		llrm::DestroyIndexBuffer(Mesh.mIbo);

		GContext.mMeshes.erase(Mesh.mId);
	}

	Mesh& GetMesh(uint32_t MeshId)
	{
		return GContext.mMeshes[MeshId];
	}

	Object CreateObject(const Mesh& Mesh, glm::vec3 Position)
	{
		Object Result;
		Result.mId = GContext.mNextMeshId;
		Result.mMeshId = Mesh.mId;
		Result.Position = Position;

		GContext.mNextMeshId++;
		GContext.mObjects.emplace(Result.mId, Result);

		return Result;
	}

	void DestroyObject(const Object& Object)
	{
		GContext.mObjects.erase(Object.mId);
		// Todo: Free up Object ID
	}

	Object& GetObject(uint32_t ObjectId)
	{
		return GContext.mObjects[ObjectId];
	}

	SceneId CreateScene()
	{
		Scene NewScene{};
		NewScene.mId = GContext.mNextSceneId;

		GContext.mNextSceneId++;
		GContext.mScenes.emplace(NewScene.mId, NewScene);

		return NewScene.mId;
	}

	void DestroyScene(SceneId Scene)
	{
		GContext.mScenes.erase(Scene);
	}

	void AddObject(SceneId Scene, const Object& Object)
	{
		GContext.mScenes[Scene].mObjects.insert(Object.mId);
	}

	void RemoveObject(SceneId Scene, const Object& Object)
	{
		GContext.mScenes[Scene].mObjects.erase(Object.mId);
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

	void InitSceneResources(const Scene& Scene)
	{
		// Create scene resources
		SceneResources NewResources{};

		GContext.mResources.emplace(Scene.mId, NewResources);
	}

	void RenderScene(const Scene& Scene, glm::ivec2 ViewportSize, const llrm::CommandBuffer& DstCmd, const llrm::FrameBuffer& DstBuf, const llrm::RenderGraph& DstGraph, const llrm::Pipeline& DstPipeline)
	{
		if(!GContext.mResources.contains(Scene.mId))
		{
			InitSceneResources(Scene);
		}

		SceneResources& Resources = GContext.mResources[Scene.mId];

		// Render the scene
		llrm::Begin(DstCmd);
		{
			std::vector<llrm::ClearValue> ClearValues = { {llrm::ClearType::Float, 0.0, 0.0, 0.0, 1.0f} };
			llrm::BeginRenderGraph(DstCmd, DstGraph, DstBuf, ClearValues);
			{
				// Geometry
				llrm::SetViewport(DstCmd, 0, 0, ViewportSize.x, ViewportSize.y);
				llrm::SetScissor(DstCmd, 0, 0, ViewportSize.x, ViewportSize.y);

				llrm::BindPipeline(DstCmd, DstPipeline);

				for(uint32_t Object : Scene.mObjects)
				{
					Ruby::Object& Obj = GetObject(Object);
					Ruby::Mesh& Mesh = GetMesh(Obj.mMeshId);

					llrm::DrawVertexBufferIndexed(DstCmd, Mesh.mVbo, Mesh.mIbo, Mesh.mIndexCount);
				}
			}
			llrm::EndRenderGraph(DstCmd);
		}
		llrm::End(DstCmd);
	}

	void RenderScene(SceneId Id, const SwapChain& Target)
	{
		Scene& ToRender = GContext.mScenes[Id];

		int32_t ImageIndex = llrm::BeginFrame(Target.mWnd, Target.mSwap, Target.mSurface);

		if(ImageIndex >= 0)
		{
			const llrm::FrameBuffer& Buffer = Target.mFrameBuffers[ImageIndex];
			const llrm::CommandBuffer& Cmd = Target.mCmdBuffers[ImageIndex];

			uint32_t Width{}, Height{};
			llrm::GetFrameBufferSize(Buffer, Width, Height);

			RenderScene(ToRender, {Width, Height}, Cmd, Buffer, Target.mGraph, Target.mPipeline);

			llrm::EndFrame({ Cmd });
		}
	}

}
