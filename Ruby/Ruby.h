#pragma once

#include <string>
#include <unordered_set>
#include "llrm.h"
#include "glm/vec2.hpp"
#include "glm/vec3.hpp"
#include "Uniforms.h"
#include "glm/gtx/quaternion.hpp"

struct DeferredShadeParameters
{
	bool UseShadows = true;

	bool operator==(const DeferredShadeParameters& Other) const 
	{
		return UseShadows == Other.UseShadows;
	}
};

namespace std
{
	template<>
	struct hash<DeferredShadeParameters>
	{
		std::size_t operator()(const DeferredShadeParameters& Obj) const noexcept
		{
			return size_t(Obj.UseShadows);
		}
	};
}

namespace Ruby
{
	const uint32_t INVALID_ID = std::numeric_limits<uint32_t>::max();

	typedef uint32_t SceneId;
	typedef uint32_t ObjectId;
	typedef uint32_t LightId;

	struct RenderSettings
	{
		bool mShadowsEnabled = false;
	};

	enum class DistanceUnits
	{
		Meters,
		Millimeters,
		Centimeters
	};

	// The units of measurement
	extern const DistanceUnits gUnits;

	enum class RenderingAPI
	{
		Vulkan
	};

	// Resources that are shared between all views of a scene
	struct SceneResources
	{
		// Full screen quad
		llrm::VertexBuffer mFullScreenQuadVbo;
		llrm::IndexBuffer  mFullScreenQuadIbo;

		llrm::ResourceSet	 mSceneResources;
		llrm::ResourceSet	 mLightResources;

		// Samplers
		llrm::Sampler		 mNearestSampler;

		// Deferred shading stage
		llrm::ResourceSet	 mDeferredShadeRes;

		// Light data resources
		llrm::Texture			mLightDataTexture;
		llrm::TextureView		mLightDataTextureView;
		std::vector<glm::vec4>  mLightData;

		// Shadow map resources
		llrm::Texture				   mShadowMaps;
		llrm::TextureView			   mShadowMapsResourceView;
		std::vector<llrm::TextureView> mShadowMapAttachmentViews;
		std::vector<llrm::FrameBuffer> mShadowMapFbos;
		std::vector<glm::vec4>		   mShadowFrustumsData;

		llrm::Texture		 mShadowMapFrustums;
		llrm::TextureView	 mShadowMapFrustumsView;
	};

	struct Camera
	{
		glm::vec3 mPosition;
		glm::quat mRotation;
		glm::mat4 mProjection;
	};

	struct Material
	{
		// Don't touch this ID or bad things happen. This is assigned to the material on creation.
		uint32_t mId{};

		// Material parameters
		float Roughness = 1.0f;
		float Metallic = 1.0f;
		glm::vec3 Albedo = glm::vec3(0.18f);

		// Material resources
		llrm::ResourceSet mMaterialResources;
	};

	struct Mesh
	{
		uint32_t mId;
		llrm::VertexBuffer mVbo;
		llrm::IndexBuffer mIbo;
		uint32_t mIndexCount;
		uint32_t mMat = INVALID_ID;
	};

	enum class ObjectType
	{
		Mesh,
		Light
	};

	struct Object
	{
		ObjectType mType;
		uint32_t mId;
		uint32_t mReferenceId;
		glm::vec3 mPosition;
		glm::vec3 mRotation;

		// Model matrix for Mesh types, shadow map resources for lights
		llrm::ResourceSet mObjectResources;

		// For light shadow maps
		//llrm::Texture mShadowDepthAttachment;
		//llrm::TextureView mShadowDepthAttachmentRenderPassView;
		//llrm::TextureView mShadowDepthAttachmentResourceView;
		//llrm::FrameBuffer mShadowFbo;
	};

	class Scene
	{
	public:

		uint32_t mId;
		std::unordered_set<uint32_t> mObjects;
	};

	enum class LightType : uint8_t
	{
		Directional = 0,
		Spot = 1
	};

	enum class LightUnit
	{
		Lumen,
		Candela,
		Lux
	};

	struct Light
	{
		uint32_t  mId;
		LightType mType;
		glm::vec3 mColor;
		float	  mIntensity; 

		// Directional lights use lux, punctual (point & spot) lights used candelas or lumens.
		// Lumens will be automatically converted to candelas depending on light angle.
		LightUnit mIntensityUnit;

		bool	  mCastShadows;

		// Punctual spotlight parameters
		float InnerAngle = glm::radians(20.0f);
		float OuterAngle = glm::radians(40.0f);
	};

	struct RubyContext
	{
		std::string ShadersRoot;
		std::string CompiledShaders;

		llrm::Context LLContext{};

		std::unordered_map<uint32_t, SceneResources> mResources;

		std::unordered_map<uint32_t, Light> mLights;
		std::unordered_map<uint32_t, Mesh> mMeshes;
		std::unordered_map<uint32_t, Material> mMaterials;
		std::unordered_map<uint32_t, Object> mObjects;
		std::unordered_map<uint32_t, Scene> mScenes;

		// Resource layouts
		llrm::ResourceLayout mTonemapLayout{};
		llrm::ResourceLayout mSceneResourceLayout;
		llrm::ResourceLayout mLightsResourceLayout;
		llrm::ResourceLayout mObjectResourceLayout;
		llrm::ResourceLayout mLightObjectResourceLayout;
		llrm::ResourceLayout mDeferredShadeRl;
		llrm::ResourceLayout mMaterialLayout;

