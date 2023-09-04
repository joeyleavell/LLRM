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

float3 CalcDirectionalLight(DirectionalLight Light, Material Mat, float3 Position, float3 Normal, float3 View)
{

	float3 L = normalize(-Light.Direction);
	float3 V = normalize(View - Position);
	float3 H = normalize(V + L);
	float NDotL = max(dot(Normal, L), 0.0f);

	float3 F0 = float3(0.1);
	F0 = lerp(F0, Mat.Albedo, Mat.Metallic);
	float3 ReflectanceRatio = FresnelSchlick(max(dot(V, Normal), 0.0), F0);

	float NDF = DistributionGGX(Normal, H, Mat.Roughness);       
	float G   = GeometrySmith(Normal, V, L, Mat.Roughness);

	float3 numerator    = NDF * G * ReflectanceRatio;
	float denominator = 4.0 * max(dot(Normal, V), 0.0) * NDotL + 0.0001;
	float3 specular     = numerator / denominator;  

	float3 RefractanceRatio = 1.0f - ReflectanceRatio;
	RefractanceRatio *= 1.0f - Mat.Metallic;

	return (RefractanceRatio * Mat.Albedo / PI + specular) * Light.Intensity * Light.Color * NDotL;
}

float4x4 ExtractViewProjMatrix(Texture2D<float4> Tex, uint FrustumIndex)
{
	float4 Row0 = Tex.Load(int3(FrustumIndex * 4 + 0, 0, 0));
	float4 Row1 = Tex.Load(int3(FrustumIndex * 4 + 1, 0, 0));
	float4 Row2 = Tex.Load(int3(FrustumIndex * 4 + 2, 0, 0));
	float4 Row3 = Tex.Load(int3(FrustumIndex * 4 + 3, 0, 0));

	return float4x4(Row0, Row1, Row2, Row3);
}