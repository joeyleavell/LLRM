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

    // Sample albedo
    float4 AlbedoColor = Albedo.Sample(Nearest, Input.UV);
    Output.Color = AlbedoColor;

    return Output;
}