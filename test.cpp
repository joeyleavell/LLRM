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

int main()
{
	// Init glfw
	if (!glfwInit())
		return 1;

	// Create window
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	GLFWwindow* Window = glfwCreateWindow(800, 600, "Test", nullptr, nullptr);

	// Create context
	llrm::Context Context = llrm::CreateContext(Window);
	if(!Context)
	{
		std::cout << "Failed to create rendering context" << std::endl;
		return 1;
	}

	// Create surface
	llrm::Surface Surface = llrm::CreateSurface(Window);
	if(!Surface)
	{
		std::cout << "Failed to create surface" << std::endl;
		return 1;
	}

	if (!CreateShaders())
		return 2;

	// Create swap chain
	llrm::SwapChain Swap = llrm::CreateSwapChain(Surface, 800, 600);

	// Create resource layout
	llrm::ResourceLayout ResourceLayout = llrm::CreateResourceLayout({
		{},
		{}
	});

	// Create render graph
	llrm::RenderGraph Graph = llrm::CreateRenderGraph({
		{{llrm::AttachmentUsage::Undefined, llrm::AttachmentUsage::Presentation, llrm::GetTextureFormat(llrm::GetSwapChainImage(Swap, 0))}},
		{{{0}}}
	});

	// Create pipeline state
	llrm::Pipeline Pipe = llrm::CreatePipeline({
		ExampleShader,
		Graph,
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
	llrm::VertexBuffer Vbo = llrm::CreateVertexBuffer(sizeof(Verts), Verts);

	std::vector<llrm::FrameBuffer> Fbos(llrm::GetSwapChainImageCount(Swap));
	std::vector<llrm::CommandBuffer> CmdBuffers(Fbos.size());

	for(uint32_t Image = 0; Image < Fbos.size(); Image++)
	{
		Fbos[Image] = llrm::CreateFrameBuffer({
			800, 600,
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
				llrm::SetViewport(Buf, 0, 0, 800, 600);
				llrm::SetScissor(Buf, 0, 0, 800, 600);

				llrm::BindPipeline(Buf, Pipe);
				llrm::DrawVertexBuffer(Buf, Vbo, 3);
			}
			llrm::EndRenderGraph(Buf);
		}
		llrm::End(Buf);
	}

	while(!glfwWindowShouldClose(Window))
	{
		glfwPollEvents();

		int32_t ImageIndex = llrm::BeginFrame(Swap, Surface, 800, 600);
		if(ImageIndex >= 0)
		{
			llrm::Present(Swap, Surface, { CmdBuffers[ImageIndex] }, 800, 600);
		}
	}

	llrm::DestroySwapChain(Swap);
	llrm::DestroyVertexBuffer(Vbo);
	//llrm::DestroyFrameBuffer(Fbo);
	//llrm::DestroyTexture(ColorAttachment);
	llrm::DestroyPipeline(Pipe);
	llrm::DestroyResourceLayout(ResourceLayout);
	llrm::DestroyRenderGraph(Graph);
	llrm::DestroyProgram(ExampleShader);
	llrm::DestroySurface(Surface);
	llrm::DestroyContext(Context);

	return 0;
}
