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

#ifndef ClassHall_h__
#define ClassHall_h__

#include "Player.h"
#include "Garrison.h"

class GameObject;
class Map;

class TC_GAME_API ClassHall : public Garrison
{
public:

    explicit ClassHall(Player* owner);
    ~ClassHall() {};

    bool LoadFromDB() override;
    void SaveToDB(CharacterDatabaseTransaction& trans) override;

    bool Create(uint32 garrSiteId) override;
    void Delete() override;

    bool IsAllowedArea(uint32 areaID) const override;
};

#endif // ClassHall_h__
