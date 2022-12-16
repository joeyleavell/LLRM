#include <iostream>

#include "llrm.h"
#include "GLFW/glfw3.h"

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

	while(!glfwWindowShouldClose(Window))
	{
		glfwPollEvents();
	}

	llrm::DestroySurface(Surface);
	llrm::DestroyContext(Context);

	return 0;
}
