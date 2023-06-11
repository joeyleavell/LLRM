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

struct ShadowLightUniforms
{
	glm::mat4 mViewProjection;
};

#define MAX_DIR_LIGHTS 1
#define MAX_SPOT_LIGHTS 10

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
	alignas(16) DirectionalLight mDirectionalLights[MAX_DIR_LIGHTS];
	alignas(16) SpotLight		 mSpotLights[MAX_SPOT_LIGHTS];
	alignas(4) uint32_t			 mNumDirLights = 0;
	alignas(4) uint32_t			 mNumSpotLights = 0;
};