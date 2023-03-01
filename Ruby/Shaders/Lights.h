#define MAX_DIR_LIGHTS 1
#define MAX_SPOT_LIGHTS 10

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

float3 CalcDirectionalLight(DirectionalLight Light, float4 Albedo, float3 Position, float3 Normal)
{
	// Directional light
	float NDotL = max(dot(Normal, -Light.Direction), 0.0f);
	return (Albedo * NDotL).rgb;
}