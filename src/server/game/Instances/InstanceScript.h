/*
 * Copyright (C) 2021 BfaCore Reforged
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TRINITY_INSTANCE_DATA_H
#define TRINITY_INSTANCE_DATA_H

#include "ZoneScript.h"
#include "Common.h"
#include "CriteriaHandler.h"
#include "Optional.h"
#include "Position.h"
#include <map>
#include <memory>
#include <set>

#define OUT_SAVE_INST_DATA             TC_LOG_DEBUG("scripts", "Saving Instance Data for Instance %s (Map %d, Instance Id %d)", instance->GetMapName(), instance->GetId(), instance->GetInstanceId())
#define OUT_SAVE_INST_DATA_COMPLETE    TC_LOG_DEBUG("scripts", "Saving Instance Data for Instance %s (Map %d, Instance Id %d) completed.", instance->GetMapName(), instance->GetId(), instance->GetInstanceId())
#define OUT_LOAD_INST_DATA(a)          TC_LOG_DEBUG("scripts", "Loading Instance Data for Instance %s (Map %d, Instance Id %d). Input is '%s'", instance->GetMapName(), instance->GetId(), instance->GetInstanceId(), a)
#define OUT_LOAD_INST_DATA_COMPLETE    TC_LOG_DEBUG("scripts", "Instance Data Load for Instance %s (Map %d, Instance Id: %d) is complete.", instance->GetMapName(), instance->GetId(), instance->GetInstanceId())
#define OUT_LOAD_INST_DATA_FAIL        TC_LOG_ERROR("scripts", "Unable to load Instance Data for Instance %s (Map %d, Instance Id: %d).", instance->GetMapName(), instance->GetId(), instance->GetInstanceId())

class AreaBoundary;
class Creature;
class CreatureGroup;
class GameObject;
class InstanceMap;
class ModuleReference;
class Player;
class Unit;
class TempSummon;
class ChallengeModeMgr;
struct Position;
enum CriteriaTypes : uint8;
enum CriteriaTimedTypes : uint8;
enum EncounterCreditType : uint8;

namespace WorldPackets
{
    namespace WorldState
    {
        class InitWorldStates;
    }
}

enum EncounterFrameType
{
    ENCOUNTER_FRAME_SET_COMBAT_RES_LIMIT        = 0,
    ENCOUNTER_FRAME_RESET_COMBAT_RES_LIMIT      = 1,
    ENCOUNTER_FRAME_ENGAGE                      = 2,
    ENCOUNTER_FRAME_DISENGAGE                   = 3,
    ENCOUNTER_FRAME_UPDATE_PRIORITY             = 4,
    ENCOUNTER_FRAME_ADD_TIMER                   = 5,
    ENCOUNTER_FRAME_ENABLE_OBJECTIVE            = 6,
    ENCOUNTER_FRAME_UPDATE_OBJECTIVE            = 7,
    ENCOUNTER_FRAME_DISABLE_OBJECTIVE           = 8,
    ENCOUNTER_FRAME_UNK7                        = 9,    // Seems to have something to do with sorting the encounter units
    ENCOUNTER_FRAME_INSTANCE_START              = 10,
    ENCOUNTER_FRAME_INSTANCE_END                = 11,
    ENCOUNTER_FRAME_UPDATE_ALLOWING_RELEASE     = 12,
    ENCOUNTER_FRAME_UPDATE_SUPPRESSING_RELEASE  = 13
};

enum EncounterState
{
    NOT_STARTED   = 0,
    IN_PROGRESS   = 1,
    FAIL          = 2,
    DONE          = 3,
    SPECIAL       = 4,
    TO_BE_DECIDED = 5
};

enum BossTalk
{
    BOSS_TALK_ENTER_COMBAT  = 200,
    BOSS_TALK_KILL_PLAYER   = 201,
    BOSS_TALK_JUST_DIED     = 202,
};

enum DoorType
{
    DOOR_TYPE_ROOM          = 0,    // Door can open if encounter is not in progress
    DOOR_TYPE_PASSAGE       = 1,    // Door can open if encounter is done
    DOOR_TYPE_SPAWN_HOLE    = 2,    // Door can open if encounter is in progress, typically used for spawning places
    MAX_DOOR_TYPES
};



struct DoorData
{
    uint32 entry, bossId;
    DoorType type;
    uint32 boundary;
};

enum BoundaryType
{
    BOUNDARY_NONE = 0,
    BOUNDARY_N,
    BOUNDARY_S,
    BOUNDARY_E,
    BOUNDARY_W,
    BOUNDARY_NE,
    BOUNDARY_NW,
    BOUNDARY_SE,
    BOUNDARY_SW,
    BOUNDARY_MAX_X = BOUNDARY_N,
    BOUNDARY_MIN_X = BOUNDARY_S,
    BOUNDARY_MAX_Y = BOUNDARY_W,
    BOUNDARY_MIN_Y = BOUNDARY_E
};

enum ChallengeSpells : uint32
{
    ChallengersMight = 206150, /// generic creature aura
    ChallengersBurden = 206151, /// generic player aura
    ChallengerBolstering = 209859,
    ChallengerNecrotic = 209858,
    ChallengerOverflowing = 221772,
    ChallengerSanguine = 226489,
    ChallengerRaging = 228318,
    ChallengerSummonVolcanicPlume = 209861,
    ChallengerVolcanicPlume = 209862,
    ChallengerBursting = 240443,
    ChallengerQuake = 240447,

    //Explosive
    SPELL_FEL_EXPLOSIVES_SUMMON_1 = 240444, //Short dist
    SPELL_FEL_EXPLOSIVES_SUMMON_2 = 243110, //Long dist
    SPELL_FEL_EXPLOSIVES_VISUAL = 240445,
    SPELL_FEL_EXPLOSIVES_DMG = 240446,
};

struct BossBoundaryEntry
{
    uint32 BossId;
    AreaBoundary const* Boundary;
};

struct TC_GAME_API BossBoundaryData
{
    typedef std::vector<BossBoundaryEntry> StorageType;
    typedef StorageType::const_iterator const_iterator;

    BossBoundaryData(std::initializer_list<BossBoundaryEntry> data) : _data(data) { }
    ~BossBoundaryData();
    const_iterator begin() const { return _data.begin(); }
    const_iterator end() const { return _data.end(); }

    private:
        StorageType _data;
};

struct MinionData
{
    uint32 entry, bossId;
};

struct ObjectData
{
    uint32 entry;
    uint32 type;
};

typedef std::set<AreaBoundary const*> CreatureBoundary;

struct BossInfo
{
    BossInfo() : state(TO_BE_DECIDED) { }
    EncounterState state;
    GuidSet door[MAX_DOOR_TYPES];
    GuidSet minion;
    CreatureBoundary boundary;
};

struct DoorInfo
{
    explicit DoorInfo(BossInfo* _bossInfo, DoorType _type)
        : bossInfo(_bossInfo), type(_type) { }
    BossInfo* bossInfo;
    DoorType type;
};

struct MinionInfo
{
    explicit MinionInfo(BossInfo* _bossInfo) : bossInfo(_bossInfo) { }
    BossInfo* bossInfo;
};

typedef std::multimap<uint32 /*entry*/, DoorInfo> DoorInfoMap;
typedef std::pair<DoorInfoMap::const_iterator, DoorInfoMap::const_iterator> DoorInfoMapBounds;

