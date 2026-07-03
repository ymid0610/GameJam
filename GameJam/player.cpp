#include "player.h"

Player::Player() : GameObject(), m_speed{Settings::PlayerSpeed}
{
}

void Player::Update(FLOAT timeElapsed)
{
	if (m_camera) m_camera->Update(timeElapsed);
	if (m_camera) m_camera->UpdateEye(GetPosition());
}

void Player::KeyboardEvent(FLOAT timeElapsed)
{
	XMFLOAT3 front{ m_camera->GetN() }; front.y = 0.f;
	front = Utiles::Vector3::Normalize(front);
	XMFLOAT3 back{ Utiles::Vector3::Negate(front) };
	XMFLOAT3 right{ m_camera->GetU() };
	XMFLOAT3 left{ Utiles::Vector3::Negate(right) };
	XMFLOAT3 direction{ 0.f, 0.f, 0.f };
	bool isMoving = false;

	// 안전한 GetAsyncKeyState 입력 확인 (& 0x8000 완전 적용)
	bool w = GetAsyncKeyState('W') & 0x8000;
	bool a = GetAsyncKeyState('A') & 0x8000;
	bool s = GetAsyncKeyState('S') & 0x8000;
	bool d = GetAsyncKeyState('D') & 0x8000;

	if (w && a) {
		direction = Utiles::Vector3::Normalize(Utiles::Vector3::Add(front, left));
	}
	else if (w && d) {
		direction = Utiles::Vector3::Normalize(Utiles::Vector3::Add(front, right));
	}
	else if (s && a) {
		direction = Utiles::Vector3::Normalize(Utiles::Vector3::Add(back, left));
	}
	else if (s && d) {
		direction = Utiles::Vector3::Normalize(Utiles::Vector3::Add(back, right));
	}
	else if (w) { direction = front; }
	else if (a) { direction = left; }
	else if (s) { direction = back; }
	else if (d) { direction = right; }

	if (w || a || s || d) {
		isMoving = true;
		XMFLOAT3 angle{ Utiles::Vector3::Angle(m_front, direction) };
		XMFLOAT3 cross{ Utiles::Vector3::Cross(m_front, direction) };

		if (cross.y >= 0.f) {
			Rotate(0.f, XMConvertToDegrees(angle.y) * 10.f * timeElapsed, 0.f);
		}
		else {
			Rotate(0.f, -XMConvertToDegrees(angle.y) * 10.f * timeElapsed, 0.f);
		}
	}

	// ==========================================
	// 🌟 통제권: 위치 강제 이동(Transform) 제거 및 
	// 속도(Velocity) 기반 이동 반영
	// ==========================================
	if (m_rigidbody) {
		auto vel = m_rigidbody->GetVelocity();

		if (isMoving) {
			vel.x = direction.x * m_speed;
			vel.z = direction.z * m_speed;
		}
		else {
			// 입력이 없을 때 X, Z 속도를 0으로 하여 제자리에 멈추게 함 
			// (단, 외부 넉백 등으로 인해 속도가 너무 클 때는 강제로 0으로 잡지 않도록 안전망 구성)
			if (abs(vel.x) <= m_speed) vel.x = 0.0f;
			if (abs(vel.z) <= m_speed) vel.z = 0.0f;
		}

		m_rigidbody->SetVelocity(vel);
	}

	// ==========================================
	// 🌟 점프(Jump) 로직
	// ==========================================
	if (GetAsyncKeyState(VK_SPACE) & 0x8000) {
		if (m_rigidbody) {
			if (m_rigidbody->IsGrounded()) {
				float jumpForce = 100.0f; // 필요시 적절히 조절하세요 (예: 5.0f ~ 15.0f)
				m_rigidbody->AddForce(XMFLOAT3(0.0f, jumpForce, 0.0f), ForceMode::Impulse);
				m_rigidbody->SetGrounded(false);
			}
		}
	}
}

void Player::MouseEvent(FLOAT timeElapsed, short wheelDelta)
{
	if (wheelDelta != 0 && m_camera)
	{
		// 캐스팅해서 스프링 암 카메라인지 확인
		auto springArm = dynamic_pointer_cast<SpringArmCamera>(m_camera);
		if (springArm)
		{
			// 원하는 감도(Sensivity)로 조절: 120이 들어오면 -1.0f 또는 1.0f 씩 조절
			float zoomSpeed = 2.0f; // 휠 한번에 변하는 길이
			if (wheelDelta > 0) {
				springArm->AddArmLength(-zoomSpeed); // 줌 인 (거리 짧아짐)
			}
			else {
				springArm->AddArmLength(zoomSpeed); // 줌 아웃 (거리 멀어짐)
			}
		}
	}
}
