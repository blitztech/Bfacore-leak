/*
  * Copyright (C) 2021 BfaCore Reforged
  *
  * This program is free software; you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation; either version 2 of the License, or
  * (at your option) any later version.
  *
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  * GNU General Public License for more details.
  *
  * You should have received a copy of the GNU General Public License
  * along with this program; if not, write to the Free Software
  * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
  */

#include "BattlegroundBFG.h"
#include "BattlegroundMgr.h"
#include "DB2Stores.h"
#include "GameObject.h"
#include "Log.h"
#include "Map.h"
#include "Object.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "SpellInfo.h"
#include "WorldStatePackets.h"
#include "ObjectMgr.h"
#include "Language.h"
#include "Battlefield.h"

// these variables aren't used outside of this file, so declare them only here
uint32 BG_BG_HonorScoreTicks[BG_HONOR_MODE_NUM] = {
    330, // normal honor
    200  // holiday
};

BattlegroundBFG::BattlegroundBFG()
{
    m_BuffChange = true;
    BgObjects.resize(BG_BG_OBJECT_MAX);
    BgCreatures.resize(BG_BFG_ALL_NODES_COUNT + 5); // +5 for aura triggers

    StartMessageIds[BG_STARTING_EVENT_FIRST] = LANG_BG_BG_START_TWO_MINUTES;
    StartMessageIds[BG_STARTING_EVENT_SECOND] = LANG_BG_BG_START_ONE_MINUTE;
    StartMessageIds[BG_STARTING_EVENT_THIRD] = LANG_BG_BG_START_HALF_MINUTE;
    StartMessageIds[BG_STARTING_EVENT_FOURTH] = LANG_BG_BG_HAS_BEGUN;
}

BattlegroundBFG::~BattlegroundBFG()
{
}

void BattlegroundBFG::PostUpdateImpl(uint32 diff)
{
    if (GetStatus() == STATUS_IN_PROGRESS)
    {
        int team_points[BG_TEAMS_COUNT] = { 0, 0 };

        for (int node = 0; node < BG_BFG_DYNAMIC_NODES_COUNT; ++node)
        {
            // 3 sec delay to spawn new banner instead previous despawned one
            if (m_BannerTimers[node].timer)
            {
                if (m_BannerTimers[node].timer > diff)
                    m_BannerTimers[node].timer -= diff;
                else
                {
                    m_BannerTimers[node].timer = 0;
                    _CreateBanner(node, m_BannerTimers[node].type, m_BannerTimers[node].teamIndex, false);
                }
            }

            // 1-minute to occupy a node from contested state
            if (m_NodeTimers[node])
            {
                if (m_NodeTimers[node] > diff)
                    m_NodeTimers[node] -= diff;
                else
                {
                    m_NodeTimers[node] = 0;
                    // Change from contested to occupied !
                    uint8 teamIndex = m_Nodes[node] - 1;
                    m_prevNodes[node] = m_Nodes[node];
                    m_Nodes[node] += 2;
                    // burn current contested banner
                    _DelBanner(node, BG_BFG_NODE_TYPE_CONTESTED, teamIndex);
                    // create new occupied banner
                    _CreateBanner(node, BG_BFG_NODE_TYPE_OCCUPIED, teamIndex, true);
                    _SendNodeUpdate(node);
                    _NodeOccupied(node, (teamIndex == 0) ? ALLIANCE : HORDE);
                    // Message to chatlog

                    if (teamIndex == 0)
                    {
                        // FIXME: team and node names not localized
                        //PSendMessageToAll(LANG_BG_BG_NODE_TAKEN, CHAT_MSG_BG_SYSTEM_ALLIANCE, NULL, LANG_BG_BG_ALLY, _GetNodeNameId(node));
                        PlaySoundToAll(BG_BFG_SOUND_NODE_CAPTURED_ALLIANCE);
                    }
                    else
                    {
                        // FIXME: team and node names not localized
                       // PSendMessageToAll(LANG_BG_BG_NODE_TAKEN, CHAT_MSG_BG_SYSTEM_HORDE, NULL, LANG_BG_BG_HORDE, _GetNodeNameId(node));
                        PlaySoundToAll(BG_BFG_SOUND_NODE_CAPTURED_HORDE);
                    }
                }
            }

            for (int team = 0; team < BG_TEAMS_COUNT; ++team)
                if (m_Nodes[node] == team + BG_BFG_NODE_TYPE_OCCUPIED)
                    ++team_points[team];
        }

        // Accumulate points
        for (int team = 0; team < BG_TEAMS_COUNT; ++team)
        {
            int points = team_points[team];
            if (!points)
                continue;
            m_lastTick[team] += diff;
            if (m_lastTick[team] > BG_BFG_TickIntervals[points])
            {
                m_lastTick[team] -= BG_BFG_TickIntervals[points];
                m_TeamScores[team] += BG_BFG_TickPoints[points];
                m_HonorScoreTics[team] += BG_BFG_TickPoints[points];
                if (!m_IsInformedNearVictory && m_TeamScores[team] > BG_BFG_WARNING_NEAR_VICTORY_SCORE)
                {
                    if (team == TEAM_ALLIANCE)
                    SendMessageToAll(LANG_BG_AB_A_NEAR_VICTORY, CHAT_MSG_BG_SYSTEM_NEUTRAL);
                    else
                    SendMessageToAll(LANG_BG_AB_H_NEAR_VICTORY, CHAT_MSG_BG_SYSTEM_NEUTRAL);
                    PlaySoundToAll(BG_BFG_SOUND_NEAR_VICTORY);
                    m_IsInformedNearVictory = true;
                }

                if (m_TeamScores[team] > BG_BFG_MAX_TEAM_SCORE)
                    m_TeamScores[team] = BG_BFG_MAX_TEAM_SCORE;
                if (team == TEAM_ALLIANCE)
                    UpdateWorldState(BG_BFG_OP_RESOURCES_ALLY, m_TeamScores[team]);
                if (team == TEAM_HORDE)
                    UpdateWorldState(BG_BFG_OP_RESOURCES_HORDE, m_TeamScores[team]);
                // update achievement flags
                // we increased m_TeamScores[team] so we just need to check if it is 500 more than other teams resources
                uint8 otherTeam = (team + 1) % BG_TEAMS_COUNT;
                if (m_TeamScores[team] > m_TeamScores[otherTeam] + 500)
                    m_TeamScores500Disadvantage[otherTeam] = true;
            }
        }

        // Test win condition
        if (m_TeamScores[TEAM_ALLIANCE] >= BG_BFG_MAX_TEAM_SCORE)
            EndBattleground(ALLIANCE);
        if (m_TeamScores[TEAM_HORDE] >= BG_BFG_MAX_TEAM_SCORE)
            EndBattleground(HORDE);
    }
}