typedef std::map<uint32 /*entry*/, MinionInfo> MinionInfoMap;
typedef std::map<uint32 /*type*/, ObjectGuid /*guid*/> ObjectGuidMap;
typedef std::map<uint32 /*entry*/, uint32 /*type*/> ObjectInfoMap;

class TC_GAME_API InstanceScript : public ZoneScript
{
    public:
        explicit InstanceScript(InstanceMap* map);

        virtual ~InstanceScript() { }

        InstanceMap* instance;
        void BroadcastPacket(WorldPacket const* data) const;
        // On creation, NOT load.
        // PLEASE INITIALIZE FIELDS IN THE CONSTRUCTOR INSTEAD !!!
        // KEEPING THIS METHOD ONLY FOR BACKWARD COMPATIBILITY !!!
        virtual void Initialize() { }

        // On load
        virtual void Load(char const* data);

        // When save is needed, this function generates the data
        virtual std::string GetSaveData();

        void SaveToDB();

        virtual void Update(uint32 /*diff*/) { }
        void UpdateOperations(uint32 const diff);
        void UpdateCombatResurrection(uint32 /*diff*/);

        // Used by the map's CannotEnter function.
        // This is to prevent players from entering during boss encounters.
        virtual bool IsEncounterInProgress() const;
       /// Challenge
     //   void SetChallenge(Challenge* challenge);
      //  Challenge* GetChallenge() const;
        bool IsChallenge() const;
        // Called when a creature/gameobject is added to map or removed from map.
        // Insert/Remove objectguid to dynamic guid store
        virtual void OnCreatureCreate(Creature* creature) override;
        virtual void OnCreatureRemove(Creature* creature) override;

