struct VSIn
{
    float2 Position : SV_Position;
};

struct VSOut
{
    float4 Position : SV_Position;
};

VSOut main(VSIn Input)
{
    VSOut Output;
    Output.Position = float4(Input.Position, 0.0, 1.0);

    return Output;
}