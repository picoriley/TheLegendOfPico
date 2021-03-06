#include "Game/HostSimulation.hpp"
#include "Game/Entities/Entity.hpp"
#include "Game/Entities/Link.hpp"
#include "Game/Entities/Pickup.hpp"
#include "Engine/Net/UDPIP/NetConnection.hpp"
#include "Engine/Net/UDPIP/NetMessage.hpp"
#include "Engine/Input/InputValues.hpp"
#include "Engine/Math/Vector2.hpp"
#include "Engine/Renderer/2D/Sprite.hpp"
#include "TheGame.hpp"
#include "Engine/Net/UDPIP/NetSession.hpp"
#include "Engine/Renderer/2D/ResourceDatabase.hpp"
#include "Engine/Input/InputOutputUtils.hpp"
#include "Engine/Time/Time.hpp"

//-----------------------------------------------------------------------------------
HostSimulation::HostSimulation()
{
    InitializeKeyMappings();
    m_players.reserve(8);
    for (unsigned int i = 0; i < 8; ++i)
    {
        m_players.push_back(nullptr);
    }
    InitializeLevelGeometry();
}

//-----------------------------------------------------------------------------------
HostSimulation::~HostSimulation()
{
    UninitializeKeyMappings();
    for (Entity* ent : m_entities)
    {
        delete ent;
    }
    m_entities.clear();
    m_players.clear();
}

//-----------------------------------------------------------------------------------
void HostSimulation::OnUpdateFromClientReceived(const NetSender& from, NetMessage& message)
{
    Vector2 rightValues;
    Vector2 upValues;
    message.Read<Vector2>(rightValues);
    message.Read<Vector2>(upValues);
    m_networkMappings[from.connection->m_index].FindInputAxis("Right")->SetValue(rightValues.x, rightValues.y);
    m_networkMappings[from.connection->m_index].FindInputAxis("Up")->SetValue(upValues.x, upValues.y);
}

//-----------------------------------------------------------------------------------
void HostSimulation::OnConnectionJoined(NetConnection* cp)
{
    uint8_t index = cp->m_index;
    bool isRequest = false;
    m_playerColors[index] = RGBA::GetRandom().ToUnsignedInt();
    BroadcastLinkCreation(index, m_playerColors[index]);

    //Bring the client up to speed.
    for (Link* link : m_players)
    {
        if (link)
        {
            NetMessage message(GameNetMessages::PLAYER_CREATE);
            message.Write<bool>(isRequest);
            message.Write<uint8_t>(link->m_netOwnerIndex);
            message.Write<unsigned int>(link->m_color.ToUnsignedInt());
            cp->SendMessage(message);
        }
    }
}

//-----------------------------------------------------------------------------------
void HostSimulation::BroadcastLinkCreation(uint8_t index, unsigned int playerColor)
{
    bool isRequest = false;

    //Let everyone know about the guy we just created (Including ourselves!).
    for (NetConnection* conn : NetSession::instance->m_allConnections)
    {
        if (conn)
        {
            NetMessage message(GameNetMessages::PLAYER_CREATE);
            message.Write<bool>(isRequest);
            message.Write<uint8_t>(index);
            message.Write<unsigned int>(playerColor);
            conn->SendMessage(message);
        }
    }
}

//-----------------------------------------------------------------------------------
void HostSimulation::OnConnectionLeave(NetConnection* cp)
{
    //Let everyone know about the guy who just disconnected (Including ourselves!).
    for (NetConnection* conn : NetSession::instance->m_allConnections)
    {
        if (conn)
        {
            NetMessage message(GameNetMessages::PLAYER_DESTROY);
            message.Write<uint8_t>(cp->m_index);
            conn->SendMessage(message);
        }
    }
}

//-----------------------------------------------------------------------------------
void HostSimulation::OnPlayerDestroy(const NetSender&, NetMessage message)
{
    uint8_t index = NetSession::INVALID_CONNECTION_INDEX;
    message.Read<uint8_t>(index);

    //Entity cleanup will delete the player within the next frame.
    if (m_players[index])
    {
        m_players[index]->m_isDead = true;
        m_players[index] = nullptr;
    }
}

