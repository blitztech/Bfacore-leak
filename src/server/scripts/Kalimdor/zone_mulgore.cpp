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

#include "Creature.h"
#include "ScriptMgr.h"
#include "MotionMaster.h"
#include "ScriptedCreature.h"
#include "WaypointMovementGenerator.h"

enum eAgitatedEarthSpirit
{
    SPELL_SOOTHE_EARTH_SPIRIT       = 69453,
    SPELL_ROCK_BARRAGE              = 81305,
    NPC_EARTH_SPIRIT_CREDIT_BUNNY   = 36872
};

// 36845 - Agitated Earth Spirit
struct npc_agitated_earth_spirit : public ScriptedAI
{
    npc_agitated_earth_spirit(Creature* creature) : ScriptedAI(creature) { }

    void SpellHit(Unit* caster, SpellInfo const* spell) override
    {
        if (spell->Id == SPELL_SOOTHE_EARTH_SPIRIT)
        {
            Position pos;
            caster->GetNearPoint(caster, pos.m_positionX, pos.m_positionY, pos.m_positionZ, 0.0f, MIN_MELEE_REACH, caster->GetAngle(me));
            me->GetMotionMaster()->MovePoint(1, pos);
            _playerGUID = caster->GetGUID();
        }
    }

    void MovementInform(uint32 type, uint32 pointId) override
    {
        if (type == POINT_MOTION_TYPE && pointId == 1)
        {
            switch (urand(0, 1))
            {
                case 0:
                {
                    me->SetFaction(35);
                    if (Player* player = ObjectAccessor::GetPlayer(*me, _playerGUID))
                        player->KilledMonsterCredit(NPC_EARTH_SPIRIT_CREDIT_BUNNY);

                    me->GetScheduler().Schedule(1s, [](TaskContext context)
                    {
                        GetContextCreature()->DisappearAndDie();
                    });

                    break;
                }
                case 1:
                    me->SetFaction(14);
                    if (Player* player = ObjectAccessor::GetPlayer(*me, _playerGUID))
                        AttackStart(player);
                    break;
            }
        }
    }

    void EnterCombat(Unit* /*victim*/) override
    {
        me->GetScheduler().Schedule(4s, 5s, [this](TaskContext context)
        {
            if (Unit* target = SelectTarget(SELECT_TARGET_RANDOM))
            {
                GetContextUnit()->CastSpell(target, SPELL_ROCK_BARRAGE, false);
                context.Repeat(18s, 21s);
            }
        });
    }

private:
    ObjectGuid _playerGUID;
};

enum eKyleTheFrenzied
{
    SPELL_LUNCH_FOR_KYLE    = 42222,
    NPC_KYLE_THE_FRENZIED   = 23616,
    NPC_KYLE_THE_FRIENDLY   = 23622
};

// 23616
struct npc_kyle_the_frenzied : public ScriptedAI
{
    npc_kyle_the_frenzied(Creature* creature) : ScriptedAI(creature) { }
    void SpellHit(Unit* caster, SpellInfo const* spell) override
    {
        if (spell->Id == SPELL_LUNCH_FOR_KYLE)
        {
            Position pos;
            caster->GetNearPoint(caster, pos.m_positionX, pos.m_positionY, pos.m_positionZ, 0.0f, 0.5f, caster->GetAngle(me));
            me->GetMotionMaster()->MovePoint(1, pos);
            _playerGUID = caster->GetGUID();
        }
    }
    void MovementInform(uint32 type, uint32 pointId) override
    {
        if (type == POINT_MOTION_TYPE && pointId == 1)
        {
            // Wait 15 seconds then resume path
            if (WaypointMovementGenerator<Creature>* move = dynamic_cast<WaypointMovementGenerator<Creature>*>(me->GetMotionMaster()->top()))
                move->GetTrackerTimer().Reset(15000);
            Talk(0);
            me->GetScheduler().Schedule(4s, [this](TaskContext /*context*/)
            {
                Talk(1);
            });
            me->GetScheduler().Schedule(9s, [this](TaskContext context)
            {
                Creature* ctxCrea = GetContextCreature();
                Talk(2);
                ctxCrea->UpdateEntry(NPC_KYLE_THE_FRIENDLY);
                ctxCrea->HandleEmoteCommand(EMOTE_STATE_DANCE);
                if (Player* player = ObjectAccessor::GetPlayer(*ctxCrea, _playerGUID))
                    player->KilledMonsterCredit(NPC_KYLE_THE_FRENZIED);
            });
            me->GetScheduler().Schedule(15s, [](TaskContext context)
            {
                Creature* ctxCrea = GetContextCreature();
                ctxCrea->UpdateEntry(NPC_KYLE_THE_FRENZIED);
                ctxCrea->HandleEmoteCommand(EMOTE_STATE_NONE);
            });
        }
    }
private:
    ObjectGuid _playerGUID;
};

enum EagleSpirit
{
    SPELL_EJECT_ALL_PASSENGERS = 50630,
    SPELL_SPIRIT_FORM = 69324
};

Position const EagleSpiritflightPath[] =
{
    { -2884.155f, -71.08681f, 242.0678f },
    { -2720.592f, -111.0035f, 242.5955f },
    { -2683.951f, -382.9010f, 231.1792f },
    { -2619.148f, -484.9288f, 231.1792f },
    { -2543.868f, -525.3333f, 231.1792f },
    { -2465.321f, -502.4896f, 190.7347f },
    { -2343.872f, -401.8281f, -8.320873f }
};
size_t const EagleSpiritflightPathSize = std::extent<decltype(EagleSpiritflightPath)>::value;

class npc_eagle_spirit : public CreatureScript
{
public:
    npc_eagle_spirit() : CreatureScript("npc_eagle_spirit") { }

    struct npc_eagle_spirit_AI : public ScriptedAI
    {
        npc_eagle_spirit_AI(Creature* creature) : ScriptedAI(creature) { }

        void PassengerBoarded(Unit* /*who*/, int8 /*seatId*/, bool apply) override
        {
            if (!apply)
                return;

            me->GetMotionMaster()->MoveSmoothPath(uint32(EagleSpiritflightPathSize), EagleSpiritflightPath, EagleSpiritflightPathSize, false, true);
            me->CastSpell(me, SPELL_SPIRIT_FORM);
        }

        void MovementInform(uint32 type, uint32 pointId) override
        {
            if (type == EFFECT_MOTION_TYPE && pointId == EagleSpiritflightPathSize)
            {
                DoCast(SPELL_EJECT_ALL_PASSENGERS);
            }
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_eagle_spirit_AI(creature);
    }
};

// 71898 Funeral Offering
class spell_mulgore_funeral_offering : public SpellScript
{
    PrepareSpellScript(spell_mulgore_funeral_offering);

    void HandleHitTarget(SpellEffIndex /*effIndex*/)
    {
        if (Creature* target = GetHitCreature())
            if (GetCaster()->IsPlayer())
                GetCaster()->ToPlayer()->KilledMonsterCredit(target->GetEntry());
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_mulgore_funeral_offering::HandleHitTarget, EFFECT_1, SPELL_EFFECT_DUMMY);
    }
};

void AddSC_mulgore()
{
    RegisterCreatureAI(npc_agitated_earth_spirit);
    RegisterCreatureAI(npc_kyle_the_frenzied);
    new npc_eagle_spirit();
    RegisterSpellScript(spell_mulgore_funeral_offering);
}
