cbuffer CameraUniforms : register(b0, space0)
{
    float4x4 ViewProjection;
    float Uniforms[10000];
}

cbuffer ModelUniforms : register(b0, space1)
{
    float4x4 Transform;
}

struct VSIn
{
    float3 Position : SV_Position;
	float3 Normal   : SV_Normal;
};

struct VSOut
{
    float4 Position        : SV_Position;
    float3 WorldPosition   : SV_TexCoord0;
    float3 Normal          : SV_Normal;
};

VSOut main(VSIn Input)
{
    VSOut Output;
    Output.WorldPosition = (Transform * float4(Input.Position, 1.0)).xyz;
	Output.Position = ViewProjection * float4(Output.WorldPosition, 1.0f);
    Output.Normal = Transform * float4(Input.Normal, 0.0);

    return Output;
}