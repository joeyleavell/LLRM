#include "Ruby.h"

#include "llrm.h"
#include "GLFW/glfw3.h"
#include "Utill.h"
#include "ShaderManager.h"
#include "glm/vec2.hpp"
#include "Vertex.h"

namespace Ruby
{
	RubyContext GContext;

#if LLRM_VULKAN
	RenderingAPI GAPI = RenderingAPI::Vulkan;
#endif

	glm::mat4 CreateDirectionalVPMatrix(glm::vec3 Rotation)
	{
		glm::mat4 Proj = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 1.0f, 100.0f);
		glm::mat4 View = BuildTransform({ 5.0, 5.0, 0.0f }, Rotation, { 1.0f, 1.0f, 1.0f });

		return View * Proj;
	}

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
				{llrm::GetSwapChainImageView(Swap, Image)},
				Graph
				});

			if (!CmdBuffers[Image])
				CmdBuffers[Image] = llrm::CreateCommandBuffer();
		}
	}

	void CreateShadowMapResources()
	{
		GContext.mShadowMapRG = llrm::CreateRenderGraph({
			{{llrm::AttachmentUsage::DepthStencilAttachment, llrm::AttachmentUsage::ShaderRead, llrm::AttachmentFormat::D24_UNORM_S8_UINT}},
			{{{0}}}
		});

		GContext.mShadowMapPipe = llrm::CreatePipeline({
			LoadRasterShader("DepthRender", "DepthRender"),
			GContext.mShadowMapRG,
			{GContext.mLightObjectResourceLayout, GContext.mObjectResourceLayout},
			sizeof(MeshVertex),
			{
				{llrm::VertexAttributeFormat::Float3, offsetof(MeshVertex, mPosition)},
				{llrm::VertexAttributeFormat::Float3, offsetof(MeshVertex, mNormal)}
			},
			llrm::PipelineRenderPrimitive::TRIANGLES,
			{{false}},
			{true},
			0
		});
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

		// Create tonemap resource layout
		NewContext.mTonemapLayout = llrm::CreateResourceLayout({
			{},
			{
				{1, llrm::ShaderStage::Fragment, 1} // HDR
			},
			{
				{0, llrm::ShaderStage::Fragment, 1} // Sampler
			}
		});

		NewContext.mSceneResourceLayout = llrm::CreateResourceLayout({
{
		{0, llrm::ShaderStage::Vertex, sizeof(CameraVertexUniforms), 1}
		}, {} });

		NewContext.mLightsResourceLayout = llrm::CreateResourceLayout({
			{{0, llrm::ShaderStage::Fragment, sizeof(SceneLights), 1}},
			{{1, llrm::ShaderStage::Fragment, 1}},
			{}
		});

		NewContext.mObjectResourceLayout = llrm::CreateResourceLayout({
{
		{0, llrm::ShaderStage::Vertex, sizeof(ModelVertexUniforms), 1}
		}, {} });

		NewContext.mLightObjectResourceLayout = llrm::CreateResourceLayout({
{
		{0, llrm::ShaderStage::Vertex, sizeof(ModelVertexUniforms), 1}
		}, {} });

		NewContext.mDeferredShadeRl = llrm::CreateResourceLayout({
{}, {
			{1, llrm::ShaderStage::Fragment, 1}, // Albedo
			{2, llrm::ShaderStage::Fragment, 1}, // Position
			{3, llrm::ShaderStage::Fragment, 1}  // Normal
		},
		{
			{0, llrm::ShaderStage::Fragment, 1} // Albedo
		}
		});

		GContext = NewContext;

		CreateShadowMapResources();

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

			Swap.mTonemapGraph = llrm::CreateRenderGraph(Info);
		}

		Swap.mTonemapResources = llrm::CreateResourceSet({ GContext.mTonemapLayout});

		// Create final pipeline stage
		Swap.mTonemapPipeline = llrm::CreatePipeline({
			LoadRasterShader("Tonemap", "Tonemap"),
			Swap.mTonemapGraph,
			{GContext.mTonemapLayout},
			sizeof(PosUV),
			{
				{llrm::VertexAttributeFormat::Float2, offsetof(PosUV, mPos)},
				{llrm::VertexAttributeFormat::Float2, offsetof(PosUV, mUV)}
			},
			llrm::PipelineRenderPrimitive::TRIANGLES,
			{{false}},
			{false},
			0
		});

		// Create command buffers and frame buffers
		UpdateSwapChain(Swap.mSwap, Swap.mTonemapGraph, Swap.mFrameBuffers, Swap.mCmdBuffers);

		return Swap;
	}

	void DestroySwapChain(const SwapChain& Swap)
	{
		llrm::DestroySwapChain(Swap.mSwap);
		llrm::DestroySurface(Swap.mSurface);
	}

	LightId CreateLight(LightType Type, glm::vec3 Color, float Intensity)
	{
		Light Result;

		Result.mId = GContext.mNextLightId;
		Result.mType = Type;
		Result.mColor = Color;
		Result.mIntensity = Intensity;

		GContext.mNextLightId++;
		GContext.mLights.emplace(Result.mId, Result);

		return Result.mId;
	}

	void DestroyLight(LightId Light)
	{
		GContext.mLights.erase(Light);
	}

	Light& GetLight(LightId Light)
	{
		return GContext.mLights[Light];
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

	ObjectId CreateMeshObject(const Mesh& Mesh, glm::vec3 Position, glm::vec3 Rotation)
	{
		Object Result;
		Result.mType = ObjectType::Mesh;
		Result.mId = GContext.mNextMeshId;
		Result.mReferenceId = Mesh.mId;
		Result.mPosition = Position;
		Result.mRotation = Rotation;

		Result.mObjectResources = llrm::CreateResourceSet({ GContext.mObjectResourceLayout });

		GContext.mNextMeshId++;
		GContext.mObjects.emplace(Result.mId, Result);

		return Result.mId;
	}

	ObjectId CreateLightObject(LightId Light, glm::vec3 Position, glm::vec3 Rotation)
	{
		Object Result;
		Result.mType = ObjectType::Light;
		Result.mId = GContext.mNextObjectId;
		Result.mReferenceId = Light;
		Result.mPosition = Position;
		Result.mRotation = Rotation;

		Result.mObjectResources = llrm::CreateResourceSet({ GContext.mObjectResourceLayout });

		// Create shadow map
		Result.mShadowDepthAttachment = llrm::CreateTexture(llrm::AttachmentFormat::D24_UNORM_S8_UINT,
			llrm::AttachmentUsage::ShaderRead, 1024, 1024, llrm::TEXTURE_USAGE_RT | llrm::TEXTURE_USAGE_SAMPLE);
		Result.mShadowDepthAttachmentRenderPassView = llrm::CreateTextureView(Result.mShadowDepthAttachment, llrm::DEPTH_ASPECT | llrm::STENCIL_ASPECT);
		Result.mShadowDepthAttachmentResourceView	= llrm::CreateTextureView(Result.mShadowDepthAttachment, llrm::DEPTH_ASPECT);

		Result.mShadowFbo = llrm::CreateFrameBuffer({
			1024, 1024,
			{Result.mShadowDepthAttachmentRenderPassView},
			GContext.mShadowMapRG
		});

		GContext.mNextObjectId++;
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

	bool IsMeshObject(ObjectId Id)
	{
		return GContext.mObjects[Id].mType == ObjectType::Mesh;
	}

	bool IsLightObject(ObjectId Id)
	{
		return GContext.mObjects[Id].mType == ObjectType::Light;
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
		NewBuffer.ColorAttachment = llrm::CreateTexture(llrm::AttachmentFormat::B8G8R8A8_SRGB, llrm::AttachmentUsage::ColorAttachment, Width, Height, llrm::TEXTURE_USAGE_RT);
		NewBuffer.ColorAttachmentView = llrm::CreateTextureView(NewBuffer.ColorAttachment, llrm::AspectFlags::COLOR_ASPECT);

		if(DepthAttachment)
		{
			NewBuffer.DepthAttachment = llrm::CreateTexture(llrm::AttachmentFormat::D24_UNORM_S8_UINT, llrm::AttachmentUsage::DepthStencilAttachment, Width, Height, llrm::TEXTURE_USAGE_RT);
			NewBuffer.DepthAttachmentView = llrm::CreateTextureView(NewBuffer.DepthAttachment, llrm::AspectFlags::DEPTH_ASPECT);
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
			{NewBuffer.ColorAttachmentView},
			NewBuffer.Graph
		});

		return NewBuffer;
	}

	void DestroyFrameBuffer()
	{

	}

	void CreateResources(SceneResources& Res)
	{
		Res.mSceneResources = llrm::CreateResourceSet({ GContext.mSceneResourceLayout});
		Res.mLightResources = llrm::CreateResourceSet({ GContext.mLightsResourceLayout });
		Res.mDeferredShadeRes = llrm::CreateResourceSet({ GContext.mDeferredShadeRl });
	}

	void CreateDeferredResources(SceneResources& Res, glm::uvec2 Size)
	{
		// Create textures
		Res.mDeferredAlbedo   = llrm::CreateTexture(llrm::AttachmentFormat::RGBA16F_Float, llrm::AttachmentUsage::ShaderRead, Size.x, Size.y, llrm::TEXTURE_USAGE_SAMPLE | llrm::TEXTURE_USAGE_RT);
		Res.mDeferredPosition = llrm::CreateTexture(llrm::AttachmentFormat::RGBA16F_Float, llrm::AttachmentUsage::ShaderRead, Size.x, Size.y, llrm::TEXTURE_USAGE_SAMPLE | llrm::TEXTURE_USAGE_RT);
		Res.mDeferredNormal   = llrm::CreateTexture(llrm::AttachmentFormat::RGBA16F_Float, llrm::AttachmentUsage::ShaderRead, Size.x, Size.y, llrm::TEXTURE_USAGE_SAMPLE | llrm::TEXTURE_USAGE_RT);
		Res.mDeferredAlbedoView = llrm::CreateTextureView(Res.mDeferredAlbedo, llrm::AspectFlags::COLOR_ASPECT);
		Res.mDeferredPositionView = llrm::CreateTextureView(Res.mDeferredPosition, llrm::AspectFlags::COLOR_ASPECT);
		Res.mDeferredNormalView = llrm::CreateTextureView(Res.mDeferredNormal, llrm::AspectFlags::COLOR_ASPECT);

		// Create render graph
		Res.mDeferredGeoRG = llrm::CreateRenderGraph({
			{
				{llrm::AttachmentUsage::ColorAttachment, llrm::AttachmentUsage::ShaderRead, llrm::AttachmentFormat::RGBA16F_Float}, // Albedo
				{llrm::AttachmentUsage::ColorAttachment, llrm::AttachmentUsage::ShaderRead, llrm::AttachmentFormat::RGBA16F_Float}, // Position
				{llrm::AttachmentUsage::ColorAttachment, llrm::AttachmentUsage::ShaderRead, llrm::AttachmentFormat::RGBA16F_Float}, // Normal
				{llrm::AttachmentUsage::DepthStencilAttachment, llrm::AttachmentUsage::DepthStencilAttachment, llrm::AttachmentFormat::D24_UNORM_S8_UINT} // Depth
			}, 
			{{{0, 1, 2, 3}}}
		});

		Res.mDeferredShadeRG = llrm::CreateRenderGraph({
	{{llrm::AttachmentUsage::ColorAttachment, llrm::AttachmentUsage::ShaderRead, llrm::AttachmentFormat::RGBA16F_Float}}, // HDR buffer
	{{{0}}}
		});

		// Create pipeline
		Res.mDeferredGeoPipe= llrm::CreatePipeline({
			LoadRasterShader("DeferredGeometry", "DeferredGeometry"),
			Res.mDeferredGeoRG,
			{GContext.mSceneResourceLayout, GContext.mObjectResourceLayout},
			sizeof(MeshVertex),
			{
				{llrm::VertexAttributeFormat::Float3, offsetof(MeshVertex, mPosition)},
				{llrm::VertexAttributeFormat::Float3, offsetof(MeshVertex, mNormal)}
			},
			llrm::PipelineRenderPrimitive::TRIANGLES,
			{{false}, {false}, {false}},
			{true},
			0
		});

		Res.mDeferredShadePipe = llrm::CreatePipeline({
			LoadRasterShader("DeferredShade", "DeferredShade"),
			Res.mDeferredShadeRG,
			{GContext.mLightsResourceLayout, GContext.mDeferredShadeRl},
			sizeof(PosUV),
			{
				{llrm::VertexAttributeFormat::Float2, offsetof(PosUV, mPos)},
				{llrm::VertexAttributeFormat::Float2, offsetof(PosUV, mUV)}
			},
			llrm::PipelineRenderPrimitive::TRIANGLES,
			{{false}},
			{false},
			0
		});

		// Create frame buffer
		Res.mDeferredGeoFB = llrm::CreateFrameBuffer({
			Size.x, Size.y,
			{Res.mDeferredAlbedoView, Res.mDeferredPositionView, Res.mDeferredNormalView, Res.mDepthView},
			Res.mDeferredGeoRG
		});

		Res.mDeferredShadeFB = llrm::CreateFrameBuffer({
			Size.x, Size.y,
			{Res.mHDRColorView},
			Res.mDeferredShadeRG
		});
	}

	void CreateSamplers(SceneResources& Res)
	{
		Res.mNearestSampler = llrm::CreateSampler({ llrm::FilterType::NEAREST, llrm::FilterType::NEAREST });
	}

	void CreateFullScreenQuad(SceneResources& Res)
	{
		PosUV Verts[4] = {
			{{-1.0f, -1.0f}, {0.0f, 1.0f}},
			{{-1.0f, 1.0f}, {0.0f, 0.0f}},
			{{1.0f, 1.0f}, {1.0f, 0.0f}},
			{{1.0f, -1.0f}, {1.0f, 1.0f}},
		};

		uint32_t Index[6] = {
			2, 1, 0,
			0, 3, 2
		};

		Res.mFullScreenQuadVbo = llrm::CreateVertexBuffer(sizeof(Verts), Verts);
		Res.mFullScreenQuadIbo = llrm::CreateIndexBuffer(sizeof(Index), Index);
	}

	void InitSceneResources(const Scene& Scene, glm::uvec2 Size)
	{
		// Create scene resources
		SceneResources NewResources{};

		// Create color texture
		NewResources.mHDRColor	   = llrm::CreateTexture(llrm::AttachmentFormat::RGBA16F_Float, llrm::AttachmentUsage::ShaderRead, Size.x, Size.y, llrm::TEXTURE_USAGE_SAMPLE | llrm::TEXTURE_USAGE_RT);
		NewResources.mHDRColorView = llrm::CreateTextureView(NewResources.mHDRColor, llrm::AspectFlags::COLOR_ASPECT);

		// Create depth buffer
		NewResources.mDepth = llrm::CreateTexture(llrm::AttachmentFormat::D24_UNORM_S8_UINT, llrm::AttachmentUsage::DepthStencilAttachment, Size.x, Size.y, llrm::TEXTURE_USAGE_RT);
		NewResources.mDepthView = llrm::CreateTextureView(NewResources.mDepth, llrm::AspectFlags::DEPTH_ASPECT);

		CreateSamplers(NewResources);
		CreateFullScreenQuad(NewResources);
		CreateResources(NewResources);
		CreateDeferredResources(NewResources, Size);

		GContext.mResources.emplace(Scene.mId, NewResources);
	}

	void UpdateCameraUniforms(llrm::ResourceSet DstRes, const Camera& Camera)
	{
		glm::mat4 ViewMatrix = glm::inverse(BuildTransform(Camera.mPosition, Camera.mRotation, { 1, 1, 1 }));
			
		CameraVertexUniforms CamUniforms{
glm::transpose(ViewMatrix * Camera.mProjection)
		};
		llrm::UpdateUniformBuffer(DstRes, 0, &CamUniforms, sizeof(CamUniforms));
	}

	void RenderShadowMap(const Scene& Scene, const llrm::CommandBuffer& DstCmd,
		glm::uvec2 ShadowMapSize, const Object& Light
	)
	{
		Ruby::Light LightData = Ruby::GetLight(Light.mReferenceId);

		ShadowLightUniforms ShadowUniforms;
		if(LightData.mType == LightType::Directional)
		{
			ShadowUniforms.mViewProjection = CreateDirectionalVPMatrix(Light.mRotation);
		}

		// Update resources
		llrm::UpdateUniformBuffer(Light.mObjectResources, 0, &ShadowUniforms, sizeof(ShadowUniforms));

		// Transition depth attachment to correct usage
		llrm::TransitionTexture(DstCmd, Light.mShadowDepthAttachment, llrm::AttachmentUsage::ShaderRead, llrm::AttachmentUsage::DepthStencilAttachment);

		// Depth render
		std::vector<llrm::ClearValue> ClearValues = {
			{llrm::ClearType::Float, 1.0f}
		};
		llrm::BeginRenderGraph(DstCmd, GContext.mShadowMapRG, Light.mShadowFbo, ClearValues);
		{	
			llrm::SetViewport(DstCmd, 0, 0, ShadowMapSize.x, ShadowMapSize.y);
			llrm::SetScissor(DstCmd, 0, 0, ShadowMapSize.x, ShadowMapSize.y);

			llrm::BindPipeline(DstCmd, GContext.mShadowMapPipe);

			for (uint32_t Object : Scene.mObjects)
			{
				Ruby::Object& Obj = GetObject(Object);
				if (IsMeshObject(Obj.mId))
				{
					Ruby::Mesh& Mesh = GetMesh(Obj.mReferenceId);

					llrm::BindResources(DstCmd, { Light.mObjectResources, Obj.mObjectResources });
					llrm::DrawVertexBufferIndexed(DstCmd, Mesh.mVbo, Mesh.mIbo, Mesh.mIndexCount);
				}
			}
		}
		llrm::EndRenderGraph(DstCmd);

		//llrm::TransitionTexture(DstCmd, Light.mShadowDepthAttachment, llrm::AttachmentUsage::DepthStencilAttachment, llrm::AttachmentUsage::ShaderRead);
	}

	void RenderScene(const Scene& Scene, 
		glm::uvec2 ViewportSize, 
		const Camera& Camera,
		const llrm::CommandBuffer& DstCmd, 
		const llrm::FrameBuffer& DstBuf, 
		const llrm::RenderGraph& DstGraph, 
		const llrm::Pipeline& DstPipeline,
		const llrm::ResourceSet& DstResources
	)
	{
		if(!GContext.mResources.contains(Scene.mId))
		{
			InitSceneResources(Scene, ViewportSize);
		}

		SceneResources& Resources = GContext.mResources[Scene.mId];

		UpdateCameraUniforms(Resources.mSceneResources, Camera);

		// Accumulate lights for this frame
		SceneLights Lights;

		// Create model uniforms
		for (uint32_t Object : Scene.mObjects)
		{
			Ruby::Object& Obj = GetObject(Object);

			if(IsMeshObject(Obj.mId))
			{
				// Create transform matrix
				ModelVertexUniforms ModelUniforms{
					glm::transpose(BuildTransform(Obj.mPosition, Obj.mRotation, {1, 1, 1}))
				};
				llrm::UpdateUniformBuffer(Obj.mObjectResources, 0, &ModelUniforms, sizeof(ModelUniforms));
			}
			if(IsLightObject(Obj.mId))
			{
				const Light& Light = GContext.mLights[Obj.mReferenceId];
				if(Light.mType == LightType::Directional)
				{
					glm::vec4 Direction = BuildTransform({}, Obj.mRotation, { 1, 1, 1 }) * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);

					Lights.mDirectionalLights[Lights.mNumDirLights].mColor = Light.mColor;
					Lights.mDirectionalLights[Lights.mNumDirLights].mDirection = { Direction.x, Direction.y, Direction.z };
					Lights.mDirectionalLights[Lights.mNumDirLights].mIntensity = Light.mIntensity;
					Lights.mNumDirLights++;
				}
				else if(Light.mType == LightType::Spot)
				{
					glm::vec4 Direction = BuildTransform({}, Obj.mRotation, { 1, 1, 1 }) * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);

					Lights.mSpotLights[Lights.mNumSpotLights].mPosition = Obj.mPosition;
					Lights.mSpotLights[Lights.mNumSpotLights].mColor = Light.mColor;
					Lights.mSpotLights[Lights.mNumSpotLights].mDirection = { Direction.x, Direction.y, Direction.z };
					Lights.mSpotLights[Lights.mNumSpotLights].mIntensity = Light.mIntensity;
					Lights.mNumSpotLights++;
				}
			}
		}

		// Gather shadow maps
		std::vector<llrm::TextureView> ShadowMaps;
		for (uint32_t Object : Scene.mObjects)
		{
			Ruby::Object& Obj = GetObject(Object);
			if (IsLightObject(Obj.mId))
			{
				ShadowMaps.push_back(Obj.mShadowDepthAttachmentResourceView);
			}
		}

		// Update lights
		llrm::UpdateUniformBuffer(Resources.mLightResources, 0, &Lights, sizeof(Lights));
		llrm::UpdateTextureResource(Resources.mLightResources, ShadowMaps, 1);

		// Update tonemap inputs
		llrm::UpdateSamplerResource(DstResources, Resources.mNearestSampler, 0);
		llrm::UpdateTextureResource(DstResources, { Resources.mHDRColorView }, 1);

		// Update deferred shade inputs
		llrm::UpdateSamplerResource(Resources.mDeferredShadeRes, Resources.mNearestSampler, 0);
		llrm::UpdateTextureResource(Resources.mDeferredShadeRes, { Resources.mDeferredAlbedoView }, 1);
		llrm::UpdateTextureResource(Resources.mDeferredShadeRes, { Resources.mDeferredPositionView }, 2);
		llrm::UpdateTextureResource(Resources.mDeferredShadeRes, { Resources.mDeferredNormalView }, 3);

		// Render the scene
		llrm::Begin(DstCmd);
		{

			// Render shadow maps
			for (uint32_t Object : Scene.mObjects)
			{
				Ruby::Object& Obj = GetObject(Object);
				if (IsLightObject(Obj.mId))
				{
					RenderShadowMap(Scene, DstCmd, { 1024, 1024 }, Obj);
				}
			}
			
			llrm::TransitionTexture(DstCmd, Resources.mDeferredAlbedo, llrm::AttachmentUsage::ShaderRead, llrm::AttachmentUsage::ColorAttachment);
			llrm::TransitionTexture(DstCmd, Resources.mDeferredPosition, llrm::AttachmentUsage::ShaderRead, llrm::AttachmentUsage::ColorAttachment);
			llrm::TransitionTexture(DstCmd, Resources.mDeferredNormal, llrm::AttachmentUsage::ShaderRead, llrm::AttachmentUsage::ColorAttachment);

			// Deferred geometry stage
			std::vector<llrm::ClearValue> ClearValues = {
				{llrm::ClearType::Float, 0.0, 0.0, 0.0, 1.0f},
				{llrm::ClearType::Float, 0.0, 0.0, 0.0, 0.0f},
				{llrm::ClearType::Float, 0.0, 0.0, 0.0, 0.0f},
				{llrm::ClearType::Float, 1.0f}
			};
			llrm::BeginRenderGraph(DstCmd, Resources.mDeferredGeoRG, Resources.mDeferredGeoFB, ClearValues);
			{
				llrm::SetViewport(DstCmd, 0, 0, ViewportSize.x, ViewportSize.y);
				llrm::SetScissor(DstCmd, 0, 0, ViewportSize.x, ViewportSize.y);

				llrm::BindPipeline(DstCmd, Resources.mDeferredGeoPipe);

				for(uint32_t Object : Scene.mObjects)
				{
					Ruby::Object& Obj = GetObject(Object);
					if(IsMeshObject(Obj.mId))
					{
						Ruby::Mesh& Mesh = GetMesh(Obj.mReferenceId);

						llrm::BindResources(DstCmd, { Resources.mSceneResources, Obj.mObjectResources });
						llrm::DrawVertexBufferIndexed(DstCmd, Mesh.mVbo, Mesh.mIbo, Mesh.mIndexCount);
					}
				}
			}
			llrm::EndRenderGraph(DstCmd);

			// Deferred shade stage

			// Update deferred inputs
			ClearValues = {
				{llrm::ClearType::Float, 0.0, 0.0, 0.0, 1.0f},
			};
			llrm::TransitionTexture(DstCmd, Resources.mHDRColor, llrm::AttachmentUsage::ShaderRead, llrm::AttachmentUsage::ColorAttachment);
			llrm::BeginRenderGraph(DstCmd, Resources.mDeferredShadeRG, Resources.mDeferredShadeFB, ClearValues);
			{
				llrm::SetViewport(DstCmd, 0, 0, ViewportSize.x, ViewportSize.y);
				llrm::SetScissor(DstCmd, 0, 0, ViewportSize.x, ViewportSize.y);

				llrm::BindPipeline(DstCmd, Resources.mDeferredShadePipe);
				llrm::BindResources(DstCmd, { Resources.mLightResources, Resources.mDeferredShadeRes });
				llrm::DrawVertexBufferIndexed(DstCmd, Resources.mFullScreenQuadVbo, Resources.mFullScreenQuadIbo, 6);
			}
			llrm::EndRenderGraph(DstCmd);

			// Tonemap stage
			llrm::BeginRenderGraph(DstCmd, DstGraph, DstBuf, ClearValues);
			{
				llrm::SetViewport(DstCmd, 0, 0, ViewportSize.x, ViewportSize.y);
				llrm::SetScissor(DstCmd, 0, 0, ViewportSize.x, ViewportSize.y);

				llrm::BindPipeline(DstCmd, DstPipeline);
				llrm::BindResources(DstCmd, { DstResources });
				llrm::DrawVertexBufferIndexed(DstCmd, Resources.mFullScreenQuadVbo, Resources.mFullScreenQuadIbo, 6);
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

			llrm::Reset(Cmd);
			RenderScene(ToRender, 
				ViewportSize, 
				Camera, 
				Cmd, 
				Buffer, 
				Target.mTonemapGraph, 
				Target.mTonemapPipeline,
				Target.mTonemapResources
			);

			llrm::EndFrame({ Cmd });
		}
	}

}
