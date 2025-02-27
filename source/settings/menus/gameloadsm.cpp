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
#include <gccore.h>
#include "settings/csettings.h"
#include "settings/cgamestatistics.h"
#include "themes/ctheme.h"
#include "prompts/promptwindows.h"
#include "prompts/discbrowser.h"
#include "prompts/filebrowser.h"
#include "usbloader/alternatedoloffsets.h"
#include "language/gettext.h"
#include "wad/nandtitle.h"
#include "system/iosloader.h"
#include "gameloadsm.hpp"

static const char * OnOffText[] =
{
	trNOOP( "Off" ),
	trNOOP( "On" ),
	trNOOP( "Auto" )
};

static const char * GamesIOSText[] =
{
	trNOOP( "Auto" ),
	trNOOP( "Custom" )
};

static const char * VideoModeText[] =
{
	trNOOP( "System default" ),
	trNOOP( "Game default" ),
	trNOOP( "Force PAL50" ),
	trNOOP( "Force PAL60" ),
	trNOOP( "Force NTSC" ),
	trNOOP( "Region patch" ),
	trNOOP( "Force PAL480p" ),
	trNOOP( "Force NTSC480p" )
};

static const char * VideoPatchDolText[] =
{
	trNOOP( "Off" ),
	trNOOP( "Region patch" ),
	trNOOP( "On" ),
	trNOOP( "All" )
};

static const char * DeflickerText[] =
{
	trNOOP( "Auto" ),
	trNOOP( "Off (Safe)" ),
	trNOOP( "Off (Extended)" ),
	trNOOP( "ON (Low)" ),
	trNOOP( "ON (Medium)" ),
	trNOOP( "ON (High)" )
};

static const char * WidthText[] =
{
	trNOOP( "Auto" ),
	trNOOP( "Frame buffer" )
};

static const char * AspectText[] =
{
	trNOOP( "Force 4:3" ),
	trNOOP( "Force 16:9" ),
	trNOOP( "System default" )
};

static const char * LanguageText[] =
{
	trNOOP( "Japanese" ),
	trNOOP( "English" ),
	trNOOP( "German" ),
	trNOOP( "French" ),
	trNOOP( "Spanish" ),
	trNOOP( "Italian" ),
	trNOOP( "Dutch" ),
	trNOOP( "Simplified Chinese" ),
	trNOOP( "Traditional Chinese" ),
	trNOOP( "Korean" ),
	trNOOP( "Console default" )
};

static const char * ParentalText[] =
{
	trNOOP( "0 (Everyone)" ),
	trNOOP( "1 (Child 7+)" ),
	trNOOP( "2 (Teen 12+)" ),
	trNOOP( "3 (Mature 16+)" ),
	trNOOP( "4 (Adults Only 18+)" )
};

static const char * AlternateDOLText[] =
{
	trNOOP( "Off" ),
	trNOOP( "Select DOL from game" ),
	trNOOP( "Load from SDC/USB" ),
	trNOOP( "List at game launch" ),
	trNOOP( "Default" )
};

static const char * NandEmuText[] =
{
	trNOOP( "Off" ),
	trNOOP( "Partial" ),
	trNOOP( "Full" ),
	trNOOP( "Neek" )
};

static const char * HooktypeText[] =
{
	trNOOP( "None" ),
	trNOOP( "VBI (Default)" ),
	trNOOP( "KPAD Read" ),
	trNOOP( "Joypad" ),
	trNOOP( "GXDraw" ),
	trNOOP( "GXFlush" ),
	trNOOP( "OSSleepThread" ),
	trNOOP( "AXNextFrame" )
};

static const char * PrivServText[] =
{
	trNOOP( "Off" ),
	trNOOP( "NoSSL only" ),
	trNOOP( "Wiimmfi" ),
	trNOOP( "Other Wi-Fi Connection" ),
	trNOOP( "Custom" )
};

static const char blocked[22] =
{
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x28, 0x27, 0x29, 0x2A,
	0x2C, 0x2F, 0x3A, 0x3B, 0x3C, 0x3E, 0x3F, 0x40, 0x5E, 0x5F, 0x00
};

