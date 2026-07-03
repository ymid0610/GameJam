#pragma once
#include "stdafx.h"
#include <type_traits>

class GameObject;

class Component
{
public:
    explicit Component(GameObject& owner) : m_owner(owner) {}
    virtual ~Component() = default;

    Component(const Component&) = delete;
    Component& operator=(const Component&) = delete;
    Component(Component&&) = delete;
    Component& operator=(Component&&) = delete;

    virtual void Update(FLOAT timeElapsed) { (void)timeElapsed; }
    GameObject& GetOwner() const { return m_owner; }

private:
    GameObject& m_owner;
};

class ShadowCasterComponent final : public Component
{
public:
    explicit ShadowCasterComponent(GameObject& owner, bool castsShadow = true)
        : Component(owner), m_castsShadow(castsShadow)
    {
    }

    void SetCastsShadow(bool castsShadow) { m_castsShadow = castsShadow; }
    bool CastsShadow() const { return m_castsShadow; }

private:
    bool m_castsShadow = true;
};
