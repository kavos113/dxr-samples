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

void createBuffer(
    ID3D12Device* device,
    ID3D12Resource** buffer,
    size_t size,
    D3D12_HEAP_TYPE heapType,
    D3D12_RESOURCE_FLAGS resourceFlags,
    D3D12_RESOURCE_STATES initialState
)
{
    D3D12_HEAP_PROPERTIES heapProps = {
        .Type = heapType,
        .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
        .CreationNodeMask = 1,
        .VisibleNodeMask = 1
    };
    D3D12_RESOURCE_DESC resourceDesc = {
        .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
        .Alignment = 0,
        .Width = size,
        .Height = 1,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = DXGI_FORMAT_UNKNOWN,
        .SampleDesc = {1, 0},
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        .Flags = resourceFlags
    };
    HRESULT hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        initialState,
        nullptr,
        IID_PPV_ARGS(buffer)
    );
    if (FAILED(hr))
    {
        throw std::runtime_error("Failed to create buffer.");
    }
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

    createAS();
}

void D3DEngine::cleanup()
{
    for (int i = 0; i < FRAME_COUNT; ++i)
    {
        waitForFence(i);
    }

    for (auto& fenceEvent : m_fenceEvents)
    {
        if (fenceEvent)
        {
            CloseHandle(fenceEvent);
            fenceEvent = nullptr;
        }
    }

    for (auto& fence : m_fences)
    {
        fence.Reset();
    }

    m_commandList.Reset();
    m_rtvHeap.Reset();
    for (auto& backBuffer : m_backBuffers)
    {
        backBuffer.Reset();
    }
    m_swapchain.Reset();

    m_commandQueue.Reset();
    for (auto& commandAllocator : m_commandAllocators)
    {
        commandAllocator.Reset();
    }

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
            D3D12_FEATURE_DATA_D3D12_OPTIONS5 options = {};
            hr = m_device->CheckFeatureSupport(
                D3D12_FEATURE_D3D12_OPTIONS5,
                &options,
                sizeof(options)
            );
            if (SUCCEEDED(hr) && options.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
            {
                std::cout << "Raytracing is supported." << std::endl;
            }
            else
            {
                std::cout << "Raytracing is not supported." << std::endl;
                continue;
            }

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
    for (UINT i = 0; i < FRAME_COUNT; ++i)
    {
        HRESULT hr = m_device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&m_commandAllocators[i])
        );
        if (FAILED(hr))
        {
            throw std::runtime_error("Failed to create command allocator.");
        }
    }

    HRESULT hr = m_device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_commandAllocators[0].Get(),
        nullptr,
        IID_PPV_ARGS(&m_commandList)
    );
    if (FAILED(hr))
    {
        throw std::runtime_error("Failed to create command list.");
    }

    D3D12_COMMAND_QUEUE_DESC queueDesc = {
        .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
        .Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
        .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
        .NodeMask = 0
    };
    hr = m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue));
    if (FAILED(hr))
    {
        throw std::runtime_error("Failed to create command queue.");
    }
}

void D3DEngine::createSwapChain(HWND hwnd)
{
    RECT rect;
    GetClientRect(hwnd, &rect);

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {
        .Width = static_cast<UINT>(rect.right - rect.left),
        .Height = static_cast<UINT>(rect.bottom - rect.top),
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .Stereo = FALSE,
        .SampleDesc = {1, 0},
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = FRAME_COUNT,
        .Scaling = DXGI_SCALING_STRETCH,
        .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
        .AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
        .Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
    };
    HRESULT hr = m_dxgiFactory->CreateSwapChainForHwnd(
        m_commandQueue.Get(),
        hwnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        reinterpret_cast<IDXGISwapChain1**>(m_swapchain.GetAddressOf())
    );
    if (FAILED(hr))
    {
        throw std::runtime_error("Failed to create swap chain.");
    }
}

void D3DEngine::createSwapChainResources()
{
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        .NumDescriptors = FRAME_COUNT,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        .NodeMask = 0
    };
    HRESULT hr = m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap));
    if (FAILED(hr))
    {
        throw std::runtime_error("Failed to create RTV descriptor heap.");
    }

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    hr = m_swapchain->GetDesc1(&swapChainDesc);
    if (FAILED(hr))
    {
        throw std::runtime_error("Failed to get swap chain description.");
    }

    for (UINT i = 0; i < FRAME_COUNT; ++i)
    {
        hr = m_swapchain->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i]));
        if (FAILED(hr))
        {
            throw std::runtime_error("Failed to get back buffer.");
        }

        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
        rtvHandle.ptr += i * m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        m_device->CreateRenderTargetView(m_backBuffers[i].Get(), nullptr, rtvHandle);
    }
}

