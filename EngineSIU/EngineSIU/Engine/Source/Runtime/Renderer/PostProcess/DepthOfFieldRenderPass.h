#pragma once

#include "RenderPassBase.h"
#include "Define.h"

struct ID3D11SamplerState;

class FDepthOfFieldRenderPass : public FRenderPassBase
{
public:
    FDepthOfFieldRenderPass();
    virtual ~FDepthOfFieldRenderPass() override = default;

    virtual void Initialize(FDXDBufferManager* InBufferManager, FGraphicsDevice* InGraphics, FDXDShaderManager* InShaderManage) override;
    virtual void PrepareRenderArr() override;
    virtual void ClearRenderArr() override;
    void UpdateDOFConstant(float ViewportWidth, float ViewportHeight);
    virtual void Render(const std::shared_ptr<FEditorViewportClient>& Viewport) override;

    FDOFConstants& GetDOFConstant() { return DOFConstant; }

protected:
    virtual void PrepareRender(const std::shared_ptr<FEditorViewportClient>& Viewport) override;
    virtual void CleanUpRender(const std::shared_ptr<FEditorViewportClient>& Viewport) override;
    void PrepareUpSample(const std::shared_ptr<FEditorViewportClient>& Viewport);
    void CleanUpUpSample(const std::shared_ptr<FEditorViewportClient>& Viewport);


    void PrepareDownSample(const std::shared_ptr<FEditorViewportClient>& Viewport);
    void CleanSample(const std::shared_ptr<FEditorViewportClient>& Viewport);
    
    virtual void CreateResource() override;

private:
    ID3D11SamplerState* LinearSampler;
    FDOFConstants DOFConstant;
};
