#include "Game/Entities/Entity.hpp"
#include "Engine/Renderer/2D/Sprite.hpp"

//-----------------------------------------------------------------------------------
Entity::Entity()
    : m_sprite(nullptr)
    , m_hp(1.0f)
    , m_rotationDegrees(0.0f)
    , m_maxHp(1.0f)
    , m_collisionRadius(1.0f)
    , m_age(0.0f)
    , m_isDead(false)
    , m_position(0.0f)
    , m_networkId(0)
{

}

//-----------------------------------------------------------------------------------
Entity::~Entity()
{
    if (m_sprite)
    {
        delete m_sprite;
    }
}

//-----------------------------------------------------------------------------------
void Entity::Update(float deltaSeconds)
{
    m_age += deltaSeconds;
}

//-----------------------------------------------------------------------------------
void Entity::Render() const
{

}

//-----------------------------------------------------------------------------------
bool Entity::IsCollidingWith(Entity* otherEntity)
{
    return MathUtils::DoDiscsOverlap(this->m_position, this->m_collisionRadius, otherEntity->m_position, otherEntity->m_collisionRadius);
}

//-----------------------------------------------------------------------------------
void Entity::ResolveCollision(Entity* otherEntity)
{
    Vector2& myPosition = m_position;
    Vector2& otherPosition = otherEntity->m_position;
    Vector2 difference = myPosition - otherPosition;
    float distanceBetweenPoints = MathUtils::CalcDistanceBetweenPoints(otherPosition, myPosition);
    float pushDistance = (this->m_collisionRadius - distanceBetweenPoints) / 8.f;
    difference *= pushDistance;
    myPosition -= difference;
    otherPosition += difference;
}

//-----------------------------------------------------------------------------------
void Entity::TakeDamage(float damage)
{
    m_hp -= damage;
    if (m_hp < 0.0f)
    {
        m_isDead = true;
    }
}

//-----------------------------------------------------------------------------------
void Entity::ApplyClientUpdate()
{
    if (m_sprite)
    {
        m_sprite->m_position = m_position;
        m_sprite->m_rotationDegrees = m_rotationDegrees;
    }
}

