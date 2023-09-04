#include "Ruby.h"

#include "llrm.h"
#include "GLFW/glfw3.h"
#include "Utill.h"
#include "ShaderManager.h"
#include "glm/vec2.hpp"
#include "Vertex.h"

#define SHADOW_MAP_RESOLUTION uint32_t(1024)

namespace Ruby
{
	RubyContext GContext;

	// Must define RUBY_UNITS
#ifndef RUBY_UNITS
	#error Must define RUBY_UNITS, see documentation for details. (0=meters, 1=centimeters)
#endif

	// The units of measurement
	const DistanceUnits gUnits =
#if RUBY_UNITS == 0
	DistanceUnits::Meters;
	const float TO_METERS = 1;
#elif RUBY_UNITS == 1
	DistanceUnits::Centimeters;
	const float TO_METERS = 1.0f / 100;
#endif

	// Converts meters to user-specified units
	constexpr float ToUser(float Distance)
	{
		return Distance * 1.0f / TO_METERS;
	}

	// Converts user-specified units to meters
	constexpr float ToMeters(float Distance)
	{
		return Distance * TO_METERS;
	}

#if LLRM_VULKAN
	RenderingAPI GAPI = RenderingAPI::Vulkan;
#endif


	struct BoundingBox {
		glm::vec3 min;
		glm::vec3 max;
	};

	BoundingBox CalculateViewFrustumBoundingBox(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) {

		// Starts in NDC
		std::vector<glm::vec4> Corners = {
			{-1.0f, -1.0f, -1.0f, 1.0f},
			{-1.0f, 1.0f, -1.0f, 1.0f},
			{1.0f, 1.0f, -1.0f, 1.0f},
			{1.0f, -1.0f, -1.0f, 1.0f},
			{-1.0f, -1.0f, 1.0f, 1.0f},
			{-1.0f, 1.0f, 1.0f, 1.0f},
			{1.0f, 1.0f, 1.0f, 1.0f},
			{1.0f, -1.0f, 1.0f, 1.0f},
		};

		// Clip Space -> View space
		glm::mat4 InvViewProjection = glm::inverse(projectionMatrix * viewMatrix);

		BoundingBox Result = {
			glm::vec3(std::numeric_limits<float>::max()),
			glm::vec3(std::numeric_limits<float>::lowest()),
		};

		for(uint32_t Index = 0; Index < 8; Index++)
		{
			glm::vec4 Corner = InvViewProjection * Corners[Index];
			Corner /= Corner.w;

			// Find min/max
			Result.max = glm::max(Result.max, glm::vec3(Corner));
			Result.min = glm::min(Result.min, glm::vec3(Corner));
		}

		return Result;

	}

	void CalculateDirectionalLightMatrices(const glm::vec3& LightRotation, const BoundingBox& boundingBox, glm::mat4& viewMatrix, glm::mat4& projectionMatrix)
	{
		// Calculate the center point of the bounding box
		glm::vec3 center = (boundingBox.min + boundingBox.max) * 0.5f;

		// Define the distance from the center to the bounding box extents
		float distance = glm::distance(boundingBox.min, boundingBox.max) * 0.5f;

		// Set the up direction for the light
		glm::vec3 up(0.0f, 1.0f, 0.0f);

		// Calculate the view matrix for the directional light
		glm::mat4 RotMat = BuildTransform(glm::vec3(0.0f), LightRotation, glm::vec3(1.0f));
		glm::vec4 Forward(0.0f, 0.0f, -1.0f, 0.0f);
		glm::vec4 LightDirection = RotMat * Forward;
		viewMatrix = glm::inverse(BuildTransform(center - glm::vec3(LightDirection) * distance, LightRotation, glm::vec3(1.0f)));

		// Calculate the dimensions of the orthographic frustum
		float width = boundingBox.max.x - boundingBox.min.x;
		float height = boundingBox.max.y - boundingBox.min.y;
		float depth = glm::distance(boundingBox.min, boundingBox.max);

		// Calculate the projection matrix for the directional light using an orthographic frustum
		projectionMatrix = glm::orthoRH_ZO(-width * 0.5f, width * 0.5f, -height * 0.5f, height * 0.5f, 0.0f, depth);
	}

