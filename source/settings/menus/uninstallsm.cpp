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
#include <unistd.h>

#include "uninstallsm.hpp"
#include "fileoperations/fileops.h"
#include "gamecube/gcgames.h"
#include "channels/channels.h"
#include "settings/csettings.h"
#include "settings/cgamesettings.h"
#include "settings/cgamestatistics.h"
#include "settings/gametitles.h"
#include "prompts/promptwindows.h"
#include "language/gettext.h"
#include "usbloader/wbfs.h"
#include "usbloader/gamelist.h"
#include "wstring.hpp"
#include "cache/cache.hpp"

UninstallSM::UninstallSM(struct discHdr * header)
	: SettingsMenu(tr("Uninstall"), &GuiOptions, MENU_NONE)
{
	DiscHeader = header;

	int Idx = 0;

	if(	  DiscHeader->type == TYPE_GAME_WII_IMG
	   || DiscHeader->type == TYPE_GAME_GC_IMG
	   || DiscHeader->type == TYPE_GAME_EMUNANDCHAN)
	{
		Options->SetName(Idx++, "%s", tr("Uninstall game disc" ));
	}
	Options->SetName(Idx++, "%s", tr("Reset play count" ));
	Options->SetName(Idx++, "%s", tr("Delete cover artwork" ));
	Options->SetName(Idx++, "%s", tr("Delete disc artwork" ));
	Options->SetName(Idx++, "%s", tr("Delete cached banner" ));
	Options->SetName(Idx++, "%s", tr("Delete TXT cheat file" ));
	Options->SetName(Idx++, "%s", tr("Delete GCT cheat file" ));

	SetOptionValues();
}

void UninstallSM::SetOptionValues()
{
	int Idx = 0;

	if(	  DiscHeader->type == TYPE_GAME_WII_IMG
	   || DiscHeader->type == TYPE_GAME_GC_IMG
	   || DiscHeader->type == TYPE_GAME_EMUNANDCHAN)
	{
		//! Settings: Uninstall Game
		Options->SetValue(Idx++, " ");
	}

	//! Settings: Reset Play Count
	Options->SetValue(Idx++, " ");

	//! Settings: Delete Cover Artwork
	Options->SetValue(Idx++, " ");

	//! Settings: Delete Disc Artwork
	Options->SetValue(Idx++, " ");

	//! Settings: Delete Cached Banner
	Options->SetValue(Idx++, " ");

	//! Settings: Delete Cheat TXT
	Options->SetValue(Idx++, " ");

	//! Settings: Delete Cheat GCT
	Options->SetValue(Idx++, " ");
}

