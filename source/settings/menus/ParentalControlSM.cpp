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
#include "parentalcontrolsm.hpp"
#include "settings/csettings.h"
#include "prompts/promptwindows.h"
#include "language/gettext.h"
#include "utils/passwordcheck.h"

static const char * OnOffText[MAX_ON_OFF] =
{
	trNOOP( "Off" ),
	trNOOP( "On" )
};

static const char * LockModeText[] =
{
	trNOOP( "Locked" ),
	trNOOP( "Unlocked" )
};

static const char * ParentalText[5] =
{
	trNOOP( "0 (Everyone)" ),
	trNOOP( "1 (Child 7+)" ),
	trNOOP( "2 (Teen 12+)" ),
	trNOOP( "3 (Mature 16+)" ),
	trNOOP( "4 (Adults Only 18+)" )
};

ParentalControlSM::ParentalControlSM()
	: SettingsMenu(tr("Parental Control"), &GuiOptions, MENU_NONE)
{
	int Idx = 0;
	Options->SetName(Idx++, "%s", tr("Lock console" ));
	Options->SetName(Idx++, "%s", tr("Password" ));
	Options->SetName(Idx++, "%s", tr("Control level" ));
	Options->SetName(Idx++, "%s", tr("Remember unlock" ));
	Options->SetName(Idx++, "%s", tr("Lock settings" ));
	Options->SetName(Idx++, "%s", tr("Lock interface settings" ));
	Options->SetName(Idx++, "%s", tr("Lock game loader settings" ));
	Options->SetName(Idx++, "%s", tr("Lock storage settings" ));
	Options->SetName(Idx++, "%s", tr("Lock feature settings" ));
	Options->SetName(Idx++, "%s", tr("Lock parental settings" ));
	Options->SetName(Idx++, "%s", tr("Lock sound settings" ));
	Options->SetName(Idx++, "%s", tr("Lock theme settings" ));
	Options->SetName(Idx++, "%s", tr("Lock custom paths" ));
	Options->SetName(Idx++, "%s", tr("Lock updates" ));
	Options->SetName(Idx++, "%s", tr("Lock reset settings" ));
	Options->SetName(Idx++, "%s", tr("Lock game settings" ));
	Options->SetName(Idx++, "%s", tr("Lock Homebrew Channel" ));
	Options->SetName(Idx++, "%s", tr("Lock title launcher" ));
	Options->SetName(Idx++, "%s", tr("Lock cover downloads" ));
	Options->SetName(Idx++, "%s", tr("Lock game install" ));
	Options->SetName(Idx++, "%s", tr("Lock GameID change" ));
	Options->SetName(Idx++, "%s", tr("Lock categories menu" ));
	Options->SetName(Idx++, "%s", tr("Lock categories edit" ));
	Options->SetName(Idx++, "%s", tr("Lock SD Card reload" ));
	Options->SetName(Idx++, "%s", tr("Lock Priiloader override" ));
	Options->SetName(Idx++, "%s", tr("Lock loader mode action" ));
	Options->SetName(Idx++, "%s", tr("Lock UI layout button" ));

	SetOptionValues();
}

