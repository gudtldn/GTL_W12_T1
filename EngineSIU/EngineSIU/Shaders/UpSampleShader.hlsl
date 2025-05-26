Texture2D DOFTexture : register(t100); // 다운샘플된 DOF 결과
SamplerState LinearSampler : register(s0);

struct PS_Input
{
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD;
};

// 정점 셰이더 (DOF와 동일)
PS_Input mainVS(uint VertexID : SV_VertexID)
{
    PS_Input Output;
    float2 QuadPositions[4] =
    {
        float2(-1, -1),
        float2(1, -1),
        float2(-1, 1),
        float2(1, 1)
    };
    float2 UVs[4] =
    {
        float2(0, 1),
    float2(1, 1),
    float2(0, 0),
    float2(1, 0)
    };
    uint Indices[6] =
    {
        0, 2, 1,
        1, 2, 3
    };
    uint Index = Indices[VertexID];
    Output.Position = float4(QuadPositions[Index], 0, 1);
    Output.UV = UVs[Index];
    return Output;
}

// 픽셀 셰이더 - 단순 업샘플링
float4 mainPS(PS_Input Input) : SV_TARGET
{

    // 선형 보간으로 업샘플링 (GPU가 자동으로 960x540 → 1920x1080)
    return DOFTexture.Sample(LinearSampler, Input.UV);
}
