#include "CompositingPass.h"

#include <array>

#include "Define.h"
#include "RendererHelpers.h"
#include "UnrealClient.h"
#include "UnrealEd/EditorViewportClient.h"

void FCompositingPass::Initialize(FDXDBufferManager* InBufferManager, FGraphicsDevice* InGraphics, FDXDShaderManager* InShaderManage)
{
    BufferManager = InBufferManager;
    Graphics = InGraphics;
    ShaderManager = InShaderManage;

    ShaderManager->AddVertexShader(L"Compositing", L"Shaders/CompositingShader.hlsl", "mainVS");
    ShaderManager->AddPixelShader(L"Compositing", L"Shaders/CompositingShader.hlsl", "mainPS");

    D3D11_SAMPLER_DESC SamplerDesc = {};
    SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    SamplerDesc.MinLOD = 0;
    SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    
    Graphics->Device->CreateSamplerState(&SamplerDesc, &Sampler);

    ViewModeBuffer = BufferManager->GetConstantBuffer("FViewModeConstants");

    UINT DiffuseMultiplierSize = sizeof(FGammaConstants);
    BufferManager->CreateBufferGeneric<FGammaConstants>("FGammaConstants", nullptr, DiffuseMultiplierSize, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
}

void FCompositingPass::PrepareRenderArr()
{
}

void FCompositingPass::Render(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    // Setup
    FViewportResource* ViewportResource = Viewport->GetViewportResource();
    if (!ViewportResource)
    {
        return;
    }

    const EResourceType ResourceType = EResourceType::ERT_Compositing; 
    FRenderTargetRHI* RenderTargetRHI = Viewport->GetViewportResource()->GetRenderTarget(ResourceType);

    Graphics->DeviceContext->RSSetViewports(1, &Viewport->GetD3DViewport());

    // TODO: 테스트 끝난 후에 바인딩 할 리소스 타입 ERT_Scene으로 되돌리기
    Graphics->DeviceContext->PSSetShaderResources(static_cast<UINT>(EShaderSRVSlot::SRV_Scene), 1, &ViewportResource->GetRenderTarget(EResourceType::ERT_DownSample2x, 2)->SRV);
    Graphics->DeviceContext->PSSetShaderResources(static_cast<UINT>(EShaderSRVSlot::SRV_PostProcess), 1, &ViewportResource->GetRenderTarget(EResourceType::ERT_PP_Fog)->SRV);
    Graphics->DeviceContext->PSSetShaderResources(static_cast<UINT>(EShaderSRVSlot::SRV_EditorOverlay), 1, &ViewportResource->GetRenderTarget(EResourceType::ERT_Editor)->SRV);
    Graphics->DeviceContext->PSSetShaderResources(static_cast<UINT>(EShaderSRVSlot::SRV_CameraEffect), 1, &ViewportResource->GetRenderTarget(EResourceType::ERT_PP_CameraEffect)->SRV);
    
    Graphics->DeviceContext->OMSetRenderTargets(1, &RenderTargetRHI->RTV, nullptr);
    Graphics->DeviceContext->ClearRenderTargetView(RenderTargetRHI->RTV, ViewportResource->GetClearColor(ResourceType).data());

    Graphics->DeviceContext->RSSetState(Graphics->RasterizerSolidBack);
    Graphics->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    Graphics->DeviceContext->PSSetSamplers(0, 1, &Sampler);

    // 버퍼 바인딩
    Graphics->DeviceContext->PSSetConstantBuffers(0, 1, &ViewModeBuffer);
    
    // Update Constant Buffer
    FViewModeConstants ViewModeConstantData = {};
    ViewModeConstantData.ViewMode = static_cast<uint32>(Viewport->GetViewMode());
    BufferManager->UpdateConstantBuffer<FViewModeConstants>("FViewModeConstants", ViewModeConstantData);

    BufferManager->BindConstantBuffer(TEXT("FGammaConstants"), 1, EShaderStage::Pixel);
    
    FGammaConstants GammaConstantData = {};
    GammaConstantData.GammaValue = GammaValue;
    BufferManager->UpdateConstantBuffer<FGammaConstants>("FGammaConstants", GammaConstantData);

    // Render
    ID3D11VertexShader* VertexShader = ShaderManager->GetVertexShaderByKey(L"Compositing");
    ID3D11PixelShader* PixelShader = ShaderManager->GetPixelShaderByKey(L"Compositing");
    Graphics->DeviceContext->VSSetShader(VertexShader, nullptr, 0);
    Graphics->DeviceContext->PSSetShader(PixelShader, nullptr, 0);
    Graphics->DeviceContext->IASetInputLayout(nullptr);
    Graphics->DeviceContext->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    
    Graphics->DeviceContext->Draw(6, 0);

    // Finish
    Graphics->DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);

    // Clear
    ID3D11ShaderResourceView* NullSRV[1] = { nullptr };
    Graphics->DeviceContext->PSSetShaderResources(static_cast<UINT>(EShaderSRVSlot::SRV_Scene), 1, NullSRV);
    Graphics->DeviceContext->PSSetShaderResources(static_cast<UINT>(EShaderSRVSlot::SRV_PostProcess), 1, NullSRV);
    Graphics->DeviceContext->PSSetShaderResources(static_cast<UINT>(EShaderSRVSlot::SRV_EditorOverlay), 1, NullSRV);
    Graphics->DeviceContext->PSSetShaderResources(static_cast<UINT>(EShaderSRVSlot::SRV_CameraEffect), 1, NullSRV);
    Graphics->DeviceContext->PSSetShaderResources(static_cast<UINT>(EShaderSRVSlot::SRV_Debug), 1, NullSRV);

}

void FCompositingPass::ClearRenderArr()
{
}

void FCompositingPass::PrepareRender(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
}

void FCompositingPass::CleanUpRender(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
}
