#define MAX_DIR_LIGHTS 1
#define MAX_SPOT_LIGHTS 10

struct DirectionalLight
{
	float3	Direction;
	float3	Color;
	float	Intensity;

	bool CastShadows;
  	uint FrustumBase;
};

struct SpotLight
{
	float3	Position;
	float3	Direction;
	float3	Color;
	float	Intensity;
};

float3 CalcDirectionalLight(DirectionalLight Light, float4 Albedo, float3 Position, float3 Normal)
{
	// Directional light
	float NDotL = max(dot(Normal, -Light.Direction), 0.0f);
	return (Albedo * NDotL).rgb;
}

float4x4 ExtractViewProjMatrix(Texture2D<float4> Tex, uint FrustumIndex)
{
	float4 Row0 = Tex.Load(int3(FrustumIndex * 4 + 0, 0, 0));
	float4 Row1 = Tex.Load(int3(FrustumIndex * 4 + 1, 0, 0));
	float4 Row2 = Tex.Load(int3(FrustumIndex * 4 + 2, 0, 0));
	float4 Row3 = Tex.Load(int3(FrustumIndex * 4 + 3, 0, 0));

	return float4x4(Row0, Row1, Row2, Row3);
}