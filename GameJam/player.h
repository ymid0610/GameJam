#pragma once
#include "object.h"
#include "camera.h"

class Player : public GameObject
{
public:
	// 생성자 소멸자
	Player();
	~Player() = default;

	// 업데이트 함수
	virtual void Update(FLOAT timeElapsed) override;

	// 이벤트 처리 함수
	void MouseEvent(FLOAT timeElapsed, short wheelDelta = 0);
	void KeyboardEvent(FLOAT timeElapsed);

	// Getter
	FLOAT GetSpeed() const { return m_speed; }
	bool IsCharacterController() const override { return true; }
	
	// Setter
	void SetCamera(const shared_ptr<Camera>& camera) { m_camera = camera; }
	void SetSpeed(FLOAT speed) { m_speed = speed; }

private:
	shared_ptr<Camera> m_camera;

	FLOAT m_speed;
};