void BattlegroundBFG::StartingEventCloseDoors()
{
    // despawn banners, auras and buffs
    for (int obj = BG_BG_OBJECT_BANNER_NEUTRAL; obj < BG_BFG_DYNAMIC_NODES_COUNT * 8; ++obj)
        SpawnBGObject(obj, RESPAWN_ONE_DAY);
    for (int i = 0; i < BG_BFG_DYNAMIC_NODES_COUNT * 3; ++i)
        SpawnBGObject(BG_BG_OBJECT_SPEEDBUFF_LIGHTHOUSE + i, RESPAWN_ONE_DAY);

    // Starting doors
    DoorClose(BG_BG_OBJECT_GATE_A);
    DoorClose(BG_BG_OBJECT_GATE_H);
    SpawnBGObject(BG_BG_OBJECT_GATE_A, RESPAWN_IMMEDIATELY);
    SpawnBGObject(BG_BG_OBJECT_GATE_H, RESPAWN_IMMEDIATELY);

    // Starting base spirit guides
    _NodeOccupied(BG_BFG_SPIRIT_ALIANCE, ALLIANCE);
    _NodeOccupied(BG_BFG_SPIRIT_HORDE, HORDE);
}

void BattlegroundBFG::StartingEventOpenDoors()
{
    // spawn neutral banners
    for (int banner = BG_BG_OBJECT_BANNER_NEUTRAL, i = 0; i < 3; banner += 8, ++i)
        SpawnBGObject(banner, RESPAWN_IMMEDIATELY);
    for (int i = 0; i < BG_BFG_DYNAMIC_NODES_COUNT; ++i)
    {
        //randomly select buff to spawn
        uint8 buff = urand(0, 2);
        SpawnBGObject(BG_BG_OBJECT_SPEEDBUFF_LIGHTHOUSE + buff + i * 3, RESPAWN_IMMEDIATELY);
    }
    DoorOpen(BG_BG_OBJECT_GATE_A);
    DoorOpen(BG_BG_OBJECT_GATE_H);
}

void BattlegroundBFG::AddPlayer(Player* player)
{
    Battleground::AddPlayer(player);
    //create score and add it to map, default values are set in the constructor
    BattlegroundBFGScore* sc = new BattlegroundBFGScore(player->GetGUID(), player->GetBGTeam());

    PlayerScores[player->GetGUID()] = sc;
}

void BattlegroundBFG::RemovePlayer(Player* /*player*/, ObjectGuid /*guid*/, uint32 /*team*/)
{
}

void BattlegroundBFG::HandleAreaTrigger(Player* player, uint32 trigger, bool entered)
{
    if (GetStatus() != STATUS_IN_PROGRESS)
        return;
    //Area triggers are not handled yet!
    return;

    switch (trigger)
    {
    case 3866:                                          // Lighthouse
    case 3869:                                          // Watterwork
    case 3867:                                          // Mine
    case 4020:                                          // Unk1
    case 4021:                                          // Unk2
                                                        //break;
    default:
        Battleground::HandleAreaTrigger(player, trigger, entered);
        break;
    }
}

