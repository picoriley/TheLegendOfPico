#include "Game/ClientSimulation.hpp"
#include "Game/Entities/Link.hpp"
#include "Engine/Renderer/2D/SpriteGameRenderer.hpp"
#include "Engine/Net/UDPIP/NetMessage.hpp"
#include "Engine/Input/InputMap.hpp"
#include "Engine/Net/UDPIP/NetConnection.hpp"
#include "TheGame.hpp"
#include "Engine/Net/UDPIP/NetSession.hpp"
#include "Engine/Input/InputValues.hpp"
#include "Engine/Math/Vector2.hpp"
#include "Engine/Renderer/2D/ResourceDatabase.hpp"
#include "Engine/Renderer/2D/ParticleSystemDefinition.hpp"
#include "Engine/Audio/Audio.hpp"
#include "Engine/Time/Time.hpp"
#include "Engine/Math/MathUtilities.hpp"

//-----------------------------------------------------------------------------------
ClientSimulation::ClientSimulation()
    : m_localPlayer(nullptr)
    , m_isTwahMode(false)
{
    m_players.reserve(8);
    for (unsigned int i = 0; i < 8; ++i)
    {
        m_players.push_back(nullptr);
    }
    for (int i = 0; i < 5; ++i)
    {
        m_hearts[i] = new Sprite("fullHeart", TheGame::FOREGROUND_LAYER, true);
    }

    TheGame::instance->m_gameplayMapping.FindInputValue("Attack")->m_OnPress.RegisterMethod(this, &ClientSimulation::OnLocalPlayerAttackInput);
    TheGame::instance->m_gameplayMapping.FindInputValue("FireBow")->m_OnPress.RegisterMethod(this, &ClientSimulation::OnLocalPlayerFireBowInput);
    TheGame::instance->m_gameplayMapping.FindInputValue("Respawn")->m_OnPress.RegisterMethod(this, &ClientSimulation::OnLocalPlayerRespawnInput);
    TheGame::instance->m_gameplayMapping.FindInputValue("Twah")->m_OnPress.RegisterMethod(this, &ClientSimulation::ToggleTwah);
}

//-----------------------------------------------------------------------------------
ClientSimulation::~ClientSimulation()
{
    for (Link* link : m_players)
    {
        delete link;
    }
    m_players.clear();

    for (int i = 0; i < 5; ++i)
    {
        delete m_hearts[i];
    }
}

//-----------------------------------------------------------------------------------
void ClientSimulation::Update(float deltaSeconds)
{
    UNUSED(deltaSeconds);
    if (m_localPlayer)
    {
        SpriteGameRenderer::instance->SetCameraPosition(m_localPlayer->m_position);
        UpdateHearts(m_localPlayer->m_hp);

    }
    else
    {
        if (NetSession::instance->GetMyConnectionIndex() != NetSession::INVALID_CONNECTION_INDEX && m_players[NetSession::instance->GetMyConnectionIndex()] != nullptr)
        {
            m_localPlayer = m_players[NetSession::instance->GetMyConnectionIndex()];
            m_localPlayerColor = m_localPlayer->m_color.ToUnsignedInt();
        }
    }
}

//-----------------------------------------------------------------------------------
void ClientSimulation::UpdateHearts(float hp)
{
    //Reminder that hp is a copy, so we use it to count down how much hp we have.
    for (int i = 0; i < 5; ++i)
    {
        m_hearts[i]->m_position = SpriteGameRenderer::instance->GetCameraPositionInWorld() + Vector2(-4.0f, 3.5f) + (Vector2(1.0f, 0.0f) * i);
        m_hearts[i]->m_tintColor = m_localPlayerColor;
        if (hp >= 2.0f)
        {
            m_hearts[i]->m_spriteResource = ResourceDatabase::instance->GetSpriteResource("fullHeart");
            hp -= 2.0f;
        }
        else if (hp >= 1.0f)
        {
            m_hearts[i]->m_spriteResource = ResourceDatabase::instance->GetSpriteResource("halfHeart");
            hp -= 1.0f;
        }
        else
        {
            m_hearts[i]->m_spriteResource = ResourceDatabase::instance->GetSpriteResource("emptyHeart");
        }
    }
}

//-----------------------------------------------------------------------------------
void ClientSimulation::OnUpdateFromHostReceived(const NetSender& from, NetMessage& message)
{
    if (from.connection)
    {
        for (Link* networkedPlayer : m_players)
        {
            if (networkedPlayer)
            {
                message.Read<Vector2>(networkedPlayer->m_position);
                message.Read<Link::Facing>(networkedPlayer->m_facing);
                message.Read<float>(networkedPlayer->m_hp);
                networkedPlayer->ApplyClientUpdate();
            }
        }
    }
//         unsigned int numEntities = 0;
//         message.Read<unsigned int>(numEntities);
//         for (unsigned int i = 0; i < numEntities; ++i)
//         {
//             uint16_t networkId = 0;
//             message.Read<uint16_t>(networkId);
//             auto localEntity = m_entities.find(networkId);
//             if (localEntity != m_entities.end())
//             {
//                 Entity* entity = localEntity->second;
//                 message.Read<Vector2>(entity->m_position);
//                 message.Read<float>(entity->m_rotationDegrees);
//                 entity->ApplyClientUpdate();
//             }
//         }
}

