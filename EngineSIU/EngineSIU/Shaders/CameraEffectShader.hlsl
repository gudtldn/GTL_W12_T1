
cbuffer CameraFadeConstants : register(b0)
{
    float4 FadeColor;
    float FadeAmount;
    float3 Padding;
};

cbuffer VignetteConstants : register(b1)
{
    float4 VignetteColor;
    
    float2 VignetteCenter;
    float VignetteRadius;
    float VignetteSmoothness;
    
    float VignetteIntensity; // 0 : Vignette 없음 ~ 1
    float3 Padding2;
}

cbuffer LetterBoxConstants : register(b2)
{
    float4 LetterBoxColor;
    
    float ScreenAspect;
    float TargetAspect;
    float2 Padding3;
}

struct PS_Input
{
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD;
};

PS_Input mainVS(uint VertexID : SV_VertexID)
{
    PS_Input Output;

    float2 QuadPositions[6] =
    {
        float2(-1, 1), // Top Left
        float2(1, 1), // Top Right
        float2(-1, -1), // Bottom Left
        float2(1, 1), // Top Right
        float2(1, -1), // Bottom Right
        float2(-1, -1) // Bottom Left
    };

    float2 UVs[6] =
    {
        float2(0, 0), float2(1, 0), float2(0, 1),
        float2(1, 0), float2(1, 1), float2(0, 1)
    };

    Output.Position = float4(QuadPositions[VertexID], 0, 1);
    Output.UV = UVs[VertexID];

    return Output;
}

float4 mainPS(PS_Input Input) : SV_Target
{
    float2 UV = Input.UV;
    
    // Letter Box
    float2 CropMin = float2(0, 0);
    float2 CropMax = float2(1, 1);

    if (ScreenAspect > TargetAspect)
    {
        float TargetHeight = TargetAspect / ScreenAspect;
        float Bar = (1.0 - TargetHeight) * 0.5;
        CropMin.x = Bar;
        CropMax.x = 1.0 - Bar;
    }
    else if (ScreenAspect < TargetAspect)
    {
        float TargetWidth = ScreenAspect / TargetAspect;
        float Bar = (1.0 - TargetWidth) * 0.5;
        CropMin.y = Bar;
        CropMax.y = 1.0 - Bar;
    }

    if (UV.x < CropMin.x || UV.x > CropMax.x || UV.y < CropMin.y || UV.y > CropMax.y)
    {
        return LetterBoxColor;
    }
    
    // Vignette 
    float2 Pos = UV - VignetteCenter;
    Pos.x *= VignetteCenter.x / VignetteCenter.y;

    float Dist = length(Pos);

    // Falloff function smoothstep or pow
    float Vignette = smoothstep(VignetteRadius, VignetteRadius - VignetteSmoothness, Dist);
    //float vignette = pow(saturate((VignetteRadius - dist) / VignetteSmoothness), 2.0);
    
    // [TEMP] Lerp시작 색상 - Scene SRV를 Sampling 하도록 하는 것이 좋아 보임
    float4 FinalColor = lerp(float4(VignetteColor.rgb, 0), VignetteColor, (1 - Vignette) * VignetteIntensity);
    
    // Camera Fade
    FinalColor = lerp(FinalColor, FadeColor, FadeAmount);
    
    return FinalColor;
}
