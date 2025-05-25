#include "DepthOfFieldRenderPass.h"

#include "RendererHelpers.h"
#include "UnrealClient.h"
#include "D3D11RHI/DXDShaderManager.h"
#include "UnrealEd/EditorViewportClient.h"

void FDepthOfFieldRenderPass::Initialize(FDXDBufferManager* InBufferManager, FGraphicsDevice* InGraphics, FDXDShaderManager* InShaderManage)
{
    FRenderPassBase::Initialize(InBufferManager, InGraphics, InShaderManage);
}

void FDepthOfFieldRenderPass::PrepareRenderArr()
{
    FRenderPassBase::PrepareRenderArr();
}

void FDepthOfFieldRenderPass::ClearRenderArr()
{
    FRenderPassBase::ClearRenderArr();
}

void FDepthOfFieldRenderPass::UpdateDOFConstant()
{
    FDOFConstants Constants;
    {
        // 🎯 DOF 핵심 파라미터들
        Constants.FocusDistance = 0.5f;          // 중간 거리에 초점 (0~1 범위)
        Constants.BlurStrength = 3.0f;           // 블러 강도 (1~5 추천)
        Constants.FocusRange = 0.08f;            // 초점 범위 (작을수록 민감)

        // 📷 카메라 설정들
        Constants.NearPlane = 0.1f;              // 카메라 near plane
        Constants.FarPlane = 1000.0f;            // 카메라 far plane

        // 🔍 렌즈 물리학 파라미터들  
        Constants.FocalLength = 85.0f;           // 85mm 망원렌즈 (인물용)
        Constants.Aperture = 1.8f;               // f/1.8 (큰 조리개, 강한 DOF)
        Constants.MaxBlurRadius = 25.0f;         // 최대 25픽셀까지 블러
    }
    //상수버퍼 업데이트
    BufferManager->UpdateConstantBuffer(TEXT("FDOFConstants"), Constants);
}

void FDepthOfFieldRenderPass::Render(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    PrepareRender(Viewport);

    /**
     * TODO: DOF 순서
     *       1. 전체 해상도 뎁스 맵을 기준으로 CoC(Circle of Confusion) 계산
     *       2. 씬 렌더 결과(Fog 적용 이후가 좋음)를 절반 크기로 다운 샘플링
     *       3. CoC값을 기반으로 가변 크기 커널 블러 적용
     *       4. 커널 블러 결과를 다시 2배 크기로 업 샘플링
     *       5. 합성
     */
    PrepareDownSample(Viewport);
    Graphics->DeviceContext->Draw(6, 0);
    CleanUpDownSample(Viewport);
    CleanUpRender(Viewport);
}

void FDepthOfFieldRenderPass::PrepareRender(const std::shared_ptr<FEditorViewportClient>& Viewport) 
{
    BufferManager->BindConstantBuffer(TEXT("FDOFConstants"), 1, EShaderStage::Pixel);
}


void FDepthOfFieldRenderPass::CleanUpRender(const std::shared_ptr<FEditorViewportClient>& Viewport) {}

void FDepthOfFieldRenderPass::PrepareUpSample(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    FViewportResource* ViewportResource = Viewport->GetViewportResource();
    if (!ViewportResource)
    {
        return;
    }

    // 🔥 입력: 다운샘플된 DOF 결과 (960x540)
    FRenderTargetRHI* RenderTargetRHI_DownSample2x = ViewportResource->GetRenderTarget(EResourceType::ERT_DownSample2x, 2);

    // 🔥 출력: 풀 해상도 DOF 결과 (1920x1080)
    FRenderTargetRHI* RenderTargetRHI_DOF = ViewportResource->GetRenderTarget(EResourceType::ERT_DOF);

    // 🔥 풀 해상도 뷰포트 설정
    Graphics->DeviceContext->RSSetViewports(1, &Viewport->GetD3DViewport());

    // 🔥 풀 해상도 렌더 타겟에 렌더링
    Graphics->DeviceContext->OMSetRenderTargets(1, &RenderTargetRHI_DOF->RTV, nullptr);

    // 🔥 다운샘플된 DOF 결과를 입력으로 바인딩
    Graphics->DeviceContext->PSSetShaderResources(static_cast<UINT>(EShaderSRVSlot::SRV_Scene), 1, &RenderTargetRHI_DownSample2x->SRV);

    // 🔥 업샘플링 셰이더 설정
    ID3D11VertexShader* VertexShader = ShaderManager->GetVertexShaderByKey(L"UpSampleVertexShader");
    ID3D11PixelShader* PixelShader = ShaderManager->GetPixelShaderByKey(L"UpSamplePixelShader");
    Graphics->DeviceContext->VSSetShader(VertexShader, nullptr, 0);
    Graphics->DeviceContext->PSSetShader(PixelShader, nullptr, 0);
    Graphics->DeviceContext->IASetInputLayout(nullptr);

    // 🔥 선형 샘플러 사용 (부드러운 업샘플링)
    Graphics->DeviceContext->PSSetSamplers(0, 1, &LinearSampler);
}

