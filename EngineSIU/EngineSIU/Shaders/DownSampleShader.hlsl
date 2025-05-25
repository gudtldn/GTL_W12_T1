
Texture2D SceneTexture : register(t100);
SamplerState SceneSampler : register(s0);
Texture2D DepthTexture : register(t99);


cbuffer ViewMode : register(b0)
{
    uint ViewMode; 
    float3 Padding;
}


cbuffer DOFParams : register(b1)
{
    float FocusDistance; // 초점 거리 (0~1)
    float BlurStrength; // 블러 강도
    float FocusRange; // 초점 범위 (focusError 임계값)
    float NearPlane; // 카메라 near plane
    
    float FarPlane; // 카메라 far plane
    float FocalLength; // 렌즈 초점거리 (mm)
    float Aperture; // F-Stop 값
    float MaxBlurRadius; // 최대 블러 반지름
    
    float2 TextureSize; // 텍스처 크기 (960, 540)
    float2 InvTextureSize; // 1/텍스처크기 (1/960, 1/540)
}


struct PS_Input
{
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD;
};

PS_Input mainVS(uint VertexID : SV_VertexID)
{
    PS_Input Output;

    float2 QuadPositions[4] = {
        float2(-1, -1),
        float2( 1, -1),
        float2(-1,  1),
        float2( 1,  1)
    };

    float2 UVs[4] = {
        float2(0, 1),
        float2(1, 1),
        float2(0, 0),
        float2(1, 0)
    };

    uint Indices[6] = {
        0, 2, 1,
        1, 2, 3
    };

    uint Index = Indices[VertexID];
    Output.Position = float4(QuadPositions[Index], 0, 1);
    Output.UV = UVs[Index];

    return Output;
}

float4 mainPS(PS_Input Input) : SV_TARGET
{
    float2 uv = Input.UV;

    // 깊이 읽기
    float depth = DepthTexture.Sample(SceneSampler, uv).r;

    // 원본 색상
    float4 color = SceneTexture.Sample(SceneSampler, uv);

    // 초점 거리에서의 오차 계산
    float focusError = abs(depth - FocusDistance);

    // 초점 범위를 넘어가면 블러 처리
    if (focusError > FocusRange)
    {
        float4 blurColor = float4(0, 0, 0, 0);
        float2 texelSize = InvTextureSize;

        // 간단한 박스 블러
        for (int x = -2; x <= 2; x++)
        {
            for (int y = -2; y <= 2; y++)
            {
                float2 offset = float2(x, y) * texelSize * BlurStrength * focusError;
                blurColor += SceneTexture.Sample(SceneSampler, uv + offset);
            }
        }

        color = blurColor / 25.0; // 5x5 커널
    }

    return color;
}
