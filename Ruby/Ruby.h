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

		// Pipelines
		llrm::Pipeline mDeferredGeometryPipe;
		llrm::RenderGraph mDeferredGeometryRg;
		llrm::ResourceLayout mDeferredGeometryRl;

		std::unordered_map<uint32_t, SceneResources> mResources;

		std::unordered_map<uint32_t, Mesh> mMeshes;
		std::unordered_map<uint32_t, Object> mObjects;
		std::unordered_map<uint32_t, Scene> mScenes;

		uint32_t mNextMeshId = 0, mNextObjectId = 0, mNextSceneId = 0;
	};

	struct SwapChain
	{
		GLFWwindow* mWnd = nullptr;
		llrm::Surface mSurface;
		llrm::SwapChain mSwap;
		llrm::RenderGraph mGraph;
		llrm::Pipeline mPipeline;
		llrm::ResourceSet mResources;
		std::vector<llrm::FrameBuffer> mFrameBuffers;
		std::vector<llrm::CommandBuffer> mCmdBuffers;
	};

	struct ContextParams
	{
		std::string ShadersRoot;
		std::string CompiledShaders;
	};

	struct MeshVertex
	{
		glm::vec3 Position;
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
