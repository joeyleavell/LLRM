#define MAX_DIR_LIGHTS 1000
#define MAX_SPOT_LIGHTS 1000

#define PI 3.14159

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

float3 FresnelSchlick(float cosTheta, float3 F0)
{
	return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
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

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}

float3 CalcDirectionalLight(DirectionalLight Light, float3 Albedo, float3 Position, float3 Normal, float3 View)
{
	float3 L = normalize(-Light.Direction);
	float3 V = normalize(View - Position);
	float3 H = normalize(V + L);
	float NDotL = max(dot(Normal, L), 0.0f);

	float Metallic = 0.0;
	float3 F0 = float3(0.1);
	F0 = lerp(F0, Albedo, Metallic);
	float3 ReflectanceRatio = FresnelSchlick(max(dot(V, Normal), 0.0), F0);

	float roughness = 0.1f;

	float NDF = DistributionGGX(Normal, H, roughness);       
	float G   = GeometrySmith(Normal, V, L, roughness);

	float3 numerator    = NDF * G * ReflectanceRatio;
	float denominator = 4.0 * max(dot(Normal, V), 0.0) * NDotL + 0.0001;
	float3 specular     = numerator / denominator;  

	float3 RefractanceRatio = 1.0f - ReflectanceRatio;

	return (RefractanceRatio * Albedo / PI + specular) * Light.Intensity * Light.Color * NDotL;
}

float4x4 ExtractViewProjMatrix(Texture2D<float4> Tex, uint FrustumIndex)
{
	float4 Row0 = Tex.Load(int3(FrustumIndex * 4 + 0, 0, 0));
	float4 Row1 = Tex.Load(int3(FrustumIndex * 4 + 1, 0, 0));
	float4 Row2 = Tex.Load(int3(FrustumIndex * 4 + 2, 0, 0));
	float4 Row3 = Tex.Load(int3(FrustumIndex * 4 + 3, 0, 0));

	return float4x4(Row0, Row1, Row2, Row3);
}