/****************************************************************************
 * Copyright (C) 2010
 * by Dimok
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any
 * damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any
 * purpose, including commercial applications, and to alter it and
 * redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you
 * must not claim that you wrote the original software. If you use
 * this software in a product, an acknowledgment in the product
 * documentation would be appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and
 * must not be misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source
 * distribution.
 ***************************************************************************/
#include <algorithm>
#include <string>
#include <malloc.h>

#include "gui/gui_searchbar.h"
#include "usbloader/wbfs.h"
#include "gamecube/gcgames.h"
#include "settings/newtitles.h"
#include "settings/csettings.h"
#include "settings/cgamesettings.h"
#include "settings/cgamestatistics.h"
#include "settings/gametitles.h"
#include "settings/cgamecategories.hpp"
#include "freetypegx.h"
#include "gamelist.h"
#include "memory/memory.h"
#include "channels/channels.h"
#include "cache/cache.hpp"

enum
{
	DISABLED,
	ENABLED,
	HIDEFORBIDDEN
};

GameList gameList;

void GameList::clear()
{
	GameFilter.clear();
	FullGameList.clear();
	GamePartitionList.clear();
	FilteredList.clear();
	EnabledGameList.clear();
	//! Clear memory of the vector completely
	std::vector<struct discHdr *>().swap(FilteredList);
	std::vector<struct discHdr *>().swap(EnabledGameList);
	std::vector<struct discHdr>().swap(FullGameList);
	std::vector<int>().swap(GamePartitionList);
}

struct discHdr *GameList::GetDiscHeader(const char *gameID) const
{
	if (!gameID)
		return NULL;

	for (u32 i = 0; i < FilteredList.size(); ++i)
	{
		if (strncasecmp(gameID, (const char *)FilteredList[i]->id, 6) == 0)
			return FilteredList[i];
	}

	return NULL;
}

int GameList::GetPartitionNumber(const u8 *gameID) const
{
	if (!gameID)
		return -1;

	for (u32 i = 0; i < FullGameList.size(); ++i)
	{
		if (strncasecmp((const char *)gameID, (const char *)FullGameList[i].id, 6) == 0)
			return GamePartitionList[i];
	}

	return -1;
}

void GameList::RemovePartition(int part)
{
	for (u32 i = 0; i < GamePartitionList.size(); ++i)
	{
		if (GamePartitionList[i] == part)
		{
			FullGameList.erase(FullGameList.begin() + i);
			GamePartitionList.erase(GamePartitionList.begin() + i);
			--i;
		}
	}

	if (FullGameList.size() > 0)
		FilterList();
}

int GameList::InternalReadList(int part)
{
	// Retrieve all stuff from WBFS
	u32 cnt = 0;

	int ret = WBFS_GetCount(part, &cnt);
	if (ret < 0)
		return -1;

	// We are done here if no games are there
	if (cnt == 0)
		return 0;

	// Sort the titles by game ID for faster searching
	GameTitles.SortTitleList();

	/* Buffer length */
	u32 len = sizeof(struct discHdr) * cnt;

	/* Allocate memory */
	struct discHdr *buffer = (struct discHdr *)allocate_memory(len);
	if (!buffer)
		return -1;

	/* Clear buffer */
	memset(buffer, 0, len);

	/* Get header list */
	ret = WBFS_GetHeaders(part, buffer, cnt, sizeof(struct discHdr));
	if (ret < 0)
	{
		free(buffer);
		return -1;
	}

	u32 oldSize = FullGameList.size();
	std::vector<struct discHdr> PartGameList(cnt);
	memcpy(&PartGameList[0], buffer, len);
	free(buffer);

	for (u32 i = 0; i < PartGameList.size(); i++)
	{
		for (u32 j = 0; j < FullGameList.size(); j++)
		{
			if (strncasecmp((const char *)PartGameList[i].id, (const char *)FullGameList[j].id, 6) == 0)
			{
				PartGameList.erase(PartGameList.begin() + i);
				--i;
				break;
			}
		}
	}
	FullGameList.resize(oldSize + PartGameList.size());
	memcpy(&FullGameList[oldSize], &PartGameList[0], PartGameList.size() * sizeof(struct discHdr));

	GamePartitionList.resize(oldSize + PartGameList.size());

	for (u32 i = oldSize; i < GamePartitionList.size(); ++i)
		GamePartitionList[i] = part;

	return PartGameList.size();
}

