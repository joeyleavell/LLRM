#pragma once

#include <cstdint>
#include <functional>

struct GLFWwindow;

namespace llrm
{
	// Rendering primitives
	typedef void* Pipeline;
	typedef void* SwapChain;
	typedef void* Surface;
	typedef void* RenderGraph;
	typedef void* FrameBuffer;
	typedef void* VertexBuffer;
	typedef void* IndexBuffer;
	typedef void* ShaderProgram;
	typedef void* CommandBuffer;
	typedef void* Fence;
	typedef void* ResourceLayout;
	typedef void* ResourceSet;
	typedef void* Texture;

	const uint32_t MAX_OUTPUT_COLORS = 8;

	enum class AttachmentFormat
	{
		B8G8R8A8_SRGB,
		R32_UINT,
		R32_SINT,
		R32_FLOAT,
		R8_UINT,
		D24_UNORM_S8_UINT
	};

	enum class AttachmentUsage
	{
		Presentation,
		TransferDestination,
		TransferSource,
		ColorAttachment,
		DepthStencilAttachment,
		ShaderRead,
		Undefined
	};

	enum class VertexAttributeFormat
	{
		Float,
		Float2,
		Float3,
		Float4,
		Int32
	};

	enum class BufferUsage
	{
		// Use if the buffer won't be updated very often.
		Static,

		// Use if the buffer gets updated very often, potentially every frame.
		Dynamic
	};

	enum class ShaderStage
	{
		Vertex,
		Fragment
	};

	enum class TextureFormat
	{
		UINT32_R8G8B8A8
	};

	struct VertexAttribute
	{
		llrm::VertexAttributeFormat Format;
		uint32_t Offset;

		VertexAttribute() :
		Format(llrm::VertexAttributeFormat::Float2),
		Offset(0) {}

		VertexAttribute(llrm::VertexAttributeFormat InFormat, uint32_t InOffset):
		Format(InFormat),
		Offset(InOffset){}
	};

	struct ConstantBufferDescription
	{
		uint32_t Binding;
		uint32_t Count;
		llrm::ShaderStage StageUsedAt;
		uint64_t BufferSize;
	};

	struct TextureDescription
	{
		uint32_t Binding = 0;
		llrm::ShaderStage StageUsedAt = llrm::ShaderStage::Fragment;
		uint32_t Count = 1; // The amount of textures in this binding, in the case of arrays
	};

	struct ResourceLayoutCreateInfo
	{
		uint32_t ConstantBufferCount = 0;
		ConstantBufferDescription* ConstantBuffers = nullptr;

		uint32_t TextureCount = 0;
		TextureDescription* Textures = nullptr;
	};

	enum class BlendOperation
	{
		Add,
		Min,
		Max
	};

	enum class BlendFactor
	{
		Zero,
		One,
		SrcAlpha,
		SrcColor,
		OneMinusSrcAlpha,
		OneMinusSrcColor,
		DstAlpha,
		DstColor,
		OneMinusDstAlpha,
		OneMinusDstColor
	};

	struct PipelineBlendSettings
	{
		bool bBlendingEnabled = false;

		BlendFactor SrcColorFactor = BlendFactor::One;
		BlendFactor DstColorFactor = BlendFactor::Zero;
		BlendFactor SrcAlphaFactor = BlendFactor::One;
		BlendFactor DstAlphaFactor = BlendFactor::Zero;

		BlendOperation ColorOp = BlendOperation::Add;
		BlendOperation AlphaOp = BlendOperation::Add;
	};

	enum class PipelineRenderPrimitive
	{
		TRIANGLES,
		LINES
	};

	enum class FilterType
	{
		NEAREST,
		LINEAR
	};

	struct PipelineDepthStencilSettings
	{
		bool bEnableDepthTest = false;
	};

	struct PipelineState
	{
		llrm::ShaderProgram Shader;

		RenderGraph CompatibleGraph;
		uint32_t PassIndex;

		ResourceLayout Layout;

		// Vertex attributes <format, offset>
		uint32_t VertexBufferStride;
		std::vector<std::pair<VertexAttributeFormat, uint32_t>> VertexAttributes;

		// General pipeline state
		PipelineRenderPrimitive Primitive = PipelineRenderPrimitive::TRIANGLES;
		std::vector<PipelineBlendSettings> BlendSettings;
		PipelineDepthStencilSettings DepthStencil;
	};

	struct RenderGraphAttachmentDescription
	{
		AttachmentUsage InitialUsage;
		AttachmentUsage FinalUsage;
		AttachmentFormat Format;
	};

	struct RenderPassInfo
	{
		std::vector<int32_t> OutputAttachments{};
	};

	// The depth stencil attachment is always at index ColorAttachmentCount since it's placed at the end
	struct RenderGraphCreateInfo
	{
		std::vector<RenderGraphAttachmentDescription> Attachments;
		std::vector<RenderPassInfo> Passes;

		// TODO: Subpass dependencies?
	};

	struct FramebufferAttachmentDescription
	{
		AttachmentUsage InitialImageUsage;
		AttachmentFormat Format;
		FilterType SamplerFilter;

