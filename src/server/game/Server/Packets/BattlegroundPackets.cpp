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

#include "BattlegroundPackets.h"

WorldPacket const* WorldPackets::Battleground::SeasonInfo::Write()
{
    _worldPacket << int32(MythicPlusSeasonID);
    _worldPacket << int32(CurrentSeason);
    _worldPacket << int32(PreviousSeason);
    _worldPacket << int32(PvpSeasonID);

    return &_worldPacket;
}

void WorldPackets::Battleground::AreaSpiritHealerQuery::Read()
{
    _worldPacket >> HealerGuid;
}

void WorldPackets::Battleground::AreaSpiritHealerQueue::Read()
{
    _worldPacket >> HealerGuid;
}

WorldPacket const* WorldPackets::Battleground::AreaSpiritHealerTime::Write()
{
    _worldPacket << HealerGuid;
    _worldPacket << int32(TimeLeft);

    return &_worldPacket;
}

ByteBuffer& operator<<(ByteBuffer& data, WorldPackets::Battleground::PVPMatchStatistics::RatingData const& ratingData)
{
    for (std::size_t i = 0; i < 2; ++i)
    {
        data << int32(ratingData.Prematch[i]);
        data << int32(ratingData.Postmatch[i]);
        data << int32(ratingData.PrematchMMR[i]);
    }
    return data;
}

ByteBuffer& operator<<(ByteBuffer& data, WorldPackets::Battleground::PVPMatchStatistics::HonorData const& honorData)
{
    data << uint32(honorData.HonorKills);
    data << uint32(honorData.Deaths);
    data << uint32(honorData.ContributionPoints);
    return data;
}

ByteBuffer& operator<<(ByteBuffer& data, WorldPackets::Battleground::PVPMatchStatistics::PVPMatchPlayerPVPStat const& pvpStat)
{
    data << int32(pvpStat.PvpStatID);
    data << int32(pvpStat.PvpStatValue);
    return data;
}

ByteBuffer& operator<<(ByteBuffer& data, WorldPackets::Battleground::PVPMatchStatistics::PVPMatchPlayerStatistics const& playerData)
{
    data << playerData.PlayerGUID;
    data << uint32(playerData.Kills);
    data << uint32(playerData.DamageDone);
    data << uint32(playerData.HealingDone);
    data << uint32(playerData.Stats.size());
    data << int32(playerData.PrimaryTalentTree);
    data << int32(playerData.Sex);
    data << int32(playerData.Race);
    data << int32(playerData.Class);
    data << int32(playerData.CreatureID);
    data << int32(playerData.HonorLevel);
    for (WorldPackets::Battleground::PVPMatchStatistics::PVPMatchPlayerPVPStat const& pvpStat : playerData.Stats)
        data << pvpStat;

    data.WriteBit(playerData.Faction != 0);
    data.WriteBit(playerData.IsInWorld);
    data.WriteBit(playerData.Honor.is_initialized());
    data.WriteBit(playerData.PreMatchRating.is_initialized());
    data.WriteBit(playerData.RatingChange.is_initialized());
    data.WriteBit(playerData.PreMatchMMR.is_initialized());
    data.WriteBit(playerData.MmrChange.is_initialized());
    data.FlushBits();

    if (playerData.Honor)
        data << *playerData.Honor;

    if (playerData.PreMatchRating)
        data << uint32(*playerData.PreMatchRating);

    if (playerData.RatingChange)
        data << int32(*playerData.RatingChange);

    if (playerData.PreMatchMMR)
        data << uint32(*playerData.PreMatchMMR);

    if (playerData.MmrChange)
        data << int32(*playerData.MmrChange);

    return data;
}

WorldPacket const* WorldPackets::Battleground::PVPMatchStatistics::Write()
{
    _worldPacket.reserve(Statistics.size() * sizeof(PVPMatchPlayerStatistics) + sizeof(PVPMatchStatistics));

    _worldPacket.WriteBit(Ratings.is_initialized());
    _worldPacket << uint32(Statistics.size());
    _worldPacket.append(PlayerCount.data(), PlayerCount.size());

    if (Ratings.is_initialized())
        _worldPacket << *Ratings;

    for (PVPMatchPlayerStatistics const& player : Statistics)
        _worldPacket << player;

    return &_worldPacket;
}

void WorldPackets::Battleground::BattlemasterJoin::Read()
{
    QueueIDs.resize(_worldPacket.read<uint32>());
    _worldPacket >> Roles;
    _worldPacket >> BlacklistMap[0] >> BlacklistMap[1];
    for (uint64& queueId : QueueIDs)
        _worldPacket >> queueId;
}

