#include "Ruby.h"
#include "ShaderManager.h"
#include "GLFW/glfw3.h"
#include "MeshGenerator.h"
#include "Utill.h"
#include <chrono>
#include <iostream>
#include "glm/gtx/rotate_vector.hpp"
#include "glm/gtx/quaternion.hpp"

#include "ImGuiSupport.h"

static Ruby::ObjectId gCube;

Ruby::SceneId CreateScene()
{
	Ruby::SceneId NewScene = Ruby::CreateScene();

	// Create material
	Ruby::Material& DefaultMaterial = Ruby::CreateMaterial();
	DefaultMaterial.Roughness = 0.4f;
	DefaultMaterial.Metallic = 0.0f;
	DefaultMaterial.Albedo = glm::vec3(1.0f, 0.0f, 1.0f);

	// Create object in front of camera
	Ruby::Mesh& CubeMesh = Ruby::CreateMesh(TesselateRectPrism({ 0.1f, 0.1f, 0.1f }), DefaultMaterial.mId);
	gCube = Ruby::CreateMeshObject(CubeMesh, { 0, 0, -0.3f }, { 0, 0, 0 });
	Ruby::AddObject(NewScene, gCube);

	// Create floor 
	Ruby::Mesh& Floor = Ruby::CreateMesh(TesselateRectPrism({ 10, 0.01, 10 }));
	Ruby::ObjectId FloorObj = Ruby::CreateMeshObject(Floor, { 0, -0.2f, 0.0f }, { 0, 0, 0 });
	Ruby::AddObject(NewScene, FloorObj);

	Ruby::Light& Dir = Ruby::CreateLight(Ruby::LightType::Directional, { 0.0f, 1.0f, 0.0f }, 10.0f, Ruby::LightUnit::Lumen, true);
	Ruby::ObjectId DirLight1 = Ruby::CreateLightObject(Dir.mId, { 0.0f, 0.3f, 0.0f}, { -45.0f, 0.0f, 0.0f });
	Ruby::AddObject(NewScene, DirLight1);

	for(uint32_t x = 0; x < 20; x++)
	{
		for(uint32_t y = 0; y < 20; y++)
		{
			float xPos = x * 0.01f, zPos = y * 0.01f;
			// Create directional light facing (0.0, 0.0, -1.0)
			Ruby::Light& Spot = Ruby::CreateSpotLight({ 1.0f, 1.0f, 1.0f }, 1.0f, Ruby::LightUnit::Lumen, 1.0f, 1.0);
			Ruby::ObjectId DirLight = Ruby::CreateLightObject(Spot.mId, { xPos, 0.3f, zPos }, { -90.0f, 0.0f, 0.0f });
			Ruby::AddObject(NewScene, DirLight);
		}
	}

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

	}

	return NewScene;
}

static bool NeedsResize = false;
uint32_t Width, Height;

glm::vec3 CamPosition = glm::vec3(0.0f, 0.0f, 0.0f);
glm::quat CamRotation = glm::quat(glm::vec3(0.0f, 0.0f, 0.0f));

glm::vec3 ForwardVector(glm::quat Rot)
{
	glm::vec4 Forward = glm::vec4(0, 0, -1.0f, 0);

	return Rot * Forward;
}

glm::vec3 RightVector(glm::quat Rot)
{
	glm::vec4 Right = glm::vec4(1, 0, 0.0f, 0);

	return Rot * Right;
}

glm::vec3 UpVector()
{
	glm::vec4 Up = glm::vec4(0, 1, 0.0f, 0);

	return Up;
}

Ruby::RenderSettings Settings = { false };

void DrawEditorWindows()
{
	ImGui::Begin("Properties");
	{
		float Values[3] = {CamPosition.x, CamPosition.y, CamPosition.z};
		ImGui::InputFloat3("Camera Pos", Values);

		glm::vec3 Euler = glm::degrees(glm::eulerAngles(CamRotation));
		float Values2[3] = { Euler.x, Euler.y, Euler.z};
		ImGui::InputFloat3("Camera Rot", Values2); 

	}
	ImGui::End();

	ImGui::Begin("RenderSettings");
	{
		if(ImGui::CollapsingHeader("Lighting"))  
		{ 
			ImGui::Checkbox("Shadows", &Settings.mShadowsEnabled);
		}
	}
	ImGui::End();

	ImGui::ShowDemoWindow();
} 

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

			if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
			{
				glm::vec3 Forward = ForwardVector(CamRotation);
				glm::vec3 Right = RightVector(CamRotation);
				glm::vec3 Up = UpVector();

				glm::vec3 Movement = glm::vec3(0.0f);
				float Speed = 0.001f;

				// Movement
				if (ImGui::IsKeyDown(ImGuiKey_A))
				{
					Movement += -1.0f * Right * Speed;
				}
				if (ImGui::IsKeyDown(ImGuiKey_D))
				{
					Movement += 1.0f * Right * Speed;
				}
				if (ImGui::IsKeyDown(ImGuiKey_W))
				{
					Movement += 1.0f * Forward * Speed;
				}
				if (ImGui::IsKeyDown(ImGuiKey_S))
				{
					Movement += -1.0f * Forward * Speed;
				}

				if (ImGui::IsKeyDown(ImGuiKey_E))
				{
					Movement += 1.0f * Up * Speed;
				}
				if (ImGui::IsKeyDown(ImGuiKey_Q))
				{
					Movement += -1.0f * Up * Speed;
				}

				ImVec2 DragDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
				if(DragDelta.x != 0 || DragDelta.y != 0)
				{
					float PitchMovement = -DragDelta.y * 0.005f;
					float YawMovement = -DragDelta.x * 0.005f;

					glm::quat YawRotation = glm::angleAxis(YawMovement, glm::vec3(0.0f, 1.0f, 0.0f));
					glm::quat PitchRotation = glm::angleAxis(PitchMovement, RightVector(CamRotation));

					glm::quat NewRot = YawRotation * PitchRotation * CamRotation;
					bool TopAngle = glm::dot(ForwardVector(NewRot), UpVector()) < std::cos(5.0f * 3.14 / 180.0f);
					bool BottomAngle = glm::dot(ForwardVector(NewRot), -UpVector()) < std::cos(5.0f * 3.14 / 180.0f);
					if(!TopAngle || !BottomAngle)
					{
						NewRot = YawRotation * CamRotation;
					}

					CamRotation = NewRot;

					ImGui::ResetMouseDragDelta(ImGuiMouseButton_Right);
				}

				CamPosition += Movement;
			}

			ImGui::End();

			DrawEditorWindows();
		}
		EndImGuiFrame();
		UpdateImGuiViewports();

		int32_t Width{}, Height{};
		glfwGetFramebufferSize(Wnd, &Width, &Height);

		Ruby::Camera Cam{};
		Cam.mProjection = Ruby::BuildPerspective(70.0f, Width / (float)Height, 0.01f, 1500.0f);
		Cam.mPosition = CamPosition;
		Cam.mRotation = CamRotation;

		Ruby::RenderScene(NewScene, Target, glm::ivec2{ Width, Height }, Cam, Swap, Settings,
		[](const llrm::CommandBuffer& Buf)
		{
			RecordImGuiCmds(Buf);
		}
		);

	}

	return 0;
}
