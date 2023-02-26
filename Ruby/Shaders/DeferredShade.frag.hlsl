struct PSIn
{
    float4 Position : SV_Position;
    float2 UV       : SV_TexCoord0;
};

struct PSOut
{
    float4 Color: SV_Target0;
};

Texture2D<float4>  Albedo     : register(t0, space0);
SamplerState       AlbedoSamp : register(t0, space0);

PSOut main(PSIn Input)
{
    PSOut Output;

    // Sample albedo
    float4 AlbedoColor = Albedo.Sample(AlbedoSamp, Input.UV);
    Output.Color = AlbedoColor;

    return Output;
}