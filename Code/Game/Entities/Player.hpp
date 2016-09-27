#pragma once
#include "Game/Entities/Ship.hpp"
#include <stdint.h>

class Player : public Ship
{
public:
    enum Facing
    {
        WEST,
        NORTH,
        EAST,
        SOUTH,
        NUM_DIRECTIONS
    };

    Player();
    ~Player();
    virtual void Update(float deltaSeconds);
    virtual void Render() const;
    virtual void ResolveCollision(Entity* otherEntity);
    Facing GetFacingFromInput(const Vector2& inputDirection);

    //MEMBER VARIABLES/////////////////////////////////////////////////////////////////////
    uint8_t m_netOwnerIndex;
    Facing m_facing;
};