GameLoadSM::GameLoadSM(struct discHdr *hdr)
	: SettingsMenu(tr("Game Loader"), &GuiOptions, MENU_NONE),
	  Header(hdr)
{
	GameConfig = *GameSettings.GetGameCFG((const char *) Header->id);

	if(!btnOutline)
		btnOutline = Resources::GetImageData("button_dialogue_box.png");
	if(!trigA)
		trigA = new GuiTrigger();
	trigA->SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	saveBtnTxt = new GuiText(tr( "Save" ), 22, thColor("r=0 g=0 b=0 a=255 - prompt windows button text color"));
	saveBtnTxt->SetMaxWidth(btnOutline->GetWidth() - 30);
	saveBtnImg = new GuiImage (btnOutline);
	if (Settings.wsprompt == ON)
	{
		saveBtnTxt->SetWidescreen(Settings.widescreen);
		saveBtnImg->SetWidescreen(Settings.widescreen);
	}
	saveBtn = new GuiButton(saveBtnImg, saveBtnImg, 2, 3, 180, 400, trigA, btnSoundOver, btnSoundClick2, 1);
	saveBtn->SetLabel(saveBtnTxt);
	Append(saveBtn);

	SetOptionNames();
	SetOptionValues();
}

GameLoadSM::~GameLoadSM()
{
	HaltGui();
	//! The rest is destroyed in SettingsMenu.cpp
	Remove(saveBtn);
	delete saveBtnTxt;
	delete saveBtnImg;
	delete saveBtn;
	ResumeGui();
}

void GameLoadSM::SetDefaultConfig()
{
	char id[7];
	snprintf(id, sizeof(id), GameConfig.id);
	GameSettings.SetDefault(GameConfig);
	snprintf(GameConfig.id, sizeof(GameConfig.id), id);
}

void GameLoadSM::SetOptionNames()
{
	int Idx = 0;

	Options->SetName(Idx++, "%s", tr("Lock game" ));
	Options->SetName(Idx++, "%s", tr("User rating" ));
	Options->SetName(Idx++, "%s", tr("Video mode" ));
	Options->SetName(Idx++, "%s", tr("DOL video patch" ));
	Options->SetName(Idx++, "%s", tr("480p patch" ));
	Options->SetName(Idx++, "%s", tr("Sneek video patch" ));
	Options->SetName(Idx++, "%s", tr("VIDTV patch" ));
	Options->SetName(Idx++, "%s", tr("Deflicker" ));
	Options->SetName(Idx++, "%s", tr("Video width" ));
	Options->SetName(Idx++, "%s", tr("Aspect ratio" ));
	Options->SetName(Idx++, "%s", tr("Region patch" ));
	Options->SetName(Idx++, "%s", tr("Language" ));
	Options->SetName(Idx++, "%s", tr("Ocarina" ));
	Options->SetName(Idx++, "%s", tr("Private server" ));
	if(GameConfig.PrivateServer == PRIVSERV_CUSTOM)
	{
		Options->SetName(Idx++, "%s", tr("Custom address" ));
	}
	Options->SetName(Idx++, "%s", tr("Parental control" ));
	Options->SetName(Idx++, "%s", tr("Joypad hook type" ));
	Options->SetName(Idx++, "%s", tr("Wiird debugger" ));
	Options->SetName(Idx++, "%s", tr("IOS" ));
	if(GameConfig.autoios == GAME_IOS_CUSTOM)
	{
		Options->SetName(Idx++, "%s", tr("Custom IOS" ));
	}
	Options->SetName(Idx++, "%s", tr("Return to" ));
	Options->SetName(Idx++, "%s", tr("Block IOS reload" ));

	//! Only wii games and EmuNAND channels
	if(Header->type == TYPE_GAME_WII_IMG || Header->type == TYPE_GAME_WII_DISC)
	{
		Options->SetName(Idx++, "%s", tr("EmuNAND saves mode" ));
		Options->SetName(Idx++, "%s", tr("EmuNAND saves path" ));
	}
	else if(Header->type == TYPE_GAME_EMUNANDCHAN)
	{
		Options->SetName(Idx++, "%s", tr("EmuNAND channels mode" ));
		Options->SetName(Idx++, "%s", tr("EmuNAND channels path" ));
	}

	//! Only on Wii games
	if(Header->type == TYPE_GAME_WII_IMG || Header->type == TYPE_GAME_WII_DISC)
	{
		Options->SetName(Idx++, "%s", tr("Alternate DOL" ));
		Options->SetName(Idx++, "%s", tr("DOL offset" ));
	}
}