//-----------------------------------------------------------------------------------
void HostSimulation::OnPlayerCreate(const NetSender&, NetMessage message)
{
    Link* player = new Link();
    unsigned int color = 0;
    bool isRequest = false;

    //Read the link data
    message.Read<bool>(isRequest);
    message.Read<uint8_t>(player->m_netOwnerIndex);
    message.Read<unsigned int>(color);

    if (isRequest)
    {
        BroadcastLinkCreation(player->m_netOwnerIndex, color);
        delete player;
    }
    else
    {
        player->SetColor(color);
        player->m_sprite->Disable();
        m_players[player->m_netOwnerIndex] = player;
        m_entities.push_back(player);
    }
}

//-----------------------------------------------------------------------------------
void HostSimulation::OnPlayerAttack(const NetSender& from, NetMessage message)
{
    bool wasSentARequest = false;
    uint8_t index = from.connection->m_index;
    message.Read<bool>(wasSentARequest);

    if (wasSentARequest)
    {
        Link* attackingPlayer = m_players[index];
        if (!attackingPlayer)
        {
            return;
        }
        Vector2 swordPosition = attackingPlayer->CalculateSwordPosition();
        float swordRotation = attackingPlayer->CalculateSwordRotationDegrees();
        attackingPlayer->m_timeOfLastAttack = GetCurrentTimeSeconds();

        for (NetConnection* conn : NetSession::instance->m_allConnections)
        {
            if (conn)
            {
                bool isRequest = false;
                NetMessage attackMessage(GameNetMessages::PLAYER_ATTACK);
                attackMessage.Write<bool>(isRequest);
                attackMessage.Write<uint8_t>(index);
                attackMessage.Write<Vector2>(swordPosition);
                attackMessage.Write<float>(swordRotation);
                conn->SendMessage(attackMessage);
            }
        }

        CheckForAndBroadcastDamage(attackingPlayer, swordPosition);
    }
}

//-----------------------------------------------------------------------------------
void HostSimulation::CheckForAndBroadcastDamage(Link* attackingPlayer, const Vector2& swordPosition)
{
    AABB2 swordBoundingBox = ResourceDatabase::instance->GetSpriteResource("swordSwing")->GetDefaultBounds();
    swordBoundingBox += swordPosition;
    for (Link* player : m_players)
    {
        if (player && player != attackingPlayer && swordBoundingBox.IsIntersecting(player->m_sprite->GetBounds()))
        {
            //Update the damaged player
            Vector2 fromAttackerToDefender = player->m_position - attackingPlayer->m_position;
            fromAttackerToDefender.Normalize();
            float distFromAttackerToDefender = MathUtils::CalcDistanceBetweenPoints(player->m_position, attackingPlayer->m_position);
            float distFromSwordToDefender = MathUtils::CalcDistanceBetweenPoints(swordPosition, attackingPlayer->m_position);
            player->m_position += fromAttackerToDefender * (distFromSwordToDefender / distFromAttackerToDefender);
            player->m_hp -= 1.0f;

            //Create messages
            NetMessage attack(GameNetMessages::PLAYER_DAMAGED);
            attack.Write<uint8_t>(player->m_netOwnerIndex);
            NetMessage died(GameNetMessages::PLAYER_DESTROY);
            died.Write<uint8_t>(player->m_netOwnerIndex);
            NetMessage update(GameNetMessages::HOST_TO_CLIENT_UPDATE);
            update.Write<Vector2>(player->m_position);
            update.Write<Link::Facing>(player->m_facing);


            for (NetConnection* conn : NetSession::instance->m_allConnections)
            {
                if (conn)
                {
                    conn->SendMessage(attack);
                    conn->SendMessage(update);
                    if (player->m_hp <= 0.0f)
                    {
                        conn->SendMessage(died);
                    }
                }
            }

        }
    }
}

//-----------------------------------------------------------------------------------
void HostSimulation::OnPlayerFireBow(const NetSender& from, NetMessage& message)
{

}

