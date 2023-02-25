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

		if (!glfwInit())
			return {}; // Error

		NewContext.LLContext = llrm::CreateContext();

		Ruby::InitShaderCompilation();

		NewContext.mDeferredGeometryRl = llrm::CreateResourceLayout({
			{
				{0, llrm::ShaderStage::Vertex, sizeof(CameraVertexUniforms), 1},
				{1, llrm::ShaderStage::Vertex, sizeof(ModelVertexUniforms), 1},
		}, {}});

		GContext = NewContext;

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

		Swap.mResources = llrm::CreateResourceSet({ Swap.mSwap, GContext.mDeferredGeometryRl });

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

	ObjectId CreateObject(const Mesh& Mesh, glm::vec3 Position, glm::vec3 Rotation)
	{
		Object Result;
		Result.mId = GContext.mNextMeshId;
		Result.mMeshId = Mesh.mId;
		Result.mPosition = Position;
		Result.mRotation = Rotation;

		GContext.mNextMeshId++;
		GContext.mObjects.emplace(Result.mId, Result);

		return Result.mId;
	}

	void DestroyObject(ObjectId Id)
	{
		GContext.mObjects.erase(Id);
		// Todo: Free up Object ID
	}

	Object& GetObject(ObjectId ObjectId)
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

	void AddObject(SceneId Scene, ObjectId Object)
	{
		GContext.mScenes[Scene].mObjects.insert(Object);
	}

	void RemoveObject(SceneId Scene, ObjectId Object)
	{
		GContext.mScenes[Scene].mObjects.erase(Object);
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

	void UpdateCameraUniforms(llrm::ResourceSet DstRes, llrm::SwapChain DstSwap, const Camera& Camera)
	{
		glm::mat4 ViewMatrix = glm::inverse(BuildTransform(Camera.mPosition, Camera.mRotation, { 1, 1, 1 }));
			
		CameraVertexUniforms CamUniforms{
glm::transpose(ViewMatrix * Camera.mProjection)
		};
		llrm::UpdateUniformBuffer(DstRes, DstSwap, 0, &CamUniforms, sizeof(CamUniforms));
	}

	void RenderScene(const Scene& Scene, 
		glm::ivec2 ViewportSize, 
		const Camera& Camera,
		const llrm::CommandBuffer& DstCmd, 
		const llrm::FrameBuffer& DstBuf, 
		const llrm::RenderGraph& DstGraph, 
		const llrm::Pipeline& DstPipeline,
		const llrm::ResourceSet& DstResources,
		const llrm::SwapChain& DstSwap
	)
	{
		if(!GContext.mResources.contains(Scene.mId))
		{
			InitSceneResources(Scene);
		}

		SceneResources& Resources = GContext.mResources[Scene.mId];

		UpdateCameraUniforms(DstResources, DstSwap, Camera);

		// Create model uniforms
		for (uint32_t Object : Scene.mObjects)
		{
			Ruby::Object& Obj = GetObject(Object);

			// Create transform matrix
			ModelVertexUniforms ModelUniforms {
				glm::transpose(BuildTransform(Obj.mPosition, Obj.mRotation, {1, 1, 1}))
			};
			llrm::UpdateUniformBuffer(DstResources, DstSwap, 1, &ModelUniforms, sizeof(ModelUniforms));
		}

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
				llrm::BindResources(DstCmd, DstResources);

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

	void RenderScene(SceneId Id, 
		glm::ivec2 ViewportSize, 
		const Camera& Camera,
		const SwapChain& Target
	)
	{
		Scene& ToRender = GContext.mScenes[Id];

		int32_t ImageIndex = llrm::BeginFrame(Target.mWnd, Target.mSwap, Target.mSurface);

		if(ImageIndex >= 0)
		{
			const llrm::FrameBuffer& Buffer = Target.mFrameBuffers[ImageIndex];
			const llrm::CommandBuffer& Cmd = Target.mCmdBuffers[ImageIndex];

			RenderScene(ToRender, 
				ViewportSize, 
				Camera, 
				Cmd, 
				Buffer, 
				Target.mGraph, 
				Target.mPipeline,
				Target.mResources,
				Target.mSwap
			);

			llrm::EndFrame({ Cmd });
		}
	}

}
