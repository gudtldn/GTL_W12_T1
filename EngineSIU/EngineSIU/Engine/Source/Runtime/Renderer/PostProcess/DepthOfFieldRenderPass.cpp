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
    CleanSample(Viewport);

    PrepareUpSample(Viewport);
    Graphics->DeviceContext->Draw(6, 0);
    CleanSample(Viewport);

    CleanUpRender(Viewport);
}

void FDepthOfFieldRenderPass::PrepareRender(const std::shared_ptr<FEditorViewportClient>& Viewport) 
{
    BufferManager->BindConstantBuffer(TEXT("FDOFConstants"), 1, EShaderStage::Pixel);
}


void FDepthOfFieldRenderPass::CleanUpRender(const std::shared_ptr<FEditorViewportClient>& Viewport) {}

void FDepthOfFieldRenderPass::PrepareUpSample(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    // 0) 리소스 가져오기
    FViewportResource* VR = Viewport->GetViewportResource();
    if (!VR) return;

    FRenderTargetRHI* RT_Down = VR->GetRenderTarget(EResourceType::ERT_DownSample2x, 2);
    FRenderTargetRHI* RT_Full = VR->GetRenderTarget(EResourceType::ERT_DOF); // 스케일 1 명시

    Graphics->DeviceContext->RSSetViewports(1, &Viewport->GetD3DViewport());

    // 3) 풀 해상도 렌더 타겟 바인딩
    Graphics->DeviceContext->OMSetRenderTargets(1, &RT_Full->RTV, nullptr);

    // 4) 다운샘플 결과를 입력으로 바인딩
    Graphics->DeviceContext->PSSetShaderResources(
        (UINT)EShaderSRVSlot::SRV_Scene, 1, &RT_Down->SRV);

    // 5) 셰이더와 IA 셋업
    ID3D11VertexShader* VS = ShaderManager->GetVertexShaderByKey(L"UpSampleVertexShader");
    ID3D11PixelShader* PS = ShaderManager->GetPixelShaderByKey(L"UpSamplePixelShader");
    Graphics->DeviceContext->VSSetShader(VS, nullptr, 0);
    Graphics->DeviceContext->PSSetShader(PS, nullptr, 0);
    Graphics->DeviceContext->IASetInputLayout(nullptr);
    Graphics->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 6) 샘플러 설정
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

void FDepthOfFieldRenderPass::CleanSample(const std::shared_ptr<FEditorViewportClient>& Viewport)
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