	glm::mat4 CreateDirectionalVPMatrix(glm::vec3 Rotation, const glm::mat4& camView, const glm::mat4& camProj)
	{
		//glm::mat4 Proj = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 1.0f, 100.0f);
		//glm::mat4 View = BuildTransform({ 5.0, 5.0, 0.0f }, Rotation, { 1.0f, 1.0f, 1.0f });

		glm::mat4 Proj;
		glm::mat4 View;

		BoundingBox FrustumBounds = CalculateViewFrustumBoundingBox(camView, camProj);
		CalculateDirectionalLightMatrices(Rotation, FrustumBounds, View, Proj);

		return glm::transpose(Proj * View);
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

	llrm::Pipeline RubyContext::DeferredShadePipeline(bool UseShadows)
	{
		DeferredShadeParameters Params = { UseShadows };
		if (mDeferredShadePipelines.contains(Params))
		{
			return mDeferredShadePipelines[Params];
		}
		else
		{
			// Create string
			std::string UberFrag = "DeferredShade_" + std::to_string(uint32_t(UseShadows));
			llrm::Pipeline NewPipe = llrm::CreatePipeline({
				LoadRasterShader("DeferredShade", UberFrag),
				GContext.mDeferredShadeRG,
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

			mDeferredShadePipelines[Params] = NewPipe;

			return NewPipe;
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
			{},
			{
				{0, llrm::ShaderStage::Fragment, 1}, // Light data texture
				{1, llrm::ShaderStage::Fragment, 1}, // Shadow map texture array
				{2, llrm::ShaderStage::Fragment, 1} // Shadow map frustum data texture
			},
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
{
				{5, llrm::ShaderStage::Fragment, sizeof(DeferredShadeResources), 1}
			},
			{
				{1, llrm::ShaderStage::Fragment, 1}, // Albedo
				{2, llrm::ShaderStage::Fragment, 1}, // Position
				{3, llrm::ShaderStage::Fragment, 1}, // Normal
				{4, llrm::ShaderStage::Fragment, 1}  // Roughness, metallic, ambient occlusion, unused
			},
			{
				{0, llrm::ShaderStage::Fragment, 1} // Albedo
			}
		});

		NewContext.mMaterialLayout = llrm::CreateResourceLayout({{
				{0, llrm::ShaderStage::Fragment, sizeof(ShaderUniform_Material), 1}
			},
			{},
			{}
		});

		// Create default material
		ShaderUniform_Material DefaultMaterial = { 0.8f, 0.0f, glm::vec3(0.18f) };
		NewContext.mDefaultMaterial = llrm::CreateResourceSet({ NewContext.mMaterialLayout });
		llrm::UpdateUniformBuffer(NewContext.mDefaultMaterial, 0, &DefaultMaterial, sizeof(DefaultMaterial), false);

		// Create render graphs
		NewContext.mDeferredGeoRG = llrm::CreateRenderGraph({
			{
				{llrm::AttachmentUsage::ColorAttachment, llrm::AttachmentUsage::ShaderRead, llrm::AttachmentFormat::RGBA16F_Float}, // Albedo
				{llrm::AttachmentUsage::ColorAttachment, llrm::AttachmentUsage::ShaderRead, llrm::AttachmentFormat::RGBA16F_Float}, // Position
				{llrm::AttachmentUsage::ColorAttachment, llrm::AttachmentUsage::ShaderRead, llrm::AttachmentFormat::RGBA16F_Float}, // Normal
				{llrm::AttachmentUsage::ColorAttachment, llrm::AttachmentUsage::ShaderRead, llrm::AttachmentFormat::RGBA16F_Float}, // Roughness, metallic, ambient occlusion, unused
				{llrm::AttachmentUsage::DepthStencilAttachment, llrm::AttachmentUsage::DepthStencilAttachment, llrm::AttachmentFormat::D24_UNORM_S8_UINT} // Depth
			},
			{{{0, 1, 2, 3, 4}}}
		});
		NewContext.mDeferredShadeRG = llrm::CreateRenderGraph({
			{{llrm::AttachmentUsage::ColorAttachment, llrm::AttachmentUsage::ShaderRead, llrm::AttachmentFormat::RGBA16F_Float}}, // HDR buffer
			{{{0}}}
		});
		NewContext.mShadowMapRG = llrm::CreateRenderGraph({
			{{llrm::AttachmentUsage::DepthStencilAttachment, llrm::AttachmentUsage::ShaderRead, llrm::AttachmentFormat::D24_UNORM_S8_UINT}},
			{{{0}}}
		});

		// Set global compiled shaders location so we can load shaders
		GContext.CompiledShaders = NewContext.CompiledShaders;

		// Create pipelines
		NewContext.mDeferredGeoPipe = llrm::CreatePipeline({
			LoadRasterShader("DeferredGeometry", "DeferredGeometry"),
			NewContext.mDeferredGeoRG,
			{NewContext.mSceneResourceLayout, NewContext.mObjectResourceLayout, NewContext.mMaterialLayout},
			sizeof(MeshVertex),
			{
				{llrm::VertexAttributeFormat::Float3, offsetof(MeshVertex, mPosition)},
				{llrm::VertexAttributeFormat::Float3, offsetof(MeshVertex, mNormal)}
			},
			llrm::PipelineRenderPrimitive::TRIANGLES,
			{{false}, {false}, {false}, {false}},
			{true},
			0
		});

		NewContext.mShadowMapPipe = llrm::CreatePipeline({
			LoadRasterShader("DepthRender", "DepthRender"),
			NewContext.mShadowMapRG,
			{NewContext.mLightObjectResourceLayout, NewContext.mObjectResourceLayout},
			sizeof(MeshVertex),
			{
				{llrm::VertexAttributeFormat::Float3, offsetof(MeshVertex, mPosition)},
				{llrm::VertexAttributeFormat::Float3, offsetof(MeshVertex, mNormal)}
			},
			llrm::PipelineRenderPrimitive::TRIANGLES,
			{{false}},
			{true},
			0,
			llrm::VertexWinding::CounterClockwise,
			llrm::CullMode::Front
		});

		GContext = NewContext;

		CompileRasterProgram("DeferredShade", "DeferredShade", 
			{
			}, 
			{
			{"UBER_SHADOWS", 0, 1}
			}
		);

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

	void CreateSwapResources(SwapChain& Swap, uint32_t Width, uint32_t Height)
	{

		// Create llrm::SwapChain
		{
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

		Swap.mTonemapResources = llrm::CreateResourceSet({ GContext.mTonemapLayout });

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
	}

	SwapChain CreateSwapChain(GLFWwindow* Wnd)
	{
		SwapChain Swap{};

		// Create surface and set mWnd
		{
			Swap.mSurface = llrm::CreateSurface(Wnd);
			Swap.mWnd = Wnd;
		}

		int Width, Height;
		glfwGetFramebufferSize(Wnd, &Width, &Height);

		CreateSwapResources(Swap, Width, Height);

		// Create command buffers and frame buffers
		UpdateSwapChain(Swap.mSwap, Swap.mTonemapGraph, Swap.mFrameBuffers, Swap.mCmdBuffers);

		return Swap;
	}

	void DestroySwapChain(const SwapChain& Swap)
	{
		llrm::DestroySwapChain(Swap.mSwap);
		llrm::DestroySurface(Swap.mSurface);
	}

	void ResizeSwapChain(SwapChain& Swap, uint32_t NewWidth, uint32_t NewHeight)
	{
		llrm::DestroySwapChain(Swap.mSwap);
		llrm::DestroyRenderGraph(Swap.mTonemapGraph);
		llrm::DestroyPipeline(Swap.mTonemapPipeline);
		llrm::DestroyResourceSet(Swap.mTonemapResources);

		// Re-create swap resources
		CreateSwapResources(Swap, NewWidth, NewHeight);

		// Recreate frame buffers
		UpdateSwapChain(Swap.mSwap, Swap.mTonemapGraph, Swap.mFrameBuffers, Swap.mCmdBuffers);
	}

	RenderTarget CreateRenderTarget(uint32_t Width, uint32_t Height)
	{
		RenderTarget Target;

		// Create scene color
		Target.mHDRColor = llrm::CreateTexture(llrm::AttachmentFormat::RGBA16F_Float, llrm::AttachmentUsage::ShaderRead, Width, Height, llrm::TEXTURE_USAGE_SAMPLE | llrm::TEXTURE_USAGE_RT, 1);
		Target.mHDRColorView = llrm::CreateTextureView(Target.mHDRColor, llrm::AspectFlags::COLOR_ASPECT);

		// Create scene depth
		Target.mDepth = llrm::CreateTexture(llrm::AttachmentFormat::D24_UNORM_S8_UINT, llrm::AttachmentUsage::DepthStencilAttachment, Width, Height, llrm::TEXTURE_USAGE_RT, 1);
		Target.mDepthView = llrm::CreateTextureView(Target.mDepth, llrm::AspectFlags::DEPTH_ASPECT);

		// Create deferred geometry textures
		Target.mDeferredAlbedo = llrm::CreateTexture(llrm::AttachmentFormat::RGBA16F_Float, llrm::AttachmentUsage::ShaderRead, Width, Height, llrm::TEXTURE_USAGE_SAMPLE | llrm::TEXTURE_USAGE_RT, 1);
		Target.mDeferredPosition = llrm::CreateTexture(llrm::AttachmentFormat::RGBA16F_Float, llrm::AttachmentUsage::ShaderRead, Width, Height, llrm::TEXTURE_USAGE_SAMPLE | llrm::TEXTURE_USAGE_RT, 1);
		Target.mDeferredNormal = llrm::CreateTexture(llrm::AttachmentFormat::RGBA16F_Float, llrm::AttachmentUsage::ShaderRead, Width, Height, llrm::TEXTURE_USAGE_SAMPLE | llrm::TEXTURE_USAGE_RT, 1);
		Target.mDeferredRMAO = llrm::CreateTexture(llrm::AttachmentFormat::RGBA16F_Float, llrm::AttachmentUsage::ShaderRead, Width, Height, llrm::TEXTURE_USAGE_SAMPLE | llrm::TEXTURE_USAGE_RT, 1);
		Target.mDeferredAlbedoView = llrm::CreateTextureView(Target.mDeferredAlbedo, llrm::AspectFlags::COLOR_ASPECT);
		Target.mDeferredPositionView = llrm::CreateTextureView(Target.mDeferredPosition, llrm::AspectFlags::COLOR_ASPECT);
		Target.mDeferredNormalView = llrm::CreateTextureView(Target.mDeferredNormal, llrm::AspectFlags::COLOR_ASPECT);
		Target.mDeferredRMAOView = llrm::CreateTextureView(Target.mDeferredRMAO, llrm::AspectFlags::COLOR_ASPECT);

		// Create deferred framebuffers
		Target.mDeferredGeoFB = llrm::CreateFrameBuffer({
			Width, Height,
			{Target.mDeferredAlbedoView, Target.mDeferredPositionView, Target.mDeferredNormalView, Target.mDeferredRMAOView, Target.mDepthView},
			GContext.mDeferredGeoRG
		});

		Target.mDeferredShadeFB = llrm::CreateFrameBuffer({
			Width, Height,
			{Target.mHDRColorView},
			GContext.mDeferredShadeRG
		});

		return Target;
	}

	void DestroyRenderTarget(RenderTarget& Target)
	{
		llrm::DestroyTexture(Target.mHDRColor);
		llrm::DestroyTexture(Target.mDepth);
		llrm::DestroyTexture(Target.mDeferredAlbedo);
		llrm::DestroyTexture(Target.mDeferredPosition);
		llrm::DestroyTexture(Target.mDeferredNormal);
		llrm::DestroyTexture(Target.mDeferredRMAO);

		llrm::DestroyFrameBuffer(Target.mDeferredGeoFB);
		llrm::DestroyFrameBuffer(Target.mDeferredShadeFB);

		llrm::DestroyTextureView(Target.mDepthView);
		llrm::DestroyTextureView(Target.mHDRColorView);
		llrm::DestroyTextureView(Target.mDeferredAlbedoView);
		llrm::DestroyTextureView(Target.mDeferredPositionView);
		llrm::DestroyTextureView(Target.mDeferredNormalView);
		llrm::DestroyTextureView(Target.mDeferredRMAOView);

		Target.mHDRColor = nullptr;
		Target.mDepth = nullptr;
		Target.mDeferredAlbedo = nullptr;
		Target.mDeferredPosition = nullptr;
		Target.mDeferredNormal = nullptr;
		Target.mDeferredRMAO = nullptr;

		Target.mDeferredGeoFB = nullptr;
		Target.mDeferredShadeFB = nullptr;

		Target.mDepthView = nullptr;
		Target.mHDRColorView = nullptr;
		Target.mDeferredAlbedoView = nullptr;
		Target.mDeferredPositionView = nullptr;
		Target.mDeferredNormalView = nullptr;
		Target.mDeferredRMAOView = nullptr;
	}

	void ResizeRenderTarget(RenderTarget& Target, uint32_t NewWidth, uint32_t NewHeight)
	{
		DestroyRenderTarget(Target);
		Target = CreateRenderTarget(NewWidth, NewHeight);
	}

	Light& CreateLight(LightType Type, glm::vec3 Color, float Intensity, LightUnit Units, bool CastShadows)
	{
		Light Result;

		Result.mId = GContext.mNextLightId;
		Result.mType = Type;
		Result.mColor = Color;
		Result.mIntensity = Intensity;
		Result.mIntensityUnit = Units;
		Result.mCastShadows = CastShadows;

		GContext.mNextLightId++;
		GContext.mLights.emplace(Result.mId, Result);

		return GContext.mLights[Result.mId];
	}

	Light& CreateSpotLight(
		glm::vec3 Color, 
		float Intensity, 
		LightUnit Units,
		float InnerAngle, float OuterAngle, 
		bool CastShadows
	)
	{
		Light& Light = CreateLight(LightType::Spot, Color, Intensity, Units, CastShadows);
		Light.InnerAngle = glm::radians(InnerAngle);
		Light.OuterAngle = glm::radians(OuterAngle);

		return Light;
	}

	void DestroyLight(LightId Light)
	{
		GContext.mLights.erase(Light);
	}

	Light& GetLight(LightId Light)
	{
		return GContext.mLights[Light];
	}

	Mesh& CreateMesh(const Tesselation& Tesselation, uint32_t MatId)
	{
		Mesh Result;

		Result.mId = GContext.mNextMeshId;
		Result.mVbo = llrm::CreateVertexBuffer(Tesselation.mVerts.size() * sizeof(MeshVertex), Tesselation.mVerts.data());
		Result.mIbo = llrm::CreateIndexBuffer(Tesselation.mIndicies.size() * sizeof(uint32_t), Tesselation.mIndicies.data());
		Result.mIndexCount = Tesselation.mIndicies.size();
		Result.mMat = MatId;

		GContext.mNextMeshId++;
		GContext.mMeshes.emplace(Result.mId, Result);

		return GContext.mMeshes[Result.mId];
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

	Material& CreateMaterial()
	{
		Material Result;

		Result.mId = GContext.mNextMaterialId;
		Result.mMaterialResources = llrm::CreateResourceSet({
			GContext.mMaterialLayout
		});

		GContext.mNextMaterialId++;
		GContext.mMaterials.emplace(Result.mId, Result);

		return GContext.mMaterials[Result.mId];
	}

	void DestroyMaterial(const Material& Material)
	{
		llrm::DestroyVertexBuffer(Material.mMaterialResources);
		GContext.mMaterials.erase(Material.mId);
	}

	Material& GetMaterial(uint32_t MaterialId)
	{
		return GContext.mMaterials[MaterialId];
	}

	ObjectId CreateMeshObject(const Mesh& Mesh, glm::vec3 Position, glm::vec3 Rotation)
	{
		Object Result;
		Result.mType = ObjectType::Mesh;
		Result.mId = GContext.mNextObjectId;
		Result.mReferenceId = Mesh.mId;
		Result.mPosition = Position;
		Result.mRotation = Rotation;

		Result.mObjectResources = llrm::CreateResourceSet({ GContext.mObjectResourceLayout });

		GContext.mNextObjectId++;
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
		NewBuffer.ColorAttachment = llrm::CreateTexture(llrm::AttachmentFormat::B8G8R8A8_SRGB, llrm::AttachmentUsage::ColorAttachment, Width, Height, llrm::TEXTURE_USAGE_RT, 1);
		NewBuffer.ColorAttachmentView = llrm::CreateTextureView(NewBuffer.ColorAttachment, llrm::AspectFlags::COLOR_ASPECT);

		if(DepthAttachment)
		{
			NewBuffer.DepthAttachment = llrm::CreateTexture(llrm::AttachmentFormat::D24_UNORM_S8_UINT, llrm::AttachmentUsage::DepthStencilAttachment, Width, Height, llrm::TEXTURE_USAGE_RT, 1);
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

	}

	void CreateLightDataResources(SceneResources& Res)
	{
		uint32_t MaxSize = llrm::GetCaps().MaxTextureSize;
		Res.mLightDataTexture = llrm::CreateTexture(llrm::AttachmentFormat::RGBA32F_Float, llrm::AttachmentUsage::ShaderRead,
			MaxSize, 1,
			llrm::TEXTURE_USAGE_SAMPLE | llrm::TEXTURE_USAGE_WRITE, 1);
		Res.mLightDataTextureView = llrm::CreateTextureView(Res.mLightDataTexture, llrm::AspectFlags::COLOR_ASPECT, llrm::TextureViewType::TYPE_2D);
	}

	void CreateShadowResources(SceneResources& Res)
	{
		uint32_t Frustums = std::min(llrm::GetCaps().MaxImageArrayLayers, (uint32_t)10);
		uint32_t Size = Frustums * 4;
		Res.mShadowMapFrustums = llrm::CreateTexture(llrm::AttachmentFormat::RGBA32F_Float, 
			llrm::AttachmentUsage::ShaderRead, 
			Size, 1, 
			llrm::TEXTURE_USAGE_SAMPLE | llrm::TEXTURE_USAGE_WRITE,
			1
		);
		Res.mShadowMapFrustumsView = llrm::CreateTextureView(Res.mShadowMapFrustums, llrm::AspectFlags::COLOR_ASPECT);

		Res.mShadowMaps = llrm::CreateTexture(llrm::AttachmentFormat::D24_UNORM_S8_UINT,
			llrm::AttachmentUsage::ShaderRead, 
			SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION,
			llrm::TEXTURE_USAGE_RT | llrm::TEXTURE_USAGE_SAMPLE,
			Frustums
		);

		Res.mShadowMapsResourceView = llrm::CreateTextureView(Res.mShadowMaps,
			llrm::AspectFlags::DEPTH_ASPECT,
			llrm::TextureViewType::TYPE_2D_ARRAY,
			0, Frustums);

		for(uint32_t Frustum = 0; Frustum < Frustums; Frustum++)
		{
			llrm::TextureView ShadowMapTextureView = llrm::CreateTextureView(Res.mShadowMaps,
				llrm::AspectFlags::DEPTH_ASPECT | llrm::AspectFlags::STENCIL_ASPECT,
				llrm::TextureViewType::TYPE_2D_ARRAY,
				Frustum, 1);

			llrm::FrameBuffer ShadowMapFbo = llrm::CreateFrameBuffer({
				SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION,
				{ShadowMapTextureView},
				GContext.mShadowMapRG
			});

			Res.mShadowMapAttachmentViews.push_back(ShadowMapTextureView);
			Res.mShadowMapFbos.push_back(ShadowMapFbo);
		}

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

		CreateSamplers(NewResources);
		CreateFullScreenQuad(NewResources);
		CreateResources(NewResources);
		CreateDeferredResources(NewResources, Size);
		CreateLightDataResources(NewResources);
		CreateShadowResources(NewResources);

		GContext.mResources.emplace(Scene.mId, NewResources);
	}

	void UpdateCameraUniforms(llrm::ResourceSet DstRes, const glm::mat4 CamView, const glm::mat4 CamProj)
	{
		CameraVertexUniforms CamUniforms{
glm::transpose(CamProj * CamView)
		};
		llrm::UpdateUniformBuffer(DstRes, 0, &CamUniforms, sizeof(CamUniforms));
	}

	void RenderShadowMap(const Scene& Scene, 
		const SceneResources& Resources,
		uint32_t FrustumBase,
		const llrm::CommandBuffer& DstCmd,
		glm::uvec2 ShadowMapSize, 
		const Object& Light,
		const glm::mat4 CamView, const glm::mat4& CamProj
	)
	{
		Ruby::Light LightData = Ruby::GetLight(Light.mReferenceId);

		ShadowLightUniforms ShadowUniforms;
		if(LightData.mType == LightType::Directional)
		{
			ShadowUniforms.mViewProjection = CreateDirectionalVPMatrix(Light.mRotation, CamView, CamProj);
		}

		// Transition shadow maps texture array to correct usage
		llrm::TransitionTexture(DstCmd, Resources.mShadowMaps, llrm::AttachmentUsage::ShaderRead, llrm::AttachmentUsage::DepthStencilAttachment, FrustumBase, 1);

		// Update resources
		llrm::UpdateUniformBuffer(Light.mObjectResources, 0, &ShadowUniforms, sizeof(ShadowUniforms));

		// TODO: Need to handle multiple light cascades/frustums

		// Depth render
		std::vector<llrm::ClearValue> ClearValues = {
			{llrm::ClearType::Float, 1.0f}
		};

		llrm::FrameBuffer FrustumFbo = Resources.mShadowMapFbos[FrustumBase];
		 
		llrm::BeginRenderGraph(DstCmd, GContext.mShadowMapRG, FrustumFbo, ClearValues);
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

	void PrepLightDataTex(const Scene& Scene, SceneResources& Res)
	{
		
	}

	void RenderScene(const Scene& Scene,
		const RenderTarget& RT,
		glm::uvec2 ViewportSize, 
		const Camera& Camera,
		const llrm::CommandBuffer& DstCmd, 
		const llrm::FrameBuffer& DstBuf, 
		const llrm::RenderGraph& DstGraph, 
		const llrm::Pipeline& DstPipeline,
		const llrm::ResourceSet& DstResources,
		const RenderSettings& Settings,
		std::function<void(const llrm::CommandBuffer&)> PostTonemap
	)
	{
		if(!GContext.mResources.contains(Scene.mId))
		{
			InitSceneResources(Scene, ViewportSize);
		}

		SceneResources& Resources = GContext.mResources[Scene.mId];

		// Update camera uniform
		glm::mat4 CamView = glm::inverse(BuildTransformQuat(Camera.mPosition, Camera.mRotation, { 1, 1, 1 }));
		UpdateCameraUniforms(Resources.mSceneResources, CamView, Camera.mProjection);

		uint32_t MAX_FRUSTUMS = std::min(llrm::GetCaps().MaxImageArrayLayers, (uint32_t)10);
		uint32_t MaxImageSize = llrm::GetCaps().MaxTextureSize;
		if(Resources.mShadowFrustumsData.size() != MAX_FRUSTUMS * 4)
		{
			Resources.mShadowFrustumsData.resize(MAX_FRUSTUMS * 4);
		}

		if (Resources.mLightData.size() != MaxImageSize)
		{
			Resources.mLightData.resize(MaxImageSize);
		}

		// Object processing:
		// 1) Assign shadow frustums to lights
		// 2) Build light data
		// 3) Object object shader uniforms
		// TODO: Ensure all of these steps are multithreading safe. What happens if we call RenderScene() with different views on the same scene?

		uint32_t ShadowMapFrustum = 0;
		uint32_t LightDataIndex = 1;
		std::unordered_map<uint32_t, uint32_t> LightFrustums; // LightID -> FrustumIndex
		uint32_t NumDirLights = 0, NumSpotLights = 0;
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
				glm::vec4 Direction = BuildTransform({}, Obj.mRotation, { 1, 1, 1 }) * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);

				// Assign next available shadow frustum if casting shadows
				if(Settings.mShadowsEnabled && Light.mCastShadows)
				{
					LightFrustums[Obj.mId] = ShadowMapFrustum;
					ShadowMapFrustum++;
				}

				if(Light.mType == LightType::Directional)
				{
					Resources.mLightData[LightDataIndex + 0] = glm::vec4((float)Light.mType, Light.mColor);
					Resources.mLightData[LightDataIndex + 1] = glm::vec4(Light.mCastShadows, LightFrustums[Obj.mId], 0.0f, 0.0f);
					Resources.mLightData[LightDataIndex + 2] = glm::vec4(Direction.x, Direction.y, Direction.z, Light.mIntensity);

					LightDataIndex += 3;

					NumDirLights++;

					// Update shadow information
					if(Settings.mShadowsEnabled)
					{
						glm::mat4 ViewProj = glm::transpose(CreateDirectionalVPMatrix(Obj.mRotation, CamView, Camera.mProjection));
						uint32_t BaseIndex = LightFrustums[Obj.mId] * 4;
						glm::mat4 LightViewProj = ViewProj;
						Resources.mShadowFrustumsData[BaseIndex + 0] = LightViewProj[0];
						Resources.mShadowFrustumsData[BaseIndex + 1] = LightViewProj[1];
						Resources.mShadowFrustumsData[BaseIndex + 2] = LightViewProj[2];
						Resources.mShadowFrustumsData[BaseIndex + 3] = LightViewProj[3];
					}

				}
				else if(Light.mType == LightType::Spot)
				{
					float Intensity = Light.mIntensity;

					if(Light.mIntensityUnit == LightUnit::Lumen)
					{
						// TODO: Convert to candela
					}

					// Shader will divide by user units, but we need shader output to be in candela/m^2
					Intensity *= (ToUser(1) * ToUser(1));

					// Inner and outer angle must satisfy conditions:
					// Outer angle >= inner angle
					// Outer angle <= pi radians, Inner angle <= pi radians
					// Divide angles by 2 so user doesn't have to specify "half angles"
					float Inner2 = Light.InnerAngle / 2.0f, Outer2 = Light.OuterAngle / 2.0f;
					float InnerAngle = 1.0f / (glm::cos(Inner2) - glm::cos(Outer2));
					float OuterAngle = glm::cos(Outer2);

					Resources.mLightData[LightDataIndex + 0] = glm::vec4((float) LightType::Spot, Light.mColor);
					Resources.mLightData[LightDataIndex + 1] = glm::vec4(Light.mCastShadows, LightFrustums[Obj.mId], 0.0f, 0.0f); // Last 2 reserved
					Resources.mLightData[LightDataIndex + 2] = glm::vec4(Direction.x, Direction.y, Direction.z, Intensity);
					Resources.mLightData[LightDataIndex + 3] = glm::vec4(Obj.mPosition, 0.0f); // Fourth unused
					Resources.mLightData[LightDataIndex + 4] = glm::vec4(InnerAngle, OuterAngle, 0.0f, 0.0f);
					LightDataIndex += 5;

					NumSpotLights++;
				}
			}
		}
		Resources.mLightData[0] = glm::vec4((float) (NumDirLights + NumSpotLights), 0.0f, 0.0f, 0.0f);

		// Material processing:
		// 1) Update material shader uniforms.
		//	TODO: Ensure this step is multithreading safe
		for (auto& Material : GContext.mMaterials)
		{
			ShaderUniform_Material MaterialParams = {
				Material.second.Roughness,
				Material.second.Metallic,
				Material.second.Albedo
			};
			llrm::UpdateUniformBuffer(Material.second.mMaterialResources, 0, &MaterialParams, sizeof(MaterialParams));
		}

		// Write light data texture. TODO: Are light textures too slow? Use uniforms instead?
		llrm::WriteTexture(Resources.mLightDataTexture,
			llrm::AttachmentUsage::ShaderRead, llrm::AttachmentUsage::ShaderRead,
			MaxImageSize, 1, 0,
			Resources.mLightData.size() * sizeof(glm::vec4), Resources.mLightData.data()
		);

		// Update frustums data texture
		if(Settings.mShadowsEnabled)
		{
			llrm::WriteTexture(Resources.mShadowMapFrustums,
				llrm::AttachmentUsage::ShaderRead, llrm::AttachmentUsage::ShaderRead,
				MAX_FRUSTUMS * 4, 1, 0,
				Resources.mShadowFrustumsData.size() * sizeof(glm::vec4), Resources.mShadowFrustumsData.data()
			);
		}

		// Update lights
		llrm::UpdateTextureResource(Resources.mLightResources, { Resources.mLightDataTextureView }, 0);
		llrm::UpdateTextureResource(Resources.mLightResources, {Resources.mShadowMapsResourceView}, 1);
		llrm::UpdateTextureResource(Resources.mLightResources, { Resources.mShadowMapFrustumsView }, 2);

		// Update tonemap inputs
		llrm::UpdateSamplerResource(DstResources, Resources.mNearestSampler, 0);
		llrm::UpdateTextureResource(DstResources, { RT.mHDRColorView }, 1);

		// Update deferred shade inputs
		llrm::UpdateSamplerResource(Resources.mDeferredShadeRes, Resources.mNearestSampler, 0);
		llrm::UpdateTextureResource(Resources.mDeferredShadeRes, { RT.mDeferredAlbedoView }, 1);
		llrm::UpdateTextureResource(Resources.mDeferredShadeRes, { RT.mDeferredPositionView }, 2);
		llrm::UpdateTextureResource(Resources.mDeferredShadeRes, { RT.mDeferredNormalView }, 3);
		llrm::UpdateTextureResource(Resources.mDeferredShadeRes, { RT.mDeferredRMAOView }, 4);

		DeferredShadeResources ShadeRes = { Camera.mPosition};
		llrm::UpdateUniformBuffer(Resources.mDeferredShadeRes, 0, &ShadeRes, sizeof(ShadeRes));

		// Render the scene
		llrm::Begin(DstCmd);
		{

			// Transition shadow maps texture array to correct usage
			//llrm::TransitionTexture(DstCmd, Resources.mShadowMaps, llrm::AttachmentUsage::ShaderRead, llrm::AttachmentUsage::DepthStencilAttachment, 0, 10);

			// Render shadow maps
			if(Settings.mShadowsEnabled)
			{
				for (uint32_t Object : Scene.mObjects)
				{
					Ruby::Object& Obj = GetObject(Object);
					if (IsLightObject(Obj.mId))
					{
						Light& Light = GetLight(Obj.mReferenceId);
						if (Light.mCastShadows)
						{
							uint32_t BaseFrustum = LightFrustums[Obj.mId];

							RenderShadowMap(Scene,
								Resources,
								BaseFrustum,
								DstCmd,
								{ SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION },
								Obj,
								CamView,
								Camera.mProjection
							);
						}
					}
				}
			}
			
			llrm::TransitionTexture(DstCmd, RT.mDeferredAlbedo, llrm::AttachmentUsage::ShaderRead, llrm::AttachmentUsage::ColorAttachment);
			llrm::TransitionTexture(DstCmd, RT.mDeferredPosition, llrm::AttachmentUsage::ShaderRead, llrm::AttachmentUsage::ColorAttachment);
			llrm::TransitionTexture(DstCmd, RT.mDeferredNormal, llrm::AttachmentUsage::ShaderRead, llrm::AttachmentUsage::ColorAttachment);
			llrm::TransitionTexture(DstCmd, RT.mDeferredRMAO, llrm::AttachmentUsage::ShaderRead, llrm::AttachmentUsage::ColorAttachment);

			// Deferred geometry stage
			std::vector<llrm::ClearValue> ClearValues = {
				{llrm::ClearType::Float, 0.0, 0.0, 0.0, 1.0f},
				{llrm::ClearType::Float, 0.0, 0.0, 0.0, 0.0f},
				{llrm::ClearType::Float, 0.0, 0.0, 0.0, 0.0f},
				{llrm::ClearType::Float, 0.0, 0.0, 0.0, 0.0f},
				{llrm::ClearType::Float, 1.0f}
			};
			llrm::BeginRenderGraph(DstCmd, GContext.mDeferredGeoRG, RT.mDeferredGeoFB, ClearValues);
			{
				llrm::SetViewport(DstCmd, 0, 0, ViewportSize.x, ViewportSize.y);
				llrm::SetScissor(DstCmd, 0, 0, ViewportSize.x, ViewportSize.y);

				llrm::BindPipeline(DstCmd, GContext.mDeferredGeoPipe);

				for(uint32_t Object : Scene.mObjects)
				{
					Ruby::Object& Obj = GetObject(Object);
					if(IsMeshObject(Obj.mId))
					{
						Ruby::Mesh& Mesh = GetMesh(Obj.mReferenceId);

						llrm::ResourceSet MaterialResources = GContext.mDefaultMaterial;
						if (IsValidId(Mesh.mMat))
							MaterialResources = GetMaterial(Mesh.mMat).mMaterialResources;

						llrm::BindResources(DstCmd, { Resources.mSceneResources, Obj.mObjectResources, MaterialResources});
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
			llrm::TransitionTexture(DstCmd, RT.mHDRColor, llrm::AttachmentUsage::ShaderRead, llrm::AttachmentUsage::ColorAttachment);
			llrm::BeginRenderGraph(DstCmd, GContext.mDeferredShadeRG, RT.mDeferredShadeFB, ClearValues);
			{
				llrm::SetViewport(DstCmd, 0, 0, ViewportSize.x, ViewportSize.y);
				llrm::SetScissor(DstCmd, 0, 0, ViewportSize.x, ViewportSize.y);

				llrm::BindPipeline(DstCmd, GContext.DeferredShadePipeline(Settings.mShadowsEnabled));
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

				// Call post tonemap
				PostTonemap(DstCmd);
			}
			llrm::EndRenderGraph(DstCmd);
		}
		llrm::End(DstCmd);
	}

	void RenderScene(SceneId Id, 
		const RenderTarget& RT,
		glm::ivec2 ViewportSize,
		const Camera& Camera,
		const SwapChain& Target,
		const RenderSettings& Settings,
		std::function<void(const llrm::CommandBuffer&)> PostTonemap
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
				RT,
				ViewportSize, 
				Camera, 
				Cmd, 
				Buffer, 
				Target.mTonemapGraph, 
				Target.mTonemapPipeline,
				Target.mTonemapResources,
				Settings,
				PostTonemap
			);

			llrm::EndFrame({ Cmd });
		}
	}

}
