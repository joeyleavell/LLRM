#include "Math.hlsl"

// Returns - The ratio of reflected light to incoming light.
// The refractance ratio (AKA diffuse) can be calculated using 1.0f - ReflectanceRatio.
// Note the metallic materials have no refractance, so diffuse lighting would not apply to them.
float3 FresnelSchlick(float cosTheta, float3 F0)
{
	return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float DistributionGGX(float NDotH, float Roughness)
{
    float a      = Roughness*Roughness;
    float a2     = a*a;
    float NdotH2 = NDotH*NDotH;
	
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    denom = PI * denom * denom;
	
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}

float GeometrySmith(float NdotL, float NdotV, float Roughness)
{
    float GGX2  = GeometrySchlickGGX(NdotV, Roughness);
    float GGX1  = GeometrySchlickGGX(NdotL, Roughness);
	
    return GGX1 * GGX2;
}

// F0 - Base reflectivity at straight-on viewing angle
// NDotL - Surface normal dot product incoming light direction
// NDotV - Surface normal dot product view vector
// NDotH - Surface normal dot product halfway vector (norm(L + V))
// Returns the contribution of an individual light ray to a reflection in a given view direction
float3 CookTorrenceBRDF(Material Mat, float NDotL, float NDotV, float NDotH)
{
    // (Normal Distribution Function * Geometry Function) / (4 * NDotV * NDotL)
	float NDF = DistributionGGX(NDotH, Mat.Roughness);       
	float G   = GeometrySmith(NDotL, NDotV, Mat.Roughness);
	float3 Num      = NDF * G;
	float  Den      = 4.0 * NDotV * NDotL + 0.0001; // Prevent divide by zero
	float3 Specular = Num / Den;

    // Ratio of reflected light to incoming light
	float3 ReflectanceRatio = FresnelSchlick(NDotV, Mat.F0);

    // Ratio of refracted light to total incoming light. RefractanceRatio + ReflectanceRatio must be <= 1 to ensure energy conservation.
	// Metallic materials produce no refractance
	float3 RefractanceRatio = (1.0f - ReflectanceRatio) * (1.0f - Mat.Metallic);

	return RefractanceRatio * Mat.Albedo / PI + ReflectanceRatio * Specular;
}