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

	// Create render graph
	llrm::RenderGraph Graph = llrm::CreateRenderGraph({
		{{llrm::AttachmentUsage::ColorAttachment, llrm::AttachmentUsage::TransferSource, llrm::AttachmentFormat::B8G8R8A8_SRGB}},
		{{{0}}}
	});

	// Create resource layout
	llrm::ResourceLayout ResourceLayout = llrm::CreateResourceLayout({
		{},
		{}
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

	while(!glfwWindowShouldClose(Window))
	{
		glfwPollEvents();
	}

	llrm::DestroyPipeline(Pipe);
	llrm::DestroyResourceLayout(ResourceLayout);
	llrm::DestroyRenderGraph(Graph);
	llrm::DestroyProgram(ExampleShader);
	llrm::DestroySurface(Surface);
	llrm::DestroyContext(Context);

	return 0;
}
