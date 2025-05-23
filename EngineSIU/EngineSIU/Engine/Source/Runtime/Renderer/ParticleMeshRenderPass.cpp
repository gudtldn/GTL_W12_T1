#include "ParticleMeshRenderPass.h"

#include "ParticleHelper.h"
#include "RendererHelpers.h"
#include "UnrealClient.h"
#include "Components/Material/Material.h"
#include "D3D11RHI/DXDBufferManager.h"
#include "D3D11RHI/DXDShaderManager.h"
#include "Engine/Engine.h"
#include "Engine/FObjLoader.h"
#include "Engine/StaticMesh.h"
#include "LevelEditor/SLevelEditor.h"
#include "Particles/ParticleSystemComponent.h"
#include "UnrealEd/EditorViewportClient.h"
#include "UObject/UObjectIterator.h"

struct FRenderTargetRHI;
struct FMeshParticleInstanceVertex;
class UParticleSystemComponent;

void FParticleMeshRenderPass::Initialize(FDXDBufferManager* InBufferManager, FGraphicsDevice* InGraphics, FDXDShaderManager* InShaderManage)
{
    FRenderPassBase::Initialize(InBufferManager, InGraphics, InShaderManage);

    D3D11_INPUT_ELEMENT_DESC StaticMeshLayoutDesc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    
    HRESULT hr = ShaderManager->AddVertexShaderAndInputLayout(L"ParticleMeshVertexShader", L"Shaders/ParticleMeshVertexShader.hlsl", "main", StaticMeshLayoutDesc, ARRAYSIZE(StaticMeshLayoutDesc));
    if (FAILED(hr))
    {
        UE_LOG(ELogLevel::Error, TEXT("Failed to create ParticleMeshVertexShader shader!"));
    }

    hr = ShaderManager->AddPixelShader(L"ParticleMeshPixelShader", L"Shaders/ParticleMeshPixelShader.hlsl", "main");
    if (FAILED(hr))
    {
        UE_LOG(ELogLevel::Error, TEXT("Failed to create ParticleMeshPixelShader shader!"));
    }
}

void FParticleMeshRenderPass::PrepareRenderArr()
{
    for (auto Iter : TObjectRange<UParticleSystemComponent>())
    {
        if (Iter->GetWorld() == GEngine->ActiveWorld)
        {
            FParticleDynamicData* Particle = Iter->GetParticleDynamicData();
            if (Particle && !Particle->DynamicEmitterDataArray.IsEmpty())
            {
                for (auto Emitter : Particle->DynamicEmitterDataArray)
                {
                    const FDynamicEmitterReplayDataBase& ReplayData = Emitter->GetSource();

                    // TODO: 최적화 필요
                    if (ReplayData.eEmitterType == EDynamicEmitterType::DET_Mesh)
                    {
                        ParticleComponents.Add(Iter);
                    }
                }
            }
        }
    }
}

void FParticleMeshRenderPass::ClearRenderArr()
{
    ParticleComponents.Empty();
}

void FParticleMeshRenderPass::Render(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    PrepareRender(Viewport);

    DrawParticles();
    
    CleanUpRender(Viewport);
}

void FParticleMeshRenderPass::PrepareRender(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    Graphics->DeviceContext->RSSetViewports(1, &Viewport->GetViewportResource()->GetD3DViewport());

    constexpr EResourceType ResourceType = EResourceType::ERT_Scene;
    FViewportResource* ViewportResource = Viewport->GetViewportResource();
    const FRenderTargetRHI* RenderTargetRHI = ViewportResource->GetRenderTarget(ResourceType);
    const FDepthStencilRHI* DepthStencilRHI = ViewportResource->GetDepthStencil(ResourceType);

    Graphics->DeviceContext->OMSetRenderTargets(1, &RenderTargetRHI->RTV, DepthStencilRHI->DSV);

    Graphics->DeviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);
    Graphics->DeviceContext->OMSetDepthStencilState(Graphics->DepthStencilState_Default, 1);

    Graphics->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ID3D11VertexShader* VertexShader = ShaderManager->GetVertexShaderByKey(L"ParticleMeshVertexShader");
    ID3D11InputLayout* InputLayout = ShaderManager->GetInputLayoutByKey(L"ParticleMeshVertexShader");
    ID3D11PixelShader* PixelShader = ShaderManager->GetPixelShaderByKey(L"ParticleMeshPixelShader");
    Graphics->DeviceContext->VSSetShader(VertexShader, nullptr, 0);
    Graphics->DeviceContext->IASetInputLayout(InputLayout);
    Graphics->DeviceContext->PSSetShader(PixelShader, nullptr, 0);
    
    BufferManager->BindStructuredBufferSRV("ParticleMeshInstanceBuffer", 1, EShaderStage::Vertex);

    BufferManager->BindConstantBuffer(TEXT("FObjectConstantBuffer"), 12, EShaderStage::Vertex);

    BufferManager->BindConstantBuffer(TEXT("FMaterialConstants"), 0, EShaderStage::Pixel);
    BufferManager->BindConstantBuffer(TEXT("FSubUVConstant"), 1, EShaderStage::Pixel);
}

