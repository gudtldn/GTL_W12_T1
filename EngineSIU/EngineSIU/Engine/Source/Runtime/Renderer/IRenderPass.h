#pragma once
#include <memory>

class FDXDBufferManager;
class FGraphicsDevice;
class FDXDShaderManager;
class FEditorViewportClient;

class IRenderPass
{
public:
    virtual ~IRenderPass() = default;
    
    virtual void Initialize(FDXDBufferManager* InBufferManager, FGraphicsDevice* InGraphics, FDXDShaderManager* InShaderManage) = 0;

    virtual void PrepareRenderArr() = 0;
    
    virtual void ClearRenderArr() = 0;

    virtual void Render(const std::shared_ptr<FEditorViewportClient>& Viewport) = 0;

protected:
    virtual void PrepareRender(const std::shared_ptr<FEditorViewportClient>& Viewport) = 0;

    virtual void CleanUpRender(const std::shared_ptr<FEditorViewportClient>& Viewport) = 0;

    virtual void CreateResource() = 0;
};
