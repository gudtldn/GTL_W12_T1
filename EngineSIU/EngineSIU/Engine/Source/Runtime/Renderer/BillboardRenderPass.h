#pragma once

#include "RenderPassBase.h"
#include "EngineBaseTypes.h"
#include "Container/Set.h"

#include "Define.h"

enum class EResourceType : uint8;
class UBillboardComponent;
class FDXDBufferManager;
class FGraphicsDevice;
class FDXDShaderManager;
class UWorld;
class FEditorViewportClient;

class FBillboardRenderPass : public FRenderPassBase
{
public:
    FBillboardRenderPass();
    virtual ~FBillboardRenderPass() override = default;

    virtual void Initialize(FDXDBufferManager* InBufferManager, FGraphicsDevice* InGraphics, FDXDShaderManager* InShaderManage) override;

    virtual void PrepareRenderArr() override;
    
    virtual void Render(const std::shared_ptr<FEditorViewportClient>& Viewport) override;

    virtual void ClearRenderArr() override;

    void SetupVertexBuffer(ID3D11Buffer* pVertexBuffer, UINT NumVertices) const;

    // 상수 버퍼 업데이트 함수
    void UpdateSubUVConstant(FVector2D UVOffset, FVector2D UVScale) const;
    
    // Primitive 드로우 함수
    void RenderTexturePrimitive(ID3D11Buffer* pVertexBuffer, UINT NumVertices,
        ID3D11Buffer* pIndexBuffer, UINT NumIndices,
        ID3D11ShaderResourceView* TextureSRV, ID3D11SamplerState* SamplerState) const;

    void RenderTextPrimitive(ID3D11Buffer* pVertexBuffer, UINT NumVertices,
        ID3D11ShaderResourceView* TextureSRV, ID3D11SamplerState* SamplerState) const;

    void UpdateShader();

protected:
    virtual void PrepareRender(const std::shared_ptr<FEditorViewportClient>& Viewport) override;
    virtual void CleanUpRender(const std::shared_ptr<FEditorViewportClient>& Viewport) override;

    virtual void CreateResource() override;
    
    TArray<UBillboardComponent*> BillboardComps;

    EResourceType ResourceType;
};
