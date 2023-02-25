#include "Ruby.h"
#include "ShaderManager.h"
#include "GLFW/glfw3.h"
#include "MeshGenerator.h"
#include "Utill.h"

Ruby::SceneId CreateScene()
{
	Ruby::SceneId NewScene = Ruby::CreateScene();

	// Create object in front of camera
	Ruby::Mesh Floor = Ruby::CreateMesh(TesselateRectPrism({ 0, 0, 0 }, { 10, 10, 10 }));
	Ruby::Object Obj = Ruby::CreateObject(Floor, { 0, 0, -90.0f });

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
		int32_t Width{}, Height{};
		glfwGetFramebufferSize(Wnd, &Width, &Height);

		Ruby::Camera Cam{};
		Cam.mProjection = Ruby::BuildPerspective(70.0f, Width / (float)Height, 0.1f, 100.0f);

		Ruby::RenderScene(NewScene, glm::ivec2{Width, Height}, Cam, Swap);
	}

	return 0;
}
