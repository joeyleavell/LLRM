#include "Ruby.h"
#include "ShaderManager.h"
#include "GLFW/glfw3.h"

int main()
{
	// Create window
	if(!glfwInit())
	{
		return 0;
	}

#if LLRM_VULKAN
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
#endif

	GLFWwindow* Wnd = glfwCreateWindow(800, 600, "Mesh Test", nullptr, nullptr);

	Ruby::ContextParams Params{};
	Ruby::CreateContext(Params);

	Ruby::SwapChain Swap = Ruby::CreateSwapChain(Wnd);

	Ruby::Scene NewScene(0);

	while(!glfwWindowShouldClose(Wnd))
	{
		glfwPollEvents();

		Ruby::RenderScene(NewScene, Swap);
	}

	return 0;
}
