#pragma once

#include "glm/glm.hpp"

struct CameraVertexUniforms
{
	alignas(4) glm::mat4 mViewProjection;
};

struct ModelVertexUniforms
{
	glm::mat4 mTransform;
};

struct ShadowLightUniforms
{
	glm::mat4 mViewProjection;
};

#define MAX_DIR_LIGHTS 1000
#define MAX_SPOT_LIGHTS 1000

struct DirectionalLight
{
	alignas(16) glm::vec3 mDirection;
	alignas(16) glm::vec3 mColor;
	alignas(4)	float	  mIntensity;
	alignas(4)  bool	  mCastShadows;
	alignas(4)  uint32_t  mFrustumBase;
};

struct SpotLight
{
	alignas(16) glm::vec3 mPosition;
	alignas(16) glm::vec3 mDirection;
	alignas(16) glm::vec3 mColor;
	alignas(4)  float	  mIntensity;
};

struct SceneLights
{

};