        virtual void OnGameObjectCreate(GameObject* go) override;
        virtual void OnGameObjectRemove(GameObject* go) override;

        ObjectGuid GetObjectGuid(uint32 type) const;
        virtual ObjectGuid GetGuidData(uint32 type) const override;

        Creature* GetCreature(uint32 type);
        GameObject* GetGameObject(uint32 type);

        void OnPlayerEnter(Player*) override;
        void GetPlayersCount();
        void OnPlayerExit(Player*) override;
        void OnPlayerDeath(Player*) override;

        virtual void OnCreatureGroupWipe(uint32 /*creatureGroupId*/) { }

        virtual void OnCompletedCriteriaTree(CriteriaTree const* /*tree*/) { }

        // Handle open / close objects
        virtual void OnGameObjectCreateForScript(GameObject* go);
        virtual void OnGameObjectRemoveForScript(GameObject* go);
        // * use HandleGameObject(0, boolen, GO); in OnObjectCreate in instance scripts
        // * use HandleGameObject(GUID, boolen, nullptr); in any other script
        void HandleGameObject(ObjectGuid guid, bool open, GameObject* go = nullptr);

        // Change active state of doors or buttons
        void DoUseDoorOrButton(ObjectGuid guid, uint32 withRestoreTime = 0, bool useAlternativeState = false);
        void DoCloseDoorOrButton(ObjectGuid guid);

        // Respawns a GO having negative spawntimesecs in gameobject-table
        void DoRespawnGameObject(ObjectGuid guid, uint32 timeToDespawn = MINUTE);

        // Sends world state update to all players in instance
        void DoUpdateWorldState(uint32 worldstateId, uint32 worldstateValue);

        // Send Notify to all players in instance
        void DoSendNotifyToInstance(char const* format, ...);

        void DoResetAchievementCriteria(CriteriaTypes type, uint64 miscValue1 = 0, uint64 miscValue2 = 0, bool evenIfCriteriaComplete = false);
     
	   

		// Update Achievement Criteria for all players in instance
        void DoUpdateCriteria(CriteriaTypes type, uint32 miscValue1 = 0, uint32 miscValue2 = 0, Unit* unit = nullptr);

        // Send Event For Scenario
        void DoSendEventScenario(uint32 eventId);

        // Start/Stop Timed Achievement Criteria for all players in instance
         void DoStartCriteriaTimer(CriteriaTimedTypes type, uint32 entry);
         void DoStopCriteriaTimer(CriteriaTimedTypes type, uint32 entry);

        // Remove Auras due to Spell on all players in instance
        void DoRemoveAurasDueToSpellOnPlayers(uint32 spell);

        // Kill all players with this aura in the instance
        void DoKillPlayersWithAura(uint32 spell);

        // Cast spell on all players in instance
        void DoCastSpellOnPlayers(uint32 spell, Unit* caster = nullptr, bool triggered = true);

        // Play scene by packageId on all players in instance
        void DoPlayScenePackageIdOnPlayers(uint32 scenePackageId);

        // Play scene by Id on all players in instance
        void DoPlaySceneOnPlayers(uint32 sceneId);

