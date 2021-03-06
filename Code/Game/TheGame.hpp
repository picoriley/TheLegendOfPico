#pragma once
#include <vector>
#include "Engine/Math/Vector2.hpp"
#include "Engine/Net/UDPIP/NetMessage.hpp"
#include "Engine/Input/InputMap.hpp"
#include "Engine/Renderer/Material.hpp"

class Entity;
class Link;
class Ship;
class NetConnection;
struct NetSender;
class HostSimulation;
class ClientSimulation;

//-----------------------------------------------------------------------------------
enum GameNetMessages
{
    CLIENT_TO_HOST_UPDATE = NetMessage::CoreMessageTypes::NUM_MESSAGES,
    HOST_TO_CLIENT_UPDATE,
    PLAYER_CREATE,
    PLAYER_DESTROY,
    PLAYER_ATTACK,
    PLAYER_FIRE_BOW,
    PLAYER_DAMAGED,
};

//-----------------------------------------------------------------------------------
class TheGame
{
public:
    TheGame();
    ~TheGame();
    void OnConnectionJoined(NetConnection* cp);
    void OnConnectionLeave(NetConnection* cp);
    void OnNetTick(NetConnection* cp);
    void Update(float deltaTime);
    void Render() const;
    void InitializePlayingState();

    static TheGame* instance;

    //CONSTANTS/////////////////////////////////////////////////////////////////////
    static const uint8_t MAX_PLAYERS = 8;

    static unsigned int const BACKGROUND_LAYER = 0;
    static unsigned int const BLOOD_LAYER = 8;
    static unsigned int const BODY_LAYER = 9;
    static unsigned int const PLAYER_LAYER = 10;
    static unsigned int const WEAPON_LAYER = 11;
    static unsigned int const ENEMY_BULLET_LAYER = 12;
    static unsigned int const FOREGROUND_LAYER = 20;
    static unsigned int const UI_LAYER = 30;

    //MEMBER VARIABLES/////////////////////////////////////////////////////////////////////
    int m_debuggingControllerIndex;
    InputMap m_gameplayMapping;
    HostSimulation* m_host;
    ClientSimulation* m_client;
    Material* m_playerDeathEffect;

private:
    TheGame& operator= (const TheGame& other) = delete;
    void CleanupGameOverState(unsigned int);
    void UpdateGameOver(float deltaSeconds);
    void RenderGameOver() const;
    void RegisterSprites();
    void RegisterParticleSystems();
    void UpdatePlaying(float deltaSeconds);
    void RenderPlaying() const;
    void InitializeGameOverState();
    void CleanupPlayingState(unsigned int);

    void InitializeMainMenuState();
    void InitializeKeyMappings();
    void CleanupMainMenuState(unsigned int);
    void UpdateMainMenu(float deltaSeconds);
    void RenderMainMenu() const;
};
