#pragma once
#include "RenderPassBase.h"
#include "EngineBaseTypes.h"
#include "Container/Set.h"
#include "Define.h"

class FDXDBufferManager;
class FGraphicsDevice;
class FDXDShaderManager;
class UWorld;
class FEditorViewportClient;

struct FLinePrimitiveBatchArgs;

class FLineRenderPass : public FRenderPassBase
{
public:
    FLineRenderPass();
    virtual ~FLineRenderPass() override = default;

    virtual void Initialize(FDXDBufferManager* InBufferManager, FGraphicsDevice* InGraphics, FDXDShaderManager* InShaderManager) override;
    virtual void PrepareRenderArr() override;
    virtual void Render(const std::shared_ptr<FEditorViewportClient>& Viewport) override;
    virtual void ClearRenderArr() override;

    // 라인 렌더링 전용 함수
    void UpdateShader();
    void PrepareLineShader() const;
    void ProcessLineRendering(const std::shared_ptr<FEditorViewportClient>& Viewport);
    void DrawLineBatch(const FLinePrimitiveBatchArgs& BatchArgs) const;

protected:
    virtual void PrepareRender(const std::shared_ptr<FEditorViewportClient>& Viewport) override;
    virtual void CleanUpRender(const std::shared_ptr<FEditorViewportClient>& Viewport) override;
    
    virtual void CreateResource() override;

    // 라인 셰이더 관련 멤버
    ID3D11VertexShader* VertexLineShader;
    ID3D11PixelShader* PixelLineShader;
};