void FParticleMeshRenderPass::CleanUpRender(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    Graphics->DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);

    Graphics->DeviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);
    Graphics->DeviceContext->OMSetDepthStencilState(Graphics->DepthStencilState_Default, 1);
}

void FParticleMeshRenderPass::DrawParticles()
{
    for (const auto PSC : ParticleComponents)
    {
        FParticleDynamicData* Particle = PSC->GetParticleDynamicData();
        if (Particle)
        {
            UpdateObjectConstant(PSC->GetWorldMatrix(), FVector4(), false);
            
            for (auto Emitter : Particle->DynamicEmitterDataArray)
            {
                const FDynamicEmitterReplayDataBase& ReplayData = Emitter->GetSource();

                if (ReplayData.eEmitterType == EDynamicEmitterType::DET_Mesh)
                {
                    FDynamicMeshEmitterData* MeshData = dynamic_cast<FDynamicMeshEmitterData*>(Emitter);
                    if (MeshData)
                    {
                        const UStaticMesh* StaticMesh = MeshData->StaticMesh;
                        const FStaticMeshRenderData* RenderData = StaticMesh->GetRenderData();
                        
                        const FDynamicSpriteEmitterReplayDataBase* SourceData = MeshData->GetSourceData();
                        const FDynamicMeshEmitterReplayData* MeshParticleData = dynamic_cast<const FDynamicMeshEmitterReplayData*>(SourceData);
                        ProcessParticles(MeshParticleData);

                        RenderStaticMeshInstanced_Internal(
                            RenderData,
                            MeshParticleData->ActiveParticleCount,
                            StaticMesh->GetMaterials(),
                            TArray<UMaterial*>(),
                            0
                        );
                    }
                }
            }
        }
    }
}

void FParticleMeshRenderPass::ProcessParticles(const FDynamicMeshEmitterReplayData* ReplayData)
{
    // ReplayData
    if (ReplayData == nullptr)
    {
        return;
    }

    TArray<FMeshParticleInstanceVertex> SpriteVertices;
    
    const uint8* ParticleData = ReplayData->DataContainer.ParticleData;
    const int32 ParticleStride = ReplayData->ParticleStride;

    for (int32 i = 0; i < ReplayData->ActiveParticleCount; i++)
    {
        const uint8* ParticleBase = ParticleData + i * ParticleStride;
        
        DECLARE_PARTICLE_CONST(Particle, ParticleBase)
        FMeshParticleInstanceVertex MeshVertex = {};
        MeshVertex.Color = Particle.Color;

        MeshVertex.TransformMatrix = FMatrix::Identity;
        FTransform Transform = FTransform(FQuat::Identity, Particle.Location, Particle.Size);
        MeshVertex.TransformMatrix = FMatrix::Transpose(Transform.ToMatrixWithScale());

        SpriteVertices.Add(MeshVertex);
    }
    
    FSubUVConstant SubUVConstant = {
        FVector2D(0.0f, 0.0f),
        FVector2D(1.0f, 1.0f)   
    };
    BufferManager->UpdateConstantBuffer(TEXT("FSubUVConstant"), SubUVConstant);
    
    BufferManager->UpdateStructuredBuffer("ParticleMeshInstanceBuffer", SpriteVertices);
}