		// Shadow map generation
		llrm::RenderGraph	 mShadowMapRG;
		llrm::Pipeline		 mShadowMapPipe;

		// Render graphs
		llrm::RenderGraph	 mDeferredGeoRG;
		llrm::RenderGraph	 mDeferredShadeRG;

		// Pipelines
		llrm::Pipeline		 mDeferredGeoPipe;

		// Default material
		llrm::ResourceSet	 mDefaultMaterial;

		std::unordered_map<DeferredShadeParameters, llrm::Pipeline> mDeferredShadePipelines;
		llrm::Pipeline DeferredShadePipeline(bool UseShadows);

		uint32_t mNextMeshId = 0, mNextObjectId = 0, mNextMaterialId = 0, mNextSceneId = 0, mNextLightId = 0;
	};

	struct SwapChain
	{
		GLFWwindow* mWnd = nullptr;
		llrm::Surface mSurface;
		llrm::SwapChain mSwap;
		std::vector<llrm::FrameBuffer> mFrameBuffers;
		std::vector<llrm::CommandBuffer> mCmdBuffers;

		// Tone mapping (HDR to backbuffer) resources
		llrm::RenderGraph mTonemapGraph;
		llrm::Pipeline mTonemapPipeline;
		llrm::ResourceSet mTonemapResources;
	};

	struct RenderTarget
	{
		// Deferred geometry stage
		llrm::Texture		 mDeferredAlbedo;
		llrm::Texture		 mDeferredPosition;
		llrm::Texture		 mDeferredNormal;
		llrm::Texture		 mDeferredRMAO;
		llrm::TextureView	 mDeferredAlbedoView;
		llrm::TextureView	 mDeferredPositionView;
		llrm::TextureView	 mDeferredNormalView;
		llrm::Texture		 mDeferredRMAOView;
		llrm::FrameBuffer	 mDeferredGeoFB;

		// Deferred shading stage
		llrm::FrameBuffer	 mDeferredShadeFB;

		// Scene color/depth
		llrm::Texture	  mHDRColor;
		llrm::TextureView mHDRColorView;
		llrm::Texture	  mDepth;
		llrm::TextureView mDepthView;
	};

	struct ContextParams
	{
		std::string ShadersRoot;
		std::string CompiledShaders;
	};

	struct MeshVertex
	{
		glm::vec3 mPosition;
		glm::vec3 mNormal;
	};

	struct Tesselation
	{
		std::vector<MeshVertex> mVerts;
		std::vector<uint32_t> mIndicies;
	};

	inline bool IsValidId(uint32_t Id)
	{
		return Id != INVALID_ID;
	}

	RubyContext CreateContext(const ContextParams& Params);
	void DestroyContext(const RubyContext& Context);
	void SetContext(const RubyContext& Context);
	SwapChain CreateSwapChain(GLFWwindow* Wnd);
	void DestroySwapChain(const SwapChain& Swap);
	void ResizeSwapChain(SwapChain& Swap, uint32_t NewWidth, uint32_t NewHeight);

	// Render target
	RenderTarget CreateRenderTarget(uint32_t Width, uint32_t Height);
	void DestroyRenderTarget(RenderTarget& Target);
	void ResizeRenderTarget(RenderTarget& Target, uint32_t Width, uint32_t Height);

	// Light
	Light& CreateLight(LightType Type, glm::vec3 Color, float Intensity, LightUnit Units, bool CastShadows);

	// Inner and outer angles in degrees
	Light& CreateSpotLight(
		glm::vec3 Color, 
		float Intensity,
		LightUnit Units = LightUnit::Lumen,
		float InnerAngle = glm::radians(20.0f), float OuterAngle = glm::radians(40.0f),
		bool CastShadows = false
	);

	void DestroyLight(LightId Light);
	Light& GetLight(LightId Light);

	// Mesh
	Mesh& CreateMesh(const Tesselation& Tesselation, uint32_t MatId = INVALID_ID);
	void DestroyMesh(const Mesh& Mesh);
	Mesh& GetMesh(uint32_t MeshId);

	// Material
	Material& CreateMaterial();
	void DestroyMaterial(const Material& Mesh);
	Material& GetMaterial(uint32_t MaterialId);

	// Object
	ObjectId CreateMeshObject(const Mesh& Mesh, glm::vec3 Position, glm::vec3 Rotation);
	ObjectId CreateLightObject(LightId Light, glm::vec3 Position, glm::vec3 Rotation);
	void DestroyObject(ObjectId Id);
	Object& GetObject(ObjectId ObjectId);
	bool IsMeshObject(ObjectId Id);
	bool IsLightObject(ObjectId Id);

	// Scene
	SceneId CreateScene();
	void DestroyScene(SceneId Scene);
	void AddObject(SceneId Scene, ObjectId Object);
	void RemoveObject(SceneId Scene, ObjectId Object);

	extern RubyContext GContext;

	struct FrameBuffer
	{
		llrm::Texture ColorAttachment;
		llrm::TextureView ColorAttachmentView;

		llrm::Texture DepthAttachment;
		llrm::TextureView DepthAttachmentView;

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

	void RenderScene(SceneId Scene,
		const RenderTarget& RT,
		glm::ivec2 ViewportSize, 
		const Camera& Camera, 
		const SwapChain& Target,
		const RenderSettings& Settings,
		std::function<void(const llrm::CommandBuffer&)> PostTonemap = [](const llrm::CommandBuffer& Buf){}
	);

}