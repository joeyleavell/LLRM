#include "Lights.h"

struct PSIn
{
    float4 Position : SV_Position;
    float2 UV       : SV_TexCoord0;
};

struct PSOut
{
    float4 Color: SV_Target0;
};

cbuffer SceneLights : register(b0, space0)
{
	DirectionalLight  DirLights  [MAX_DIR_LIGHTS];
    SpotLight         SpotLights[MAX_SPOT_LIGHTS];
    uint NumDirLights;
    uint NumSpotLights;
}

Texture2D<float>  ShadowMap    : register(t1, space0);

SamplerState       Nearest      : register(t0, space1);
Texture2D<float4>  Albedo       : register(t1, space1);
Texture2D<float4>  Position     : register(t2, space1);
Texture2D<float4>  Normal       : register(t3, space1);

PSOut main(PSIn Input)
{
    PSOut Output;

    float Shadow   = ShadowMap.Sample(Nearest, Input.UV);

    // Sample geometry
    float4 Albedo   = Albedo.Sample(Nearest, Input.UV);
    float3 Position = Position.Sample(Nearest, Input.UV);
    float3 Normal   = Normal.Sample(Nearest, Input.UV);

    float3 Radiance = float(0.0);
    for(uint DirLight = 0; DirLight < NumDirLights; DirLight++)
    {
        Radiance += CalcDirectionalLight(DirLights[DirLight], Albedo, Position, Normal);
    }

    Output.Color = float4(Radiance, Albedo.a);

    return Output;
}