//-----------------------------------------------------------------------------------
void HostSimulation::SendNetHostUpdate(NetConnection* cp)
{
    NetMessage update(GameNetMessages::HOST_TO_CLIENT_UPDATE);
    for (Link* link : m_players)
    {
        if (link)
        {
            update.Write<Vector2>(link->m_position);
            update.Write<Link::Facing>(link->m_facing);
            update.Write<float>(link->m_hp);
        }
    }
    cp->SendMessage(update);

//         unsigned int* numEntities = update.Reserve<unsigned int>(m_entities.size());
//         for (Entity* entity : m_entities)
//         {
//             if (entity->IsPlayer())
//             {
//                 ByteSwap<unsigned int>(numEntities, sizeof(unsigned int));
//                 *numEntities -= 1;
//                 ByteSwap<unsigned int>(numEntities, sizeof(unsigned int));
//                 continue;
//             }
//             update.Write<uint16_t>(entity->m_networkId);
//             update.Write<Vector2>(entity->m_position);
//             update.Write<float>(entity->m_rotationDegrees);
//         }
}

//-----------------------------------------------------------------------------------
void HostSimulation::Update(float deltaSeconds)
{
    UpdateEntities(deltaSeconds);
    AddNewEntities();
    CleanUpDeadEntities();
}

//-----------------------------------------------------------------------------------
void HostSimulation::UpdateEntities(float deltaSeconds)
{
    for (Entity* ent : m_entities)
    {
        ent->Update(deltaSeconds);
        for (Entity* other : m_entities)
        {
            if ((ent != other) && (ent->IsCollidingWith(other)))
            {
                ent->ResolveCollision(other);
            }
        }
    }
}

//-----------------------------------------------------------------------------------
void HostSimulation::AddNewEntities()
{
    for (Entity* ent : m_newEntities)
    {
        m_entities.push_back(ent);
    }
    m_newEntities.clear();
}

//-----------------------------------------------------------------------------------
void HostSimulation::CleanUpDeadEntities()
{
    for (auto iter = m_entities.begin(); iter != m_entities.end(); ++iter)
    {
        Entity* gameObject = *iter;
        if (gameObject->m_isDead)
        {
            delete gameObject;
            iter = m_entities.erase(iter);
        }
        if (iter == m_entities.end())
        {
            break;
        }
    }
}

//-----------------------------------------------------------------------------------
void HostSimulation::InitializeKeyMappings()
{
    m_networkMappings.resize(8);
    for (unsigned int i = 0; i < 8; ++i)
    {
        m_networkMappings[i].AddInputAxis("Up", new InputValue(&m_networkMappings[i]), new InputValue(&m_networkMappings[i]));
        m_networkMappings[i].AddInputAxis("Right", new InputValue(&m_networkMappings[i]), new InputValue(&m_networkMappings[i]));
    }
}

//-----------------------------------------------------------------------------------
void HostSimulation::UninitializeKeyMappings()
{
    for (unsigned int i = 0; i < 8; ++i)
    {
        InputAxis* tempAxis = m_networkMappings[i].FindInputAxis("Up");
        delete tempAxis->m_positiveValue;
        delete tempAxis->m_negativeValue;

        tempAxis = m_networkMappings[i].FindInputAxis("Right");
        delete tempAxis->m_positiveValue;
        delete tempAxis->m_negativeValue;
    }
}

