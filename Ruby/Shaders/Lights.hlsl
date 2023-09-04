#include "Material.hlsl"
#include "BRDF.hlsl"

#define MAX_DIR_LIGHTS 1000
#define MAX_SPOT_LIGHTS 1000

struct DirectionalLight
{
	float3	Direction;
	float3	Color;
	float	Intensity;
};

struct SpotLight
{
	float3	Position;
	float3	Direction;
	float3	Color;
	float	Intensity;
};

// Returns the step-wise solution to the standard reflectance rendering equation
float3 LightRadiance(float3 BRDF, float3 Radiance, float3 NDotL)
{
	return BRDF * Radiance * NDotL;
}

// Calculates the radiance for a directional light using a BRDF, units are in candelas/m^2
float3 CalcDirectionalLightRadiance(DirectionalLight Light, Material Mat, float3 Normal, float3 V, float NDotV)
{
	float3 L = normalize(-Light.Direction);
	float3 H = normalize(V + L);
	float NDotL = max(dot(Normal, L), 0.0f);
	float NDotH = max(dot(Normal, H), 0.0f);

	float3 BRDF = CookTorrenceBRDF(Mat, NDotL, NDotV, NDotH);
	return LightRadiance(BRDF, Light.Intensity * Light.Color, NDotL);
}

float4x4 ExtractViewProjMatrix(Texture2D<float4> Tex, uint FrustumIndex)
{
	float4 Row0 = Tex.Load(int3(FrustumIndex * 4 + 0, 0, 0));
	float4 Row1 = Tex.Load(int3(FrustumIndex * 4 + 1, 0, 0));
	float4 Row2 = Tex.Load(int3(FrustumIndex * 4 + 2, 0, 0));
	float4 Row3 = Tex.Load(int3(FrustumIndex * 4 + 3, 0, 0));

	return float4x4(Row0, Row1, Row2, Row3);
}