void FDepthOfFieldRenderPass::PrepareDownSample(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    FViewportResource* ViewportResource = Viewport->GetViewportResource();
    if (!ViewportResource)
    {
        return;
    }
    
    FRenderTargetRHI* RenderTargetRHI_ScenePure = ViewportResource->GetRenderTarget(EResourceType::ERT_Scene);
    FRenderTargetRHI* RenderTargetRHI_DownSample2x = ViewportResource->GetRenderTarget(EResourceType::ERT_DownSample2x, 2);

    const FRect ViewportRect = Viewport->GetViewport()->GetRect();
    const float DownSampledWidth = static_cast<float>(FMath::FloorToInt(ViewportRect.Width / 2));
    const float DownSampledHeight = static_cast<float>(FMath::FloorToInt(ViewportRect.Height / 2));

    D3D11_VIEWPORT Viewport_DownSample2x;
    Viewport_DownSample2x.Width = DownSampledWidth;
    Viewport_DownSample2x.Height = DownSampledHeight;
    Viewport_DownSample2x.MinDepth = 0.0f;
    Viewport_DownSample2x.MaxDepth = 1.0f;
    Viewport_DownSample2x.TopLeftX = 0.f;
    Viewport_DownSample2x.TopLeftY = 0.f;
    Graphics->DeviceContext->RSSetViewports(1, &Viewport_DownSample2x);
    
    Graphics->DeviceContext->OMSetRenderTargets(1, &RenderTargetRHI_DownSample2x->RTV, nullptr);
    Graphics->DeviceContext->PSSetShaderResources(static_cast<UINT>(EShaderSRVSlot::SRV_Scene), 1, &RenderTargetRHI_ScenePure->SRV);

    Graphics->DeviceContext->PSSetShaderResources(static_cast<UINT>(EShaderSRVSlot::SRV_SceneDepth), 1,
        &ViewportResource->GetDepthStencil(EResourceType::ERT_Scene)->SRV);  // 깊이 추가!

    UpdateDOFConstant();

    ID3D11VertexShader* VertexShader = ShaderManager->GetVertexShaderByKey(L"DownSampleVertexShader");
    ID3D11PixelShader* PixelShader = ShaderManager->GetPixelShaderByKey(L"DownSamplePixelShader");
    Graphics->DeviceContext->VSSetShader(VertexShader, nullptr, 0);
    Graphics->DeviceContext->PSSetShader(PixelShader, nullptr, 0);
    Graphics->DeviceContext->IASetInputLayout(nullptr);

    Graphics->DeviceContext->PSSetSamplers(0, 1, &LinearSampler);
}

void FDepthOfFieldRenderPass::CleanUpDownSample(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    Graphics->DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);

    ID3D11ShaderResourceView* NullSRV[1] = { nullptr };
    Graphics->DeviceContext->PSSetShaderResources(static_cast<UINT>(EShaderSRVSlot::SRV_Scene), 1, NullSRV);
}

void FDepthOfFieldRenderPass::CreateResource()
{
    HRESULT hr = ShaderManager->AddVertexShader(L"DownSampleVertexShader", L"Shaders/DownSampleShader.hlsl", "mainVS");
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to Compile DownSampleShader: mainVS", L"Error", MB_ICONERROR | MB_OK);
        return;
    }
    
    hr = ShaderManager->AddPixelShader(L"DownSamplePixelShader", L"Shaders/DownSampleShader.hlsl", "mainPS");
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to Compile DownSampleShader: mainPS", L"Error", MB_ICONERROR | MB_OK);
        return;
    }

    hr = ShaderManager->AddVertexShader(L"UpSampleVertexShader", L"Shaders/UpSampleShader.hlsl", "mainVS");
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to Compile UpSampleShader: mainVS", L"Error", MB_ICONERROR | MB_OK);
        return;
    }

    hr = ShaderManager->AddPixelShader(L"UpSamplePixelShader", L"Shaders/UpSampleShader.hlsl", "mainPS");
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to Compile UpSampleShader: mainPS", L"Error", MB_ICONERROR | MB_OK);
        return;
    }


    D3D11_SAMPLER_DESC SamplerDesc = {};
    SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    SamplerDesc.MinLOD = 0;
    SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    hr = Graphics->Device->CreateSamplerState(&SamplerDesc, &LinearSampler);
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to Create SamplerState DownSample2x", L"Error", MB_ICONERROR | MB_OK);
        return;
    }
}