void ParentalControlSM::SetOptionValues()
{
	int Idx = 0;

	// Settings: Console
	Options->SetValue(Idx++, "%s", tr( LockModeText[Settings.godmode] ));

	if(!Settings.godmode)
	{
		for(int i = Idx; i < Options->GetLength(); ++i)
			Options->SetValue(i, "********");

		return;
	}

	// Settings: Password
	if (strcmp(Settings.unlockCode, "") == 0)
		Options->SetValue(Idx++, "%s", tr("Not set" ));
	else
		Options->SetValue(Idx++, Settings.unlockCode);

	// Settings: Controllevel
	Options->SetValue(Idx++, "%s", tr(ParentalText[Settings.parentalcontrol]));

	// Settings: Remember Unlock
	Options->SetValue(Idx++, "%s", tr(OnOffText[Settings.RememberUnlock]));

	// Settings: Block Global Settings
	Options->SetValue(Idx++, "%s", tr(OnOffText[((Settings.ParentalBlocks & BLOCK_GLOBAL_SETTINGS) != 0)]));

	// Settings: Block Gui Settings
	Options->SetValue(Idx++, "%s", tr(OnOffText[((Settings.ParentalBlocks & BLOCK_GUI_SETTINGS) != 0)]));

	// Settings: Block Loader Settings
	Options->SetValue(Idx++, "%s", tr(OnOffText[((Settings.ParentalBlocks & BLOCK_LOADER_SETTINGS) != 0)]));

	// Settings: Block Hard Drive Settings
	Options->SetValue(Idx++, "%s", tr(OnOffText[((Settings.ParentalBlocks & BLOCK_HARD_DRIVE_SETTINGS) != 0)]));

	// Settings: Block Feature Settings
	Options->SetValue(Idx++, "%s", tr(OnOffText[((Settings.ParentalBlocks & BLOCK_FEATURE_SETTINGS) != 0)]));

	// Settings: Block Parental Settings
	Options->SetValue(Idx++, "%s", tr(OnOffText[((Settings.ParentalBlocks & BLOCK_PARENTAL_SETTINGS) != 0)]));

	// Settings: Block Sound Settings
	Options->SetValue(Idx++, "%s", tr(OnOffText[((Settings.ParentalBlocks & BLOCK_SOUND_SETTINGS) != 0)]));

	// Settings: Block Theme Menu
	Options->SetValue(Idx++, "%s", tr(OnOffText[((Settings.ParentalBlocks & BLOCK_THEME_MENU) != 0)]));

	// Settings: Block Custom Paths
	Options->SetValue(Idx++, "%s", tr(OnOffText[((Settings.ParentalBlocks & BLOCK_CUSTOMPATH_SETTINGS) != 0)]));

	// Settings: Block Updates
	Options->SetValue(Idx++, "%s", tr(OnOffText[((Settings.ParentalBlocks & BLOCK_UPDATES) != 0)]));

	// Settings: Block Reset Settings
	Options->SetValue(Idx++, "%s", tr(OnOffText[((Settings.ParentalBlocks & BLOCK_RESET_SETTINGS) != 0)]));

	// Settings: Block Game Settings
	Options->SetValue(Idx++, "%s", tr(OnOffText[((Settings.ParentalBlocks & BLOCK_GAME_SETTINGS) != 0)]));

	// Settings: Block HBC Menu
	Options->SetValue(Idx++, "%s", tr(OnOffText[((Settings.ParentalBlocks & BLOCK_HBC_MENU) != 0)]));

	// Settings: Block Title Launcher
	Options->SetValue(Idx++, "%s", tr(OnOffText[((Settings.ParentalBlocks & BLOCK_TITLE_LAUNCHER_MENU) != 0)]));

	// Settings: Block Cover Downloads
	Options->SetValue(Idx++, "%s", tr(OnOffText[((Settings.ParentalBlocks & BLOCK_COVER_DOWNLOADS) != 0)]));

	// Settings: Block Game Install
	Options->SetValue(Idx++, "%s", tr(OnOffText[((Settings.ParentalBlocks & BLOCK_GAME_INSTALL) != 0)]));

	// Settings: Block GameID Change
	Options->SetValue(Idx++, "%s", tr(OnOffText[((Settings.ParentalBlocks & BLOCK_GAMEID_CHANGE) != 0)]));

	// Settings: Block Categories Menu
	Options->SetValue(Idx++, "%s", tr(OnOffText[((Settings.ParentalBlocks & BLOCK_CATEGORIES_MENU) != 0)]));

	// Settings: Block Categories Modify
	Options->SetValue(Idx++, "%s", tr(OnOffText[((Settings.ParentalBlocks & BLOCK_CATEGORIES_MOD) != 0)]));

	// Settings: Block SD Reload Button
	Options->SetValue(Idx++, "%s", tr(OnOffText[((Settings.ParentalBlocks & BLOCK_SD_RELOAD_BUTTON) != 0)]));

	// Settings: Block Priiloader Override
	Options->SetValue(Idx++, "%s", tr(OnOffText[((Settings.ParentalBlocks & BLOCK_PRIILOADER_OVERRIDE) != 0)]));

	// Settings: Block Loader Mode Button
	Options->SetValue(Idx++, "%s", tr(OnOffText[((Settings.ParentalBlocks & BLOCK_LOADER_MODE_BUTTON) != 0)]));

	// Settings: Block Loader Layout Button
	Options->SetValue(Idx++, "%s", tr(OnOffText[((Settings.ParentalBlocks & BLOCK_LOADER_LAYOUT_BUTTON) != 0)]));
}