		FramebufferAttachmentDescription() :
		InitialImageUsage(AttachmentUsage::ColorAttachment),
		Format(AttachmentFormat::B8G8R8A8_SRGB),
		SamplerFilter(FilterType::LINEAR)
		{

		}

		FramebufferAttachmentDescription(AttachmentUsage InUsage,
			AttachmentFormat InFormat, FilterType InType) :
			InitialImageUsage(InUsage),
			Format(InFormat),
			SamplerFilter(InType)
		{

		}
	};

	struct FrameBufferCreateInfo
	{
		uint32_t							Width;
		uint32_t							Height;
		uint32_t							ColorAttachmentCount;
		FramebufferAttachmentDescription*	ColorAttachmentDescriptions;

		// Depth/stencil info
		bool								bHasDepthStencilAttachment;
		FramebufferAttachmentDescription    DepthStencilDescription;

		/**
		 * The render graph that this frame buffer is targeting
		 */ 
		RenderGraph							TargetGraph;
	};

	enum class ClearType : uint8_t
	{
		Float,
		UInt,
		SInt,
		DepthStencil
	};

	struct ClearValue
	{
		ClearType Clear;
		float FloatClearValue[4];// If the attachment uses floats, this will be used
		int32_t   IntClearValue[4] = { 0, 0, 0, 0 }; // If the attachment uses ints, this will be used
		uint32_t  UIntClearValue[4] = { 0, 0, 0, 0 }; // If the attachment uses uints, this will be used
		float Depth = 0.0f;
		uint32_t Stencil = 0;
	};

	struct RenderGraphInfo
	{
		uint32_t ClearValueCount = 1;
		ClearValue ClearValues[MAX_OUTPUT_COLORS] = { ClearValue {}};
	};

	struct ResourceSetCreateInfo
	{
		SwapChain TargetSwap;
		ResourceLayout Layout;
	};

	inline bool IsDepthFormat(AttachmentFormat Format)
	{
		return Format == AttachmentFormat::D24_UNORM_S8_UINT;
	}

	inline bool IsColorFormat(AttachmentFormat Format)
	{
		return !IsDepthFormat(Format);
	}

	// An opaque pointer to a context type
	typedef void* Context;

	/*
	 * Creates a new LLRM instance using the specified GLFW window handle.
	 *
	 * TODO: In the future, allow passing in null here for offscreen rendering.
	 *
	 * LLRM currently relies on GLFW, but a future plan is to remove the tie to a particular windowing framework.
	 */
	Context CreateContext(GLFWwindow* Window);
	void DestroyContext(llrm::Context Context);
	void SetContext(llrm::Context Context);

	/*
	 * Creates a surface for a particular window.
	 *
	 * Can be used for multi-window rendering.
	 */
	Surface CreateSurface(GLFWwindow* Window);

	template<VertexAttributeFormat Format>
	uint32_t GetVertexFormatSizeBytes()
	{
		static constexpr bool Is4Bytes = Format == VertexAttributeFormat::Float || Format == VertexAttributeFormat::Int32;
		static constexpr bool Is8Bytes = Format == VertexAttributeFormat::Float2;
		static constexpr bool Is12Bytes = Format == VertexAttributeFormat::Float3;
		static constexpr bool Is16Bytes = Format == VertexAttributeFormat::Float4;

		static_assert(Is4Bytes || Is8Bytes || Is12Bytes || Is16Bytes);

		if constexpr (Is4Bytes)
			return 4;
		else if constexpr (Is8Bytes)
			return 8;
		else if constexpr (Is12Bytes)
			return 12;
		else if constexpr (Is16Bytes)
			return 16;
		else
			return -1;
	}

	// Create primitives
	ShaderProgram CreateRasterProgram(const std::vector<uint32_t>& VertexShader, const std::vector<uint32_t>& FragmentShader);
	SwapChain CreateSwapChain(Surface TargetSurface, int32_t DesiredWidth, int32_t DesiredHeight);
	ResourceLayout CreateResourceLayout(const ResourceLayoutCreateInfo* CreateInfo);
	Pipeline CreatePipeline(const PipelineState& CreateInfo);
	RenderGraph CreateRenderGraph(const RenderGraphCreateInfo& CreateInfo);
	FrameBuffer CreateFrameBuffer(FrameBufferCreateInfo* CreateInfo);
	VertexBuffer CreateVertexBuffer(uint64_t Size, void* Data = nullptr);
	IndexBuffer CreateIndexBuffer(uint64_t Size, void* Data = nullptr);
	CommandBuffer CreateSwapChainCommandBuffer(SwapChain Target, bool bDynamic);
	CommandBuffer CreateCommandBuffer(bool bOneTimeUse = false);
	ResourceSet CreateResourceSet(ResourceSetCreateInfo* CreateInfo);
	Texture CreateTexture(uint64_t ImageSize, TextureFormat Format, uint32_t Width, uint32_t Height, void* Data);