        // Remove all movement forces related to forceGuid
        void DoRemoveForcedMovementsOnPlayers(ObjectGuid forceGuid);

        void DoSetAlternatePowerOnPlayers(int32 value);

        void DoModifyPlayerCurrencies(uint32 id, int32 value);
	    
		void RepopPlayersAtGraveyard();

        void DoNearTeleportPlayers(const Position pos, bool casting = false);

        void DoTeleportPlayers(uint32 mapId, const Position pos);

        void DoKilledMonsterCredit(uint32 questId, uint32 entry, ObjectGuid guid = ObjectGuid::Empty);

        // Complete Achievement for all players in instance
        void DoCompleteAchievement(uint32 achievement);

        // Update Achievement Criteria for all players in instance
        void DoUpdateAchievementCriteria(CriteriaTypes type, uint32 miscValue1 = 0, uint32 miscValue2 = 0, Unit* unit = nullptr);

        // Add aura on all players in instance
        void DoAddAuraOnPlayers(uint32 spell);

        // Do combat stop on all players in instance
        void DoCombatStopOnPlayers();

        // Start movie for all players in instance
        void DoStartMovie(uint32 movieId);

        void DoPlayConversation(uint32 conversationId);

        //Scenarios
        void DoSendScenarioEvent(uint32 eventId);
        void GetScenarioByID(Player* p_Player, uint32 p_ScenarioId);

        // Add item on all players in instance
        void DoAddItemOnPlayers(uint32 itemId, uint32 count);

        // Remove item on all players in instance
        void DoDestroyItemCountOnPlayers(uint32 item, uint32 count);

        // Add item by class on all players in instance
        void DoAddItemByClassOnPlayers(uint8 classId, uint32 itemId, uint32 count);

        // Remove item by class on all players in instance
        void DoDestroyItemCountByClassOnPlayers(uint8 classId, uint32 item, uint32 count);

        // Resurrect all players in instance
        void DoResurrectPlayers(float restore_percent);

        // Return wether server allow two side groups or not
        bool ServerAllowsTwoSideGroups();

        CreatureGroup* SummonCreatureGroup(uint32 creatureGroupID, std::list<TempSummon*>* list = nullptr);
        CreatureGroup* GetCreatureGroup(uint32 creatureGroupID);
        bool IsCreatureGroupWiped(uint32 creatureGroupID);
        void DespawnCreatureGroup(uint32 creatureGroupID);

        virtual bool SetBossState(uint32 id, EncounterState state);
        EncounterState GetBossState(uint32 id) const { return id < bosses.size() ? bosses[id].state : TO_BE_DECIDED; }
        static std::string GetBossStateName(uint8 state);
        CreatureBoundary const* GetBossBoundary(uint32 id) const { return id < bosses.size() ? &bosses[id].boundary : nullptr; }

        // Achievement criteria additional requirements check
        // NOTE: not use this if same can be checked existed requirement types from AchievementCriteriaRequirementType
        virtual bool CheckAchievementCriteriaMeet(uint32 /*criteria_id*/, Player const* /*source*/, Unit const* /*target*/ = nullptr, uint32 /*miscvalue1*/ = 0);

        // Checks boss requirements (one boss required to kill other)
        virtual bool CheckRequiredBosses(uint32 /*bossId*/, Player const* /*player*/ = nullptr) const { return true; }

        // Checks encounter state at kill/spellcast
        void UpdateEncounterStateForKilledCreature(uint32 creatureId, Unit* source);
        void UpdateEncounterStateForSpellCast(uint32 spellId, Unit* source);

        // Used only during loading
        void SetCompletedEncountersMask(uint32 newMask) { completedEncounters = newMask; }

        // Returns completed encounters mask for packets
        uint32 GetCompletedEncounterMask() const { return completedEncounters; }

        // Sets the entrance location (WorldSafeLoc) id
        void SetEntranceLocation(uint32 worldSafeLocationId);

        // Sets a temporary entrance that does not get saved to db
        void SetTemporaryEntranceLocation(uint32 worldSafeLocationId) { _temporaryEntranceId = worldSafeLocationId; }