void GameLoadSM::SetOptionValues()
{
	int Idx = 0;

	//! Settings: Game Lock
	Options->SetValue(Idx++, "%s", tr( OnOffText[GameConfig.Locked] ));

	//! Settings: Favorite Level
	Options->SetValue(Idx++, "%i", GameStatistics.GetFavoriteRank(Header->id));

	//! Settings: Video Mode
	if(GameConfig.video == INHERIT)
		Options->SetValue(Idx++, tr("Use global settings"));
	else
		Options->SetValue(Idx++, "%s", tr(VideoModeText[GameConfig.video]));

	//! Settings: Dol Video Patch
	if(GameConfig.videoPatchDol == INHERIT)
		Options->SetValue(Idx++, tr("Use global settings"));
	else
		Options->SetValue(Idx++, "%s", tr(VideoPatchDolText[GameConfig.videoPatchDol]));

	//! Settings: 480p Pixel Fix Patch
	if(GameConfig.patchFix480p == INHERIT)
		Options->SetValue(Idx++, tr("Use global settings"));
	else
		Options->SetValue(Idx++, "%s", tr(OnOffText[GameConfig.patchFix480p]));

	//! Settings: Sneek Video Patch
	if(GameConfig.sneekVideoPatch == INHERIT)
		Options->SetValue(Idx++, tr("Use global settings"));
	else
		Options->SetValue(Idx++, "%s", tr(OnOffText[GameConfig.sneekVideoPatch]));

	//! Settings: VIDTV Patch
	if(GameConfig.vipatch == INHERIT)
		Options->SetValue(Idx++, tr("Use global settings"));
	else
		Options->SetValue(Idx++, "%s", tr(OnOffText[GameConfig.vipatch]));

	//! Settings: Deflicker Filter
	if(GameConfig.deflicker == INHERIT)
		Options->SetValue(Idx++, tr("Use global settings"));
	else
		Options->SetValue(Idx++, "%s", tr(DeflickerText[GameConfig.deflicker]));

	//! Settings: Video Width
	if(GameConfig.videoWidth == INHERIT)
		Options->SetValue(Idx++, tr("Use global settings"));
	else
		Options->SetValue(Idx++, "%s", tr(WidthText[GameConfig.videoWidth]));

	//! Settings: Aspect Ratio
	if(GameConfig.aspectratio == INHERIT)
		Options->SetValue(Idx++, tr("Use global settings"));
	else
		Options->SetValue(Idx++, "%s", tr(AspectText[GameConfig.aspectratio]));

	//! Settings: Patch Country Strings
	if(GameConfig.patchcountrystrings == INHERIT)
		Options->SetValue(Idx++, tr("Use global settings"));
	else
		Options->SetValue(Idx++, "%s", tr(OnOffText[GameConfig.patchcountrystrings]));

	//! Settings: Game Language
	if(GameConfig.language == INHERIT)
		Options->SetValue(Idx++, tr("Use global settings"));
	else
		Options->SetValue(Idx++, "%s", tr(LanguageText[GameConfig.language]));

	//! Settings: Ocarina
	if(GameConfig.ocarina == INHERIT)
		Options->SetValue(Idx++, tr("Use global settings"));
	else
		Options->SetValue(Idx++, "%s", tr(OnOffText[GameConfig.ocarina]));

	//! Settings: Private Server
	if(GameConfig.PrivateServer == INHERIT)
		Options->SetValue(Idx++, tr("Use global settings"));
	else
		Options->SetValue(Idx++, "%s", tr(PrivServText[GameConfig.PrivateServer]));

	//! Settings: Custom Address
	if(GameConfig.PrivateServer == PRIVSERV_CUSTOM)
	{
		if(GameConfig.CustomAddress.size() == 0)
			Options->SetValue(Idx++, tr("Use global settings"));
		else
			Options->SetValue(Idx++, "%s", GameConfig.CustomAddress.c_str());
	}

	//! Settings: Parental Control
	Options->SetValue(Idx++, "%s", tr(ParentalText[GameConfig.parentalcontrol]));

	//! Settings: Hooktype
	if(GameConfig.Hooktype == INHERIT)
		Options->SetValue(Idx++, tr("Use global settings"));
	else
		Options->SetValue(Idx++, "%s", tr( HooktypeText[GameConfig.Hooktype] ));

	//! Settings: Wiird Debugger
	if(GameConfig.WiirdDebugger == INHERIT)
		Options->SetValue(Idx++, tr("Use global settings"));
	else
		Options->SetValue(Idx++, "%s", tr( OnOffText[GameConfig.WiirdDebugger] ));

	//! Settings: Game IOS
	if(GameConfig.autoios == INHERIT)
		Options->SetValue(Idx++, tr("Use global settings"));
	else
		Options->SetValue(Idx++, "%s", tr( GamesIOSText[GameConfig.autoios] ));

	//! Settings: Custom Game IOS
	if(GameConfig.autoios == GAME_IOS_CUSTOM)
	{
		if(GameConfig.ios == INHERIT)
			Options->SetValue(Idx++, tr("Use global settings"));
		else
			Options->SetValue(Idx++, "%i", GameConfig.ios);
	}

	//! Settings: Return To
	if(Header->type == TYPE_GAME_EMUNANDCHAN && EMUNAND_NEEK == (GameConfig.NandEmuMode == INHERIT ? Settings.NandEmuChanMode : GameConfig.NandEmuMode))
	{
		Options->SetValue(Idx++, "%s", tr( OnOffText[GameConfig.returnTo] ));
	}
	else if(GameConfig.returnTo)
	{
		const char* TitleName = NULL;
		u64 tid = NandTitles.FindU32(Settings.returnTo);
		if (tid > 0)
			TitleName = NandTitles.NameOf(tid);
		Options->SetValue(Idx++, "%s", TitleName ? TitleName : strlen(Settings.returnTo) > 0 ?
										Settings.returnTo : tr( OnOffText[0] ));
	}
	else
	{
		Options->SetValue(Idx++, "%s", tr( OnOffText[0] ));
	}

	//! Settings: Block IOS Reload
	if(GameConfig.iosreloadblock == INHERIT)
		Options->SetValue(Idx++, tr("Use global settings"));
	else
		Options->SetValue(Idx++, "%s", tr( OnOffText[GameConfig.iosreloadblock]) );

	//! Only wii games and EmuNAND channels
	if(   Header->type == TYPE_GAME_WII_IMG
	   || Header->type == TYPE_GAME_WII_DISC
	   || Header->type == TYPE_GAME_EMUNANDCHAN)
	{
		//! Settings: EmuNAND Save/Channel Mode
		if(GameConfig.NandEmuMode == INHERIT)
			Options->SetValue(Idx++, tr("Use global settings"));
		else
			Options->SetValue(Idx++, "%s", tr( NandEmuText[GameConfig.NandEmuMode] ));

		//! Settings: EmuNAND Save/Channel Path
		if(GameConfig.NandEmuPath.size() == 0)
			Options->SetValue(Idx++, tr("Use global settings"));
		else
			Options->SetValue(Idx++, "%s", GameConfig.NandEmuPath.c_str());
	}

	//! Only on Wii games
	if(Header->type == TYPE_GAME_WII_IMG || Header->type == TYPE_GAME_WII_DISC)
	{
		//! Settings: Alternate DOL
		Options->SetValue(Idx++, "%s", tr( AlternateDOLText[GameConfig.loadalternatedol] ));

		//! Settings: Select DOL Offset
		if(GameConfig.loadalternatedol != 1)
			Options->SetValue(Idx++, tr("Not required"));
		else
		{
			if(GameConfig.alternatedolname.size() != 0)
				Options->SetValue(Idx++, "%i <%s>", (int)GameConfig.alternatedolstart, GameConfig.alternatedolname.c_str());
			else
				Options->SetValue(Idx++, "%i", (int)GameConfig.alternatedolstart);
		}
	}
}

