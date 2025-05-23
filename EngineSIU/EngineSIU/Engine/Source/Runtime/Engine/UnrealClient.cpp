#include "UnrealClient.h"

#include "EngineLoop.h"
#include <array>

FViewportResource::FViewportResource()
{
    ClearColors.Add(EResourceType::ERT_Compositing, { 0.f, 0.f, 0.f, 1.f });
    ClearColors.Add(EResourceType::ERT_Scene,  { 0.025f, 0.025f, 0.025f, 1.0f });
    ClearColors.Add(EResourceType::ERT_ScenePure,  { 0.f, 0.f, 0.f, 1.f });
    ClearColors.Add(EResourceType::ERT_PP_Fog, { 0.f, 0.f, 0.f, 0.f });
    ClearColors.Add(EResourceType::ERT_PP_CameraEffect, { 0.f, 0.f, 0.f, 0.f });
    ClearColors.Add(EResourceType::ERT_Debug, { 0.f, 0.f, 0.f, 0.f });
    ClearColors.Add(EResourceType::ERT_Editor, { 0.f, 0.f, 0.f, 0.f });
    ClearColors.Add(EResourceType::ERT_Gizmo, { 0.f, 0.f, 0.f, 0.f });
    ClearColors.Add(EResourceType::ERT_Overlay, { 0.f, 0.f, 0.f, 0.f });
    ClearColors.Add(EResourceType::ERT_PostProcessCompositing, { 0.f, 0.f, 0.f, 0.f });
    ClearColors.Add(EResourceType::ERT_DownSample2x, { 0.f, 0.f, 0.f, 0.f });
    ClearColors.Add(EResourceType::ERT_DownSample4x, { 0.f, 0.f, 0.f, 0.f });
}

FViewportResource::~FViewportResource()
{
    Release();

    ClearColors.Empty();
}

void FViewportResource::Initialize(uint32 InWidth, uint32 InHeight)
{
    D3DViewport.TopLeftX = 0.f;
    D3DViewport.TopLeftY = 0.f;
    D3DViewport.Height = static_cast<float>(InHeight);
    D3DViewport.Width = static_cast<float>(InWidth);
    D3DViewport.MaxDepth = 1.0f;
    D3DViewport.MinDepth = 0.0f;
    
    // Essential resources
    HRESULT hr = S_OK;
    hr = CreateDepthStencil(EResourceType::ERT_Scene);
    if (FAILED(hr))
    {
        return;
    }
    
    hr = CreateDepthStencil(EResourceType::ERT_Gizmo);
    if (FAILED(hr))
    {
        return;
    }
    
    hr = CreateRenderTarget(EResourceType::ERT_Compositing);
    if (FAILED(hr))
    {
        return;
    }

    hr = CreateRenderTarget(EResourceType::ERT_Scene);
    if (FAILED(hr))
    {
        return;
    }
}

void FViewportResource::Resize(uint32 NewWidth, uint32 NewHeight)
{
    Release();

    D3DViewport.Height = static_cast<float>(NewHeight);
    D3DViewport.Width = static_cast<float>(NewWidth);

    for (auto& [Type, Resource] : DepthStencils)
    {
        CreateDepthStencil(Type);
    }

    for (auto& [Type, Resource] : RenderTargets)
    {
        CreateRenderTarget(Type);
    }
}

void FViewportResource::Release()
{
    ReleaseAllResources();
}