/*  type: 0-neutral, 1-contested, 3-occupied
teamIndex: 0-ally, 1-horde                        */
void BattlegroundBFG::_CreateBanner(uint8 node, uint8 type, uint8 teamIndex, bool delay)
{
    // Just put it into the queue
    if (delay)
    {
        m_BannerTimers[node].timer = 2000;
        m_BannerTimers[node].type = type;
        m_BannerTimers[node].teamIndex = teamIndex;
        return;
    }

    uint8 obj = node * 8 + type + teamIndex;

    SpawnBGObject(obj, RESPAWN_IMMEDIATELY);

    // handle aura with banner
    if (!type)
        return;
    obj = node * 8 + ((type == BG_BFG_NODE_TYPE_OCCUPIED) ? (3 + teamIndex) : 5);
    SpawnBGObject(obj, RESPAWN_IMMEDIATELY);
}

void BattlegroundBFG::_DelBanner(uint8 node, uint8 type, uint8 teamIndex)
{
    uint8 obj = node * 8 + type + teamIndex;
    SpawnBGObject(obj, RESPAWN_ONE_DAY);

    // handle aura with banner
    if (!type)
        return;
    obj = node * 8 + ((type == BG_BFG_NODE_TYPE_OCCUPIED) ? (3 + teamIndex) : 5);
    SpawnBGObject(obj, RESPAWN_ONE_DAY);
}

//int32 BattlegroundBFG::_GetNodeNameId(uint8 node)
//{
//    switch (node)
//    {
//    case BG_BG_NODE_WATERWORKS: return LANG_BG_BG_NODE_WATERWORKS;
//    case BG_BG_NODE_LIGHTHOUSE:      return LANG_BG_BG_NODE_LIGHTHOUSE;
//    case BG_BG_NODE_MINE:       return LANG_BG_BG_NODE_MINE;
//    default:
//        ASSERT(0);
//    }
//    return 0;
//}

void BattlegroundBFG::FillInitialWorldStates(WorldPackets::WorldState::InitWorldStates& packet)
{
    const uint8 plusArray[] = { 0, 2, 3, 0, 1 };

    // Node icons
    for (uint8 node = 0; node < BG_BFG_DYNAMIC_NODES_COUNT; ++node)
    packet.Worldstates.emplace_back(uint32(BG_BFG_OP_NODEICONS[node]), int32((m_Nodes[node] == 0) ? 1 : 0));

    // Node occupied states
    for (uint8 node = 0; node < BG_BFG_DYNAMIC_NODES_COUNT; ++node)
        for (uint8 i = 1; i < BG_BFG_DYNAMIC_NODES_COUNT; ++i)
            packet.Worldstates.emplace_back(uint32(BG_BFG_OP_NODESTATES[node] + plusArray[i]), int32((m_Nodes[node] == i) ? 1 : 0));

    // How many bases each team owns
    uint8 ally = 0, horde = 0;
    for (uint8 node = 0; node < BG_BFG_DYNAMIC_NODES_COUNT; ++node)
        if (m_Nodes[node] == BG_BFG_NODE_STATUS_ALLY_OCCUPIED)
            ++ally;
        else if (m_Nodes[node] == BG_BFG_NODE_STATUS_HORDE_OCCUPIED)
            ++horde;

    packet.Worldstates.emplace_back(uint32(BG_BFG_OP_OCCUPIED_BASES_ALLY), int32(ally));
    packet.Worldstates.emplace_back(uint32(BG_BFG_OP_OCCUPIED_BASES_HORDE), int32(horde));

    // Team scores
    packet.Worldstates.emplace_back(uint32(BG_BFG_OP_RESOURCES_MAX), int32(BG_BFG_MAX_TEAM_SCORE));
    packet.Worldstates.emplace_back(uint32(BG_BFG_OP_RESOURCES_WARNING), int32(BG_BFG_WARNING_NEAR_VICTORY_SCORE));
    packet.Worldstates.emplace_back(uint32(BG_BFG_OP_RESOURCES_ALLY), int32(m_TeamScores[TEAM_ALLIANCE]));
    packet.Worldstates.emplace_back(uint32(BG_BFG_OP_RESOURCES_HORDE), int32(m_TeamScores[TEAM_HORDE]));

    // other unknown
    packet.Worldstates.emplace_back(uint32(0x745), 0x2);           // 37 1861 unk
}