int GameList::ReadGameList(bool use_cache)
{
	if (use_cache && Settings.CacheTitles && isCacheFile(WII_HEADER_CACHE_FILE))
	{
		if (FullGameList.empty() && GamePartitionList.empty())
			LoadGameHeaderCache(FullGameList, GamePartitionList);
		if (!FullGameList.empty())
			return FullGameList.size();
	}
	// Clear list
	FullGameList.clear();
	GamePartitionList.clear();
	//! Clear memory of the vector completely
	std::vector<struct discHdr>().swap(FullGameList);
	std::vector<int>().swap(GamePartitionList);

	int cnt = 0;

	if (Settings.SDMode || !Settings.MultiplePartitions)
	{
		cnt = InternalReadList(Settings.partition);
	}
	else
	{
		int partitions = DeviceHandler::GetUSBPartitionCount();

		for (int part = 0; part < partitions; ++part)
		{
			int ret = InternalReadList(part);
			if (ret > 0)
				cnt += ret;
		}
	}

	if (Settings.CacheTitles)
		SaveGameHeaderCache(FullGameList, GamePartitionList);
	return cnt;
}

void GameList::InternalFilterList(std::vector<struct discHdr *> &FullList)
{
	for (u32 i = 0; i < FullList.size(); ++i)
	{
		struct discHdr *header = FullList[i];

		/* Register game */
		NewTitles::Instance()->CheckGame(header->id);

		/* Filters */
		if (Settings.GameSort & SORT_FAVORITE)
		{
			GameStatus *GameStats = GameStatistics.GetGameStatus(header->id);
			if (Settings.marknewtitles)
			{
				bool isNew = NewTitles::Instance()->IsNew(header->id);
				if (!isNew && (!GameStats || GameStats->FavoriteRank == 0))
					continue;
			}
			else
			{
				if (!GameStats || GameStats->FavoriteRank == 0)
					continue;
			}
		}

		// Ignore uLoader cfg "iso".  i was told it is "__CFG_"  but not confirmed
		if (strncasecmp((char *)header->id, "__CFG_", 6) == 0)
			continue;

		GameCFG *GameConfig = GameSettings.GetGameCFG(header);

		/* Rating based parental control method */
		if (Settings.parentalcontrol != PARENTAL_LVL_ADULT && !Settings.godmode)
		{
			if (GameConfig && GameConfig->parentalcontrol > Settings.parentalcontrol)
				continue;

			// Check game rating in GameTDB, since the default Wii parental control setting is enabled
			int rating = GameTitles.GetParentalRating((char *)header->id);
			if (rating > Settings.parentalcontrol)
				continue;
		}

		//! Per game lock method
		if (!Settings.godmode && GameConfig && GameConfig->Locked)
			continue;

		//! Category filter
		u32 n;
		int allType = DISABLED;
		// Verify the display mode for category "All"
		for (n = 0; n < Settings.EnabledCategories.size(); ++n)
		{
			if (Settings.EnabledCategories[n] == 0)
			{
				allType = ENABLED; // All = Enabled
				break;
			}
		}
		for (n = 0; n < Settings.ForbiddenCategories.size(); ++n)
		{
			if (Settings.ForbiddenCategories[n] == 0)
			{
				allType = HIDEFORBIDDEN; // All = Enabled but hide Forbidden categories
				break;
			}
		}

		if (allType == DISABLED)
		{
			// Remove TitleID if it contains a forbidden categories
			for (n = 0; n < Settings.ForbiddenCategories.size(); ++n)
			{
				if (GameCategories.isInCategory((char *)header->id, Settings.ForbiddenCategories[n]))
					break;
			}
			if (n < Settings.ForbiddenCategories.size())
				continue;

			// Remove TitleID is it doesn't contain a required categories
			for (n = 0; n < Settings.RequiredCategories.size(); ++n)
			{
				if (!GameCategories.isInCategory((char *)header->id, Settings.RequiredCategories[n]))
					break;
			}
			if (n < Settings.RequiredCategories.size())
				continue;

			// If there's no required categories, verify if the TitleID should be kept or removed
			if (Settings.RequiredCategories.size() == 0)
			{
				for (n = 0; n < Settings.EnabledCategories.size(); ++n)
				{
					if (GameCategories.isInCategory((char *)header->id, Settings.EnabledCategories[n]))
						break;
				}
				if (n == Settings.EnabledCategories.size())
					continue;
			}
		}

		if (allType == HIDEFORBIDDEN)
		{
			// Remove TitleID if it contains a forbidden categories
			for (n = 0; n < Settings.ForbiddenCategories.size(); ++n)
			{
				if (GameCategories.isInCategory((char *)header->id, Settings.ForbiddenCategories[n]))
					if (Settings.ForbiddenCategories[n] > 0)
						break;
			}
			if (n < Settings.ForbiddenCategories.size())
				continue;
		}

		FilteredList.push_back(header);
	}
}