HRESULT FViewportResource::CreateDepthStencil(EResourceType Type, uint32 DownSampleScale)
{
    if (HasDepthStencil(Type))
    {
        ReleaseDepthStencil(Type);
    }

    DownSampleScale = FMath::Max(DownSampleScale, 1U);

    FDepthStencilRHI NewResource;
    
    HRESULT hr = S_OK;
    
    D3D11_TEXTURE2D_DESC DepthStencilTextureDesc = {};
    DepthStencilTextureDesc.Width = static_cast<uint32>(D3DViewport.Width / static_cast<float>(DownSampleScale));
    DepthStencilTextureDesc.Height = static_cast<uint32>(D3DViewport.Height / static_cast<float>(DownSampleScale));
    DepthStencilTextureDesc.MipLevels = 1;
    DepthStencilTextureDesc.ArraySize = 1;
    DepthStencilTextureDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
    DepthStencilTextureDesc.SampleDesc.Count = 1;
    DepthStencilTextureDesc.SampleDesc.Quality = 0;
    DepthStencilTextureDesc.Usage = D3D11_USAGE_DEFAULT;
    DepthStencilTextureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    DepthStencilTextureDesc.CPUAccessFlags = 0;
    DepthStencilTextureDesc.MiscFlags = 0;
    hr = FEngineLoop::GraphicDevice.Device->CreateTexture2D(&DepthStencilTextureDesc, nullptr, &NewResource.Texture2D);
    if (FAILED(hr))
    {
        return hr;
    }
    
    D3D11_DEPTH_STENCIL_VIEW_DESC DepthStencilViewDesc = {};
    DepthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    DepthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    DepthStencilViewDesc.Texture2D.MipSlice = 0;
    hr = FEngineLoop::GraphicDevice.Device->CreateDepthStencilView(NewResource.Texture2D,  &DepthStencilViewDesc,  &NewResource.DSV);
    if (FAILED(hr))
    {
        return hr;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC DepthStencilDesc = {};
    DepthStencilDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    DepthStencilDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    DepthStencilDesc.Texture2D.MostDetailedMip = 0;
    DepthStencilDesc.Texture2D.MipLevels = 1;
    hr = FEngineLoop::GraphicDevice.Device->CreateShaderResourceView(NewResource.Texture2D, &DepthStencilDesc, &NewResource.SRV);
    if (FAILED(hr))
    {
        return hr;
    }

    DepthStencils.Add(Type, NewResource);

    return hr;
}

FDepthStencilRHI* FViewportResource::GetDepthStencil(EResourceType Type, uint32 DownSampleScale)
{
    if (!HasDepthStencil(Type))
    {
        if (FAILED(CreateDepthStencil(Type, DownSampleScale)))
        {
            return nullptr;
        }
    }
    return DepthStencils.Find(Type);
}

bool FViewportResource::HasDepthStencil(EResourceType Type) const
{
    return DepthStencils.Contains(Type);
}

void FViewportResource::ClearDepthStencils(ID3D11DeviceContext* DeviceContext)
{
    for (auto& [Type, Resource] : DepthStencils)
    {
        ClearDepthStencil(DeviceContext, Type);
    }
}

void FViewportResource::ClearDepthStencil(ID3D11DeviceContext* DeviceContext, EResourceType Type)
{
    if (FDepthStencilRHI* Resource = GetDepthStencil(Type))
    {
        DeviceContext->ClearDepthStencilView(Resource->DSV, D3D11_CLEAR_DEPTH /*| D3D11_CLEAR_STENCIL*/, 1.0f, 0);
    }
}

HRESULT FViewportResource::CreateRenderTarget(EResourceType Type, uint32 DownSampleScale)
{
    if (HasRenderTarget(Type))
    {
        ReleaseRenderTarget(Type);
    }

    DownSampleScale = FMath::Max(DownSampleScale, 1U);
    
    FRenderTargetRHI NewResource;
    
    HRESULT hr = S_OK;
    
    D3D11_TEXTURE2D_DESC TextureDesc = {};
    TextureDesc.Width = static_cast<uint32>(D3DViewport.Width / static_cast<float>(DownSampleScale));
    TextureDesc.Height = static_cast<uint32>(D3DViewport.Height / static_cast<float>(DownSampleScale));
    TextureDesc.MipLevels = 1;
    TextureDesc.ArraySize = 1;
    TextureDesc.Format = DXGI_FORMAT_R16G16B16A16_UNORM;
    TextureDesc.SampleDesc.Count = 1;
    TextureDesc.SampleDesc.Quality = 0;
    TextureDesc.Usage = D3D11_USAGE_DEFAULT;
    TextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    TextureDesc.CPUAccessFlags = 0;
    TextureDesc.MiscFlags = 0;
    NewResource.Texture2D = FEngineLoop::GraphicDevice.CreateTexture2D(TextureDesc, nullptr);

    D3D11_RENDER_TARGET_VIEW_DESC RTVDesc = {};
    RTVDesc.Format = DXGI_FORMAT_R16G16B16A16_UNORM;
    RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    hr = FEngineLoop::GraphicDevice.Device->CreateRenderTargetView(NewResource.Texture2D, &RTVDesc, &NewResource.RTV);
    if (FAILED(hr))
    {
        return hr;
    }
    
    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
    SRVDesc.Format = TextureDesc.Format;
    SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    SRVDesc.Texture2D.MostDetailedMip = 0;
    SRVDesc.Texture2D.MipLevels = 1;
    hr = FEngineLoop::GraphicDevice.Device->CreateShaderResourceView(NewResource.Texture2D, &SRVDesc, &NewResource.SRV);
    if (FAILED(hr))
    {
        return hr;
    }

    RenderTargets.Add(Type, NewResource);

    return hr;
}

FRenderTargetRHI* FViewportResource::GetRenderTarget(EResourceType Type, uint32 DownSampleScale)
{
    if (!HasRenderTarget(Type))
    {
        if (FAILED(CreateRenderTarget(Type, DownSampleScale)))
        {
            return nullptr;
        }
    }
    return RenderTargets.Find(Type);
}

bool FViewportResource::HasRenderTarget(EResourceType Type) const
{
    return RenderTargets.Contains(Type);
}

void FViewportResource::ClearRenderTargets(ID3D11DeviceContext* DeviceContext)
{
    for (auto& [Type, Resource] : RenderTargets)
    {
        ClearRenderTarget(DeviceContext, Type);
    }
}

void FViewportResource::ClearRenderTarget(ID3D11DeviceContext* DeviceContext, EResourceType Type)
{
    if (FRenderTargetRHI* Resource = GetRenderTarget(Type))
    {
        DeviceContext->ClearRenderTargetView(Resource->RTV, ClearColors[Type].data());
    }
}

std::array<float, 4> FViewportResource::GetClearColor(EResourceType Type) const
{
    if (const std::array<float, 4>* Found = ClearColors.Find(Type))
    {
        return *Found;
    }
    return { 0.0f, 0.0f, 0.0f, 1.0f };
}

void FViewportResource::ReleaseAllResources()
{
    for (auto& [Type, Resource] : RenderTargets)
    {
        Resource.Release();
    }
    for (auto& [Type, Resource] : DepthStencils)
    {
        Resource.Release();
    }
}

void FViewportResource::ReleaseDepthStencil(EResourceType Type)
{
    if (HasDepthStencil(Type))
    {
        DepthStencils[Type].Release();
    }
}

void FViewportResource::ReleaseRenderTarget(EResourceType Type)
{
    if (HasRenderTarget(Type))
    {
        RenderTargets[Type].Release();
    }
}


FViewport::FViewport()
    : FViewport(EViewScreenLocation::EVL_MAX)
{
}

FViewport::FViewport(EViewScreenLocation InViewLocation)
    : ViewportResource(new FViewportResource())
    , ViewLocation(InViewLocation) 
{
}

FViewport::~FViewport()
{
    delete ViewportResource;
}

void FViewport::Initialize(const FRect& InRect)
{
    Rect = InRect;
    const uint32 Width = static_cast<uint32>(Rect.Width);
    const uint32 Height = static_cast<uint32>(Rect.Height);

    ViewportResource->Initialize(Width, Height);
}

void FViewport::ResizeViewport(const FRect& InRect)
{
    Rect = InRect;
    const uint32 Width = static_cast<uint32>(Rect.Width);
    const uint32 Height = static_cast<uint32>(Rect.Height);

    ViewportResource->Resize(Width, Height);
}

void FViewport::ResizeViewport(const FRect& Top, const FRect& Bottom, const FRect& Left, const FRect& Right)
{
    switch (ViewLocation)
    {
    case EViewScreenLocation::EVL_TopLeft:
        Rect.TopLeftX = Left.TopLeftX;
        Rect.TopLeftY = Top.TopLeftY;
        Rect.Width = Left.Width;
        Rect.Height = Top.Height;
        break;
    case EViewScreenLocation::EVL_TopRight:
        Rect.TopLeftX = Right.TopLeftX;
        Rect.TopLeftY = Top.TopLeftY;
        Rect.Width = Right.Width;
        Rect.Height = Top.Height;
        break;
    case EViewScreenLocation::EVL_BottomLeft:
        Rect.TopLeftX = Left.TopLeftX;
        Rect.TopLeftY = Bottom.TopLeftY;
        Rect.Width = Left.Width;
        Rect.Height = Bottom.Height;
        break;
    case EViewScreenLocation::EVL_BottomRight:
        Rect.TopLeftX = Right.TopLeftX;
        Rect.TopLeftY = Bottom.TopLeftY;
        Rect.Width = Right.Width;
        Rect.Height = Bottom.Height;
        break;
    default:
        return;
    }
    
    const uint32 Width = static_cast<uint32>(Rect.Width);
    const uint32 Height = static_cast<uint32>(Rect.Height);
    ViewportResource->Resize(Width, Height);
}

bool FViewport::bIsHovered(const FVector2D& InPoint) const
{
    return (Rect.TopLeftX <= static_cast<float>(InPoint.X) && static_cast<float>(InPoint.X) <= Rect.TopLeftX + Rect.Width) &&
           (Rect.TopLeftY <= static_cast<float>(InPoint.Y) && static_cast<float>(InPoint.Y) <= Rect.TopLeftY + Rect.Height);
}
