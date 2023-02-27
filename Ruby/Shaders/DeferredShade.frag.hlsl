struct PSIn
{
    float4 Position : SV_Position;
    float2 UV       : SV_TexCoord0;
};

struct PSOut
{
    float4 Color: SV_Target0;
};

SamplerState       Nearest      : register(t0, space0);
Texture2D<float4>  Albedo       : register(t1, space0);
Texture2D<float4>  Position     : register(t2, space0);
Texture2D<float4>  Normal       : register(t3, space0);

PSOut main(PSIn Input)
{
    PSOut Output;

    // Sample geometry
    float4 Albedo   = Albedo.Sample(Nearest, Input.UV);
    float3 Position = Position.Sample(Nearest, Input.UV);
    float3 Normal   = Normal.Sample(Nearest, Input.UV);

    // Directional light
    float3 Dir = normalize(float3(0.0f, 0.0f, -1.0f));
    float NDotL = max(dot(Normal, -Dir), 0.0f);

	Output.Color = Albedo * NDotL;

    return Output;
}