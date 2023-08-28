#include "Ruby.h"
#include "ShaderManager.h"
#include "GLFW/glfw3.h"
#include "MeshGenerator.h"
#include "Utill.h"
#include <chrono>

#include "ImGuiSupport.h"

static Ruby::ObjectId gCube;

Ruby::SceneId CreateScene()
{
	Ruby::SceneId NewScene = Ruby::CreateScene();

	// Create object in front of camera
	Ruby::Mesh CubeMesh = Ruby::CreateMesh(TesselateRectPrism({ 10, 10, 10 }));
	gCube = Ruby::CreateMeshObject(CubeMesh, { 0, 0, -30.0f }, { 0, 0, 0 });
	Ruby::AddObject(NewScene, gCube);

	// Create floor 
	Ruby::Mesh Floor = Ruby::CreateMesh(TesselateRectPrism({ 1000, 1, 1000 }));
	Ruby::ObjectId FloorObj = Ruby::CreateMeshObject(Floor, { 0, -20.0f, 0.0f }, { 0, 0, 0 });
	Ruby::AddObject(NewScene, FloorObj);

	uint32_t Lights = 1;
	float Scaler = 0.1f;
	for (uint32_t i = 0; i < Lights; i++)
	{
		float rand1 = ((rand() % 1000) / 1000.0f);
		float rand2 = ((rand() % 1000) / 1000.0f);
		float Min = -10.0f;
		float Max = -170.0f;
		float AngleX = rand1 * (Max - Min) + Min;
		float AngleY = rand2 * (Max - Min) + Min;

		// Create directional light facing (0.0, 0.0, -1.0)
		Ruby::LightId Dir = Ruby::CreateLight(Ruby::LightType::Directional, { 1.0f, 1.0f, 1.0f }, 1.0f / Lights * Scaler, true);
		Ruby::ObjectId DirLight = Ruby::CreateLightObject(Dir, {}, { AngleX, AngleY, 0.0f });
		Ruby::AddObject(NewScene, DirLight);
	}

	return NewScene;
}

static bool NeedsResize = false;
uint32_t Width, Height;

int main()
{
	// Create window
	if (!glfwInit())
	{
		return 0;
	}

#if LLRM_VULKAN
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
#endif

	GLFWwindow* Wnd = glfwCreateWindow(800, 600, "Mesh Test", nullptr, nullptr);

	Ruby::ContextParams Params{};
	Ruby::RubyContext Context = Ruby::CreateContext(Params);

	Ruby::SwapChain Swap = Ruby::CreateSwapChain(Wnd);
	Ruby::RenderTarget Target = Ruby::CreateRenderTarget(800, 600);

	Ruby::SceneId NewScene = CreateScene();

	auto Last = std::chrono::high_resolution_clock::now();

	InitImGui(Wnd, Context.LLContext, Swap.mSwap, Swap.mTonemapGraph, { true, false,  true });

	glfwSetWindowSizeCallback(Wnd, [](GLFWwindow* Wnd, int NewWidth, int NewHeight)
	{
		NeedsResize = true;
		Width = NewWidth;
		Height = NewHeight;
	});

	while (!glfwWindowShouldClose(Wnd))
	{
		auto Now = std::chrono::high_resolution_clock::now();
		auto Delta = std::chrono::duration_cast<std::chrono::nanoseconds>(Now - Last);
		Last = Now;
		float DeltaSeconds = static_cast<float>(Delta.count() / 1e9) * 50.0f;

		Ruby::GetObject(gCube).mRotation.x += DeltaSeconds;
		Ruby::GetObject(gCube).mRotation.y += DeltaSeconds;
		Ruby::GetObject(gCube).mRotation.z += DeltaSeconds;

		glfwPollEvents();
		if(NeedsResize)
		{
			Ruby::ResizeRenderTarget(Target, Width, Height);
			Ruby::ResizeSwapChain(Swap, Width, Height);

			NeedsResize = false;
		}

		BeginImGuiFrame();
		{
			const ImGuiViewport* Viewport = ImGui::GetMainViewport();
			ImGuiDockNodeFlags DockspaceFlags = ImGuiDockNodeFlags_PassthruCentralNode;

			ImGui::SetNextWindowPos(Viewport->WorkPos);
			ImGui::SetNextWindowSize(Viewport->WorkSize);
			ImGui::SetNextWindowViewport(Viewport->ID);

			ImGuiWindowFlags HostWindowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking;
			HostWindowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
			HostWindowFlags |= ImGuiWindowFlags_NoBackground;

			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
			ImGui::Begin("Editor", NULL, HostWindowFlags);

			ImGui::PopStyleVar(3);

			ImGuiID DockspaceId = ImGui::GetID("DockSpace");
			ImGui::DockSpace(DockspaceId, ImVec2(0.0f, 0.0f), DockspaceFlags, nullptr);
			ImGui::End();

			ImGui::ShowDemoWindow();
		}
		EndImGuiFrame();
		UpdateImGuiViewports();

		int32_t Width{}, Height{};
		glfwGetFramebufferSize(Wnd, &Width, &Height);

		Ruby::Camera Cam{};
		Cam.mProjection = Ruby::BuildPerspective(70.0f, Width / (float)Height, 0.1f, 150.0f);
		Cam.mPosition.z = 10.0f;

		Ruby::RenderScene(NewScene, Target, glm::ivec2{ Width, Height }, Cam, Swap,
		[](const llrm::CommandBuffer& Buf)
		{
			RecordImGuiCmds(Buf);
		}
		);

	}

	return 0;
}