void WorldPackets::Battleground::BattlemasterJoinArena::Read()
{
    _worldPacket >> TeamSizeIndex;
    _worldPacket >> Roles;
}

void WorldPackets::Battleground::BattlemasterJoinArenaSkirmish::Read()
{
    _worldPacket.clear();
}

ByteBuffer& operator<<(ByteBuffer& data, WorldPackets::Battleground::BattlefieldStatusHeader const& header)
{
    data << header.Ticket;
    data << uint32(header.QueueID.size());
    data << uint8(header.RangeMin);
    data << uint8(header.RangeMax);
    data << uint8(header.TeamSize);
    data << uint32(header.InstanceID);
    for (uint64 queueID : header.QueueID)
        data << uint64(queueID);
    data.WriteBit(header.RegisteredMatch);
    data.WriteBit(header.TournamentRules);
    data.FlushBits();
    return data;
}

WorldPacket const* WorldPackets::Battleground::BattlefieldStatusNone::Write()
{
    _worldPacket << Ticket;
    return &_worldPacket;
}

WorldPacket const* WorldPackets::Battleground::BattlefieldStatusNeedConfirmation::Write()
{
    _worldPacket << Hdr;
    _worldPacket << uint32(Mapid);
    _worldPacket << uint32(Timeout);
    _worldPacket << uint8(Role);
    return &_worldPacket;
}

WorldPacket const* WorldPackets::Battleground::BattlefieldStatusActive::Write()
{
    _worldPacket << Hdr;
    _worldPacket << uint32(Mapid);
    _worldPacket << uint32(ShutdownTimer);
    _worldPacket << uint32(StartTimer);
    _worldPacket.WriteBit(ArenaFaction != 0);
    _worldPacket.WriteBit(LeftEarly);
    _worldPacket.FlushBits();
    return &_worldPacket;
}

WorldPacket const* WorldPackets::Battleground::BattlefieldStatusQueued::Write()
{
    _worldPacket << Hdr;
    _worldPacket << uint32(AverageWaitTime);
    _worldPacket << uint32(WaitTime);
    _worldPacket.WriteBit(AsGroup);
    _worldPacket.WriteBit(EligibleForMatchmaking);
    _worldPacket.WriteBit(SuspendedQueue);
    _worldPacket.FlushBits();
    return &_worldPacket;
}

WorldPacket const* WorldPackets::Battleground::BattlefieldStatusFailed::Write()
{
    _worldPacket << Ticket;
    _worldPacket << uint64(QueueID);
    _worldPacket << uint32(Reason);
    _worldPacket << ClientID;
    return &_worldPacket;
}

void WorldPackets::Battleground::BattlefieldPort::Read()
{
    _worldPacket >> Ticket;
    AcceptedInvite = _worldPacket.ReadBit();
}

void WorldPackets::Battleground::BattlefieldListRequest::Read()
{
    _worldPacket >> ListID;
}

WorldPacket const* WorldPackets::Battleground::BattlefieldList::Write()
{
    _worldPacket << BattlemasterGuid;
    _worldPacket << int32(BattlemasterListID);
    _worldPacket << uint8(MinLevel);
    _worldPacket << uint8(MaxLevel);
    _worldPacket << uint32(Battlefields.size());
    if (!Battlefields.empty())
        _worldPacket.append(Battlefields.data(), Battlefields.size());

    _worldPacket.WriteBit(PvpAnywhere);
    _worldPacket.WriteBit(HasRandomWinToday);
    _worldPacket.FlushBits();
    return &_worldPacket;
}

WorldPacket const* WorldPackets::Battleground::PVPOptionsEnabled::Write()
{
    _worldPacket.WriteBit(RatedBattlegrounds);
    _worldPacket.WriteBit(PugBattlegrounds);
    _worldPacket.WriteBit(WargameBattlegrounds);
    _worldPacket.WriteBit(WargameArenas);
    _worldPacket.WriteBit(RatedArenas);
    _worldPacket.WriteBit(ArenaSkirmish);
    _worldPacket.FlushBits();
    return &_worldPacket;
}

void WorldPackets::Battleground::ReportPvPPlayerAFK::Read()
{
    _worldPacket >> Offender;
}

WorldPacket const* WorldPackets::Battleground::ReportPvPPlayerAFKResult::Write()
{
    _worldPacket << Offender;
    _worldPacket << uint8(Result);
    _worldPacket << uint8(NumBlackMarksOnOffender);
    _worldPacket << uint8(NumPlayersIHaveReported);
    return &_worldPacket;
}

