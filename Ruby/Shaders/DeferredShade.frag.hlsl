#include "Lights.h"

struct PSIn
{
    float4 Position : SV_Position;
    float2 UV       : SV_TexCoord0;
};

struct PSOut
{
    float4 Color : SV_Target0;
};

// Entire scene uniforms
cbuffer SceneLights : register(b0, space0)
{
	DirectionalLight   DirLights  [MAX_DIR_LIGHTS];
    SpotLight          SpotLights [MAX_SPOT_LIGHTS];
    uint               NumDirLights;
    uint               NumSpotLights;
}

Texture2DArray<float>  ShadowMaps          : register(t1, space0);
Texture2D<float4>      ShadowMapFrustums   : register(t2, space0);

// Per material uniforms
SamplerState           Nearest             : register(t0, space1);
Texture2D<float4>      Albedo              : register(t1, space1);
Texture2D<float4>      Position            : register(t2, space1);
Texture2D<float4>      Normal              : register(t3, space1);

PSOut main(PSIn Input)
{
    PSOut Output;

    float3 Position = Position.Sample(Nearest, Input.UV);

    // Sample geometry
    float4 Albedo = Albedo.Sample(Nearest, Input.UV);
    float3 Normal = Normal.Sample(Nearest, Input.UV);

    float3 Radiance = float(0.0);
    for (uint DirLight = 0; DirLight < NumDirLights; DirLight++)
    {
        bool LightEffectsSurface = true;
        bool CastShadows = DirLights[DirLight].CastShadows;

    	if(CastShadows)
        {
            uint FrustumBase = DirLights[DirLight].FrustumBase;

            // Read shadow map view projection matrix
            float4x4 DirViewProj = ExtractViewProjMatrix(ShadowMapFrustums, FrustumBase);

            // Project position onto shadow map
            float4 Projected = DirViewProj * float4(Position, 1.0f);
            float2 SampleCoord = (1.0f + Projected.xy) * 0.5f; // Map to the [0, 1] range
    		SampleCoord.y = 1.0f - SampleCoord.y; // Invert Y

            // Sample shadow map
    		float ShadowSample = ShadowMaps.Sample(Nearest, float3(SampleCoord, FrustumBase));

    		if (Projected.z - 0.01f >= ShadowSample)
            {
                LightEffectsSurface = false;
            }
        }

        if(LightEffectsSurface)
        {
            Radiance += CalcDirectionalLight(DirLights[DirLight], Albedo, Position, Normal);
        }
    }

    Output.Color = float4(Radiance, Albedo.a);

    return Output;
}