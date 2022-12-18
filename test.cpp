#include <iostream>

#include "llrm.h"
#include "shadercompile.h"
#include "GLFW/glfw3.h"

llrm::ShaderProgram ExampleShader{};

bool CreateShaders()
{
	InitShaderCompilation();

	ShaderCompileResult Result;
	if(CompileShader("Example.vert", "Example.frag", Result))
	{
		ExampleShader = llrm::CreateRasterProgram(Result.OutVertShader, Result.OutFragShader);
	}

	FinishShaderCompilation();

	return ExampleShader != nullptr;
}

struct ShaderVertex
{
	float Position[2];
};

void CreateSwapChain(llrm::SwapChain Swap, llrm::RenderGraph Graph, llrm::Pipeline Pipe, llrm::VertexBuffer Vbo, std::vector<llrm::FrameBuffer>& Fbos, std::vector<llrm::CommandBuffer>& CmdBuffers)
{
	uint32_t Width, Height;
	llrm::GetSwapChainSize(Swap, Width, Height);

	CmdBuffers.resize(llrm::GetSwapChainImageCount(Swap), nullptr);
	Fbos.resize(CmdBuffers.size(), nullptr);

	for (uint32_t Image = 0; Image < Fbos.size(); Image++)
	{
		if(Fbos[Image])
			llrm::DestroyFrameBuffer(Fbos[Image]);

		if(CmdBuffers[Image])
			llrm::DestroyCommandBuffer(CmdBuffers[Image]);

		Fbos[Image] = llrm::CreateFrameBuffer({
			Width, Height,
			{llrm::GetSwapChainImage(Swap, Image)},
			Graph
			});

		CmdBuffers[Image] = llrm::CreateCommandBuffer();
		llrm::CommandBuffer Buf = CmdBuffers[Image];

		llrm::Begin(Buf);
		{
			//llrm::TransitionTexture(Buf, llrm::GetSwapChainImage(Swap, Image), llrm::AttachmentUsage::Presentation, llrm::AttachmentUsage::ColorAttachment);
			llrm::BeginRenderGraph(Buf, Graph, Fbos[Image], { {llrm::ClearType::Float, 0.0f, 0.0f, 0.0f, 1.0f} });
			{
				llrm::SetViewport(Buf, 0, 0, Width, Height);
				llrm::SetScissor(Buf, 0, 0, Width, Height);

				llrm::BindPipeline(Buf, Pipe);
				llrm::DrawVertexBuffer(Buf, Vbo, 3);
			}
			llrm::EndRenderGraph(Buf);
		}
		llrm::End(Buf);
	}
}

int main()
{
	// Init glfw
	if (!glfwInit())
		return 1;

	// Create window
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	GLFWwindow* Window = glfwCreateWindow(800, 600, "Test", nullptr, nullptr);

	struct WindowData
	{
		llrm::SwapChain Swap;
		llrm::Surface Surface;
		std::vector<llrm::FrameBuffer> Fbos;
		std::vector<llrm::CommandBuffer> CmdBuffers;

		llrm::VertexBuffer Vbo;
		llrm::RenderGraph Graph;
		llrm::Pipeline Pipe;
	} WndDat;

	glfwSetWindowUserPointer(Window, &WndDat);

	// Create context
	llrm::Context Context = llrm::CreateContext(Window);
	if(!Context)
	{
		std::cout << "Failed to create rendering context" << std::endl;
		return 1;
	}

	// Create surface
	WndDat.Surface = llrm::CreateSurface(Window);
	if(!WndDat.Surface)
	{
		std::cout << "Failed to create surface" << std::endl;
		return 1;
	}

	if (!CreateShaders())
		return 2;

	// Create swap chain
	WndDat.Swap = llrm::CreateSwapChain(WndDat.Surface, 800, 600);

	// Create resource layout
	llrm::ResourceLayout ResourceLayout = llrm::CreateResourceLayout({
		{},
		{}
	});

	// Create render graph
	WndDat.Graph = llrm::CreateRenderGraph({
		{{llrm::AttachmentUsage::Undefined, llrm::AttachmentUsage::Presentation, llrm::GetTextureFormat(llrm::GetSwapChainImage(WndDat.Swap, 0))}},
		{{{0}}}
	});

	// Create pipeline state
	WndDat.Pipe = llrm::CreatePipeline({
		ExampleShader,
		WndDat.Graph,
		ResourceLayout,
		sizeof(ShaderVertex),
		{std::make_pair(llrm::VertexAttributeFormat::Float2, offsetof(ShaderVertex, Position))},
		llrm::PipelineRenderPrimitive::TRIANGLES,
		{{false}},
		{{false}},
		0
	});

	// Create frame buffer attachments
	/*llrm::Texture ColorAttachment = llrm::CreateTexture(llrm::AttachmentFormat::B8G8R8A8_SRGB, 800, 600, llrm::TEXTURE_USAGE_RT);
	llrm::FrameBuffer Fbo = llrm::CreateFrameBuffer({
		800, 600,
		{ColorAttachment},
		Graph
	});*/

	// Create vertex buffer
	ShaderVertex Verts[3] = {
		{1.0f, -1.0f},
		{0.0f, 1.0f},
		{-1.0f, -1.0f}
	};
	WndDat.Vbo = llrm::CreateVertexBuffer(sizeof(Verts), Verts);

	CreateSwapChain(WndDat.Swap, WndDat.Graph, WndDat.Pipe, WndDat.Vbo, WndDat.Fbos, WndDat.CmdBuffers);

	glfwSetWindowSizeCallback(Window, [](GLFWwindow* Wnd, int32_t Width, int32_t Height)
	{
		WindowData* WndDat = static_cast<WindowData*>(glfwGetWindowUserPointer(Wnd));

		if(Width > 0 || Height > 0)
		{
			// Recreate swap
			llrm::RecreateSwapChain(WndDat->Swap, WndDat->Surface, Width, Height);

			// Recreate attachments
			CreateSwapChain(WndDat->Swap, WndDat->Graph, WndDat->Pipe, WndDat->Vbo, WndDat->Fbos, WndDat->CmdBuffers);
		}
	});

	while(!glfwWindowShouldClose(Window))
	{
		glfwPollEvents();

		int32_t ImageIndex = llrm::BeginFrame(Window, WndDat.Swap, WndDat.Surface);
		if(ImageIndex >= 0)
		{
			llrm::EndFrame({ WndDat.CmdBuffers[ImageIndex] });
		}
	}

	for (uint32_t Image = 0; Image < WndDat.Fbos.size(); Image++)
	{
		llrm::DestroyFrameBuffer(WndDat.Fbos[Image]);
		llrm::DestroyCommandBuffer(WndDat.CmdBuffers[Image]);
	}

	llrm::DestroySwapChain(WndDat.Swap);
	llrm::DestroyVertexBuffer(WndDat.Vbo);
	//llrm::DestroyFrameBuffer(Fbo);
	//llrm::DestroyTexture(ColorAttachment);
	llrm::DestroyPipeline(WndDat.Pipe);
	llrm::DestroyResourceLayout(ResourceLayout);
	llrm::DestroyRenderGraph(WndDat.Graph);
	llrm::DestroyProgram(ExampleShader);
	llrm::DestroySurface(WndDat.Surface);
	llrm::DestroyContext(Context);

	return 0;
}
