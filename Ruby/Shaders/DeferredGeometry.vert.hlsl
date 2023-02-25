struct VSIn
{
    float3 Position : SV_Position;
};

struct VSOut
{
    float4 Position : SV_Position;
};

VSOut main(VSIn Input)
{
    VSOut Output;
    Output.Position = float4(Input.Position, 100.0) + float4(0.0f, 0.0f, 5.4f, 0.0f);

    return Output;
}