ByteBuffer& operator<<(ByteBuffer& data, WorldPackets::Battleground::BattlegroundPlayerPosition const& playerPosition)
{
    data << playerPosition.Guid;
    data << playerPosition.Pos;
    data << int8(playerPosition.IconID);
    data << int8(playerPosition.ArenaSlot);
    return data;
}

WorldPacket const* WorldPackets::Battleground::BattlegroundPlayerPositions::Write()
{
    _worldPacket << uint32(FlagCarriers.size());
    for (BattlegroundPlayerPosition const& pos : FlagCarriers)
        _worldPacket << pos;

    return &_worldPacket;
}

WorldPacket const* WorldPackets::Battleground::BattlegroundPlayerJoined::Write()
{
    _worldPacket << Guid;
    return &_worldPacket;
}

WorldPacket const* WorldPackets::Battleground::BattlegroundPlayerLeft::Write()
{
    _worldPacket << Guid;
    return &_worldPacket;
}

WorldPacket const* WorldPackets::Battleground::DestroyArenaUnit::Write()
{
    _worldPacket << Guid;
    return &_worldPacket;
}

WorldPacket const* WorldPackets::Battleground::RatedPvpInfo::Write()
{
    for (RatedInfo& info : Infos)
    {
        _worldPacket << uint32(info.ArenaPersonalRating);
        _worldPacket << uint32(info.ArenaMatchMakerRating);
        _worldPacket << uint32(info.SeasonGames);
        _worldPacket << uint32(info.SeasonWins);
        _worldPacket << uint32(info.PrevWeekGames);
        _worldPacket << uint32(info.PrevWeekWins);
        _worldPacket << uint32(info.WeekGames);
        _worldPacket << uint32(info.WeekWins);
        _worldPacket << uint32(info.BestRatingOfWeek);
        _worldPacket << uint32(info.ProjectedConquestCap);
        _worldPacket << uint32(info.BestRatingOfSeason);
        _worldPacket << uint32(info.Ranking);
        _worldPacket << uint32(info.WeekWins);
    }

    return &_worldPacket;
}

WorldPacket const* WorldPackets::Battleground::ConquestFormulaConstants::Write()
{
    _worldPacket << uint32(PvpMinCPPerWeek);
    _worldPacket << uint32(PvpMaxCPPerWeek);
    _worldPacket << uint32(PvpCPBaseCoefficient);
    _worldPacket << float(PvpCPExpCoefficient);
    _worldPacket << float(PvpCPNumerator);

    return &_worldPacket;
}

WorldPacket const* WorldPackets::Battleground::SendPvpBrawlInfo::Write()
{
    _worldPacket << uint32(BrawlType);
    _worldPacket << int32(TimeToEnd);
    _worldPacket.FlushBits();
    _worldPacket.WriteBit(IsActive);

    return &_worldPacket;
}

void WorldPackets::Battleground::StartWargame::Read()
{
    _worldPacket >> OpposingPartyMember;
    _worldPacket >> OpposingPartyMemberVirtualRealmAddress;
    _worldPacket >> UnkShort;
    _worldPacket >> QueueID;
    TournamentRules = _worldPacket.ReadBit();
}

WorldPacket const* WorldPackets::Battleground::CheckWargameEntry::Write()
{
    _worldPacket << OpposingPartyMember;
    _worldPacket << RealmID;
    _worldPacket << UnkShort;
    _worldPacket << OpposingPartyUserServer;
    _worldPacket << OpposingPartyBnetAccountID;
    _worldPacket << QueueID;
    _worldPacket << TimeoutSeconds;
    _worldPacket << TournamentRules;

    return &_worldPacket;
}

WorldPacket const* WorldPackets::Battleground::WargameRequestSuccessfullySentToOpponent::Write()
{
    _worldPacket << UnkInt;
    _worldPacket.WriteBit(UnkInt2.is_initialized());
    _worldPacket.WriteBit(UnkInt3.is_initialized());
    _worldPacket.FlushBits();

    if (UnkInt2.is_initialized())
        _worldPacket << *UnkInt2;

    if (UnkInt3.is_initialized())
        _worldPacket << *UnkInt3;

    return &_worldPacket;
}

WorldPacket const* WorldPackets::Battleground::BattlegroundInit::Write()
{
    _worldPacket << ServerTime;
    _worldPacket << MaxPoints;

    return &_worldPacket;
}

WorldPacket const* WorldPackets::Battleground::BattlegroundPoints::Write()
{
    _worldPacket << BgPoints;
    _worldPacket.WriteBit(Team);
    _worldPacket.FlushBits();

    return &_worldPacket;
}