void BattlegroundBFG::_SendNodeUpdate(uint8 node)
{
    // Send node owner state update to refresh map icons on client
    const uint8 plusArray[] = { 0, 2, 3, 0, 1 };

    if (m_prevNodes[node])
        UpdateWorldState(BG_BFG_OP_NODESTATES[node] + plusArray[m_prevNodes[node]], 0);
    else
        UpdateWorldState(BG_BFG_OP_NODEICONS[node], 0);

    UpdateWorldState(BG_BFG_OP_NODESTATES[node] + plusArray[m_Nodes[node]], 1);

    // How many bases each team owns
    uint8 ally = 0, horde = 0;
    for (uint8 i = 0; i < BG_BFG_DYNAMIC_NODES_COUNT; ++i)
        if (m_Nodes[i] == BG_BFG_NODE_STATUS_ALLY_OCCUPIED)
            ++ally;
        else if (m_Nodes[i] == BG_BFG_NODE_STATUS_HORDE_OCCUPIED)
            ++horde;

    UpdateWorldState(BG_BFG_OP_OCCUPIED_BASES_ALLY, ally);
    UpdateWorldState(BG_BFG_OP_OCCUPIED_BASES_HORDE, horde);
}

void BattlegroundBFG::_NodeOccupied(uint8 node, Team team)
{
    if (!AddSpiritGuide(node, BG_BFG_SpiritGuidePos[node], GetTeamIndexByTeamId(team)))
        TC_LOG_ERROR("bg.battleground", "Failed to spawn spirit guide! point: %u, team: %u,", node, team);

    uint8 capturedNodes = 0;
    for (uint8 i = 0; i < BG_BFG_DYNAMIC_NODES_COUNT; ++i)
    {
        if (m_Nodes[node] == GetTeamIndexByTeamId(team) + BG_BFG_NODE_TYPE_OCCUPIED && !m_NodeTimers[i])
            ++capturedNodes;
    }

    if (node >= BG_BFG_DYNAMIC_NODES_COUNT)//only dynamic nodes, no start points
        return;

    Creature* trigger = GetBGCreature(node + 5);//0-6 spirit guides
    if (!trigger)
        trigger = AddCreature(WORLD_TRIGGER, node + 5, BG_BFG_NodePositions[node][0], BG_BFG_NodePositions[node][1], BG_BFG_NodePositions[node][2], BG_BFG_NodePositions[node][3], GetTeamIndexByTeamId(team));

    //add bonus honor aura trigger creature when node is accupied
    //cast bonus aura (+50% honor in 25yards)
    //aura should only apply to players who have accupied the node, set correct faction for trigger
    if (trigger)
    {
        trigger->SetFaction(team == ALLIANCE ? 84 : 83);
        trigger->CastSpell(trigger, SPELL_HONORABLE_DEFENDER_25Y, false);
    }
}

void BattlegroundBFG::_NodeDeOccupied(uint8 node)
{
    if (node >= BG_BFG_DYNAMIC_NODES_COUNT)
        return;

    //remove bonus honor aura trigger creature when node is lost
    if (node < BG_BFG_DYNAMIC_NODES_COUNT)//only dynamic nodes, no start points
        DelCreature(node + 7);//NULL checks are in DelCreature! 0-6 spirit guides

                              // Those who are waiting to resurrect at this node are taken to the closest own node's graveyard
    std::vector<ObjectGuid> ghost_list = m_ReviveQueue[BgCreatures[node]];
    if (!ghost_list.empty())
    {
        WorldSafeLocsEntry const* ClosestGrave = NULL;
        for (std::vector<ObjectGuid>::const_iterator itr = ghost_list.begin(); itr != ghost_list.end(); ++itr)
        {
            Player* player = ObjectAccessor::FindPlayer(*itr);
            if (!player)
                continue;

            if (!ClosestGrave)                              // cache
                ClosestGrave = GetClosestGraveYard(player);

            if (ClosestGrave)
                player->TeleportTo(GetMapId(), ClosestGrave->Loc.m_positionX, ClosestGrave->Loc.m_positionY, ClosestGrave->Loc.m_positionZ, player->GetOrientation());
        }
    }

    DelCreature(node);

    // buff object isn't despawned
}