int UninstallSM::GetMenuInternal()
{
	int ret = optionBrowser->GetClickedOption();

	if (ret < 0)
		return MENU_NONE;

	int Idx = -1;

	//! Settings: Uninstall Game
	if(	  (DiscHeader->type == TYPE_GAME_WII_IMG
	   ||  DiscHeader->type == TYPE_GAME_GC_IMG
	   ||  DiscHeader->type == TYPE_GAME_EMUNANDCHAN)
	   && ret == ++Idx)
	{
		int choice = WindowPrompt(GameTitles.GetTitle(DiscHeader), tr("What to delete for this game?"), tr("Game only"), tr("Uninstall all"), tr("Cancel"));
		if (choice == 0)
			return MENU_NONE;

		char GameID[7];
		snprintf(GameID, sizeof(GameID), "%s", (char *) DiscHeader->id);

		std::string Title(GameTitles.GetTitle(DiscHeader));
		GameSettings.Remove(DiscHeader->id);
		GameSettings.Save();
		GameStatistics.Remove(DiscHeader->id);
		GameStatistics.Save();

		int ret = 0;
		char filepath[512];

		if(DiscHeader->type == TYPE_GAME_WII_IMG)
		{
			ret = WBFS_RemoveGame((u8 *) GameID);
			if(ret >= 0)
			{
				wString oldFilter(gameList.GetCurrentFilter());
				gameList.ReadGameList();
				gameList.FilterList(oldFilter.c_str());
				isCacheCurrent();
			}
		}
		else if(DiscHeader->type == TYPE_GAME_GC_IMG)
		{
			GCGames::Instance()->RemoveGame(GameID);
			// Reload list
			GCGames::Instance()->LoadAllGames();
			isCacheCurrent();
		}
		else if(DiscHeader->type == TYPE_GAME_EMUNANDCHAN && DiscHeader->tid != 0)
		{
			// Remove ticket
			snprintf(filepath, sizeof(filepath), "%s/ticket/%08x/%08x.tik", Settings.NandEmuChanPath, (unsigned int) (DiscHeader->tid >> 32), (unsigned int) DiscHeader->tid);
			RemoveFile(filepath);

			// Remove contents / data
			snprintf(filepath, sizeof(filepath), "%s/title/%08x/%08x/", Settings.NandEmuChanPath, (unsigned int) (DiscHeader->tid >> 32), (unsigned int) DiscHeader->tid);
			RemoveDirectory(filepath);

			Channels::Instance()->GetEmuChannelList();
			isCacheCurrent();
		}

		if(choice == 2)
		{
			snprintf(filepath, sizeof(filepath), "%s%s.png", Settings.covers_path, GameID);
			if (CheckFile(filepath)) remove(filepath);
			snprintf(filepath, sizeof(filepath), "%s%s.png", Settings.covers2d_path, GameID);
			if (CheckFile(filepath)) remove(filepath);
			snprintf(filepath, sizeof(filepath), "%s%s.png", Settings.coversFull_path, GameID);
			if (CheckFile(filepath)) remove(filepath);
			snprintf(filepath, sizeof(filepath), "%s%s.png", Settings.disc_path, GameID);
			if (CheckFile(filepath)) remove(filepath);
			snprintf(filepath, sizeof(filepath), "%s%s.bnr", Settings.BNRCachePath, GameID);
			if (CheckFile(filepath)) remove(filepath);
			snprintf(filepath, sizeof(filepath), "%s%.4s.bnr", Settings.BNRCachePath, GameID);
			if (CheckFile(filepath)) remove(filepath);
			snprintf(filepath, sizeof(filepath), "%s%.3s.bnr", Settings.BNRCachePath, GameID);
			if (CheckFile(filepath)) remove(filepath);
			snprintf(filepath, sizeof(filepath), "%s%s.txt", Settings.TxtCheatcodespath, GameID);
			if (CheckFile(filepath)) remove(filepath);
			snprintf(filepath, sizeof(filepath), "%s%s.gct", Settings.Cheatcodespath, GameID);
			if (CheckFile(filepath)) remove(filepath);
		}

		if (ret < 0)
			WindowPrompt(tr("Error"), Title.c_str(), tr("could not be deleted"), tr("OK"));
		else
			WindowPrompt(tr("Delete Game"), Title.c_str(), tr("deleted successfully"), tr("OK"));

		return MENU_DISCLIST;
	}

	//! Settings: Reset Play Count
	else if (ret == ++Idx)
	{
		int result = WindowPrompt(tr("Reset Settings"), tr("Reset game play count?"), tr("Yes"), tr("Cancel"));
		if (result == 1)
		{
			GameStatistics.SetPlayCount(DiscHeader->id, 0);
			GameStatistics.Save();
		}
	}

	//! Settings: Delete Cover Artwork
	else if (ret == ++Idx)
	{
		int choice = WindowPrompt(tr("Delete Files"), tr("Delete game cover artwork?"), tr("Yes"), tr("No"));
		if (choice != 1)
			return MENU_NONE;

		char GameID[7];
		snprintf(GameID, sizeof(GameID), "%s", (char *) DiscHeader->id);
		char filepath[200];
		snprintf(filepath, sizeof(filepath), "%s%s.png", Settings.covers_path, GameID);
		if (CheckFile(filepath)) remove(filepath);
		snprintf(filepath, sizeof(filepath), "%s%s.png", Settings.covers2d_path, GameID);
		if (CheckFile(filepath)) remove(filepath);
		snprintf(filepath, sizeof(filepath), "%s%s.png", Settings.coversFull_path, GameID);
		if (CheckFile(filepath)) remove(filepath);
	}

	//! Settings: Delete Disc Artwork
	else if (ret == ++Idx)
	{
		char GameID[7];
		snprintf(GameID, sizeof(GameID), "%s", (char *) DiscHeader->id);
		char filepath[200];
		snprintf(filepath, sizeof(filepath), "%s%s.png", Settings.disc_path, GameID);

		int choice = WindowPrompt(tr("Delete Files"), tr("Delete"), filepath, tr("Yes"), tr("No"));
		if (choice == 1)
			if (CheckFile(filepath)) remove(filepath);
	}

	//! Settings: Delete Cached Banner
	else if (ret == ++Idx)
	{
		int choice = WindowPrompt(tr("Delete Files"), tr("Delete game cached banner?"), tr("Yes"), tr("No"));
		if (choice != 1)
			return MENU_NONE;

		char GameID[7];
		snprintf(GameID, sizeof(GameID), "%s", (char *) DiscHeader->id);
		char filepath[200];
		snprintf(filepath, sizeof(filepath), "%s%s.bnr", Settings.BNRCachePath, GameID);
		if (CheckFile(filepath)) remove(filepath);
		snprintf(filepath, sizeof(filepath), "%s%.4s.bnr", Settings.BNRCachePath, GameID);
		if (CheckFile(filepath)) remove(filepath);
		snprintf(filepath, sizeof(filepath), "%s%.3s.bnr", Settings.BNRCachePath, GameID);
		if (CheckFile(filepath)) remove(filepath);
	}

	//! Settings: Delete Cheat TXT
	else if (ret == ++Idx)
	{
		char GameID[7];
		snprintf(GameID, sizeof(GameID), "%s", (char *) DiscHeader->id);
		char filepath[200];
		snprintf(filepath, sizeof(filepath), "%s%s.txt", Settings.TxtCheatcodespath, GameID);

		int choice = WindowPrompt(tr("Delete Files"), tr("Delete TXT game cheat file?"), filepath, tr("Yes"), tr("No"));
		if (choice == 1)
			if (CheckFile(filepath)) remove(filepath);
	}

	//! Settings: Delete Cheat GCT
	else if (ret == ++Idx)
	{
		char GameID[7];
		snprintf(GameID, sizeof(GameID), "%s", (char *) DiscHeader->id);
		char filepath[200];
		snprintf(filepath, sizeof(filepath), "%s%s.gct", Settings.Cheatcodespath, GameID);

		int choice = WindowPrompt(tr("Delete Files"), tr("Delete GCT game cheat file?"), filepath, tr("Yes"), tr("No"));
		if (choice == 1)
			if (CheckFile(filepath)) remove(filepath);
	}

	SetOptionValues();

	return MENU_NONE;
}