//-----------------------------------------------------------------------------------
void ClientSimulation::SendNetClientUpdate(NetConnection* cp)
{
    NetMessage update(GameNetMessages::CLIENT_TO_HOST_UPDATE);
    InputMap& input = TheGame::instance->m_gameplayMapping;
    InputAxis* rightAxis = input.FindInputAxis("Right");
    InputAxis* upAxis = input.FindInputAxis("Up");
    Vector2 rightAxisValues(rightAxis->m_positiveValue->m_currentValue, rightAxis->m_negativeValue->m_currentValue);
    Vector2 upAxisValues(upAxis->m_positiveValue->m_currentValue, upAxis->m_negativeValue->m_currentValue);
    update.Write<Vector2>(rightAxisValues);
    update.Write<Vector2>(upAxisValues);
    cp->SendMessage(update);
}

//-----------------------------------------------------------------------------------
void ClientSimulation::OnPlayerCreate(const NetSender&, NetMessage message)
{
    static const SoundID spawnSound = AudioSystem::instance->CreateOrGetSound("Data\\SFX\\Oracle_SwordShimmer.wav");
    static const SoundID twahSound = AudioSystem::instance->CreateOrGetSound("Data\\SFX\\mars1e.wav");
    Link* player = new Link();
    unsigned int color = 0;
    bool isRequest = false;

    //Read in link data
    message.Read<bool>(isRequest);
    message.Read<uint8_t>(player->m_netOwnerIndex);
    message.Read<unsigned int>(color);

    if (!isRequest)
    {
        player->SetColor(color);
        m_players[player->m_netOwnerIndex] = player;
        if (player->m_netOwnerIndex == NetSession::instance->GetMyConnectionIndex())
        {
            m_localPlayer = player;
            m_localPlayerColor = color;
            SpriteGameRenderer::instance->RemoveEffectFromLayer(TheGame::instance->m_playerDeathEffect, TheGame::FOREGROUND_LAYER);
        }
        AudioSystem::instance->PlaySound(m_isTwahMode ? twahSound : spawnSound);
    }
    else
    {
        delete player;
    }
}

//-----------------------------------------------------------------------------------
void ClientSimulation::OnPlayerDestroy(const NetSender&, NetMessage message)
{
    static const SoundID deathSound = AudioSystem::instance->CreateOrGetSound("Data\\SFX\\Oracle_Link_Dying.wav");
    static const SoundID twahSound = AudioSystem::instance->CreateOrGetSound("Data\\SFX\\mars16.wav");
    uint8_t index = NetSession::INVALID_CONNECTION_INDEX;
    message.Read<uint8_t>(index);

    //Spawn a deadboy right here.
    if (m_players[index])
    {
        const std::string particleEffect = MathUtils::GetRandomIntFromZeroTo(2) == 0 ? "DeadLink1" : "DeadLink2";
        ResourceDatabase::instance->GetParticleSystemResource(particleEffect)->m_emitterDefinitions[0]->m_initialTintPerParticle = m_players[index]->m_color;
        ParticleSystem::PlayOneShotParticleEffect(particleEffect, TheGame::BODY_LAYER, m_players[index]->m_position, 0.0f);
        ParticleSystem::PlayOneShotParticleEffect("BloodPool", TheGame::BLOOD_LAYER, m_players[index]->m_position, GetRandomFloatInRange(0.0f, 360.0f));
    }

    if (m_players[index] == m_localPlayer)
    {
        m_localPlayer = nullptr;
        UpdateHearts(0.0f);
        SpriteGameRenderer::instance->AddEffectToLayer(TheGame::instance->m_playerDeathEffect, TheGame::FOREGROUND_LAYER);
    }
    delete m_players[index];
    m_players[index] = nullptr;

    AudioSystem::instance->PlaySound(m_isTwahMode ? twahSound : deathSound);
}

//-----------------------------------------------------------------------------------
void ClientSimulation::OnLocalPlayerAttackInput(const InputValue*)
{
    if (m_localPlayer && !m_localPlayer->IsAttacking())
    {
        bool isRequest = true;
        NetMessage attackMessage(GameNetMessages::PLAYER_ATTACK);
        attackMessage.Write<bool>(isRequest);
        NetSession::instance->m_hostConnection->SendMessage(attackMessage);
    }
}