/* Invoked if a player used a banner as a gameobject */
void BattlegroundBFG::EventPlayerClickedOnFlag(Player* source, GameObject* /*target_obj*/)
{
    if (GetStatus() != STATUS_IN_PROGRESS)
        return;

    uint8 node = BG_BFG_NODE_LIGHTHOUSE;
    GameObject* obj = GetBgMap()->GetGameObject(BgObjects[node * 8 + 5]);
    while ((node < BG_BFG_DYNAMIC_NODES_COUNT) && ((!obj) || (!source->IsWithinDistInMap(obj, 10))))
    {
        ++node;
        obj = GetBgMap()->GetGameObject(BgObjects[node * 8 + BG_BG_OBJECT_AURA_CONTESTED]);
    }

    if (node == BG_BFG_DYNAMIC_NODES_COUNT)
    {
        // this means our player isn't close to any of banners - maybe cheater ??
        return;
    }

    TeamId teamIndex = GetTeamIndexByTeamId(source->GetTeam());

    // Check if player really could use this banner, not cheated
    if (!(m_Nodes[node] == 0 || teamIndex == m_Nodes[node] % 2))
        return;

    source->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_ENTER_PVP_COMBAT);
    uint32 sound = 0;
    // If node is neutral, change to contested
    if (m_Nodes[node] == BG_BFG_NODE_TYPE_NEUTRAL)
    {
        UpdatePlayerScore(source, SCORE_BASES_ASSAULTED, 1);
        m_prevNodes[node] = m_Nodes[node];
        m_Nodes[node] = teamIndex + 1;
        // burn current neutral banner
        _DelBanner(node, BG_BFG_NODE_TYPE_NEUTRAL, 0);
        // create new contested banner
        _CreateBanner(node, BG_BFG_NODE_TYPE_CONTESTED, teamIndex, true);
        _SendNodeUpdate(node);
        m_NodeTimers[node] = BG_BFG_FLAG_CAPTURING_TIME;

        // FIXME: team and node names not localized
        if (teamIndex == 0)
       // PSendMessageToAll(LANG_BG_BG_NODE_CLAIMED, CHAT_MSG_BG_SYSTEM_ALLIANCE, source, _GetNodeNameId(node), LANG_BG_BG_ALLY);
       // else
        //PSendMessageToAll(LANG_BG_BG_NODE_CLAIMED, CHAT_MSG_BG_SYSTEM_HORDE, source, _GetNodeNameId(node), LANG_BG_BG_ALLY);

        sound = BG_BFG_SOUND_NODE_CLAIMED;
    }
    // If node is contested
    else if ((m_Nodes[node] == BG_BFG_NODE_STATUS_ALLY_CONTESTED) || (m_Nodes[node] == BG_BFG_NODE_STATUS_HORDE_CONTESTED))
    {
        // If last state is NOT occupied, change node to enemy-contested
        if (m_prevNodes[node] < BG_BFG_NODE_TYPE_OCCUPIED)
        {
            UpdatePlayerScore(source, SCORE_BASES_ASSAULTED, 1);
            m_prevNodes[node] = m_Nodes[node];
            m_Nodes[node] = teamIndex + BG_BFG_NODE_TYPE_CONTESTED;
            // burn current contested banner
            _DelBanner(node, BG_BFG_NODE_TYPE_CONTESTED, !teamIndex);
            // create new contested banner
            _CreateBanner(node, BG_BFG_NODE_TYPE_CONTESTED, teamIndex, true);
            _SendNodeUpdate(node);
            m_NodeTimers[node] = BG_BFG_FLAG_CAPTURING_TIME;

            // FIXME: node names not localized
            // if (teamIndex == TEAM_ALLIANCE)
            //PSendMessageToAll(LANG_BG_BG_NODE_ASSAULTED, CHAT_MSG_BG_SYSTEM_ALLIANCE, source, _GetNodeNameId(node));
            //else
            //PSendMessageToAll(LANG_BG_BG_NODE_ASSAULTED, CHAT_MSG_BG_SYSTEM_HORDE, source, _GetNodeNameId(node));
        }
        // If contested, change back to occupied
        else
        {
            UpdatePlayerScore(source, SCORE_BASES_DEFENDED, 1);
            m_prevNodes[node] = m_Nodes[node];
            m_Nodes[node] = teamIndex + BG_BFG_NODE_TYPE_OCCUPIED;
            // burn current contested banner
            _DelBanner(node, BG_BFG_NODE_TYPE_CONTESTED, !teamIndex);
            // create new occupied banner
            _CreateBanner(node, BG_BFG_NODE_TYPE_OCCUPIED, teamIndex, true);
            _SendNodeUpdate(node);
            m_NodeTimers[node] = 0;
            _NodeOccupied(node, (teamIndex == TEAM_ALLIANCE) ? ALLIANCE : HORDE);

            // FIXME: node names not localized
            // if (teamIndex == TEAM_ALLIANCE)
                 //SendBroadcastText(BFGNodes[node].TextAllianceAssaulted, CHAT_MSG_BG_SYSTEM_ALLIANCE, source);
            //else
                 //SendBroadcastText(BFGNodes[node].TextHordeAssaulted, CHAT_MSG_BG_SYSTEM_HORDE, source);
        }
        sound = (teamIndex == TEAM_ALLIANCE) ? BG_BFG_SOUND_NODE_ASSAULTED_ALLIANCE : BG_BFG_SOUND_NODE_ASSAULTED_HORDE;
    }
    // If node is occupied, change to enemy-contested
    else
    {
        UpdatePlayerScore(source, SCORE_BASES_ASSAULTED, 1);
        m_prevNodes[node] = m_Nodes[node];
        m_Nodes[node] = teamIndex + BG_BFG_NODE_TYPE_CONTESTED;
        // burn current occupied banner
        _DelBanner(node, BG_BFG_NODE_TYPE_OCCUPIED, !teamIndex);
        // create new contested banner
        _CreateBanner(node, BG_BFG_NODE_TYPE_CONTESTED, teamIndex, true);
        _SendNodeUpdate(node);
        _NodeDeOccupied(node);
        m_NodeTimers[node] = BG_BFG_FLAG_CAPTURING_TIME;

        // FIXME: node names not localized
        /* if (teamIndex == TEAM_ALLIANCE)
        SendMessage2ToAll(LANG_BG_AB_NODE_ASSAULTED, CHAT_MSG_BG_SYSTEM_ALLIANCE, source, _GetNodeNameId(node));
        else
        SendMessage2ToAll(LANG_BG_AB_NODE_ASSAULTED, CHAT_MSG_BG_SYSTEM_HORDE, source, _GetNodeNameId(node));*/

        sound = (teamIndex == TEAM_ALLIANCE) ? BG_BFG_SOUND_NODE_ASSAULTED_ALLIANCE : BG_BFG_SOUND_NODE_ASSAULTED_HORDE;
    }

    // If node is occupied again, send "X has taken the Y" msg.
    if (m_Nodes[node] >= BG_BFG_NODE_TYPE_OCCUPIED)
    {
        // FIXME: team and node names not localized
       //if (teamIndex == TEAM_ALLIANCE)
       //SendBroadcastText(LANG_BG_BG_NODE_TAKEN, CHAT_MSG_BG_SYSTEM_ALLIANCE, NULL, LANG_BG_BG_ALLY, _GetNodeNameId(node));
       //else
       //SendBroadcastText(LANG_BG_BG_NODE_TAKEN, CHAT_MSG_BG_SYSTEM_HORDE, NULL, LANG_BG_BG_ALLY, _GetNodeNameId(node));
    }

    PlaySoundToAll(sound);
}