int GameList::FilterList(const wchar_t *gameFilter)
{
	if (gameFilter)
		GameFilter.assign(gameFilter);

	FilteredList.clear();

	EnabledList();
	InternalFilterList(EnabledGameList);

	NewTitles::Instance()->Save();
	GuiSearchBar::FilterList(FilteredList, GameFilter);

	SortList();

	return FilteredList.size();
}

void GameList::InternalLoadUnfiltered(std::vector<struct discHdr *> &FullList)
{
	for (u32 i = 0; i < FullList.size(); ++i)
	{
		struct discHdr *header = FullList[i];

		/* Register game */
		NewTitles::Instance()->CheckGame(header->id);

		FilteredList.push_back(header);
	}
}

int GameList::LoadUnfiltered()
{
	GameFilter.clear();
	FilteredList.clear();

	EnabledList();
	InternalLoadUnfiltered(EnabledGameList);

	NewTitles::Instance()->Save();
	GuiSearchBar::FilterList(FilteredList, GameFilter);

	SortList();

	return FilteredList.size();
}

int GameList::EnabledList()
{
	EnabledGameList.clear();

	if (Settings.TitlesType == TITLETYPE_FROMWIITDB)
		GameTitles.LoadTitlesFromGameTDB(Settings.titlestxt_path);
	
	GetGameListHeaders(EnabledGameList, Settings.LoaderMode);

	return EnabledGameList.size();
}

void GameList::InternalGetGameListHeaders(std::vector<struct discHdr *> &tmplist, std::vector<struct discHdr> &FullList)
{
	for (u32 i = 0; i < FullList.size(); ++i)
	{
		struct discHdr *header = &FullList[i];
		tmplist.push_back(header);
	}
}

int GameList::GetGameListHeaders(std::vector<struct discHdr *> &tmplist, short LoaderMode)
{
	// Filter current game list if selected
	if (LoaderMode & MODE_WIIGAMES)
	{
		if (FullGameList.size() == 0)
			ReadGameList(true);
		InternalGetGameListHeaders(tmplist, FullGameList);
	}

	// Filter GC game list if selected
	if (LoaderMode & MODE_GCGAMES)
		InternalGetGameListHeaders(tmplist, GCGames::Instance()->GetHeaders());

	// Filter NAND channel list if selected
	if (LoaderMode & MODE_NANDCHANNELS)
		InternalGetGameListHeaders(tmplist, Channels::Instance()->GetNandHeaders());

	// Filter EmuNAND channel list if selected
	if (LoaderMode & MODE_EMUCHANNELS)
		InternalGetGameListHeaders(tmplist, Channels::Instance()->GetEmuHeaders());

	return tmplist.size();
}

void GameList::SortList()
{
	if (FilteredList.size() < 2)
		return;

	if (Settings.GameSort & SORT_PLAYCOUNT)
	{
		std::sort(FilteredList.begin(), FilteredList.end(), PlaycountSortCallback);
	}
	else if (Settings.GameSort & SORT_RANKING)
	{
		std::sort(FilteredList.begin(), FilteredList.end(), RankingSortCallback);
	}
	else if (Settings.GameSort & SORT_PLAYERS)
	{
		std::sort(FilteredList.begin(), FilteredList.end(), PlayersSortCallback);
	}
	else
	{
		std::sort(FilteredList.begin(), FilteredList.end(), NameSortCallback);
	}
}

bool GameList::NameSortCallback(const struct discHdr *a, const struct discHdr *b)
{
	return (strcasecmp(GameTitles.GetTitle((struct discHdr *)a), GameTitles.GetTitle((struct discHdr *)b)) < 0);
}

bool GameList::PlaycountSortCallback(const struct discHdr *a, const struct discHdr *b)
{
	int count1 = GameStatistics.GetPlayCount(a->id);
	int count2 = GameStatistics.GetPlayCount(b->id);

	if (count1 == count2)
		return NameSortCallback(a, b);

	return (count1 > count2);
}

bool GameList::RankingSortCallback(const struct discHdr *a, const struct discHdr *b)
{
	int fav1 = GameStatistics.GetFavoriteRank(a->id);
	int fav2 = GameStatistics.GetFavoriteRank(b->id);

	if (fav1 == fav2)
		return NameSortCallback(a, b);

	return (fav1 > fav2);
}

bool GameList::PlayersSortCallback(const struct discHdr *a, const struct discHdr *b)
{
	int count1 = GameTitles.GetPlayersCount((const char *)a->id);
	int count2 = GameTitles.GetPlayersCount((const char *)b->id);

	if (count1 == count2)
		return NameSortCallback(a, b);

	return (count1 > count2);
}
