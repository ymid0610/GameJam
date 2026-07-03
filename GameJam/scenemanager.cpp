#include "scenemanager.h"

void SceneManager::Update(FLOAT timeElapsed)
{
    if (!m_scenes.empty()) m_scenes.back()->Update(timeElapsed);
}

void SceneManager::RenderShadowMap(const ComPtr<ID3D12GraphicsCommandList>& commandList)
{
    if (!m_scenes.empty()) m_scenes.back()->RenderShadowMap(commandList);
}

void SceneManager::Render(const ComPtr<ID3D12GraphicsCommandList>& commandList) const
{
    if (!m_scenes.empty()) m_scenes.back()->Render(commandList);
}

void SceneManager::ChangeScene(const ComPtr<ID3D12Device>& device,
    const ComPtr<ID3D12GraphicsCommandList>& commandList,
    const ComPtr<ID3D12RootSignature>& rootSignature,
    unique_ptr<Scene> newScene)
{
    if (!m_scenes.empty()) m_scenes.pop_back();
    PushScene(device, commandList, rootSignature, std::move(newScene));
}

void SceneManager::PushScene(const ComPtr<ID3D12Device>& device,
    const ComPtr<ID3D12GraphicsCommandList>& commandList,
    const ComPtr<ID3D12RootSignature>& rootSignature,
    unique_ptr<Scene> newScene)
{
    if (!newScene) return;

    m_scenes.emplace_back(std::move(newScene));
    m_scenes.back()->BuildObjects(device, commandList, rootSignature);
}

void SceneManager::PopScene()
{
    if (!m_scenes.empty()) m_scenes.pop_back();
}

void SceneManager::ReleaseUploadBuffer()
{
    if (!m_scenes.empty()) m_scenes.back()->ReleaseUploadBuffer();
}

void SceneManager::MouseEvent(HWND hWnd, FLOAT timeElapsed)
{
    if (!m_scenes.empty()) m_scenes.back()->MouseEvent(hWnd, timeElapsed);
}
void SceneManager::MouseButtonEvent(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (!m_scenes.empty()) m_scenes.back()->MouseButtonEvent(hWnd, message, wParam, lParam);
}

void SceneManager::KeyboardEvent(FLOAT timeElapsed)
{
    if (!m_scenes.empty()) m_scenes.back()->KeyboardEvent(timeElapsed);
}

void SceneManager::KeyboardEvent(UINT message, WPARAM wParam)
{
    if (message != WM_KEYDOWN || m_scenes.empty()) return;
    if (m_scenes.back()->OnKeyDown(wParam)) return;
    if (wParam != VK_ESCAPE) return;

    PopScene();
}

void SceneManager::MouseWheelEvent(WPARAM wParam)
{
    if (!m_scenes.empty()) m_scenes.back()->MouseWheelEvent(wParam);
}
