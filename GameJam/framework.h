#pragma once
#include "stdafx.h"
#include "timer.h"
#include "scenemanager.h"

class GameFramework
{
public:
    GameFramework(UINT windowWidth, UINT windowHeight);
    ~GameFramework();

    void OnCreate(HINSTANCE hInstance, HWND hWnd);
    void OnDestroy();
    void FrameAdvance();

    void MouseEvent(HWND hWnd, FLOAT timeElapsed);
    void KeyboardEvent(FLOAT timeElapsed);
    void KeyboardEvent(UINT message, WPARAM wParam);
    void MouseWheelEvent(WPARAM wParam);

    void SetActive(BOOL isActive);

    FLOAT GetAspectRatio();
    UINT GetWindowWidth();
    UINT GetWindowHeight();

private:
    void InitDirect3D();

    void CreateDevice();
    void CreateFence();
    void Check4xMSAAMultiSampleQuality();
    void CreateCommandQueueAndList();
    void CreateSwapChain();
    void CreateRtvDsvDescriptorHeap();
    void CreateRenderTargetView();
    void CreateDepthStencilView();
    void CreateRootSignature();

    void BuildObjects();
    void WaitForGpuComplete();
    void Update();
    void Render();

private:
    static constexpr INT SwapChainBufferCount = 2;

    BOOL m_activate = false;
    HINSTANCE m_hInstance = nullptr;
    HWND m_hWnd = nullptr;
    UINT m_windowWidth = 0;
    UINT m_windowHeight = 0;
    FLOAT m_aspectRatio = 1.0f;

    D3D12_VIEWPORT m_viewport{};
    D3D12_RECT m_scissorRect{};
    ComPtr<IDXGIFactory4> m_factory;
    ComPtr<IDXGISwapChain3> m_swapChain;
    ComPtr<ID3D12Device> m_device;
    INT m_MSAA4xQualityLevel = 0;
    ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    ComPtr<ID3D12Resource> m_renderTargets[SwapChainBufferCount];
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    UINT m_rtvDescriptorSize = 0;
    ComPtr<ID3D12Resource> m_depthStencil;
    ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
    ComPtr<ID3D12RootSignature> m_rootSignature;

    ComPtr<ID3D12Fence> m_fence;
    UINT m_frameIndex = 0;
    UINT64 m_fenceValue = 0;
    HANDLE m_fenceEvent = nullptr;

    Timer m_timer;
    unique_ptr<SceneManager> m_scene;
};