bool BattlegroundBFG::SetupBattleground()
{
    for (int i = 0; i < BG_BFG_DYNAMIC_NODES_COUNT; ++i)
    {
        if (!AddObject(BG_BG_OBJECT_BANNER_NEUTRAL + 8 * i, BG_BG_OBJECTID_NODE_BANNER_0 + i, BG_BFG_NodePositions[i][0], BG_BFG_NodePositions[i][1], BG_BFG_NodePositions[i][2], BG_BFG_NodePositions[i][3], 0, 0, sin(BG_BFG_NodePositions[i][3] / 2), cos(BG_BFG_NodePositions[i][3] / 2), RESPAWN_ONE_DAY)
            || !AddObject(BG_BG_OBJECT_BANNER_CONT_A + 8 * i, BG_BFG_OBJECTID_BANNER_CONT_A, BG_BFG_NodePositions[i][0], BG_BFG_NodePositions[i][1], BG_BFG_NodePositions[i][2], BG_BFG_NodePositions[i][3], 0, 0, sin(BG_BFG_NodePositions[i][3] / 2), cos(BG_BFG_NodePositions[i][3] / 2), RESPAWN_ONE_DAY)
            || !AddObject(BG_BG_OBJECT_BANNER_CONT_H + 8 * i, BG_BFG_OBJECTID_BANNER_CONT_H, BG_BFG_NodePositions[i][0], BG_BFG_NodePositions[i][1], BG_BFG_NodePositions[i][2], BG_BFG_NodePositions[i][3], 0, 0, sin(BG_BFG_NodePositions[i][3] / 2), cos(BG_BFG_NodePositions[i][3] / 2), RESPAWN_ONE_DAY)
            || !AddObject(BG_BG_OBJECT_BANNER_ALLY + 8 * i, BG_BFG_OBJECTID_BANNER_A, BG_BFG_NodePositions[i][0], BG_BFG_NodePositions[i][1], BG_BFG_NodePositions[i][2], BG_BFG_NodePositions[i][3], 0, 0, sin(BG_BFG_NodePositions[i][3] / 2), cos(BG_BFG_NodePositions[i][3] / 2), RESPAWN_ONE_DAY)
            || !AddObject(BG_BG_OBJECT_BANNER_HORDE + 8 * i, BG_BFG_OBJECTID_BANNER_H, BG_BFG_NodePositions[i][0], BG_BFG_NodePositions[i][1], BG_BFG_NodePositions[i][2], BG_BFG_NodePositions[i][3], 0, 0, sin(BG_BFG_NodePositions[i][3] / 2), cos(BG_BFG_NodePositions[i][3] / 2), RESPAWN_ONE_DAY)
            || !AddObject(BG_BG_OBJECT_AURA_ALLY + 8 * i, BG_BFG_OBJECTID_AURA_A, BG_BFG_NodePositions[i][0], BG_BFG_NodePositions[i][1], BG_BFG_NodePositions[i][2], BG_BFG_NodePositions[i][3], 0, 0, sin(BG_BFG_NodePositions[i][3] / 2), cos(BG_BFG_NodePositions[i][3] / 2), RESPAWN_ONE_DAY)
            || !AddObject(BG_BG_OBJECT_AURA_HORDE + 8 * i, BG_BFG_OBJECTID_AURA_H, BG_BFG_NodePositions[i][0], BG_BFG_NodePositions[i][1], BG_BFG_NodePositions[i][2], BG_BFG_NodePositions[i][3], 0, 0, sin(BG_BFG_NodePositions[i][3] / 2), cos(BG_BFG_NodePositions[i][3] / 2), RESPAWN_ONE_DAY)
            || !AddObject(BG_BG_OBJECT_AURA_CONTESTED + 8 * i, BG_BFG_OBJECTID_AURA_C, BG_BFG_NodePositions[i][0], BG_BFG_NodePositions[i][1], BG_BFG_NodePositions[i][2], BG_BFG_NodePositions[i][3], 0, 0, sin(BG_BFG_NodePositions[i][3] / 2), cos(BG_BFG_NodePositions[i][3] / 2), RESPAWN_ONE_DAY))
        {
            TC_LOG_ERROR("sql.sql", "BatteGroundAB: Failed to spawn some object Battleground not created!");
            return false;
        }
    }

    if (!AddObject(BG_BG_OBJECT_GATE_A, BG_BFG_OBJECTID_GATE_A, BG_BFG_DoorPositions[0][0], BG_BFG_DoorPositions[0][1], BG_BFG_DoorPositions[0][2], BG_BFG_DoorPositions[0][3], BG_BFG_DoorPositions[0][4], BG_BFG_DoorPositions[0][5], BG_BFG_DoorPositions[0][6], BG_BFG_DoorPositions[0][7], RESPAWN_IMMEDIATELY)
        || !AddObject(BG_BG_OBJECT_GATE_H, BG_BFG_OBJECTID_GATE_H, BG_BFG_DoorPositions[1][0], BG_BFG_DoorPositions[1][1], BG_BFG_DoorPositions[1][2], BG_BFG_DoorPositions[1][3], BG_BFG_DoorPositions[1][4], BG_BFG_DoorPositions[1][5], BG_BFG_DoorPositions[1][6], BG_BFG_DoorPositions[1][7], RESPAWN_IMMEDIATELY))
    {
        TC_LOG_ERROR("sql.sql", "BatteGroundAB: Failed to spawn door object Battleground not created!");
        return false;
    }

    //buffs
    for (int i = 0; i < BG_BFG_DYNAMIC_NODES_COUNT; ++i)
    {
        if (!AddObject(BG_BG_OBJECT_SPEEDBUFF_LIGHTHOUSE + 3 * i, Buff_Entries[0], BG_BFG_BuffPositions[i][0], BG_BFG_BuffPositions[i][1], BG_BFG_BuffPositions[i][2], BG_BFG_BuffPositions[i][3], 0, 0, sin(BG_BFG_BuffPositions[i][3] / 2), cos(BG_BFG_BuffPositions[i][3] / 2), RESPAWN_ONE_DAY)
            || !AddObject(BG_BG_OBJECT_SPEEDBUFF_LIGHTHOUSE + 3 * i + 1, Buff_Entries[1], BG_BFG_BuffPositions[i][0], BG_BFG_BuffPositions[i][1], BG_BFG_BuffPositions[i][2], BG_BFG_BuffPositions[i][3], 0, 0, sin(BG_BFG_BuffPositions[i][3] / 2), cos(BG_BFG_BuffPositions[i][3] / 2), RESPAWN_ONE_DAY)
            || !AddObject(BG_BG_OBJECT_SPEEDBUFF_LIGHTHOUSE + 3 * i + 2, Buff_Entries[2], BG_BFG_BuffPositions[i][0], BG_BFG_BuffPositions[i][1], BG_BFG_BuffPositions[i][2], BG_BFG_BuffPositions[i][3], 0, 0, sin(BG_BFG_BuffPositions[i][3] / 2), cos(BG_BFG_BuffPositions[i][3] / 2), RESPAWN_ONE_DAY)
            )
            TC_LOG_ERROR("sql.sql", "BatteGroundBG: Failed to spawn buff object!");
    }

    return true;
}