void D3DEngine::createFence()
{
    for (UINT i = 0; i < FRAME_COUNT; ++i)
    {
        HRESULT hr = m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fences[i]));
        if (FAILED(hr))
        {
            throw std::runtime_error("Failed to create fence.");
        }
        m_fenceValues[i] = 0;

        m_fenceEvents[i] = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_fenceEvents[i] == nullptr)
        {
            throw std::runtime_error("Failed to create fence event.");
        }
    }
}

void D3DEngine::createVertexBuffer()
{
    createBuffer(
        m_device.Get(),
        &m_vertexBuffer,
        sizeof(DirectX::XMFLOAT3) * m_vertices.size(),
        D3D12_HEAP_TYPE_UPLOAD,
        D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATE_GENERIC_READ
    );

    DirectX::XMFLOAT3* mappedData = nullptr;
    HRESULT hr = m_vertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mappedData));
    if (FAILED(hr))
    {
        throw std::runtime_error("Failed to map vertex buffer.");
    }
    std::ranges::copy(m_vertices, mappedData);
    m_vertexBuffer->Unmap(0, nullptr);

    m_vertexBufferView = {
        .BufferLocation = m_vertexBuffer->GetGPUVirtualAddress(),
        .SizeInBytes = static_cast<UINT>(sizeof(DirectX::XMFLOAT3) * m_vertices.size()),
        .StrideInBytes = sizeof(DirectX::XMFLOAT3)
    };
}

void D3DEngine::beginFrame(UINT frameIndex)
{
    D3D12_RESOURCE_BARRIER barrier = {
        .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
        .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
        .Transition = {
            .pResource = m_backBuffers[frameIndex].Get(),
            .Subresource = 0,
            .StateBefore = D3D12_RESOURCE_STATE_PRESENT,
            .StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET
        }
    };
    m_commandList->ResourceBarrier(1, &barrier);

    auto rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += frameIndex * m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
    m_commandList->ClearRenderTargetView(rtvHandle, m_clearColor.data(), 0, nullptr);
}

void D3DEngine::recordCommands(UINT frameIndex) const
{
}

void D3DEngine::endFrame(UINT frameIndex)
{
    D3D12_RESOURCE_BARRIER barrier = {
        .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
        .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
        .Transition = {
            .pResource = m_backBuffers[frameIndex].Get(),
            .Subresource = 0,
            .StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET,
            .StateAfter = D3D12_RESOURCE_STATE_PRESENT
        }
    };
    m_commandList->ResourceBarrier(1, &barrier);

    executeCommand(frameIndex);

    HRESULT hr = m_swapchain->Present(1, 0);
    if (FAILED(hr))
    {
        std::cerr << "Failed to present swap chain." << std::endl;
        return;
    }
}

void D3DEngine::executeCommand(UINT frameIndex)
{
    HRESULT hr = m_commandList->Close();
    if (FAILED(hr))
    {
        std::cerr << "Failed to close command list." << std::endl;
        return;
    }

    std::array<ID3D12CommandList*, 1> commandLists = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(static_cast<UINT>(commandLists.size()), commandLists.data());

    waitForFence(frameIndex);

    hr = m_commandAllocators[frameIndex]->Reset();
    if (FAILED(hr))
    {
        std::cerr << "Failed to reset command allocator." << std::endl;
        return;
    }

    hr = m_commandList->Reset(m_commandAllocators[frameIndex].Get(), nullptr);
    if (FAILED(hr))
    {
        std::cerr << "Failed to reset command list." << std::endl;
        return;
    }
}

void D3DEngine::waitForFence(UINT frameIndex)
{
    m_fenceValues[frameIndex]++;
    UINT64 fenceValue = m_fenceValues[frameIndex];
    HRESULT hr = m_commandQueue->Signal(m_fences[frameIndex].Get(), fenceValue);
    if (FAILED(hr))
    {
        std::cerr << "Failed to signal command queue." << std::endl;
        return;
    }

    if (m_fences[frameIndex]->GetCompletedValue() < fenceValue)
    {
        hr = m_fences[frameIndex]->SetEventOnCompletion(fenceValue, m_fenceEvents[frameIndex]);
        if (FAILED(hr))
        {
            std::cerr << "Failed to set event on fence completion." << std::endl;
            return;
        }
        WaitForSingleObject(m_fenceEvents[frameIndex], INFINITE);
    }
}