//-----------------------------------------------------------------------------------
void ClientSimulation::OnLocalPlayerFireBowInput(const InputValue*)
{
    if (m_localPlayer)
    {
        bool isRequest = true;
        NetMessage attackMessage(GameNetMessages::PLAYER_FIRE_BOW);
        attackMessage.Write<bool>(isRequest);
        NetSession::instance->m_hostConnection->SendMessage(attackMessage);
    }
}

//-----------------------------------------------------------------------------------
void ClientSimulation::OnLocalPlayerRespawnInput(const InputValue* respawnInput)
{
    if (m_localPlayer == nullptr)
    {
        bool isRequest = true;
        NetMessage requestPlayerCreate(GameNetMessages::PLAYER_CREATE);
        requestPlayerCreate.Write<bool>(isRequest);
        requestPlayerCreate.Write<uint8_t>(NetSession::instance->GetMyConnectionIndex());
        requestPlayerCreate.Write<unsigned int>(m_localPlayerColor);
        NetSession::instance->m_hostConnection->SendMessage(requestPlayerCreate);
    }
}

//-----------------------------------------------------------------------------------
void ClientSimulation::OnPlayerAttack(const NetSender&, NetMessage message)
{
    static const SoundID swordSound1 = AudioSystem::instance->CreateOrGetSound("Data\\SFX\\Oracle_Sword_Slash1.wav");
    static const SoundID swordSound2 = AudioSystem::instance->CreateOrGetSound("Data\\SFX\\Oracle_Sword_Slash2.wav");
    static const SoundID swordSound3 = AudioSystem::instance->CreateOrGetSound("Data\\SFX\\Oracle_Sword_Slash3.wav");
    static const SoundID twahSound = AudioSystem::instance->CreateOrGetSound("Data\\SFX\\mars14.wav");

    bool isRequest = true;
    uint8_t index = NetSession::INVALID_CONNECTION_INDEX;
    Vector2 swordPosition(0.0f);
    float swordRotation(0.0f);
    Link* attackingPlayer = nullptr;
    message.Read<bool>(isRequest);
    message.Read<uint8_t>(index);
    message.Read<Vector2>(swordPosition);
    message.Read<float>(swordRotation);

    if (!isRequest)
    {
        ASSERT_OR_DIE(index < TheGame::MAX_PLAYERS, "Invalid index attached to attack message");
        attackingPlayer = m_players[index];

        if (attackingPlayer)
        {
            ResourceDatabase::instance->GetParticleSystemResource("SwordAttack")->m_emitterDefinitions[0]->m_initialTintPerParticle = attackingPlayer->m_color;
            ParticleSystem::PlayOneShotParticleEffect("SwordAttack", TheGame::WEAPON_LAYER, swordPosition, swordRotation);

            if (m_isTwahMode)
            {
                AudioSystem::instance->PlaySound(twahSound);
            }
            else
            {
                switch (MathUtils::GetRandomIntFromZeroTo(3))
                {
                case 0:
                    AudioSystem::instance->PlaySound(swordSound1);
                case 1:
                    AudioSystem::instance->PlaySound(swordSound2);
                case 2:
                    AudioSystem::instance->PlaySound(swordSound3);
                default:
                    break;
                }
            }
        }
    }
}

//-----------------------------------------------------------------------------------
void ClientSimulation::OnPlayerDamaged(const NetSender&, NetMessage message)
{
    static const SoundID hurtSound = AudioSystem::instance->CreateOrGetSound("Data\\SFX\\Oracle_Link_Hurt.wav");
    static const SoundID twahSound = AudioSystem::instance->CreateOrGetSound("Data\\SFX\\mars24.wav");
    Link* hurtPlayer = nullptr;
    uint8_t index = NetSession::INVALID_CONNECTION_INDEX;

    message.Read<uint8_t>(index);

    ASSERT_OR_DIE(index < TheGame::MAX_PLAYERS, "Invalid index attached to attack message");
    hurtPlayer = m_players[index];
    if (hurtPlayer)
    {
        hurtPlayer->m_timeOfLastHurt = GetCurrentTimeSeconds();
    }

    AudioSystem::instance->PlaySound(m_isTwahMode ? twahSound : hurtSound);
}

//-----------------------------------------------------------------------------------
void ClientSimulation::OnPlayerFireBow(const NetSender& from, NetMessage& message)
{
    static const SoundID shootSound = AudioSystem::instance->CreateOrGetSound("Data\\SFX\\Oracle_Enemy_Spit.wav");
    static const SoundID twahSound = AudioSystem::instance->CreateOrGetSound("Data\\SFX\\mars1d.wav");
    AudioSystem::instance->PlaySound(m_isTwahMode ? twahSound : shootSound);
}
