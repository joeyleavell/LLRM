#pragma once
#include "llrm.h"
#include "imgui.h"

#ifdef LLRM_BUILD_IMGUI

struct ImGuiFeatures
{
	bool EnableDocking = true;
	bool EnableViewports = true;
	bool EnableSRGB = true;
};

ImGuiContext* InitImGui(GLFWwindow* Wnd, llrm::Context Context, llrm::SwapChain Swap, llrm::RenderGraph Graph, ImGuiFeatures Features);
void BeginImGuiFrame();
void EndImGuiFrame();
void RecordImGuiCmds(llrm::CommandBuffer Dst);
void ShutdownImGui();

void UpdateImGuiViewports();

#endif