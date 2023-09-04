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
	float	InnerAngle;
	float	OuterAngle;
};

// Returns the step-wise solution to the standard reflectance rendering equation
float3 LightRadiance(float3 BRDF, float3 Radiance, float3 NDotL)
{
	return BRDF * Radiance * NDotL;
}

float CalcSpotAttenuation(float3 Point, float3 LightPosition, float3 SpotDirection, float InnerAngle, float OuterAngle)
{
	float3 ToLight = LightPosition - Point;
	float Distance2 = dot(ToLight, ToLight);
	float3 L = normalize(ToLight);

	// Inner angle is 1 - cos(InnerAngle), Outer angle is 1 - cos(Outer angle)
	// Allows for fast computation in shader
	float RadialAtten = saturate((dot(L, -SpotDirection) - OuterAngle) * InnerAngle);
	RadialAtten *= RadialAtten; // Square it

	return rcp(Distance2) * RadialAtten;
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

// Calculates the radiance for a spot light using a BRDF, units are in candelas/m^2
float3 CalcSpotLightRadiance(
	SpotLight Light, // Spot light data
	Material Mat, // Material parameters
	float3 Normal, // Surface normal
	float3 Position, // World-space surface position
	float3 V, // Unit vector pointing toward's viewer
	float NDotV // Pre-computed surface normal dot product unit view vector
)
{
	float Atten = CalcSpotAttenuation(Position, Light.Position, Light.Direction, Light.InnerAngle, Light.OuterAngle);
	float Radiance = Light.Intensity * Light.Color * Atten; // Attenuation is 1/m^2, this becomes candelas/m^2

	float3 L = normalize(Light.Position - Position);
	float3 H = normalize(V + L);
	float NDotL = max(dot(Normal, L), 0.0f);
	float NDotH = max(dot(Normal, H), 0.0f);

	float3 BRDF = CookTorrenceBRDF(Mat, NDotL, NDotV, NDotH);
	return LightRadiance(BRDF, Radiance, NDotL);
}

float4x4 ExtractViewProjMatrix(Texture2D<float4> Tex, uint FrustumIndex)
{
	float4 Row0 = Tex.Load(int3(FrustumIndex * 4 + 0, 0, 0));
	float4 Row1 = Tex.Load(int3(FrustumIndex * 4 + 1, 0, 0));
	float4 Row2 = Tex.Load(int3(FrustumIndex * 4 + 2, 0, 0));
	float4 Row3 = Tex.Load(int3(FrustumIndex * 4 + 3, 0, 0));

	return float4x4(Row0, Row1, Row2, Row3);
}