int ParentalControlSM::GetMenuInternal()
{
	int ret = optionBrowser->GetClickedOption();

	if (ret < 0)
		return MENU_NONE;

	int Idx = -1;

	// Settings: Console
	if (ret == ++Idx)
	{
		if (!Settings.godmode)
		{
			//password check to unlock Install,Delete and Format
			SetState(STATE_DISABLED);
			int result = PasswordCheck(Settings.unlockCode);
			SetState(STATE_DEFAULT);
			if (result > 0)
			{
				if(result == 1)
					WindowPrompt( tr( "Parental Control"), tr("Password correct. USB Loader GX features are now unlocked"), tr("OK"));
				Settings.godmode = 1;
			}
			else if(result < 0)
				WindowPrompt(tr("Parental Control"), tr("Wrong password. USB Loader GX features are locked"), tr("OK"));
		}
		else
		{
			int choice = WindowPrompt(tr("Parental Control"), tr("Lock console behind parental control password?"), tr("Yes"), tr("No"));
			if (choice == 1)
			{
				WindowPrompt(tr("Parental Control"), tr("Console locked. Enter parental control password to unlock"), tr("OK"));
				Settings.godmode = 0;
			}
		}
	}

	// General permission check for all following
	else if(!Settings.godmode)
	{
		WindowPrompt(tr("Parental Control"), tr("Permission denied. Unlock console to access settings"), tr("OK"));
	}

	// Settings: Password
	else if (ret == ++Idx)
	{
		char entered[20];
		SetState(STATE_DISABLED);
		snprintf(entered, sizeof(entered), Settings.unlockCode);
		int result = OnScreenKeyboard(entered, 20, 0);
		SetState(STATE_DEFAULT);
		if (result == 1)
		{
			snprintf(Settings.unlockCode, sizeof(Settings.unlockCode), entered);
			WindowPrompt(tr("Parental Control"), tr("Parental control password updated successfully"), tr("OK"));
		}
	}

	// Settings: Controllevel
	else if (ret == ++Idx)
	{
		if (++Settings.parentalcontrol >= 5) Settings.parentalcontrol = 0;
	}

	// Settings: Remember Unlock
	else if (ret == ++Idx)
	{
		if (++Settings.RememberUnlock >= MAX_ON_OFF) Settings.RememberUnlock = 0;
	}

	// Settings: Block Global Settings
	else if (ret == ++Idx)
	{
		Settings.ParentalBlocks ^= BLOCK_GLOBAL_SETTINGS;
	}

	// Settings: Block Gui Settings
	else if (ret == ++Idx)
	{
		Settings.ParentalBlocks ^= BLOCK_GUI_SETTINGS;
	}

	// Settings: Block Loader Settings
	else if (ret == ++Idx)
	{
		Settings.ParentalBlocks ^= BLOCK_LOADER_SETTINGS;
	}

	// Settings: Hard Drive Settings
	else if (ret == ++Idx)
	{
		Settings.ParentalBlocks ^= BLOCK_HARD_DRIVE_SETTINGS;
	}

	// Settings: Block Feature Settings
	else if (ret == ++Idx)
	{
		Settings.ParentalBlocks ^= BLOCK_FEATURE_SETTINGS;
	}

	// Settings: Block Parental Settings
	else if (ret == ++Idx)
	{
		Settings.ParentalBlocks ^= BLOCK_PARENTAL_SETTINGS;
	}

	// Settings: Block Sound Settings
	else if (ret == ++Idx)
	{
		Settings.ParentalBlocks ^= BLOCK_SOUND_SETTINGS;
	}

	// Settings: Block Theme Menu
	else if (ret == ++Idx)
	{
		Settings.ParentalBlocks ^= BLOCK_THEME_MENU;
	}

	// Settings: Block Custom Paths
	else if (ret == ++Idx)
	{
		Settings.ParentalBlocks ^= BLOCK_CUSTOMPATH_SETTINGS;
	}

	// Settings: Block Updates
	else if (ret == ++Idx)
	{
		Settings.ParentalBlocks ^= BLOCK_UPDATES;
	}

	// Settings: Block Reset Settings
	else if (ret == ++Idx)
	{
		Settings.ParentalBlocks ^= BLOCK_RESET_SETTINGS;
	}

	// Settings: Block Game Settings
	else if (ret == ++Idx)
	{
		Settings.ParentalBlocks ^= BLOCK_GAME_SETTINGS;
	}

	// Settings: Block HBC Menu
	else if (ret == ++Idx)
	{
		Settings.ParentalBlocks ^= BLOCK_HBC_MENU;
	}

	// Settings: Block Title Launcher
	else if (ret == ++Idx)
	{
		Settings.ParentalBlocks ^= BLOCK_TITLE_LAUNCHER_MENU;
	}

	// Settings: Block Cover Downloads
	else if (ret == ++Idx)
	{
		Settings.ParentalBlocks ^= BLOCK_COVER_DOWNLOADS;
	}

	// Settings: Block Game Install
	else if (ret == ++Idx)
	{
		Settings.ParentalBlocks ^= BLOCK_GAME_INSTALL;
	}

	// Settings: Block GameID Change
	else if (ret == ++Idx)
	{
		Settings.ParentalBlocks ^= BLOCK_GAMEID_CHANGE;
	}

	// Settings: Block Categories Menu
	else if (ret == ++Idx)
	{
		Settings.ParentalBlocks ^= BLOCK_CATEGORIES_MENU;
	}

	// Settings: Block Categories Modify
	else if (ret == ++Idx)
	{
		Settings.ParentalBlocks ^= BLOCK_CATEGORIES_MOD;
	}

	// Settings: Block SD Reload Button
	else if (ret == ++Idx)
	{
		Settings.ParentalBlocks ^= BLOCK_SD_RELOAD_BUTTON;
	}

	// Settings: Block Priiloader Override
	else if (ret == ++Idx)
	{
		Settings.ParentalBlocks ^= BLOCK_PRIILOADER_OVERRIDE;
	}

	// Settings: Block Loader Mode Button
	else if (ret == ++Idx)
	{
		Settings.ParentalBlocks ^= BLOCK_LOADER_MODE_BUTTON;
	}

	// Settings: Block Loader Layout Button
	else if (ret == ++Idx)
	{
		Settings.ParentalBlocks ^= BLOCK_LOADER_LAYOUT_BUTTON;
	}

	SetOptionValues();

	return MENU_NONE;
}