	// Destroy primitives
	void DestroyVertexBuffer(VertexBuffer VertexBuffer);
	void DestroyIndexBuffer(IndexBuffer IndexBuffer);
	void DestroyRenderGraph(RenderGraph Graph);
	void DestroyPipeline(Pipeline Pipeline);
	void DestroyResourceLayout(ResourceLayout Layout);
	void DestroyResourceSet(ResourceSet Resources);
	void DestroyTexture(Texture Image);
	void DestroyProgram(ShaderProgram Shader);
	void DestroySwapChain(SwapChain Swap);
	void DestroySurface(Surface Surface);
	void DestroyCommandBuffer(CommandBuffer CmdBuffer);

	// Swap chain operations
	void BeginFrame(SwapChain Swap, Surface Target, int32_t FrameWidth, int32_t FrameHeight);
	void EndFrame(SwapChain Swap, Surface Target, int32_t FrameWidth, int32_t FrameHeight);
    void RecreateSwapChain(SwapChain Swap, Surface Target, int32_t DesiredWidth, int32_t DesiredHeight);
	void SubmitSwapCommandBuffer(SwapChain Target, CommandBuffer Buffer);
	void GetSwapChainSize(SwapChain Swap, uint32_t& Width, uint32_t& Height);

	// Vertex buffer operations
	void UploadVertexBufferData(VertexBuffer Buffer, void* Data, uint64_t Size);
	void UploadIndexBufferData(IndexBuffer Buffer, uint32_t* Data, uint64_t Size);
	void ResizeVertexBuffer(VertexBuffer Buffer, uint64_t NewSize);
	void ResizeIndexBuffer(IndexBuffer Buffer, uint64_t NewSize);

	// Frame buffer operations
	void GetFrameBufferSize(FrameBuffer Fbo, uint32_t& Width, uint32_t& Height) ;
	void ResizeFrameBuffer(FrameBuffer Fbo, uint32_t NewWidth, uint32_t NewHeight) ;

	// Resource set operations
	void UpdateUniformBuffer(ResourceSet Resources, SwapChain Target, uint32_t BufferIndex, void* Data, uint64_t DataSize) ;

	void UpdateTextureResource(ResourceSet Resources, SwapChain Target, Texture* Images, uint32_t ImageCount, uint32_t Binding) ;

	// deprecated, use UpdateAttachmentResources instead
	void UpdateAttachmentResource(ResourceSet Resources, SwapChain Target, FrameBuffer SrcBuffer, uint32_t AttachmentIndex, uint32_t Binding) ;
	void UpdateAttachmentResources(ResourceSet Resources, SwapChain Target, FrameBuffer* Buffers, uint32_t BufferCount, uint32_t AttachmentIndex, uint32_t Binding);

	void ReadFramebufferAttachment(FrameBuffer SrcBuffer, uint32_t Attachment, void* Dst, uint64_t BufferSize);

	// Command buffer operations
	void SubmitCommandBuffer(CommandBuffer Buffer, bool bWait = false, Fence WaitFence = nullptr);
	void Reset(CommandBuffer Buf);
	void Begin(CommandBuffer Buf);
	void End(CommandBuffer Buf);
	void TransitionFrameBufferColorAttachment(CommandBuffer Buf, FrameBuffer Source, uint32_t AttachmentIndex, AttachmentUsage Old, AttachmentUsage New);
	void TransitionFrameBufferDepthStencilAttachment(CommandBuffer Buf, FrameBuffer Source, AttachmentUsage Old, AttachmentUsage New);
	void TransitionTexture(CommandBuffer Buf, Texture Image, AttachmentUsage Old, AttachmentUsage New);
	void BeginRenderGraph(CommandBuffer Buf, RenderGraph Graph, FrameBuffer Target, RenderGraphInfo Info = {});
	void EndRenderGraph(CommandBuffer Buf);
	void BindPipeline(CommandBuffer Buf, Pipeline PipelineObject);
	void BindResources(CommandBuffer Buf, ResourceSet Resources);
	void DrawVertexBuffer(CommandBuffer Buf, VertexBuffer Vbo, uint32_t VertexCount) ;
	void DrawVertexBufferIndexed(CommandBuffer Buf, VertexBuffer Vbo, IndexBuffer Ibo, uint32_t IndexCount) ;
	void SetViewport(CommandBuffer Buf, uint32_t X, uint32_t Y, uint32_t W, uint32_t H);
	void SetScissor(CommandBuffer Buf, uint32_t X, uint32_t Y, uint32_t W, uint32_t H);

	inline void ImmediateSubmit(std::function<void(CommandBuffer)> InFunc, Fence WaitFence = nullptr, bool bWait = false)
	{
		CommandBuffer NewCmd = CreateCommandBuffer(true);

		Begin(NewCmd);
		{
			InFunc(NewCmd);
		}
		End(NewCmd);

		SubmitCommandBuffer(NewCmd, bWait, WaitFence);
		DestroyCommandBuffer(NewCmd);
	}

	inline void ImmediateSubmitAndWait(std::function<void(CommandBuffer)> InFunc, Fence WaitFence = nullptr)
	{
		ImmediateSubmit(InFunc, WaitFence, true);
	}
};