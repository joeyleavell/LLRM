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

	enum class RenderingAPI
	{
		Vulkan
	};

	struct SceneResources
	{
		llrm::Texture mHDRColor;
		llrm::Texture mDepth;

		// Full screen quad
		llrm::VertexBuffer mFullScreenQuadVbo;
		llrm::IndexBuffer mFullScreenQuadIbo;

		llrm::ResourceSet	 mSceneResources;

		// Deferred geometry stage
		llrm::Texture		 mDeferredAlbedo;
		llrm::Texture		 mDeferredPosition;
		llrm::Texture		 mDeferredNormal;

		llrm::RenderGraph	 mDeferredGeoRG;
		llrm::FrameBuffer	 mDeferredGeoFB;
		llrm::Pipeline		 mDeferredGeoPipe;

		llrm::RenderGraph	 mDeferredShadeRG;
		llrm::FrameBuffer	 mDeferredShadeFB;
		llrm::Pipeline		 mDeferredShadePipe;
		llrm::ResourceSet	 mDeferredShadeRes;

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

	struct Object
	{
		uint32_t mId;
		uint32_t mMeshId;
		glm::vec3 mPosition;
		glm::vec3 mRotation;

		llrm::ResourceSet mObjectResources;
	};

	class Scene
	{
	public:

		uint32_t mId;
		std::unordered_set<uint32_t> mObjects;
	};

	struct RubyContext
	{
		std::string ShadersRoot;
		std::string CompiledShaders;

		llrm::Context LLContext{};

		std::unordered_map<uint32_t, SceneResources> mResources;

		std::unordered_map<uint32_t, Mesh> mMeshes;
		std::unordered_map<uint32_t, Object> mObjects;
		std::unordered_map<uint32_t, Scene> mScenes;

		// Resource layouts
		llrm::ResourceLayout mTonemapLayout{};
		llrm::ResourceLayout mSceneResourceLayout;
		llrm::ResourceLayout mObjectResourceLayout;
		llrm::ResourceLayout mDeferredShadeRl;

		uint32_t mNextMeshId = 0, mNextObjectId = 0, mNextSceneId = 0;
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

	// Mesh
	Mesh CreateMesh(const Tesselation& Tesselation);
	void DestroyMesh(const Mesh& Mesh);
	Mesh& GetMesh(uint32_t MeshId);

	// Object
	ObjectId CreateObject(const Mesh& Mesh, glm::vec3 Position, glm::vec3 Rotation);
	void DestroyObject(ObjectId Id);
	Object& GetObject(ObjectId ObjectId);

	// Scene
	SceneId CreateScene();
	void DestroyScene(SceneId Scene);
	void AddObject(SceneId Scene, ObjectId Object);
	void RemoveObject(SceneId Scene, ObjectId Object);

	extern RubyContext GContext;

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

	void RenderScene(SceneId Scene, 
		glm::ivec2 ViewportSize, 
		const Camera& Camera, 
		const SwapChain& Target
	);

}