        // Get's the current entrance id
        uint32 GetEntranceLocation() const { uint32 locationId = _temporaryEntranceId ? _temporaryEntranceId : _entranceId; OnGetEntranceLocation(locationId); return locationId; }

        virtual void OnGetEntranceLocation(uint32& /*worldSafeLocationId*/) const { }

        /// Add timed delayed operation
        /// @p_Timeout  : Delay time
        /// @p_Function : Callback function
        void AddTimedDelayedOperation(uint32 timeout, std::function<void()> && function)
        {
            emptyWarned = false;
            timedDelayedOperations.push_back(std::pair<uint32, std::function<void()>>(timeout, function));
        }

        /// Called after last delayed operation was deleted
        /// Do whatever you want
        virtual void LastOperationCalled() { }

        std::vector<std::pair<int32, std::function<void()>>>    timedDelayedOperations;   ///< Delayed operations
        bool                                                    emptyWarned;              ///< Warning when there are no more delayed operations

        void SendEncounterUnit(uint32 type, Unit* unit = nullptr, uint8 priority = 0, uint8 param2 = 0);
        void SendEncounterStart(uint32 inCombatResCount = 0, uint32 maxInCombatResCount = 0, uint32 inCombatResChargeRecovery = 0, uint32 nextCombatResChargeTime = 0);
        void SendEncounterEnd();

        bool IsAllowingRelease;

        void SendBossKillCredit(uint32 encounterId);

        virtual void FillInitialWorldStates(WorldPackets::WorldState::InitWorldStates& /*packet*/) { }

        /// SetData auto get lastdataid
        void SendNextData(uint32 Value);

        /////////////////////////////////////////////////////////////////////////////
        bool IsScenarioComplete() { return m_IsScenarioComplete; }
        bool m_IsScenarioComplete;
        void CompleteScenario();
        void CompleteCurrStep();

        // Check if all players are dead (except gamemasters)
        bool IsWipe() const;

        // ReCheck PhaseTemplate related conditions
        void UpdatePhasing();

        uint32 GetEncounterCount() const { return uint32(bosses.size()); }

        void InitializeCombatResurrections(uint8 charges = 1, uint32 interval = 0);
        void AddCombatResurrectionCharge();
        void UseCombatResurrection();
        void ResetCombatResurrections();
        uint8 GetCombatResurrectionCharges() const { return _combatResurrectionCharges; }
        uint32 GetCombatResurrectionChargeInterval() const;

        // Challenge Modes
        void StartChallengeMode(uint8 level);
        void CompleteChallengeMode();

        bool IsChallengeModeStarted() const { return _challengeModeStarted; }
        uint8 GetChallengeModeLevel() const { return _challengeModeLevel; }
        uint32 GetChallengeModeCurrentDuration() const;

        void SendChallengeModeStart(Player* player = nullptr) const;
        void SendChallengeModeDeathCount(Player* player = nullptr) const;
        void SendChallengeModeElapsedTimer(Player* player = nullptr) const;
        void SendChallengeModeNewPlayerRecord(Player* player);
       

        void CastChallengeCreatureSpell(Creature* creature);
        void CastChallengePlayerSpell(Player* player);

        // load scenario after challenge mode started
        void SetChallengeModeScenario(uint32 scenarioId) { _challengeModeScenario = scenarioId; }
        void SetChallengeDoorPos(Position pos) { _challengeModeDoorPosition = pos; }
        virtual void SpawnChallengeModeRewardChest() { }
	
        void ResetChallengeMode();
        
		void SetFontOfPowerPos(Position pos) { _challengeModeFontOfPowerPosition = pos; }
        void SetFontOfPowerPos2(Position pos) { _challengeModeFontOfPowerPosition2 = pos; }
        void SpawnFontOfPower();

        virtual void ShowChallengeDoor() { }
        virtual void HideChallengeDoor() { }

        void AfterChallengeModeStarted();

        std::vector<ObjectGuid> _challengeDoorGuids;
        std::vector<ObjectGuid> _challengeChestGuids;
        ObjectGuid _challengeOrbGuid;
        ObjectGuid _challengeChest;

