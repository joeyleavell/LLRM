#pragma once

#include "glm/glm.hpp"

struct CameraVertexUniforms
{
	glm::mat4 mViewProjection;
};

struct ModelVertexUniforms
{
	glm::mat4 mTransform;
};