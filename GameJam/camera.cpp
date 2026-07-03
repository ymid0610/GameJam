#include "camera.h"

Camera::Camera() : m_eye{0.f, 0.f, 0.f}, m_at{0.f, 0.f, 1.f}, m_up{0.f, 1.f, 0.f},
	m_u{1.f, 0.f, 0.f}, m_v{0.f, 1.f, 0.f}, m_n{0.f, 0.f, 1.f}
{
	XMStoreFloat4x4(&m_viewMatrix, XMMatrixIdentity());
	XMStoreFloat4x4(&m_projectionMatrix, XMMatrixIdentity());
}

// 업데이트 함수

void Camera::UpdateShaderVariable(const ComPtr<ID3D12GraphicsCommandList>& commandList)
{
	XMStoreFloat4x4(&m_viewMatrix, XMMatrixLookAtLH(XMLoadFloat3(&m_eye), XMLoadFloat3(&m_at), XMLoadFloat3(&m_up)));

	XMFLOAT4X4 viewMatrix;
	XMStoreFloat4x4(&viewMatrix, XMMatrixTranspose(XMLoadFloat4x4(&m_viewMatrix)));
	commandList->SetGraphicsRoot32BitConstants(1, 16, &viewMatrix, 0);

	XMFLOAT4X4 projectionMatrix;
	XMStoreFloat4x4(&projectionMatrix, XMMatrixTranspose(XMLoadFloat4x4(&m_projectionMatrix)));
	commandList->SetGraphicsRoot32BitConstants(1, 16, &projectionMatrix, 16);

	XMFLOAT4 cameraPosition{ m_eye.x, m_eye.y, m_eye.z, 1.0f };
	commandList->SetGraphicsRoot32BitConstants(1, 4, &cameraPosition, 32);
}

void Camera::UpdateBasis()
{
	m_n = Utiles::Vector3::Normalize(Utiles::Vector3::Sub(m_at, m_eye));
	m_u = Utiles::Vector3::Normalize(Utiles::Vector3::Cross(m_up, m_n));
	m_v = Utiles::Vector3::Normalize(Utiles::Vector3::Cross(m_n, m_u));
}

//
//////////////////////////////////////////////////////////////////////////////
FirstPersonCamera::FirstPersonCamera() : Camera{},
	m_pitch{ 0.0f },
	m_yaw{ Settings::DefaultCameraYaw },
	m_eyeOffset{ 0.0f, Settings::FirstPersonEyeHeight, 0.0f }
{
}

void FirstPersonCamera::Update(FLOAT timeElapsed)
{
	(void)timeElapsed;
}

void FirstPersonCamera::UpdateEye(XMFLOAT3 position)
{
	m_eye = Utiles::Vector3::Add(position, m_eyeOffset);

	XMFLOAT3 lookDirection{
		sinf(m_yaw) * cosf(m_pitch),
		sinf(m_pitch),
		cosf(m_yaw) * cosf(m_pitch)
	};

	m_at = Utiles::Vector3::Add(m_eye, Utiles::Vector3::Normalize(lookDirection));
	UpdateBasis();
}

void FirstPersonCamera::RotatePitch(FLOAT radian)
{
	m_pitch += radian;
	m_pitch = clamp(m_pitch, Settings::FirstPersonMinPitch, Settings::FirstPersonMaxPitch);
}

void FirstPersonCamera::RotateYaw(FLOAT radian)
{
	m_yaw -= radian;
}

//
//////////////////////////////////////////////////////////////////////////////
ThirdPersonCamera::ThirdPersonCamera() : Camera{}, m_radius{Settings::DefaultCameraRadius},
	m_phi{Settings::DefaultCameraPitch}, m_theta{Settings::DefaultCameraYaw}
{

}

void ThirdPersonCamera::Update(FLOAT timeElapsed)
{

}

void ThirdPersonCamera::UpdateEye(XMFLOAT3 position)
{
	XMFLOAT3 offset{
	m_radius * sin(m_phi) * cos(m_theta),
	m_radius * cos(m_phi),
	m_radius * sin(m_phi) * sin(m_theta) };

	m_eye = Utiles::Vector3::Add(position, offset);
	m_at = position;
	UpdateBasis();
}

void ThirdPersonCamera::RotatePitch(FLOAT radian)
{
	m_phi += radian;
	m_phi = clamp(m_phi, Settings::CameraMinPitch, Settings::CameraMaxPitch);
}

void ThirdPersonCamera::RotateYaw(FLOAT radian)
{
	m_theta += radian;
}

//
///////////////////////////////////////////////////////////////////////////////

SpringArmCamera::SpringArmCamera() : Camera{},
m_targetArmLength{ Settings::DefaultCameraRadius },
m_currentArmLength{ Settings::DefaultCameraRadius },
m_phi{ Settings::DefaultCameraPitch },
m_theta{ Settings::DefaultSpringArmYaw },
m_offset{ 0.0f, 0.f, 0.0f } // 캐릭터 중심점 오프셋 (예: Y축으로 1.5만큼 위를 바라봄)
{
}

