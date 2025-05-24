#pragma once

#include "Define.h"
#include "RenderPassBase.h"
#include "EngineBaseTypes.h"
#include "Container/Set.h"

class UGizmoBaseComponent;
class FDXDBufferManager;
class FGraphicsDevice;
class UWorld;
class FEditorViewportClient;

class FGizmoRenderPass : public FRenderPassBase
{
public:
    FGizmoRenderPass() = default;
    virtual ~FGizmoRenderPass() override = default;

    virtual void Initialize(FDXDBufferManager* InBufferManager, FGraphicsDevice* InGraphics, FDXDShaderManager* InShaderManage) override;

    virtual void PrepareRenderArr() override;

    virtual void Render(const std::shared_ptr<FEditorViewportClient>& Viewport) override;

    virtual void ClearRenderArr() override;

    void PrepareRenderState() const;

    // Gizmo 한 개 렌더링 함수
    void RenderGizmoComponent(UGizmoBaseComponent* GizmoComp, const std::shared_ptr<FEditorViewportClient>& Viewport);

    void UpdateShader();
    void ReleaseShader();

    void CreateBuffer();
    
protected:
    virtual void PrepareRender(const std::shared_ptr<FEditorViewportClient>& Viewport) override;
    virtual void CleanUpRender(const std::shared_ptr<FEditorViewportClient>& Viewport) override;
    
    virtual void CreateResource() override;

    ID3D11VertexShader* VertexShader;
    ID3D11PixelShader* PixelShader;
    ID3D11InputLayout* InputLayout;

    ID3D11SamplerState* Sampler;
};
