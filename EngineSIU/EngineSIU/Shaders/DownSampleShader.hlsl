
Texture2D SceneTexture : register(t100);
SamplerState SceneSampler : register(s0);
Texture2D DepthTexture : register(t99);


cbuffer ViewMode : register(b0)
{
    uint ViewMode; 
    float3 Padding;
}

// 정리된 DOF 셰이더 - 불필요한 상수 제거

cbuffer DOFParams : register(b1)
{
    float FocusDistance; // 초점 거리 (world units)
    float BlurStrength; // 블러 강도
    float NearPlane; // 카메라 near plane
    float FarPlane; // 카메라 far plane
    
    float FocalLength; // 렌즈 초점거리 (mm)
    float Aperture; // F-Stop 값
    float MaxBlurRadius; // 최대 블러 반지름
    float _padding; // 16바이트 정렬용
    
    float2 InvTextureSize; // 1/텍스처크기 (1/960, 1/540)
    float2 _padding2; // 16바이트 정렬용
    
    // 제거된 상수들:
    // float FocusRange;        // ← 사용 안함 (CoC 공식 사용)
    // float2 TextureSize;      // ← 사용 안함 (InvTextureSize로 충분)
}


struct PS_Input
{
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD;
};

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

float LinearizeDepth(float z)
{
    float ndc = z * 2.0 - 1.0; // Depth buffer [0,1] → NDC [-1,1]
    return (2.0 * NearPlane * FarPlane) / (FarPlane + NearPlane - ndc * (FarPlane - NearPlane));
}

float GetCoC(float d) // d: linearized depth
{
    float f = FocalLength * 0.001f; // mm → m로 단위 변환
    float a = Aperture; // f-stop
    float s = FocusDistance; // 초점 거리 (world units)
    
    // 0으로 나누기 방지
    if (abs(s - f) < 0.001f)
        return 0.0f;
    if (d <= 0.001f)
        return 0.0f;
    
    float coc = abs(f * f * (d - s) / (d * (s - f))) * (a / f);
    return coc;
}

float4 mainPS(PS_Input Input) : SV_TARGET
{
    float2 uv = Input.UV;
    float depth = DepthTexture.Sample(SceneSampler, uv).r;
    float linearDepth = LinearizeDepth(depth);
    
    float4 color = SceneTexture.Sample(SceneSampler, uv);
    float coc = GetCoC(linearDepth);
    
    float blurRadius = saturate(coc / MaxBlurRadius) * MaxBlurRadius;
    if (blurRadius > 0.01)
    {
        float4 blurColor = float4(0, 0, 0, 0);
        float totalWeight = 0.0;
        int kernel = 2;
        
        for (int x = -kernel; x <= kernel; x++)
        {
            for (int y = -kernel; y <= kernel; y++)
            {
                float2 offset = float2(x, y) * InvTextureSize * blurRadius * BlurStrength;
                float weight = 1.0;
                blurColor += SceneTexture.Sample(SceneSampler, uv + offset) * weight;
                totalWeight += weight;
            }
        }
        
        if (totalWeight > 0)
            color = blurColor / totalWeight;
    }
    
    return color;
}