void BattlegroundBFG::Reset()
{
    //call parent's class reset
    Battleground::Reset();

    m_TeamScores[TEAM_ALLIANCE] = 0;
    m_TeamScores[TEAM_HORDE] = 0;
    m_lastTick[TEAM_ALLIANCE] = 0;
    m_lastTick[TEAM_HORDE] = 0;
    m_HonorScoreTics[TEAM_ALLIANCE] = 0;
    m_HonorScoreTics[TEAM_HORDE] = 0;
    m_IsInformedNearVictory = false;
    bool isBGWeekend = sBattlegroundMgr->IsBGWeekend(GetTypeID());
    m_HonorTics = (isBGWeekend) ? BG_BFG_BGBGWeekendHonorTicks : BG_BFG_NotBGBGWeekendHonorTicks;
    m_TeamScores500Disadvantage[TEAM_ALLIANCE] = false;
    m_TeamScores500Disadvantage[TEAM_HORDE] = false;

    for (uint8 i = 0; i < BG_BFG_DYNAMIC_NODES_COUNT; ++i)
    {
        m_Nodes[i] = 0;
        m_prevNodes[i] = 0;
        m_NodeTimers[i] = 0;
        m_BannerTimers[i].timer = 0;
    }

    for (uint8 i = 0; i < BG_BFG_ALL_NODES_COUNT + 3; ++i)//+3 for aura triggers
        DelCreature(i);
}

