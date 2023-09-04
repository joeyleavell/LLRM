#include "Lights.hlsl"

#ifndef UBER_SHADOWS
	#error Must define UBER_SHADOWS to be 0 or 1
#endif

struct PSIn
{
    float4 Position : SV_Position;
    float2 UV       : SV_TexCoord0;
};

struct PSOut
{
    float4 Color : SV_Target0;
};

// Num direction lights, directional light, etc.
// Num spot lights, spot light, etc.
Texture2D<float4>      LightDataTexture    : register(t0, space0);
Texture2DArray<float>  ShadowMaps          : register(t1, space0);
Texture2D<float4>      ShadowMapFrustums   : register(t2, space0);

// Per material uniforms
SamplerState           Nearest             : register(t0, space1);
Texture2D<float4>      Albedo              : register(t1, space1);
Texture2D<float4>      Position            : register(t2, space1);
Texture2D<float4>      Normal              : register(t3, space1);
Texture2D<float4>      RMAOTex             : register(t4, space1);
cbuffer SceneUniforms : register(b5, space1)
{
    float3 ViewPosition;
}

PSOut main(PSIn Input)
{
    PSOut Output;

    float3 Position = Position.Sample(Nearest, Input.UV);

    // Sample geometry
    float4 Albedo = Albedo.Sample(Nearest, Input.UV);
    float3 Normal = Normal.Sample(Nearest, Input.UV);
    float3 RMAO   = RMAOTex.Sample(Nearest, Input.UV);

    // Used throughout
    float3 V = normalize(ViewPosition - Position);
    float NDotV = max(dot(Normal, V), 0.0f);

    // Create pixel material
	Material Mat;
    Mat.Roughness = RMAO.x;
    Mat.Metallic = RMAO.y;
    Mat.Albedo = Albedo.rgb;
    Mat.F0 = float3(0.1); // Assume F0 of 0.1 for dielectrics, possibly make configurable?
	Mat.F0 = lerp(Mat.F0, Mat.Albedo, Mat.Metallic); // If metallic, F0 becomes the Albedo

    float3 Radiance = float(0.0);

    float4 LightCounts = LightDataTexture.Load(int3(0, 0, 0));
    uint LightDataIndex = 1;
    for (uint Light = 0; Light < LightCounts.x; Light++)
    {
        float4 LightType = LightDataTexture.Load(int3(LightDataIndex + 0, 0, 0));
        float3 LightColor = LightType.yzw;

        float4 ShadowProperties = LightDataTexture.Load(int3(LightDataIndex + 1, 0, 0));

    	bool LightEffectsSurface = true;

#if UBER_SHADOWS
    	bool CastShadows = ShadowProperties.x > 0.0;
    	if(CastShadows)
        {
            uint FrustumBase = (uint) ShadowProperties.y;

            // Read shadow map view projection matrix
            float4x4 LightViewProj = ExtractViewProjMatrix(ShadowMapFrustums, FrustumBase);

            // Project position onto shadow map
            float4 Projected = LightViewProj * float4(Position, 1.0f);
            float2 SampleCoord = (1.0f + Projected.xy) * 0.5f; // Map to the [0, 1] range
    		SampleCoord.y = 1.0f - SampleCoord.y; // Invert Y

            // Sample shadow map (todo implement PCF/PCSS)
    		float ShadowSample = ShadowMaps.Sample(Nearest, float3(SampleCoord, FrustumBase));

            // Shadow bias
    		if (Projected.z >= ShadowSample)
            {
                LightEffectsSurface = false;
            }
        }
#endif

        if(LightEffectsSurface)
        {
            if((uint) LightType.x == 0) // Light type == directional 
            {
                // Lux (lumens/m^2)
                float4 DirIntensity = LightDataTexture.Load(int3(LightDataIndex + 2, 0, 0));
                DirectionalLight DirLight;
                DirLight.Color = LightColor;
                DirLight.Direction = DirIntensity.xyz;
            	DirLight.Intensity = DirIntensity.w;

                Radiance += CalcDirectionalLightRadiance(DirLight, Mat, Normal, V, NDotV);
            }
        }

        if ((uint) LightType.x == 0) // Light type == directional
        {
            LightDataIndex += 3;
        }
    }

    Output.Color = float4(Radiance, Albedo.a);

    return Output;
}