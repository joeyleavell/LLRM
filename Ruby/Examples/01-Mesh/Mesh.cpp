#include "Ruby.h"
#include "ShaderManager.h"
#include "GLFW/glfw3.h"
#include "MeshGenerator.h"

Ruby::SceneId CreateScene()
{
	Ruby::SceneId NewScene = Ruby::CreateScene();

	// Create object in front of camera
	Ruby::Mesh Floor = Ruby::CreateMesh(TesselateRectPrism({ 0, 0, 0 }, { 10, 10, 10 }));
	Ruby::Object Obj = Ruby::CreateObject(Floor, { 0, 0, 5.0f });

	Ruby::AddObject(NewScene, Obj);

	return NewScene;
}

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

	Ruby::SceneId NewScene = CreateScene();

	while(!glfwWindowShouldClose(Wnd))
	{
		glfwPollEvents();

		Ruby::RenderScene(NewScene, Swap);
	}

	return 0;
}
