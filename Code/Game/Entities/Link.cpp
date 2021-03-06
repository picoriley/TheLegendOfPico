#include "Game/Entities/Link.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Input/XInputController.hpp"
#include "Engine/Math/Vector2.hpp"
#include "Engine/Renderer/2D/SpriteGameRenderer.hpp"
#include "Game/TheGame.hpp"
#include "Engine/Renderer/2D/ResourceDatabase.hpp"
#include "Game/HostSimulation.hpp"
#include "Engine/Time/Time.hpp"
#include "Engine/Renderer/AABB2.hpp"

//-----------------------------------------------------------------------------------
Link::Link(const RGBA& color) 
    : Entity()
    , m_netOwnerIndex(0)
    , m_facing(Facing::SOUTH)
    , m_rateOfFire(0.0f)
    , m_timeOfLastHurt(0.0f)
    , m_timeOfLastAttack(0.0f)
    , m_color(color)
{
    m_collisionRadius = 0.3f;
    m_isDead = false;
    m_maxHp = 10.0f;
    m_hp = 10.0f;
    m_sprite = new Sprite("pDown", TheGame::PLAYER_LAYER);
    m_sprite->m_scale = Vector2(1.0f, 1.0f);
    m_sprite->m_tintColor = m_color;
    m_speed = 1.0f;
    m_rateOfFire = 0.5f;
}

//-----------------------------------------------------------------------------------
Link::~Link()
{

}

//-----------------------------------------------------------------------------------
void Link::Update(float deltaSeconds)
{
    const float SPEED_DIVISOR = 20.0f;
    ASSERT_OR_DIE(TheGame::instance->m_host, "Update for the player should not be called on the clients.");

    Entity::Update(deltaSeconds);
    m_timeOfLastHurt += deltaSeconds;
    float adjustedSpeed = m_speed / SPEED_DIVISOR;
    InputMap& input = TheGame::instance->m_host->m_networkMappings[m_netOwnerIndex];
    
    Vector2 inputDirection = input.GetVector2("Right", "Up");
    if (this->CanMove())
    {
        Vector2 attemptedPosition = m_position + inputDirection * adjustedSpeed;
        AttemptMove(attemptedPosition);

    }
    m_facing = GetFacingFromInput(inputDirection);
}

//-----------------------------------------------------------------------------------
void Link::AttemptMove(Vector2& attemptedPosition)
{
    for (AABB2& geometry : TheGame::instance->m_host->m_levelGeometry)
    {
        while (geometry.IsIntersecting(attemptedPosition, m_collisionRadius))
        {
            AABB2 minkowskiBox = AABB2(Vector2(geometry.mins.x - m_collisionRadius, geometry.mins.y - m_collisionRadius), Vector2(geometry.maxs.x + m_collisionRadius, geometry.maxs.y + m_collisionRadius));
            Vector2 distInside = minkowskiBox.GetSmallestResolutionVector(attemptedPosition);
            bool xIsSmaller = abs(distInside.x) < abs(distInside.y);
            Vector2 displacement = Vector2(xIsSmaller ? distInside.x : 0.0f, xIsSmaller ? 0.0f : distInside.y);
            attemptedPosition += displacement;
        }
    }
    m_position = attemptedPosition;
    if (m_sprite)
    {
        m_sprite->m_position = attemptedPosition;
    }
}

//-----------------------------------------------------------------------------------
void Link::Render() const
{

}

//-----------------------------------------------------------------------------------
void Link::ResolveCollision(Entity* otherEntity)
{
    Entity::ResolveCollision(otherEntity);
}

//-----------------------------------------------------------------------------------
void Link::UpdateSpriteFromFacing()
{
    switch (m_facing)
    {
    case Link::WEST:
        m_sprite->m_spriteResource = ResourceDatabase::instance->GetSpriteResource("pLeft");
        break;
    case Link::NORTH:
        m_sprite->m_spriteResource = ResourceDatabase::instance->GetSpriteResource("pUp");
        break;
    case Link::EAST:
        m_sprite->m_spriteResource = ResourceDatabase::instance->GetSpriteResource("pRight");
        break;
    case Link::SOUTH:
        m_sprite->m_spriteResource = ResourceDatabase::instance->GetSpriteResource("pDown");
        break;
    default:
        break;
    }
}

//-----------------------------------------------------------------------------------
float Link::CalculateSwordRotationDegrees()
{
    switch (m_facing)
    {
    case WEST:
        return 270.0f;
    case NORTH:
        return 0.0f;
    case EAST:
        return 0.0f;
    case SOUTH:
        return 180.0f;
    default:
        ERROR_AND_DIE("Invalid state for facing");
    }
}

//-----------------------------------------------------------------------------------
Vector2 Link::CalculateSwordPosition()
{
    switch (m_facing)
    {
    case WEST:
        return m_sprite->GetBounds().GetTopLeft();
    case NORTH:
        return m_sprite->GetBounds().maxs;
    case EAST:
        return m_sprite->GetBounds().maxs;
    case SOUTH:
        return m_sprite->GetBounds().mins;
    default:
        ERROR_AND_DIE("Invalid state for facing");
    }
}

//-----------------------------------------------------------------------------------
Link::Facing Link::GetFacingFromInput(const Vector2& inputDirection)
{
    if (inputDirection.CalculateMagnitude() < 0.01f)
    {
        return m_facing;
    }
    float rightward = inputDirection.Dot(Vector2::UNIT_X);
    float leftward = inputDirection.Dot(-Vector2::UNIT_X);
    float upward = inputDirection.Dot(Vector2::UNIT_Y);
    float downward  = inputDirection.Dot(-Vector2::UNIT_Y);

    Facing bestDirection = Facing::SOUTH;
    float bestDifference = 1.0f - downward;
    if (1.0f - upward < bestDifference)
    {
        bestDifference = 1.0f - upward;
        bestDirection = Facing::NORTH;
    }
    if (1.0f - rightward < bestDifference)
    {
        bestDifference = 1.0f - rightward;
        bestDirection = Facing::EAST;
    }
    if (1.0f - leftward < bestDifference)
    {
        bestDifference = 1.0f - leftward;
        bestDirection = Facing::WEST;
    }
    return bestDirection;
}

//-----------------------------------------------------------------------------------
void Link::SetColor(unsigned int color)
{
    m_color = RGBA(color);
    m_sprite->m_tintColor = m_color;
}

//-----------------------------------------------------------------------------------
void Link::ApplyClientUpdate()
{
    m_sprite->m_position = m_position;
    UpdateSpriteFromFacing();
    ApplyDamageEffect();
}

//-----------------------------------------------------------------------------------
void Link::ApplyDamageEffect()
{
    const float flashSpeed = 60.0f;
    double currentTime = GetCurrentTimeSeconds();
    
    if (m_timeOfLastHurt > (currentTime - HURT_FLASH_DURATION_SECONDS))
    {
        double colorVariation = sin(currentTime * flashSpeed);
        if (colorVariation < 0.0)
        {
            m_sprite->m_tintColor = m_color.GetInverse();
        }
        else
        {
            m_sprite->m_tintColor = m_color;
        }
    }
    else
    {
        m_sprite->m_tintColor = m_color;
    }
}

//-----------------------------------------------------------------------------------
bool Link::CanMove()
{
    return !IsAttacking();
}

//-----------------------------------------------------------------------------------
bool Link::IsAttacking()
{
    return (m_timeOfLastAttack > (GetCurrentTimeSeconds() - SWORD_STUN_DURATION_SECONDS));
}

