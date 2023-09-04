struct PSIn
{
    float4 Position        : SV_Position;
    float3 WorldPosition   : SV_TexCoord0;
	float3 Normal          : SV_Normal;
};

struct PSOut
{
    float4 Albedo       : SV_Target0;
    float4 Position     : SV_Target1;
    float4 Normal       : SV_Target2;
    float4 RMAO         : SV_Target3;
};

cbuffer MaterialUniforms : register(b0, space2)
{
    float Roughness;
    float Metallic;
    float3 Albedo;
}

PSOut main(PSIn Input)
{
    PSOut Output;
    Output.Albedo = float4(Albedo, 1.0f);
    Output.Position = float4(Input.WorldPosition, 0.0f);
    Output.Normal = float4(Input.Normal, 0.0f);
    Output.RMAO = float4(Roughness, Metallic, 0.0f, 0.0f);

    return Output;
}