void SpringArmCamera::Update(FLOAT timeElapsed)
{
	// 목표 길이를 향해 현재 길이를 부드럽게 보간(Lerp)합니다.
	m_currentArmLength += (m_targetArmLength - m_currentArmLength) * 10.0f * timeElapsed;
}

void SpringArmCamera::UpdateEye(XMFLOAT3 targetPosition)
{
	// 바라보는 실제 지점 (캐릭터 위치 + 오프셋)
	XMFLOAT3 lookAt = Utiles::Vector3::Add(targetPosition, m_offset);

	// 구면 좌표계로 카메라 위치 오프셋 계산 (타겟 기준)
	XMFLOAT3 offset{
		m_currentArmLength * sin(m_phi) * cos(m_theta),
		m_currentArmLength * cos(m_phi),
		m_currentArmLength * sin(m_phi) * sin(m_theta)
	};

	// 눈 위치 = 타겟 + 카메라 오프셋
	m_eye = Utiles::Vector3::Add(lookAt, offset);
	m_at = lookAt; // 카메라는 항상 타겟을 바라봄

	// 여기서 레이캐스트(Raycast)를 날려 지형이나 벽과 충돌을 검사하고
	// 충돌 시 m_eye 위치를 벽 앞으로 당겨오는 코드를 추가하면 
	// 완벽한 충돌 방지 스프링 암이 됩니다.

	UpdateBasis();
}

void SpringArmCamera::RotatePitch(FLOAT radian)
{
	m_phi += radian;
	m_phi = clamp(m_phi, Settings::CameraMinPitch, Settings::CameraMaxPitch);
}

void SpringArmCamera::RotateYaw(FLOAT radian)
{
	m_theta += radian;
}

void SpringArmCamera::SetArmLength(FLOAT length)
{
	m_targetArmLength = std::clamp(length, 0.1f, 15.0f);
}

void SpringArmCamera::AddArmLength(FLOAT length)
{
	SetArmLength(m_targetArmLength + length);
}

XMFLOAT3 SpringArmCamera::GetDirectionToCamera() const
{
	XMFLOAT3 dir{
		sin(m_phi) * cos(m_theta),
		cos(m_phi),
		sin(m_phi) * sin(m_theta)
	};
	return Utiles::Vector3::Normalize(dir);
}

// 충돌(Raycast) 거리에 맞춰 카메라 강제 이동
void SpringArmCamera::SetCollisionDistance(float distance)
{
	// 벽에 너무 붙으면 시야가 뚫릴 수 있으므로 0.2f 정도 여백(margin)을 줍니다.
	float actualDist = max(0.0f, distance - 0.2f);

	XMFLOAT3 offset{
		actualDist * sin(m_phi) * cos(m_theta),
		actualDist * cos(m_phi),
		actualDist * sin(m_phi) * sin(m_theta)
	};

	// 타겟 지점을 기준으로 충돌 거리만큼만 뒤로 물리침
	m_eye = Utiles::Vector3::Add(m_at, offset);
	UpdateBasis(); // 방향 갱신
}

//
///////////////////////////////////////////////////////////////////////////////
SpectatorCamera::SpectatorCamera() : Camera{},
	m_pitch{ 0.0f },
	m_yaw{ Settings::DefaultCameraYaw }
{
	UpdateEye(m_eye);
}

void SpectatorCamera::Update(FLOAT timeElapsed)
{
	(void)timeElapsed;
}

void SpectatorCamera::UpdateEye(XMFLOAT3 position)
{
	(void)position;

	XMFLOAT3 lookDirection{
		sinf(m_yaw) * cosf(m_pitch),
		sinf(m_pitch),
		cosf(m_yaw) * cosf(m_pitch)
	};

	m_at = Utiles::Vector3::Add(m_eye, Utiles::Vector3::Normalize(lookDirection));
	UpdateBasis();
}

void SpectatorCamera::RotatePitch(FLOAT radian)
{
	m_pitch += radian;
	m_pitch = clamp(m_pitch, Settings::FirstPersonMinPitch, Settings::FirstPersonMaxPitch);
	UpdateEye(m_eye);
}

void SpectatorCamera::RotateYaw(FLOAT radian)
{
	m_yaw -= radian;
	UpdateEye(m_eye);
}

void SpectatorCamera::SetPose(const XMFLOAT3& eye, const XMFLOAT3& forward)
{
	XMFLOAT3 direction = Utiles::Vector3::Normalize(forward);
	m_eye = eye;
	m_pitch = asinf(clamp(direction.y, -1.0f, 1.0f));
	m_yaw = atan2f(direction.x, direction.z);
	UpdateEye(m_eye);
}

void SpectatorCamera::Move(const XMFLOAT3& displacement)
{
	m_eye = Utiles::Vector3::Add(m_eye, displacement);
	UpdateEye(m_eye);
}