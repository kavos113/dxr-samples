#include "D3DEngine.h"

#include <iostream>

namespace
{
void enableDebugLayer()
{
    Microsoft::WRL::ComPtr<ID3D12Debug1> debugController;
    HRESULT hr = D3D12GetDebugInterface(IID_PPV_ARGS(&debugController));
    if (hr == S_OK)
    {
        debugController->EnableDebugLayer();
        debugController->SetEnableGPUBasedValidation(true);
    }
    else
    {
        std::cerr << "Failed to get D3D12 debug interface." << std::endl;
    }

    std::cout << "D3D12 debug layer enabled." << std::endl;
}
}

D3DEngine::D3DEngine(HWND hwnd)
{
#ifdef DEBUG
    enableDebugLayer();
#endif

    createDXGIFactory();
    createDevice();
    createCommandResources();
    createSwapChain(hwnd);
    createSwapChainResources();
    createFence();
    createVertexBuffer();
}

void D3DEngine::cleanup()
{
    waitForFence();
    CloseHandle(m_fenceEvent);
    m_fenceEvent = nullptr;

    m_fence.Reset();
    m_commandList.Reset();
    m_rtvHeap.Reset();
    for (auto& backBuffer : m_backBuffers)
    {
        backBuffer.Reset();
    }
    m_swapchain.Reset();
    m_commandQueue.Reset();
    m_commandAllocator.Reset();
    m_device.Reset();
    m_dxgiFactory.Reset();
}

void D3DEngine::render()
{
    UINT frameIndex = m_swapchain->GetCurrentBackBufferIndex();

    beginFrame(frameIndex);
    recordCommands(frameIndex);
    endFrame(frameIndex);
}

void D3DEngine::createDXGIFactory()
{
    HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&m_dxgiFactory));
    if (FAILED(hr))
    {
        throw std::runtime_error("Failed to create DXGI Factory.");
    }
}

void D3DEngine::getAdapter(IDXGIAdapter1** adapter)
{
    Microsoft::WRL::ComPtr<IDXGIAdapter1> dxgiAdapter;

    for (
        UINT i = 0;
        m_dxgiFactory->EnumAdapterByGpuPreference(
            i,
            DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
            IID_PPV_ARGS(&dxgiAdapter)
        ) != DXGI_ERROR_NOT_FOUND;
        ++i
    )
    {
        DXGI_ADAPTER_DESC1 desc;
        HRESULT hr = dxgiAdapter->GetDesc1(&desc);
        if (FAILED(hr))
        {
            continue;
        }

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            continue;
        }

        break;
    }

    if (dxgiAdapter == nullptr)
    {
        UINT adapterIndex = 0;
        SIZE_T memorySize = 0;

        for (
            UINT i = 0;
            m_dxgiFactory->EnumAdapters1(i, &dxgiAdapter) != DXGI_ERROR_NOT_FOUND;
            ++i
        )
        {
            DXGI_ADAPTER_DESC1 desc;
            HRESULT hr = dxgiAdapter->GetDesc1(&desc);
            if (FAILED(hr))
            {
                std::cerr << "Failed to get adapter description." << std::endl;
                continue;
            }

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                continue;
            }

            if (desc.DedicatedVideoMemory > memorySize)
            {
                memorySize = desc.DedicatedVideoMemory;
                adapterIndex = i;
            }
        }

        HRESULT hr = m_dxgiFactory->EnumAdapterByGpuPreference(
            adapterIndex,
            DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
            IID_PPV_ARGS(&dxgiAdapter)
        );
        if (FAILED(hr) && hr != DXGI_ERROR_NOT_FOUND)
        {
            std::cerr << "Failed to enumerate adapter by GPU preference." << std::endl;
            return;
        }
    }

    *adapter = dxgiAdapter.Detach();
}

void D3DEngine::createDevice()
{
    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    getAdapter(&adapter);

    std::array featureLevels = {
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };

    for (const auto& level : featureLevels)
    {
        HRESULT hr = D3D12CreateDevice(
            adapter.Get(),
            level,
            IID_PPV_ARGS(&m_device)
        );
        if (SUCCEEDED(hr))
        {
            std::cout << "D3D12 device created with feature level: " << std::hex << level << std::dec << std::endl;
            break;
        }
    }

    if (m_device == nullptr)
    {
        throw std::runtime_error("Failed to create D3D12 device.");
    }
}

void D3DEngine::createCommandResources()
{
}

void D3DEngine::createSwapChain(HWND hwnd)
{
}

void D3DEngine::createSwapChainResources()
{
}

void D3DEngine::createFence()
{
}

void D3DEngine::createVertexBuffer()
{
}

void D3DEngine::beginFrame(UINT frameIndex)
{
}

void D3DEngine::recordCommands(UINT frameIndex) const
{
}

void D3DEngine::endFrame(UINT frameIndex)
{
}

void D3DEngine::waitForFence()
{
}