void D3DEngine::createAS()
{
    // blas
    D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {
        .Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES,
        .Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE,
        .Triangles = {
            .VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT,
            .VertexCount = static_cast<UINT>(m_vertices.size()),
            .VertexBuffer = {
                .StartAddress = m_vertexBuffer->GetGPUVirtualAddress(),
                .StrideInBytes = sizeof(DirectX::XMFLOAT3)
            },
        }
    };

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS asInputs = {
        .Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL,
        .Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE,
        .NumDescs = 1,
        .DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
        .pGeometryDescs = &geometryDesc
    };

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO asPrebuildInfo = {};
    m_device->GetRaytracingAccelerationStructurePrebuildInfo(&asInputs, &asPrebuildInfo);

    Microsoft::WRL::ComPtr<ID3D12Resource> blasScratch;
    createBuffer(
        m_device.Get(),
        &blasScratch,
        asPrebuildInfo.ScratchDataSizeInBytes,
        D3D12_HEAP_TYPE_DEFAULT,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS
    );
    createBuffer(
        m_device.Get(),
        &m_blas,
        asPrebuildInfo.ResultDataMaxSizeInBytes,
        D3D12_HEAP_TYPE_DEFAULT,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE
    );

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC blasDesc = {
        .DestAccelerationStructureData = m_blas->GetGPUVirtualAddress(),
        .Inputs = asInputs,
        .ScratchAccelerationStructureData = blasScratch->GetGPUVirtualAddress()
    };
    m_commandList->BuildRaytracingAccelerationStructure(&blasDesc, 0, nullptr);

    D3D12_RESOURCE_BARRIER barrier = {
        .Type = D3D12_RESOURCE_BARRIER_TYPE_UAV,
        .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
        .UAV = {
            .pResource = m_blas.Get()
        }
    };
    m_commandList->ResourceBarrier(1, &barrier);

    // tlas
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS tlasInputs = {
        .Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL,
        .Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE,
        .NumDescs = 1,
        .DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
    };
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO tlasPrebuildInfo = {};
    m_device->GetRaytracingAccelerationStructurePrebuildInfo(&tlasInputs, &tlasPrebuildInfo);

    Microsoft::WRL::ComPtr<ID3D12Resource> tlasScratch;
    Microsoft::WRL::ComPtr<ID3D12Resource> instanceDescBuffer;
    createBuffer(
        m_device.Get(),
        tlasScratch.GetAddressOf(),
        tlasPrebuildInfo.ScratchDataSizeInBytes,
        D3D12_HEAP_TYPE_DEFAULT,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS
    );
    createBuffer(
        m_device.Get(),
        m_tlas.GetAddressOf(),
        tlasPrebuildInfo.ResultDataMaxSizeInBytes,
        D3D12_HEAP_TYPE_DEFAULT,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE
    );
    createBuffer(
        m_device.Get(),
        instanceDescBuffer.GetAddressOf(),
        sizeof(D3D12_RAYTRACING_INSTANCE_DESC),
        D3D12_HEAP_TYPE_UPLOAD,
        D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATE_GENERIC_READ
    );

    D3D12_RAYTRACING_INSTANCE_DESC* instanceDesc = nullptr;
    HRESULT hr = instanceDescBuffer->Map(0, nullptr, reinterpret_cast<void**>(&instanceDesc));
    if (FAILED(hr))
    {
        throw std::runtime_error("Failed to map instance descriptor buffer.");
    }
    instanceDesc[0].InstanceID = 0;
    instanceDesc[0].InstanceMask = 0xFF;
    instanceDesc[0].InstanceContributionToHitGroupIndex = 0;
    instanceDesc[0].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
    instanceDesc[0].AccelerationStructure = m_blas->GetGPUVirtualAddress();
    DirectX::XMFLOAT3X4 transformMatrix;
    DirectX::XMStoreFloat3x4(&transformMatrix, DirectX::XMMatrixIdentity());
    memcpy(instanceDesc[0].Transform, &transformMatrix, sizeof(transformMatrix));
    instanceDescBuffer->Unmap(0, nullptr);

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC tlasDesc = {
        .DestAccelerationStructureData = m_tlas->GetGPUVirtualAddress(),
        .Inputs = tlasInputs,
        .ScratchAccelerationStructureData = tlasScratch->GetGPUVirtualAddress()
    };
    tlasDesc.Inputs.InstanceDescs = instanceDescBuffer->GetGPUVirtualAddress();

    m_commandList->BuildRaytracingAccelerationStructure(&tlasDesc, 0, nullptr);

    D3D12_RESOURCE_BARRIER tlasBarrier = {
        .Type = D3D12_RESOURCE_BARRIER_TYPE_UAV,
        .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
        .UAV = {
            .pResource = m_tlas.Get()
        }
    };
    m_commandList->ResourceBarrier(1, &tlasBarrier);

    executeCommand(0);
}
