#ifndef D3DENGINE_H
#define D3DENGINE_H

#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <DirectXMath.h>

#include <array>
#include <vector>
#include <string>

class D3DEngine
{
public:
    explicit D3DEngine(HWND hwnd);
    ~D3DEngine() = default;

    void cleanup();

    void render();

private:
    void createDXGIFactory();
    void getAdapter(IDXGIAdapter1 **adapter);
    void createDevice();
    void createCommandResources();
    void createSwapChain(HWND hwnd);
    void createSwapChainResources();
    void createFence();

    void createVertexBuffer();

    void beginFrame(UINT frameIndex);
    void recordCommands(UINT frameIndex) const;
    void endFrame(UINT frameIndex);

    void executeCommand(UINT frameIndex);
    void waitForFence(UINT frameIndex);

    void createAS();
    void createRaytracingPipelineState();
    void createRaytracingResources();
    void createShaderTable();

    static constexpr UINT FRAME_COUNT = 2;

    Microsoft::WRL::ComPtr<IDXGIFactory7> m_dxgiFactory;
    Microsoft::WRL::ComPtr<ID3D12Device5> m_device;
    std::array<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>, FRAME_COUNT> m_commandAllocators;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> m_commandList;

    Microsoft::WRL::ComPtr<IDXGISwapChain4> m_swapchain;
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, FRAME_COUNT> m_backBuffers;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    std::array<float, 4> m_clearColor = {0.0f, 0.0f, 0.0f, 1.0f};

    std::array<Microsoft::WRL::ComPtr<ID3D12Fence>, FRAME_COUNT> m_fences;
    std::array<UINT64, FRAME_COUNT> m_fenceValues = {};
    std::array<HANDLE, FRAME_COUNT> m_fenceEvents = {};

    const std::vector<DirectX::XMFLOAT3> m_vertices = {
        {0.0, 0.5f, 0.0f},
        {0.5f, -0.5f, 0.0f},
        {-0.5f, -0.5f, 0.0f}
    };

    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView = {};

    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_blas;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_tlas;

    Microsoft::WRL::ComPtr<ID3D12StateObject> m_raytracingPipelineState;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_globalRootSignature;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_descHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_raytracingOutput;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_shaderTable;
    UINT m_shaderRecordSize = 0;

    RECT m_windowRect = {};

    const std::wstring SHADER_FILE = L"shader.hlsl";
    const std::wstring RAYGEN_SHADER = L"RayGen";
    const std::wstring MISS_SHADER = L"MissShader";
    const std::wstring CLOSEST_HIT_SHADER = L"ClosestHitShader";
    const std::wstring HIT_GROUP = L"HitGroup";

    struct RaytracingPayload
    {
        bool hit;
    };

    struct BuiltInTriangleIntersectionAttributes
    {
        DirectX::XMFLOAT2 barycentrics;
    };
};


#endif //D3DENGINE_H