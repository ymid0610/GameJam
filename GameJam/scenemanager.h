#pragma once
#include "stdafx.h"
#include "scene.h"

class SceneManager
{
public:
    SceneManager() = default;
    ~SceneManager() = default;

    void Update(FLOAT timeElapsed);
    void Render(const ComPtr<ID3D12GraphicsCommandList>& commandList) const;
    void ReleaseUploadBuffer();

    void ChangeScene(const ComPtr<ID3D12Device>& device,
        const ComPtr<ID3D12GraphicsCommandList>& commandList,
        const ComPtr<ID3D12RootSignature>& rootSignature,
        unique_ptr<Scene> newScene);
    void PushScene(const ComPtr<ID3D12Device>& device,
        const ComPtr<ID3D12GraphicsCommandList>& commandList,
        const ComPtr<ID3D12RootSignature>& rootSignature,
        unique_ptr<Scene> newScene);
    void PopScene();

    void MouseEvent(HWND hWnd, FLOAT timeElapsed);
    void KeyboardEvent(FLOAT timeElapsed);
    void KeyboardEvent(UINT message, WPARAM wParam);
    void MouseWheelEvent(WPARAM wParam);

private:
    vector<unique_ptr<Scene>> m_scenes;
};