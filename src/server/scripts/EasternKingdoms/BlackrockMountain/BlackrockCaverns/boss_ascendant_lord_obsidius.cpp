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

#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "blackrock_caverns.h"

enum Text
{
    YELL_AGGRO                     = 0,
    YELL_KILL                      = 1,
    YELL_SWITCHING_SHADOWS         = 2,
    YELL_DEATH                     = 3,
    EMOTE_SWITCHING_SHADOWS        = 4
};

enum Spells
{
    SPELL_MANA_TAP                 = 36021,
    SPELL_ARCANE_TORRENT           = 36022,
    SPELL_DOMINATION               = 35280,
    SPELL_TWITCHY                  = 76167,
    SPELL_SHADOW_OF_OBSIDIUS       = 76164,
    SPELL_CREPUSCULAR_VEIL         = 76189
};

enum Events
{
    EVENT_MANA_TAP                 = 1,
    EVENT_ARCANE_TORRENT           = 2,
    EVENT_DOMINATION               = 3,
    EVENT_CREPUSCULAR_VEIL         = 4
};

class boss_ascendant_lord_obsidius : public CreatureScript
{
    public:
        boss_ascendant_lord_obsidius(): CreatureScript("boss_ascendant_lord_obsidius") { }

        struct boss_ascendant_lord_obsidiusAI : public BossAI
        {
            boss_ascendant_lord_obsidiusAI(Creature* creature) : BossAI(creature, DATA_ASCENDANT_LORD_OBSIDIUS) { }

            void Reset() override
            {
                _Reset();
            }

            void EnterCombat(Unit* /*who*/) override
            {
                _EnterCombat();
                events.ScheduleEvent(EVENT_MANA_TAP, 30000);
                Talk(YELL_AGGRO);
            }

            void KilledUnit(Unit* who) override
            {
                if (who->GetTypeId() == TYPEID_PLAYER)
                    Talk(YELL_KILL);
            }

            void JustDied(Unit* /*killer*/) override
            {
                _JustDied();
                Talk(YELL_DEATH);
            }

            void UpdateAI(uint32 diff) override
            {
                if (!UpdateVictim())
                    return;

                events.Update(diff);

                if (me->HasUnitState(UNIT_STATE_CASTING))
                    return;

                while (uint32 eventId = events.ExecuteEvent())
                {
                    switch (eventId)
                    {
                        case EVENT_MANA_TAP:
                            DoCastVictim(SPELL_MANA_TAP, true);
                            events.ScheduleEvent(EVENT_MANA_TAP, urand(14000, 22000));
                            break;
                    }
                }

                DoMeleeAttackIfReady();
            }
        };

        CreatureAI* GetAI(Creature* creature) const override
        {
            return GetBlackrockCavernsAI<boss_ascendant_lord_obsidiusAI>(creature);
        }
};

class npc_shadow_of_obsidius : public CreatureScript
{
public:
    npc_shadow_of_obsidius() : CreatureScript("npc_shadow_of_obsidius") {}

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_shadow_of_obsidiusAI(creature);
    }

    struct npc_shadow_of_obsidiusAI : public ScriptedAI
    {
        npc_shadow_of_obsidiusAI(Creature* creature) : ScriptedAI(creature)
        {
            me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_GRIP, true);
            instance = creature->GetInstanceScript();
        }

        InstanceScript* instance;
        EventMap events;

        void Reset() override
        {
            DoCast(me, SPELL_TWITCHY);
            DoCast(me, SPELL_SHADOW_OF_OBSIDIUS);
        }

        void EnterCombat(Unit* /*attacker*/) override
        {
            me->RemoveAura(75054);
            events.RescheduleEvent(EVENT_CREPUSCULAR_VEIL, 3900);
        }

        void DamageTaken(Unit* attacker, uint32& /*damage*/) override
        {
            if (me->GetVictim() != attacker)
            {
                DoResetThreat();
                me->AddThreat(attacker, 1000000.0f);
                me->Attack(attacker, true);
                me->GetMotionMaster()->MoveChase(attacker);
            }
        }

        void UpdateAI(uint32 diff) override
        {
            if (!UpdateVictim())
                return;

            events.Update(diff);

            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;

            if (uint32 eventId = events.ExecuteEvent())
            {
                switch (eventId)
                {
                case EVENT_CREPUSCULAR_VEIL:
                    DoCast(me->GetVictim(), SPELL_CREPUSCULAR_VEIL);
                    events.RescheduleEvent(EVENT_CREPUSCULAR_VEIL, 3900);
                    break;
                }
            }
            DoMeleeAttackIfReady();
        }
    };
};

void AddSC_boss_ascendant_lord_obsidius()
{
    new boss_ascendant_lord_obsidius();
    new npc_shadow_of_obsidius();
}