int GameLoadSM::GetMenuInternal()
{
	if (saveBtn->GetState() == STATE_CLICKED)
	{
		if (GameSettings.AddGame(GameConfig) && GameSettings.Save())
		{
			WindowPrompt(tr("Game Loader"), tr("Game settings saved successfully"), tr("OK"));
		}
		else
			WindowPrompt(tr("File Error"), tr("An error occurred while saving game settings"), tr("OK"));

		saveBtn->ResetState();
	}

	int ret = optionBrowser->GetClickedOption();

	if (ret < 0)
		return MENU_NONE;

	int Idx = -1;

	//! Settings: Game Lock
	if (ret == ++Idx)
	{
		if (++GameConfig.Locked >= MAX_ON_OFF) GameConfig.Locked = 0;
	}

	//! Settings: Favorite Level
	else if (ret == ++Idx)
	{
		int Level = GameStatistics.GetFavoriteRank(Header->id);
		if (++Level > 5) Level = 0;

		GameStatistics.SetFavoriteRank(Header->id, Level);
		GameStatistics.Save();
	}

	//! Settings: Video Mode
	else if (ret == ++Idx)
	{
		if (++GameConfig.video >= VIDEO_MODE_MAX) GameConfig.video = INHERIT;
	}

	//! Settings: Dol Video Patch
	else if (ret == ++Idx)
	{
		if (++GameConfig.videoPatchDol >= VIDEO_PATCH_DOL_MAX) GameConfig.videoPatchDol = INHERIT;
	}

	//! Settings: 480p Pixel Fix Patch
	else if (ret == ++Idx)
	{
		if (++GameConfig.patchFix480p >= MAX_ON_OFF) GameConfig.patchFix480p = INHERIT;
	}

	//! Settings: Sneek Video Patch
	else if (ret == ++Idx)
	{
		if (++GameConfig.sneekVideoPatch >= MAX_ON_OFF) GameConfig.sneekVideoPatch = INHERIT;
	}

	//! Settings: VIDTV Patch
	if (ret == ++Idx)
	{
		if (++GameConfig.vipatch >= MAX_ON_OFF) GameConfig.vipatch = INHERIT;
	}

	//! Settings: Deflicker Filter
	else if (ret == ++Idx)
	{
		if (++GameConfig.deflicker >= DEFLICKER_MAX) GameConfig.deflicker = INHERIT;
	}

	//! Settings: Video Width
	else if (ret == ++Idx)
	{
		if (++GameConfig.videoWidth >= WIDTH_MAX) GameConfig.videoWidth = INHERIT;
	}

	//! Settings: Aspect Ratio
	else if (ret == ++Idx)
	{
		if (++GameConfig.aspectratio >= ASPECT_MAX) GameConfig.aspectratio = INHERIT;
	}

	//! Settings: Patch Country Strings
	if (ret == ++Idx)
	{
		if (++GameConfig.patchcountrystrings >= MAX_ON_OFF) GameConfig.patchcountrystrings = INHERIT;
	}

	//! Settings: Game Language
	if (ret == ++Idx)
	{
		if (++GameConfig.language >= MAX_LANGUAGE) GameConfig.language = INHERIT;
	}

	//! Settings: Ocarina
	else if (ret == ++Idx)
	{
		if (++GameConfig.ocarina >= MAX_ON_OFF) GameConfig.ocarina = INHERIT;
	}

	//! Settings: Private Server
	else if (ret == ++Idx)
	{
		if (++GameConfig.PrivateServer >= PRIVSERV_MAX_CHOICE) GameConfig.PrivateServer = INHERIT;
		Options->ClearList();
		SetOptionNames();
		SetOptionValues();
	}

	//! Settings: Custom Address
	else if (GameConfig.PrivateServer == PRIVSERV_CUSTOM && ret == ++Idx)
	{
		char entered[300];
		snprintf(entered, sizeof(entered), "%s", GameConfig.CustomAddress.c_str());
		if (OnScreenKeyboard(entered, sizeof(entered), 0, false, true))
		{
			// Only allow letters, numbers, periods and hyphens
			if ((strlen(entered) > 0 && strlen(entered) <= 3) || strpbrk(entered, blocked))
				WindowPrompt(tr("Address Error"), tr("Enter a valid address. Example: wiimmfi.de"), tr("OK"));
			else
				GameConfig.CustomAddress = entered;
		}
	}

	//! Settings: Parental Control
	else if (ret == ++Idx)
	{
		if (++GameConfig.parentalcontrol >= 5) GameConfig.parentalcontrol = 0;
	}

	//! Settings: Hooktype
	if (ret == ++Idx)
	{
		if (++GameConfig.Hooktype >= 8) GameConfig.Hooktype = INHERIT;
	}

	//! Settings: Wiird Debugger
	else if (ret == ++Idx)
	{
		if (++GameConfig.WiirdDebugger >= MAX_ON_OFF) GameConfig.WiirdDebugger = INHERIT;
	}

	//! Settings: Game IOS
	else if (ret == ++Idx)
	{
		if (++GameConfig.autoios >= GAME_IOS_MAX) GameConfig.autoios = INHERIT;
		Options->ClearList();
		SetOptionNames();
		SetOptionValues();
	}

	//! Settings: Custom Game IOS
	else if (GameConfig.autoios == GAME_IOS_CUSTOM && ret == ++Idx)
	{
		char entered[8];
		snprintf(entered, sizeof(entered), "%i", GameConfig.ios);
		if(OnScreenNumpad(entered, sizeof(entered) / 2))
		{
			int newIOS = atoi(entered);

			if(newIOS != INHERIT && (newIOS < 200 ||  newIOS > 255)) {
				WindowPrompt(tr("Error"), tr("Invalid IOS number. IOS number must be -1 for inherit or 200 - 255 for custom"), tr("OK"));
			}
			else
			{
				GameConfig.ios  = newIOS;

				if(GameConfig.ios != INHERIT && NandTitles.IndexOf(TITLE_ID(1, GameConfig.ios)) < 0)
				{
					WindowPrompt(tr("Attention"), tr("IOS not found on the titles list. If you are sure it is installed in this slot, ignore this warning"), tr("OK"));
				}
				else if(GameConfig.ios == 254)
				{
					WindowPrompt(tr("Attention"), tr("Selected IOS is BootMii IOS. If you are sure it is not BootMii and another IOS is installed in this slot, ignore this warning"), tr("OK"));
				}
			}
		}
	}

	//! Settings: Return To
	else if (ret == ++Idx)
	{
		if (++GameConfig.returnTo >= MAX_ON_OFF) GameConfig.returnTo = 0;
	}

	//! Settings: Block IOS Reload
	if (ret == ++Idx)
	{
		if(++GameConfig.iosreloadblock >= 3) GameConfig.iosreloadblock = INHERIT;
	}

	//! Only wii games and EmuNAND channels
	if(	Header->type == TYPE_GAME_WII_IMG
			||  Header->type == TYPE_GAME_WII_DISC
			||  Header->type == TYPE_GAME_EMUNANDCHAN)
	{
		//! Settings: EmuNAND Save/Channel Mode
		if (ret == ++Idx)
		{
			if (++GameConfig.NandEmuMode >= EMUNAND_MAX) GameConfig.NandEmuMode = INHERIT;

			// neek available only for EmuNAND Channels
			if(Header->type != TYPE_GAME_EMUNANDCHAN && GameConfig.NandEmuMode >= EMUNAND_NEEK)
				GameConfig.NandEmuMode = INHERIT;

			//! On titles from emulated NAND path disabling the NAND emu mode is not allowed
			if(Header->type == TYPE_GAME_EMUNANDCHAN && GameConfig.NandEmuMode == OFF)
				GameConfig.NandEmuMode = 1;
		}

		//! Settings: EmuNAND Save/Channel Path
		else if (ret == ++Idx)
		{
			int autoIOS = GameConfig.autoios == INHERIT ? Settings.AutoIOS : GameConfig.autoios;
			// If NandEmuPath is on root of the first FAT32 partition, allow rev17-21 cIOS for EmuNAND Channels
			bool NandEmu_compatible = false;
			if(!autoIOS && Header->type == TYPE_GAME_EMUNANDCHAN)
			{
				NandEmu_compatible = IosLoader::is_NandEmu_compatible(NULL, GameConfig.ios == INHERIT ? Settings.cios : GameConfig.ios);
			}

			if(autoIOS == GAME_IOS_CUSTOM && !IosLoader::IsD2X(GameConfig.ios == INHERIT ? Settings.cios : GameConfig.ios) && !NandEmu_compatible)
				WindowPrompt(tr("EmuNAND Error"), tr("NAND emulation is only available with D2X cIOS"), tr("OK"));
			else
			{
				char entered[300];
				snprintf(entered, sizeof(entered), GameConfig.NandEmuPath.c_str());

				HaltGui();
				GuiWindow * parent = (GuiWindow *) parentElement;
				if(parent) parent->SetState(STATE_DISABLED);
				this->SetState(STATE_DEFAULT);
				this->Remove(optionBrowser);
				ResumeGui();

				int result = BrowseDevice(entered, sizeof(entered), FB_DEFAULT, noFILES);

				if(parent) parent->SetState(STATE_DEFAULT);
				this->Append(optionBrowser);

				if (result == 1)
				{
					if (entered[strlen(entered)-1] != '/')
						strcat(entered, "/");

					GameConfig.NandEmuPath = entered;
					WindowPrompt(tr("Game Loader"), tr("EmuMAND path updated successfully"), tr("OK"));
				}
			}
		}
	}

	//! Only on Wii games
	if(Header->type == TYPE_GAME_WII_IMG || Header->type == TYPE_GAME_WII_DISC)
	{
		//! Settings: Alternate DOL
		if (ret == ++Idx)
		{
			if (++GameConfig.loadalternatedol >= ALT_DOL_MAX_CHOICE)
				GameConfig.loadalternatedol = 0;
		}

		//! Settings: Select DOL Offset from Game
		else if (	(ret == ++Idx)
					&& (GameConfig.loadalternatedol == 1))
		{
			GuiWindow * parentWindow = (GuiWindow *) parentElement;
			if(parentWindow) parentWindow->SetState(STATE_DISABLED);
			//alt dol menu for games that require more than a single alt dol
			int autodol = autoSelectDolPrompt((char *) GameConfig.id);
			if(autodol == 0)
			{
				if(parentWindow) parentWindow->SetState(STATE_DEFAULT);
				return MENU_NONE; //Cancel Button pressed
			}

			char tmp[170];

			if (autodol > 0)
			{
				GameConfig.alternatedolstart = autodol;
				snprintf(tmp, sizeof(tmp), "%s <%i>", tr("AUTO" ), autodol);
				GameConfig.alternatedolname = tmp;
				SetOptionValues();
				if(parentWindow) parentWindow->SetState(STATE_DEFAULT);
				return MENU_NONE;
			}

			int res = DiscBrowse(GameConfig.id, tmp, sizeof(tmp));
			if (res >= 0)
			{
				GameConfig.alternatedolname = tmp;
				GameConfig.alternatedolstart = res;
				snprintf(tmp, sizeof(tmp), "%s %.6s - %i", tr("You might have some information that will be helpful to us. Please pass it along to the USB Loader GX Team" ), (char *) GameConfig.id, (int)GameConfig.alternatedolstart);
				WindowPrompt(0, tmp, tr("OK"));
			}

			if(GameConfig.alternatedolstart == 0)
				GameConfig.loadalternatedol = 0;
			if(parentWindow) parentWindow->SetState(STATE_DEFAULT);
		}
	}

	SetOptionValues();

	return MENU_NONE;
}
