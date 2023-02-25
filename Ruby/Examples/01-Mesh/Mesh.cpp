#include "Ruby.h"
#include "ShaderManager.h"
#include "GLFW/glfw3.h"
#include "MeshGenerator.h"
#include "Utill.h"
#include <chrono>

static Ruby::ObjectId gCube;

Ruby::SceneId CreateScene()
{
	Ruby::SceneId NewScene = Ruby::CreateScene();

	// Create object in front of camera
	Ruby::Mesh Floor = Ruby::CreateMesh(TesselateRectPrism({ 0, 0, 0 }, { 10, 10, 10 }));
	gCube = Ruby::CreateObject(Floor, { 0, 0, -30.0f }, {0, 0, 0});

	Ruby::AddObject(NewScene, gCube);

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

	auto Last = std::chrono::high_resolution_clock::now();

	while(!glfwWindowShouldClose(Wnd))
	{
		auto Now  = std::chrono::high_resolution_clock::now();
		auto Delta = std::chrono::duration_cast<std::chrono::nanoseconds>(Now - Last);
		Last = Now;
		float DeltaSeconds = static_cast<float>(Delta.count() / 1e9) * 50.0f;

		Ruby::GetObject(gCube).mRotation.x += DeltaSeconds;
		Ruby::GetObject(gCube).mRotation.y += DeltaSeconds;
		Ruby::GetObject(gCube).mRotation.z += DeltaSeconds;

		glfwPollEvents();
		int32_t Width{}, Height{};
		glfwGetFramebufferSize(Wnd, &Width, &Height);

		Ruby::Camera Cam{};
		Cam.mProjection = Ruby::BuildPerspective(70.0f, Width / (float)Height, 0.1f, 100.0f);

		Ruby::RenderScene(NewScene, glm::ivec2{Width, Height}, Cam, Swap);
	}

	return 0;
}
