#include "LineRenderPass.h"

#include "D3D11RHI/DXDBufferManager.h"

#include "D3D11RHI/GraphicDevice.h"

#include "D3D11RHI/DXDShaderManager.h"

#include "RendererHelpers.h"

#include "Math/JungleMath.h"

#include "EngineLoop.h"
#include "UnrealClient.h"

#include "UObject/UObjectIterator.h"

#include "UnrealEd/EditorViewportClient.h"

// 생성자/소멸자
FLineRenderPass::FLineRenderPass()
    : VertexLineShader(nullptr)
    , PixelLineShader(nullptr)
{
}

void FLineRenderPass::Initialize(FDXDBufferManager* InBufferManager, FGraphicsDevice* InGraphics, FDXDShaderManager* InShaderManager)
{
    FRenderPassBase::Initialize(InBufferManager, InGraphics, InShaderManager);
}

void FLineRenderPass::PrepareRenderArr()
{
}

void FLineRenderPass::ClearRenderArr()
{
    // 필요에 따라 내부 배열을 초기화
}

void FLineRenderPass::CreateResource()
{
    HRESULT Result = ShaderManager->AddVertexShader(L"VertexLineShader", L"Shaders/ShaderLine.hlsl", "mainVS");
    Result = ShaderManager->AddPixelShader(L"PixelLineShader", L"Shaders/ShaderLine.hlsl", "mainPS");

    VertexLineShader = ShaderManager->GetVertexShaderByKey(L"VertexLineShader");
    PixelLineShader = ShaderManager->GetPixelShaderByKey(L"PixelLineShader");
}

void FLineRenderPass::UpdateShader()
{
    VertexLineShader = ShaderManager->GetVertexShaderByKey(L"VertexLineShader");
    PixelLineShader = ShaderManager->GetPixelShaderByKey(L"PixelLineShader");
}

void FLineRenderPass::PrepareLineShader() const
{
    Graphics->DeviceContext->VSSetShader(VertexLineShader, nullptr, 0);
    Graphics->DeviceContext->IASetInputLayout(nullptr);
    Graphics->DeviceContext->PSSetShader(PixelLineShader, nullptr, 0);

    FEngineLoop::PrimitiveDrawBatch.PrepareLineResources();
}

void FLineRenderPass::DrawLineBatch(const FLinePrimitiveBatchArgs& BatchArgs) const
{
    constexpr UINT Stride = sizeof(FSimpleVertex);
    constexpr UINT Offset = 0;
    Graphics->DeviceContext->IASetVertexBuffers(0, 1, &BatchArgs.VertexBuffer, &Stride, &Offset);
    Graphics->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

    constexpr UINT VertexCountPerInstance = 2;
    const UINT InstanceCount = BatchArgs.GridParam.NumGridLines + 3 +
        (BatchArgs.BoundingBoxCount * 12) +
        (BatchArgs.ConeCount * (2 * BatchArgs.ConeSegmentCount)) +
        (12 * BatchArgs.OBBCount);

    Graphics->DeviceContext->DrawInstanced(VertexCountPerInstance, InstanceCount, 0, 0);
    Graphics->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void FLineRenderPass::PrepareRender(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    constexpr EResourceType ResourceType = EResourceType::ERT_Editor;

    FViewportResource* ViewportResource = Viewport->GetViewportResource();
    FRenderTargetRHI* RenderTargetRHI = ViewportResource->GetRenderTarget(ResourceType);
    FDepthStencilRHI* DepthStencilRHI = ViewportResource->GetDepthStencil(EResourceType::ERT_Scene);
    Graphics->DeviceContext->OMSetRenderTargets(1, &RenderTargetRHI->RTV, DepthStencilRHI->DSV);

    Graphics->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

    Graphics->DeviceContext->OMSetBlendState(Graphics->BlendState_AlphaBlend, nullptr, 0xffffffff);
    Graphics->DeviceContext->OMSetDepthStencilState(Graphics->DepthStencilState_Default, 1);
}

void FLineRenderPass::CleanUpRender(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    Graphics->DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);

    Graphics->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void FLineRenderPass::ProcessLineRendering(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    UpdateShader();
    PrepareLineShader();

    // 상수 버퍼 업데이트: Identity 모델, 기본 색상 등
    UpdateObjectConstant(FMatrix::Identity, FVector4(0, 0, 0, 0), false);

    FLinePrimitiveBatchArgs BatchArgs;
    FEngineLoop::PrimitiveDrawBatch.PrepareBatch(BatchArgs);
    DrawLineBatch(BatchArgs);
    FEngineLoop::PrimitiveDrawBatch.RemoveArr();
}

void FLineRenderPass::Render(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    PrepareRender(Viewport);

    ProcessLineRendering(Viewport);

    CleanUpRender(Viewport);
}
