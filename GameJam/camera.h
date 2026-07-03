#pragma once
#include "stdafx.h"

class Camera
{
public:
	// 생성자 소멸자
	Camera();
	~Camera() = default;

	// 업데이트 함수
	virtual void Update(FLOAT timeElapsed) = 0;
	virtual void UpdateEye(XMFLOAT3 position) = 0;
	void UpdateShaderVariable(const ComPtr<ID3D12GraphicsCommandList>& commandList);

protected:
	void UpdateBasis();

public:
	// 멤버	함수
	virtual void RotatePitch(FLOAT radian) = 0;
	virtual void RotateYaw(FLOAT radian) = 0;

	// Setter
	void SetLens(FLOAT fovy, FLOAT aspect, FLOAT minZ, FLOAT maxZ) {
		XMStoreFloat4x4(&m_projectionMatrix, XMMatrixPerspectiveFovLH(fovy, aspect, minZ, maxZ));
	}

	// Getter
	XMFLOAT3 GetEye() const { return m_eye; }
	XMFLOAT3 GetU() const { return m_u; }
	XMFLOAT3 GetV() const { return m_v; }
	XMFLOAT3 GetN() const { return m_n; }
	virtual bool IsFirstPerson() const { return false; }

protected:
	XMFLOAT4X4 m_viewMatrix;
	XMFLOAT4X4 m_projectionMatrix;

	XMFLOAT3 m_eye;
	XMFLOAT3 m_at;
	XMFLOAT3 m_up;

	XMFLOAT3 m_u;
	XMFLOAT3 m_v;
	XMFLOAT3 m_n;
};

class FirstPersonCamera : public Camera
{
public:
	FirstPersonCamera();
	~FirstPersonCamera() = default;

	void Update(FLOAT timeElapsed) override;
	void UpdateEye(XMFLOAT3 position) override;

	void RotatePitch(FLOAT radian) override;
	void RotateYaw(FLOAT radian) override;
	bool IsFirstPerson() const override { return true; }

private:
	FLOAT m_pitch;
	FLOAT m_yaw;
	XMFLOAT3 m_eyeOffset;
};
class ThirdPersonCamera : public Camera
{
public:
	ThirdPersonCamera();
	~ThirdPersonCamera() = default;

	void Update(FLOAT timeElapsed) override;
	void UpdateEye(XMFLOAT3 position) override;

	void RotatePitch(FLOAT radian) override;
	void RotateYaw(FLOAT radian) override;
private:
	FLOAT m_radius;
	FLOAT m_phi;
	FLOAT m_theta;
};

class SpringArmCamera : public Camera
{
public:
	// 생성자 소멸자
	SpringArmCamera();
	~SpringArmCamera() = default;

	// 업데이트 함수
	void Update(FLOAT timeElapsed) override;
	void UpdateEye(XMFLOAT3 position) override;

	// 멤버 함수
	void RotatePitch(FLOAT radian) override;
	void RotateYaw(FLOAT radian) override;

	void AddArmLength(FLOAT length);

	// Setter
	void SetArmLength(FLOAT length);
	void SetCollisionDistance(float distance);

	// Getter
	FLOAT GetArmLength() const { return m_targetArmLength; }

	// 레이캐스팅을 위한 원점(타겟)과 방향 반환
	XMFLOAT3 GetLookAtPosition() const { return m_at; }
	XMFLOAT3 GetDirectionToCamera() const;

private:
	FLOAT m_currentArmLength; // 현재 적용중인 길이 (보간용)
	FLOAT m_targetArmLength;  // 목표 길이

	FLOAT m_phi;   // Pitch (위/아래)
	FLOAT m_theta; // Yaw (좌/우)

	XMFLOAT3 m_offset;  // 캐릭터의 발바닥이 아닌 상체(머리/어깨)를 찍기 위한 오프셋
};

class SpectatorCamera : public Camera
{
public:
	SpectatorCamera();
	~SpectatorCamera() = default;

	void Update(FLOAT timeElapsed) override;
	void UpdateEye(XMFLOAT3 position) override;

	void RotatePitch(FLOAT radian) override;
	void RotateYaw(FLOAT radian) override;

	void SetPose(const XMFLOAT3& eye, const XMFLOAT3& forward);
	void Move(const XMFLOAT3& displacement);

private:
	FLOAT m_pitch;
	FLOAT m_yaw;
};