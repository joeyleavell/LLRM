struct PSIn
{
    float4 Position : SV_Position;
    float2 UV       : SV_TexCoord0;
};

struct PSOut
{
    float4 Color: SV_Target0;
};

Texture2D<float4>  HDR     : register(t0, space0);
SamplerState       HDRSamp : register(t0, space0);

PSOut main(PSIn Input)
{
    PSOut Output;

    // Sample HDR
    float4 HDRColor = HDR.Sample(HDRSamp, Input.UV);
    Output.Color = saturate(HDRColor);

    return Output;
}