void BattlegroundBFG::EndBattleground(uint32 winner)
{
    //win reward
    if (winner == ALLIANCE)
        RewardHonorToTeam(GetBonusHonorFromKill(1), ALLIANCE);
    if (winner == HORDE)
        RewardHonorToTeam(GetBonusHonorFromKill(1), HORDE);
    //complete map_end rewards (even if no team wins)
    RewardHonorToTeam(GetBonusHonorFromKill(1), HORDE);
    RewardHonorToTeam(GetBonusHonorFromKill(1), ALLIANCE);

    Battleground::EndBattleground(winner);
}

WorldSafeLocsEntry const* BattlegroundBFG::GetClosestGraveYard(Player* player)
{
    TeamId teamIndex = GetTeamIndexByTeamId(player->GetTeam());

    // Is there any occupied node for this team?
    std::vector<uint8> nodes;
    for (uint8 i = 0; i < BG_BFG_DYNAMIC_NODES_COUNT; ++i)
        if (m_Nodes[i] == teamIndex + 3)
            nodes.push_back(i);

    WorldSafeLocsEntry const* good_entry = NULL;
    // If so, select the closest node to place ghost on
    if (!nodes.empty())
    {
        float player_x = player->GetPositionX();
        float player_y = player->GetPositionY();

        float mindist = 999999.0f;
        for (uint8 i = 0; i < nodes.size(); ++i)
        {
            //WorldSafeLocsEntry const*entry = GetWorldSafeLoc.LookupEntry(BG_BFG_GraveyardIds[nodes[i]]);
            //if (!entry)
            //    continue;
            //float dist = (entry->Loc.m_positionX - player_x)*(entry->Loc.m_positionY - player_x) + (entry->Loc.m_positionZ - player_y)*(entry->Loc.Y - player_y);
            //if (mindist > dist)
            //{
            //    mindist = dist;
            //    good_entry = entry;
            //}
        }
        nodes.clear();
    }
    // If not, place ghost on starting location
    //if (!good_entry)
    //    good_entry = GetWorldSafeLoc.LookupEntry(BG_BFG_GraveyardIds[teamIndex + 3]);

    return good_entry;
}

bool BattlegroundBFG::UpdatePlayerScore(Player* Source, uint32 type, uint32 value, bool doAddHonor)
{
    BattlegroundScoreMap::iterator itr = PlayerScores.find(Source->GetGUID());
    if (itr == PlayerScores.end())                         // player not found...
        return false;

    switch (type)
    {
    case SCORE_BASES_ASSAULTED:
        ((BattlegroundBFGScore*)itr->second)->BasesAssaulted += value;
        Source->UpdateCriteria(CRITERIA_TYPE_BG_OBJECTIVE_CAPTURE, BG_OBJECTIVE_ASSAULT_BASE);
        break;
    case SCORE_BASES_DEFENDED:
        ((BattlegroundBFGScore*)itr->second)->BasesDefended += value;
        Source->UpdateCriteria(CRITERIA_TYPE_BG_OBJECTIVE_CAPTURE, BG_OBJECTIVE_DEFEND_BASE);
        break;
    default:
        Battleground::UpdatePlayerScore(Source, type, value, doAddHonor);
        break;
    }
    return true;
}

bool BattlegroundBFG::IsAllNodesControlledByTeam(uint32 team) const
{
    uint32 count = 0;
    for (int i = 0; i < BG_BFG_DYNAMIC_NODES_COUNT; ++i)
        if ((team == ALLIANCE && m_Nodes[i] == BG_BFG_NODE_STATUS_ALLY_OCCUPIED) ||
            (team == HORDE && m_Nodes[i] == BG_BFG_NODE_STATUS_HORDE_OCCUPIED))
            ++count;

    return count == BG_BFG_DYNAMIC_NODES_COUNT;
}

