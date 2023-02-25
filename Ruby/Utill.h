#pragma once
#include "Ruby.h"
#include "glm/glm.hpp"
#include <glm/gtc/matrix_transform.hpp>

namespace Ruby
{
	std::string GetProgramPath();

	std::string GetDefaultCompiledShadersPath(Ruby::RenderingAPI API);
	std::string GetDefaultShadersPath();

	void WriteBinaryFile(std::string Path, std::vector<uint32_t>& OutBytes);
	bool LoadBinaryFile(std::string Path, std::vector<uint32_t>& OutBytes);

    inline glm::mat4 BuildPerspective(float FieldOfView, float AspectRatio, float NearClip, float FarClip)
    {
        return glm::perspective(glm::radians(FieldOfView), AspectRatio, NearClip, FarClip);
    }

    inline glm::mat4 BuildTransform(glm::vec3 Position, glm::vec3 Rotation, glm::vec3 Scale)
    {
        glm::mat4 Transform(1.0f); // Initialize to identity matrix

        // Apply scale transformation
        Transform = glm::scale(Transform, Scale);

        // Apply rotation transformations (yaw, pitch, roll)
        Transform = glm::rotate(Transform, glm::radians(Rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
        Transform = glm::rotate(Transform, glm::radians(Rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
        Transform = glm::rotate(Transform, glm::radians(Rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));

        // Apply translation transformation
        Transform = glm::translate(Transform, Position);

        return Transform;
    }
}
