#pragma once

#include <string>
#include <unordered_set>

#include "llrm.h"
#include "Ruby.h"
#include "Ruby.h"
#include "glm/vec3.hpp"

namespace Ruby
{
	enum class RenderingAPI
	{
		Vulkan
	};

	struct SceneResources
	{

	};

	struct Mesh
	{
		uint32_t mId;
		llrm::VertexBuffer mVbo;
		llrm::IndexBuffer mIbo;
	};

	struct Object
	{
		uint32_t mId;
		uint32_t mMeshId;
		glm::vec3 Position;
	};

	struct RubyContext
	{
		std::string ShadersRoot;
		std::string CompiledShaders;

		llrm::Context LLContext{};

		std::unordered_map<uint32_t, SceneResources> mResources;

		std::unordered_map<uint32_t, Mesh> mMeshes;
		std::unordered_map<uint32_t, Object> mObjects;

		uint32_t mNextMeshId = 0, mNextObjectId = 0;
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

	// Object
	Object CreateObject(const Mesh& Mesh, glm::vec3 Position);
	void DestroyObject(const Object& Object);

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

	class Scene
	{
	public:

		uint32_t mId;

		Scene(uint32_t Id)
		{
			mId = Id;
		}

		void AddObject(const Object& Object)
		{
			mObjects.insert(Object.mId);
		}

		void RemoveObject(const Object& Object)
		{
			mObjects.erase(Object.mId);
		}

	private:

		std::unordered_set<uint32_t> mObjects;

	};

	void RenderScene(const Scene& Scene, const SwapChain& Target);

}
