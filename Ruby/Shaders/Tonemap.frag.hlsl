struct PSIn
{
    float4 Position : SV_Position;
    float2 UV       : SV_TexCoord0;
};

struct PSOut
{
    float4 Color: SV_Target0;
};

SamplerState       Nearest : register(t0, space0);
Texture2D<float4>  HDR     : register(t1, space0);

PSOut main(PSIn Input)
{
    PSOut Output;

    // Sample HDR
    float4 HDRColor = HDR.Sample(Nearest, Input.UV);
    Output.Color = saturate(HDRColor);

    return Output;
}