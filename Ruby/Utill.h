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
        return glm::perspectiveRH_ZO(glm::radians(FieldOfView), AspectRatio, NearClip, FarClip);
    }

    inline glm::mat4 BuildTransformQuat(glm::vec3 Position, glm::quat Rotation, glm::vec3 Scale)
    {
        return glm::translate(glm::mat4(1.0f), Position) *
            glm::toMat4(Rotation) *
            //glm::rotate(glm::mat4(1.0f), glm::radians(Rotation.z), glm::vec3(0.0f, 0.0f, 1.0f)) *
            //glm::rotate(glm::mat4(1.0f), glm::radians(Rotation.y), glm::vec3(0.0f, 1.0f, 0.0f)) *
            //glm::rotate(glm::mat4(1.0f), glm::radians(Rotation.x), glm::vec3(1.0f, 0.0f, 0.0f)) *
            glm::scale(glm::mat4(1.0f), Scale);
    }

    inline glm::mat4 BuildTransform(glm::vec3 Position, glm::vec3 Rotation, glm::vec3 Scale)
    {
        return glm::translate(glm::mat4(1.0f), Position) *
        	glm::rotate(glm::mat4(1.0f), glm::radians(Rotation.z), glm::vec3(0.0f, 0.0f, 1.0f)) *
            glm::rotate(glm::mat4(1.0f), glm::radians(Rotation.y), glm::vec3(0.0f, 1.0f, 0.0f)) *
            glm::rotate(glm::mat4(1.0f), glm::radians(Rotation.x), glm::vec3(1.0f, 0.0f, 0.0f)) *
            glm::scale(glm::mat4(1.0f), Scale);
    }
}