        void SetCheckPointPos(Position pos) { _checkPointPosition = pos; }
        Optional<Position> GetCheckPoint() { return _checkPointPosition; }

    protected:
        void SetHeaders(std::string const& dataHeaders);
        void SetBossNumber(uint32 number) { bosses.resize(number); }
        void LoadBossBoundaries(BossBoundaryData const& data);
        void LoadDoorData(DoorData const* data);
        void LoadMinionData(MinionData const* data);
        void LoadObjectData(ObjectData const* creatureData, ObjectData const* gameObjectData);

        void AddObject(Creature* obj, bool add);
        void AddObject(GameObject* obj, bool add);
        void AddObject(WorldObject* obj, uint32 type, bool add);

        virtual void AddDoor(GameObject* door, bool add);
        void AddMinion(Creature* minion, bool add);

        virtual void UpdateDoorState(GameObject* door);
        void UpdateMinionState(Creature* minion, EncounterState state);

        // Exposes private data that should never be modified unless exceptional cases.
        // Pay very much attention at how the returned BossInfo data is modified to avoid issues.
        BossInfo* GetBossInfo(uint32 id);

        // Instance Load and Save
        bool ReadSaveDataHeaders(std::istringstream& data);
        void ReadSaveDataBossStates(std::istringstream& data);
        virtual void ReadSaveDataMore(std::istringstream& /*data*/) { }
        void WriteSaveDataHeaders(std::ostringstream& data);
        void WriteSaveDataBossStates(std::ostringstream& data);
        virtual void WriteSaveDataMore(std::ostringstream& /*data*/) { }


        GuidUnorderedSet _challengers;
        bool _checkStart;
        bool _canRun;
        bool _run;
        bool _complete;

        ObjectGuid m_gguid;
        ObjectGuid m_ownerGuid;
        ObjectGuid m_itemGuid;
        InstanceScript* _instanceScript;
        uint32 _challengeTimer;
        uint32 _affixQuakingTimer;
        uint8 _rewardLevel;

        bool _SkipCheckRequiredBosses(Player const* player = nullptr) const;
        uint32 _mapID;
        
        std::array<uint32, 3> _affixes;
        Map* _map;
        Item* _item;
        uint32 _challengeLevel;
        uint32 _instanceID;
        bool _isKeyDepleted;


    private:
        static void LoadObjectData(ObjectData const* creatureData, ObjectInfoMap& objectInfo);
        void UpdateEncounterState(EncounterCreditType type, uint32 creditEntry, Unit* source);
        ChallengeModeMgr* _challenge;
        std::vector<char> headers;

      
      

        std::vector<BossInfo> bosses;
        DoorInfoMap doors;
        MinionInfoMap minions;
        ObjectInfoMap _creatureInfo;
        ObjectInfoMap _gameObjectInfo;
        ObjectGuidMap _objectGuids;
        uint32 completedEncounters; // completed encounter mask, bit indexes are DungeonEncounter.dbc boss numbers, used for packets
        uint32 _entranceId;
        uint32 _temporaryEntranceId;
        uint32 _inCombatResCount;
        uint32 _combatResurrectionTimer;
        uint8 _combatResurrectionCharges; // the counter for available battle resurrections
        bool _combatResurrectionTimerStarted;

        std::map<uint32, std::list<ObjectGuid>> summonBySummonGroupIDs;

        bool _challengeModeStarted;
        uint8 _challengeModeLevel;
        uint32 _challengeModeStartTime;
        uint32 _challengeModeDeathCount;
        Optional<uint32> _challengeModeScenario;
        Optional<Position> _challengeModeDoorPosition;
        Optional<Position> _challengeModeFontOfPowerPosition;
        Optional<Position> _challengeModeFontOfPowerPosition2;
        Optional<Position> _checkPointPosition;

    #ifdef TRINITY_API_USE_DYNAMIC_LINKING
        // Strong reference to the associated script module
        std::shared_ptr<ModuleReference> module_reference;
    #endif // #ifndef TRINITY_API_USE_DYNAMIC_LINKING
};

#endif // TRINITY_INSTANCE_DATA_H
