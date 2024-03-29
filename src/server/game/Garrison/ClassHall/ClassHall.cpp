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

#include "ClassHall.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "GameObject.h"
#include "GarrisonMgr.h"
#include "Log.h"
#include "MapManager.h"
#include "ObjectMgr.h"

ClassHall::ClassHall(Player* owner) : Garrison(owner)
{
    _garrisonType = GARRISON_TYPE_CLASS_HALL;
}

bool ClassHall::LoadFromDB()
{
    if (!Garrison::LoadFromDB())
        return false;

    return true;
}

void ClassHall::SaveToDB(CharacterDatabaseTransaction& trans)
{
    Garrison::SaveToDB(trans);
}

bool ClassHall::Create(uint32 garrSiteId)
{
    if (!Garrison::Create(garrSiteId))
        return false;

    return true;
}

void ClassHall::Delete()
{
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    DeleteFromDB(trans);
    CharacterDatabase.CommitTransaction(trans);

    Garrison::Delete();
}

bool ClassHall::IsAllowedArea(uint32 areaID) const
{
    AreaTableEntry const* area = sAreaTableStore.LookupEntry(areaID);

    if(!area)
        return false;

    // TODO : Find a better way to handle this
    return area->Flags[1] & AREA_FLAG_GARRISON && (area->ID >= 7638 && area->ID <= 8023);
}
