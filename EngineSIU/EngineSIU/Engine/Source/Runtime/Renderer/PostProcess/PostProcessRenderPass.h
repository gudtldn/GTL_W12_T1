#pragma once
#include "RenderPassBase.h"


class FDepthOfFieldRenderPass;
class FFogRenderPass;
class FCameraEffectRenderPass;
class FPostProcessCompositingPass;

class FPostProcessRenderPass : public FRenderPassBase
{
public:
    FPostProcessRenderPass();
    virtual ~FPostProcessRenderPass() override = default;

    virtual void Render(const std::shared_ptr<FEditorViewportClient>& Viewport) override;

protected:
    virtual void PrepareRender(const std::shared_ptr<FEditorViewportClient>& Viewport) override;
    virtual void CleanUpRender(const std::shared_ptr<FEditorViewportClient>& Viewport) override;
    
    FFogRenderPass* FogRenderPass = nullptr;
    FCameraEffectRenderPass* CameraEffectRenderPass = nullptr;
    FPostProcessCompositingPass* PostProcessCompositingPass = nullptr;
    FDepthOfFieldRenderPass* DepthOfFieldRenderPass = nullptr;
};
