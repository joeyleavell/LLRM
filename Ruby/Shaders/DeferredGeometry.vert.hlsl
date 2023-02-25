cbuffer CameraUniforms : register(b0, space0)
{
    float4x4 ViewProjection;
}

cbuffer ModelUniforms : register(b1, space0)
{
    float4x4 Transform;
}

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
    Output.Position = ViewProjection * Transform * float4(Input.Position, 1.0);

    return Output;
}