//-----------------------------------------------------------------------------------
void HostSimulation::InitializeLevelGeometry()
{
    const float UNITS_PER_PIXEL = 1.0f / 16.0f;
    const Vector2 UNITS_OFFSET = Vector2(15.0f, 8.0f);

    //OuterBox
    m_levelGeometry.emplace_back(Vector2(0.0f, -500.0f) - UNITS_OFFSET, Vector2(480.0f, 16.0f) * UNITS_PER_PIXEL - UNITS_OFFSET);
    m_levelGeometry.emplace_back(Vector2(-500.0f, 0.0f) - UNITS_OFFSET, Vector2(16.0f, 256.0f) * UNITS_PER_PIXEL - UNITS_OFFSET);
    m_levelGeometry.emplace_back(Vector2(480.0f - 16.0f, 0.0f) * UNITS_PER_PIXEL - UNITS_OFFSET, Vector2(480.0f + 500.0f, 256.0f) * UNITS_PER_PIXEL - UNITS_OFFSET);
    m_levelGeometry.emplace_back(Vector2(0.0f, 256.0f - 16.0f) * UNITS_PER_PIXEL - UNITS_OFFSET, Vector2(480.0f, 256.0f + 500.0f) * UNITS_PER_PIXEL - UNITS_OFFSET);

    //Upper Houses
    m_levelGeometry.emplace_back(Vector2(0.0f, 209.0f) * UNITS_PER_PIXEL - UNITS_OFFSET, Vector2(63.0f, 256.0f) * UNITS_PER_PIXEL - UNITS_OFFSET);
    m_levelGeometry.emplace_back(Vector2(416.0f, 209.0f) * UNITS_PER_PIXEL - UNITS_OFFSET, Vector2(463.0f, 256.0f) * UNITS_PER_PIXEL - UNITS_OFFSET);

    //Lower Houses
    m_levelGeometry.emplace_back(Vector2(368.0f, 64.f) * UNITS_PER_PIXEL - UNITS_OFFSET, Vector2(415.f, 95.f) * UNITS_PER_PIXEL - UNITS_OFFSET);
    m_levelGeometry.emplace_back(Vector2(64.0f, 64.0f) * UNITS_PER_PIXEL - UNITS_OFFSET, Vector2(111.0f, 95.0f) * UNITS_PER_PIXEL - UNITS_OFFSET);

    //Lower Fountains
    m_levelGeometry.emplace_back(Vector2(16.0f, 16.0f) * UNITS_PER_PIXEL - UNITS_OFFSET, Vector2(47.0f, 47.0f) * UNITS_PER_PIXEL - UNITS_OFFSET);
    m_levelGeometry.emplace_back(Vector2(432.0f, 16.0f) * UNITS_PER_PIXEL - UNITS_OFFSET, Vector2(463.0f, 47.0f) * UNITS_PER_PIXEL - UNITS_OFFSET);

    //Center Area
    m_levelGeometry.emplace_back(Vector2(224.f, 48.0f) * UNITS_PER_PIXEL - UNITS_OFFSET, Vector2(255.f, 144.0f) * UNITS_PER_PIXEL - UNITS_OFFSET);
    m_levelGeometry.emplace_back(Vector2(191.0f, 65.0f) * UNITS_PER_PIXEL - UNITS_OFFSET, Vector2(287.0f, 95.0f) * UNITS_PER_PIXEL - UNITS_OFFSET);
    
    //Trees
    m_levelGeometry.emplace_back(Vector2(144.0f, 16.0f) * UNITS_PER_PIXEL - UNITS_OFFSET, Vector2(175.0f, 47.0f) * UNITS_PER_PIXEL - UNITS_OFFSET);
    m_levelGeometry.emplace_back(Vector2(304.0f, 112.0f) * UNITS_PER_PIXEL - UNITS_OFFSET, Vector2(335.0f, 144.0f) * UNITS_PER_PIXEL - UNITS_OFFSET);
    m_levelGeometry.emplace_back(Vector2(144.0f, 112.0f) * UNITS_PER_PIXEL - UNITS_OFFSET, Vector2(175.0f, 144.0f) * UNITS_PER_PIXEL - UNITS_OFFSET);
    m_levelGeometry.emplace_back(Vector2(304.0f, 16.0f) * UNITS_PER_PIXEL - UNITS_OFFSET, Vector2(335.0f, 47.0f) * UNITS_PER_PIXEL - UNITS_OFFSET);

    //Volcano
    m_levelGeometry.emplace_back(Vector2(192.0f, 193.0f) * UNITS_PER_PIXEL - UNITS_OFFSET, Vector2(287.0f, 240.0f) * UNITS_PER_PIXEL - UNITS_OFFSET);
}
