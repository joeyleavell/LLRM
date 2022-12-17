struct PSIn
{
    float4 Position : SV_Position;
};

struct PSOut
{
    float4 Color       : SV_Target0;
};

PSOut main(PSIn Input)
{
    PSOut Output;
    Output.Color = float4(0.0f, 1.0f, 0.0f, 1.0f);

    return Output;
}