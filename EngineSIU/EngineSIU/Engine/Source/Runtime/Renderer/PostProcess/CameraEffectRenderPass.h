#pragma once
#include "RenderPassBase.h"
#include "Camera/CameraComponent.h"

class FCameraEffectRenderPass : public FRenderPassBase
{
public:
    FCameraEffectRenderPass() = default;
    virtual ~FCameraEffectRenderPass() override = default;
    
    virtual void PrepareRenderArr() override {}
    virtual void ClearRenderArr() override {}

    virtual void Initialize(FDXDBufferManager* InBufferManager, FGraphicsDevice* InGraphics, FDXDShaderManager* InShaderManager) override;

    virtual void Render(const std::shared_ptr<FEditorViewportClient>& Viewport) override;

private:
    virtual void PrepareRender(const std::shared_ptr<FEditorViewportClient>& Viewport) override;
    virtual void CleanUpRender(const std::shared_ptr<FEditorViewportClient>& Viewport) override;
    
    virtual void CreateResource() override;

    void UpdateCameraEffectConstant(const std::shared_ptr<FEditorViewportClient>& Viewport);
};
