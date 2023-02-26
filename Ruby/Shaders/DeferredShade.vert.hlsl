struct VSIn
{
    float3 Position : SV_Position;
    float2 UV       : SV_TexCoord0;
};

struct VSOut
{
    float4 Position : SV_Position;
    float2 UV       : SV_TexCoord0;
};

VSOut main(VSIn Input)
{
    VSOut Output;
    Output.Position = float4(Input.Position, 1.0);
    Output.UV       = Input.UV;

    return Output;
}