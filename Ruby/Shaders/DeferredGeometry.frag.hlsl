struct PSIn
{
    float4 Position : SV_Position;
};

struct PSOut
{
    float4 Albedo       : SV_Target0;
};

PSOut main(PSIn Input)
{
    PSOut Output;
    Output.Albedo = float4(0.0f, 5.0f, 0.0f, 1.0f);

    return Output;
}