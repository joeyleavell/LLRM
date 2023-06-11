#pragma once

#include <string>
#include <unordered_set>
#include "llrm.h"
#include "glm/vec2.hpp"
#include "glm/vec3.hpp"
#include "Uniforms.h"

namespace Ruby
{

	typedef uint32_t SceneId;
	typedef uint32_t ObjectId;
	typedef uint32_t LightId;

	enum class RenderingAPI
	{
		Vulkan
	};

	struct SceneResources
	{
		llrm::Texture	  mHDRColor;
		llrm::TextureView mHDRColorView;
		llrm::Texture	  mDepth;
		llrm::TextureView mDepthView;

		// Full screen quad
		llrm::VertexBuffer mFullScreenQuadVbo;
		llrm::IndexBuffer mFullScreenQuadIbo;

		llrm::ResourceSet	 mSceneResources;
		llrm::ResourceSet	 mLightResources;

		// Samplers
		llrm::Sampler		 mNearestSampler;

		// Deferred geometry stage
		llrm::Texture		 mDeferredAlbedo;
		llrm::Texture		 mDeferredPosition;
		llrm::Texture		 mDeferredNormal;
		llrm::TextureView	 mDeferredAlbedoView;
		llrm::TextureView	 mDeferredPositionView;
		llrm::TextureView	 mDeferredNormalView;

		llrm::RenderGraph	 mDeferredGeoRG;
		llrm::FrameBuffer	 mDeferredGeoFB;
		llrm::Pipeline		 mDeferredGeoPipe;

		// Deferred shading stage
		llrm::RenderGraph	 mDeferredShadeRG;
		llrm::FrameBuffer	 mDeferredShadeFB;
		llrm::Pipeline		 mDeferredShadePipe;
		llrm::ResourceSet	 mDeferredShadeRes;

		// Shadow map resources
		llrm::Texture				   mShadowMaps;
		llrm::TextureView			   mShadowMapsResourceView;
		std::vector<llrm::TextureView> mShadowMapAttachmentViews;
		std::vector<llrm::FrameBuffer> mShadowMapFbos;

		llrm::Texture		 mShadowMapFrustums;
		llrm::TextureView	 mShadowMapFrustumsView;
	};

	struct Camera
	{
		glm::vec3 mPosition;
		glm::vec3 mRotation;
		glm::mat4 mProjection;
	};

	struct Mesh
	{
		uint32_t mId;
		llrm::VertexBuffer mVbo;
		llrm::IndexBuffer mIbo;
		uint32_t mIndexCount;
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

	enum class LightType
	{
		Directional,
		Spot
	};

	struct Light
	{
		uint32_t  mId;
		LightType mType;
		glm::vec3 mColor;
		float	  mIntensity;
		bool	  mCastShadows;
	};

	struct RubyContext
	{
		std::string ShadersRoot;
		std::string CompiledShaders;

		llrm::Context LLContext{};

		std::unordered_map<uint32_t, SceneResources> mResources;

		std::unordered_map<uint32_t, Light> mLights;
		std::unordered_map<uint32_t, Mesh> mMeshes;
		std::unordered_map<uint32_t, Object> mObjects;
		std::unordered_map<uint32_t, Scene> mScenes;

		// Resource layouts
		llrm::ResourceLayout mTonemapLayout{};
		llrm::ResourceLayout mSceneResourceLayout;
		llrm::ResourceLayout mLightsResourceLayout;
		llrm::ResourceLayout mObjectResourceLayout;
		llrm::ResourceLayout mLightObjectResourceLayout;
		llrm::ResourceLayout mDeferredShadeRl;

		// Shadow map generation
		llrm::RenderGraph	 mShadowMapRG;
		llrm::Pipeline		 mShadowMapPipe;

		uint32_t mNextMeshId = 0, mNextObjectId = 0, mNextSceneId = 0, mNextLightId = 0;
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

	RubyContext CreateContext(const ContextParams& Params);
	void DestroyContext(const RubyContext& Context);
	void SetContext(const RubyContext& Context);
	SwapChain CreateSwapChain(GLFWwindow* Wnd);
	void DestroySwapChain(const SwapChain& Swap);

	// Light
	LightId CreateLight(LightType Type, glm::vec3 Color, float Intensity, bool CastShadows);
	void DestroyLight(LightId Light);
	Light& GetLight(LightId Light);

	// Mesh
	Mesh CreateMesh(const Tesselation& Tesselation);
	void DestroyMesh(const Mesh& Mesh);
	Mesh& GetMesh(uint32_t MeshId);

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
		glm::ivec2 ViewportSize, 
		const Camera& Camera, 
		const SwapChain& Target
	);

}
