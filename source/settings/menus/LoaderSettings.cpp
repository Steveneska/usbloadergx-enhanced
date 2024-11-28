/****************************************************************************
 * Copyright (C) 2012-2014 Cyan
 * Copyright (C) 2010 by Dimok
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
#include "loadersettings.hpp"
#include "usbloader/usbstorage2.h"
#include "settings/csettings.h"
#include "settings/gametitles.h"
#include "settings/meta.h"
#include "prompts/promptwindows.h"
#include "language/gettext.h"
#include "wad/nandtitle.h"
#include "prompts/titlebrowser.h"
#include "system/iosloader.h"
#include "usbloader/wbfs.h"
#include "usbloader/gamelist.h"
#include "utils/tools.h"
#include "menu.h"
#include "gamecube/gcgames.h"

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

static const char * AspectText[] =
{
	trNOOP( "Force 4:3" ),
	trNOOP( "Force 16:9" ),
	trNOOP( "System default" )
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
	trNOOP( "Off (safe)" ),
	trNOOP( "Off (extended)" ),
	trNOOP( "ON (low)" ),
	trNOOP( "ON (medium)" ),
	trNOOP( "ON (high)" )
};

static const char * WidthText[] =
{
	trNOOP( "Auto" ),
	trNOOP( "Frame buffer" )
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

static const char * ChannelLaunchText[] =
{
	trNOOP( "Main DOL" ),
	trNOOP( "Boot content" )
};

static const char * GCMode[] =
{
	trNOOP( "MIOS (Default and custom)" ),
	trNOOP( "Devolution" ),
	trNOOP( "Nintendont" )
};

static const char * GCSourceText[][3] =
{
	{ trNOOP( "USB" ), " ", trNOOP( "device" ) },
	{ trNOOP( "SD" ), " ", trNOOP( "Card" ) },
	{ trNOOP( "A" ), "u", trNOOP( "to" ) },
	{ trNOOP( "USB device" ), "/", trNOOP( "SD Card" ) },
	{ trNOOP( "SD Card" ), "/", trNOOP( "USB device" ) }
};

static const char * DMLVideoText[] =
{
	trNOOP( "Auto" ),
	trNOOP( "System default" ),
	trNOOP( "Game default" ),
	trNOOP( "Force PAL50" ),
	trNOOP( "Force PAL60" ),
	trNOOP( "Force NTSC" ),
	"", // unused
	trNOOP( "Force PAL480p" ),
	trNOOP( "Force NTSC480p" ),
	trNOOP( "None" )
};

static const char * DMLNMMMode[] =
{
	trNOOP( "Off" ),
	trNOOP( "On" ),
	trNOOP( "Debug" )
};

static const char * DMLDebug[] =
{
	trNOOP( "Off" ),
	trNOOP( "On" ),
	trNOOP( "Debug wait" )
};

static const char * DEVOMCText[] =
{
	trNOOP( "Off" ),
	trNOOP( "On" ),
	trNOOP( "Individual" ),
	trNOOP( "By region" )
};

static const char * NINMCText[] =
{
	trNOOP( "Off" ),
	trNOOP( "Individual" ),
	trNOOP( "Shared" )
};

static const char * NINCfgText[] =
{
	trNOOP( "Delete" ),
	trNOOP( "Create" ),
	trNOOP( "Don't change" )
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

LoaderSettings::LoaderSettings()
	: SettingsMenu(tr("Game Loader"), &GuiOptions, MENU_NONE)
{

	SetOptionNames();
	SetOptionValues();

	oldLoaderMode = Settings.LoaderMode;
	oldGameCubeSource = Settings.GameCubeSource;
	oldLoaderIOS = Settings.LoaderIOS;
}

LoaderSettings::~LoaderSettings()
{
	if(oldLoaderMode != Settings.LoaderMode)
	{
		if(Settings.LoaderMode & MODE_WIIGAMES && (gameList.GameCount() == 0))
		{
			WBFS_ReInit(Settings.SDMode ? WBFS_DEVICE_SDHC : WBFS_DEVICE_USB);
			gameList.ReadGameList();
		}

		gameList.LoadUnfiltered();
	}
	
	if(oldGameCubeSource != Settings.GameCubeSource)
	{
		GCGames::Instance()->LoadAllGames();
	}
	
	if(oldLoaderIOS != Settings.LoaderIOS)
	{
		editMetaArguments();
	}
}

void LoaderSettings::SetOptionNames()
{
	int Idx = 0;

	Options->SetName(Idx++, "%s", tr("Video mode" ));
	Options->SetName(Idx++, "%s", tr("DOL video patch" ));
	Options->SetName(Idx++, "%s", tr("480p patch" ));
	Options->SetName(Idx++, "%s", tr("Sneek video patch" ));
	Options->SetName(Idx++, "%s", tr("VIDTV patch" ));
	Options->SetName(Idx++, "%s", tr("Deflicker" ));
	Options->SetName(Idx++, "%s", tr("Video width" ));
	Options->SetName(Idx++, "%s", tr("Aspect ratio" ));
	Options->SetName(Idx++, "%s", tr("Language" ));
	Options->SetName(Idx++, "%s", tr("Change country string" ));
	Options->SetName(Idx++, "%s", tr("Ocarina" ));
	Options->SetName(Idx++, "%s", tr("Private server" ));
	if(Settings.PrivateServer == PRIVSERV_CUSTOM)
	{
		Options->SetName(Idx++, "%s", tr("Custom address" ));
	}
	Options->SetName(Idx++, "%s", tr("Application IOS" ));
	Options->SetName(Idx++, "%s", tr("Game IOS" ));
	if(Settings.AutoIOS == GAME_IOS_CUSTOM)
	{
		Options->SetName(Idx++, "%s", tr("Custom IOS" ));
	}
	Options->SetName(Idx++, "%s", tr("Enable quick boot" ));
	Options->SetName(Idx++, "%s", tr("Block IOS reload" ));
	Options->SetName(Idx++, "%s", tr("Return to" ));
	Options->SetName(Idx++, "%s", tr("EmuNAND save mode" ));
	Options->SetName(Idx++, "%s", tr("EmuNAND channel mode" ));
	Options->SetName(Idx++, "%s", tr("Joypad hook type" ));
	Options->SetName(Idx++, "%s", tr("Wiird debugger" ));
	Options->SetName(Idx++, "%s", tr("Debugger paused start" ));
	Options->SetName(Idx++, "%s", tr("Channel launcher" ));
	Options->SetName(Idx++, "%s", tr("GameCube mode options" ));
	Options->SetName(Idx++, "%s", tr("  Game source" ));
	Options->SetName(Idx++, "%s", tr("  Game loader" ));
	Options->SetName(Idx++, "%s", tr("  Progressive patch" ));
	Options->SetName(Idx++, "%s", tr("DML + Nintendont" ));
	Options->SetName(Idx++, "%s", tr("  Video mode" ));
	Options->SetName(Idx++, "%s", tr("  Force widescreen" ));
	Options->SetName(Idx++, "%s", tr("  Debug" ));
	Options->SetName(Idx++, "%s", tr("  Disc select prompt" ));
	Options->SetName(Idx++, "%s", tr("DIOS MIOS Lite" ));
	Options->SetName(Idx++, "%s", tr("  NMM mode" ));
	Options->SetName(Idx++, "%s", tr("  Joypad hook" ));
	Options->SetName(Idx++, "%s", tr("  No Disc+" ));
	Options->SetName(Idx++, "%s", tr("  Screenshot" ));
	Options->SetName(Idx++, "%s", tr("  LED activity" ));
	Options->SetName(Idx++, "%s", tr("  Japanese patch" ));
	Options->SetName(Idx++, "%s", tr("Nintendont" ));
	Options->SetName(Idx++, "%s", tr("  Auto boot" ));
	Options->SetName(Idx++, "%s", tr("  Settings file" ));
	Options->SetName(Idx++, "%s", tr("  Deflicker" ));
	Options->SetName(Idx++, "%s", tr("  PAL50 patch" ));
	Options->SetName(Idx++, "%s", tr("  Wii U widescreen" ));
	Options->SetName(Idx++, "%s", tr("  Video scaling" ));
	if(Settings.NINVideoScale != 0)
	{
		Options->SetName(Idx++, "%s", tr("  Video scaling value" ));
	}
	Options->SetName(Idx++, "%s", tr("  Video offset" ));
	Options->SetName(Idx++, "%s", tr("  Remove read speed limit" ));
	Options->SetName(Idx++, "%s", tr("  Triforce arcade mode" ));
	Options->SetName(Idx++, "%s", tr("  Classic Controller rumble" ));
	Options->SetName(Idx++, "%s", tr("  Skip BIOS" ));
	Options->SetName(Idx++, "%s", tr("  BBA emulation" ));
	Options->SetName(Idx++, "%s", tr("  BBA network profile" ));
	Options->SetName(Idx++, "%s", tr("  Memory card emulation" ));
	Options->SetName(Idx++, "%s", tr("  Memory card size" ));
	Options->SetName(Idx++, "%s", tr("  USB HID controller" ));
	Options->SetName(Idx++, "%s", tr("  GameCube controller" ));
	Options->SetName(Idx++, "%s", tr("  Native controller" ));
	Options->SetName(Idx++, "%s", tr("  LED activity" ));
	Options->SetName(Idx++, "%s", tr("  OS Report" ));
	Options->SetName(Idx++, "%s", tr("  Write Log to file" ));
	Options->SetName(Idx++, "%s", tr("Devolution" ));
	Options->SetName(Idx++, "%s", tr("  Memory card emulation" ));
	Options->SetName(Idx++, "%s", tr("  Force widescreen" ));
	Options->SetName(Idx++, "%s", tr("  LED activity" ));
	Options->SetName(Idx++, "%s", tr("  Enable F-Zero AX" ));
	Options->SetName(Idx++, "%s", tr("  Timer fix" ));
	Options->SetName(Idx++, "%s", tr("  D-Pad buttons" ));
	Options->SetName(Idx++, "%s", tr("  Crop overscan" ));
	Options->SetName(Idx++, "%s", tr("  Disc read delay" ));

}

void LoaderSettings::SetOptionValues()
{
	int Idx = 0;

	//! Settings: Video Mode
	Options->SetValue(Idx++, "%s", tr(VideoModeText[Settings.videomode]));

	//! Settings: Dol Video Patch
	Options->SetValue(Idx++, "%s", tr( VideoPatchDolText[Settings.videoPatchDol] ));

	//! Settings: 480p Pixel Fix Patch
	Options->SetValue(Idx++, "%s", tr( OnOffText[Settings.patchFix480p] ));

	//! Settings: Sneek Video Patch
	Options->SetValue(Idx++, "%s", tr( OnOffText[Settings.sneekVideoPatch] ));

	//! Settings: VIDTV Patch
	Options->SetValue(Idx++, "%s", tr( OnOffText[Settings.videopatch] ));

	//! Settings: Deflicker Filter
	Options->SetValue(Idx++, "%s", tr( DeflickerText[Settings.deflicker] ));

	//! Settings: Video Width
	Options->SetValue(Idx++, "%s", tr( WidthText[Settings.videoWidth] ));

	//! Settings: Aspect Ratio
	Options->SetValue(Idx++, "%s", tr( AspectText[Settings.GameAspectRatio] ));

	//! Settings: Game Language
	Options->SetValue(Idx++, "%s", tr( LanguageText[Settings.language] ));

	//! Settings: Patch Country Strings
	Options->SetValue(Idx++, "%s", tr( OnOffText[Settings.patchcountrystrings] ));

	//! Settings: Ocarina
	Options->SetValue(Idx++, "%s", tr( OnOffText[Settings.ocarina] ));

	//! Settings: Private Server
	Options->SetValue(Idx++, "%s", tr( PrivServText[Settings.PrivateServer] ));

	//! Settings: Custom Address
	if(Settings.PrivateServer == PRIVSERV_CUSTOM)
		Options->SetValue(Idx++, "%s", Settings.CustomAddress);

	//! Settings: Loaders IOS
	if (Settings.godmode)
		Options->SetValue(Idx++, "%i", Settings.LoaderIOS);
	else
		Options->SetValue(Idx++, "********");

	//! Settings: Games IOS
	if (Settings.godmode)
		Options->SetValue(Idx++, "%s", tr( GamesIOSText[Settings.AutoIOS] ));
	else
		Options->SetValue(Idx++, "********");

	//! Settings: Custom Games IOS
	if(Settings.AutoIOS == GAME_IOS_CUSTOM)
	{
		if (Settings.godmode)
			Options->SetValue(Idx++, "%i", Settings.cios);
		else
			Options->SetValue(Idx++, "********");
	}

	//! Settings: Quick Boot
	Options->SetValue(Idx++, "%s", tr( OnOffText[Settings.quickboot] ));

	//! Settings: Block IOS Reload
	Options->SetValue(Idx++, "%s", tr( OnOffText[Settings.BlockIOSReload] ));

	//! Settings: Return To
	const char* TitleName = NULL;
	u64 tid = NandTitles.FindU32(Settings.returnTo);
	if (tid > 0)
		TitleName = NandTitles.NameOf(tid);
	TitleName = TitleName ? TitleName : strlen(Settings.returnTo) > 0 ? Settings.returnTo : tr(OnOffText[0]);
	Options->SetValue(Idx++, "%s", TitleName);

	//! Settings: EmuNAND Save Mode
	Options->SetValue(Idx++, "%s", tr( NandEmuText[Settings.NandEmuMode] ));

	//! Settings: EmuNAND Channel Mode
	Options->SetValue(Idx++, "%s", tr( NandEmuText[Settings.NandEmuChanMode] ));

	//! Settings: Hooktype
	Options->SetValue(Idx++, "%s", tr( HooktypeText[Settings.Hooktype] ));

	//! Settings: Wiird Debugger
	Options->SetValue(Idx++, "%s", tr( OnOffText[Settings.WiirdDebugger] ));

	//! Settings: Wiird Debugger Pause on Start
	Options->SetValue(Idx++, "%s", tr( OnOffText[Settings.WiirdDebuggerPause] ));

	//! Settings: Channel Launcher
	Options->SetValue(Idx++, "%s", tr( ChannelLaunchText[Settings.UseChanLauncher] ));

	//! Settings: TITLE - GameCube Settings
	Options->SetValue(Idx++, " ");

	//! Settings: GameCube Source
	Options->SetValue(Idx++, "%s%s%s", tr(GCSourceText[Settings.GameCubeSource][0]),
	                GCSourceText[Settings.GameCubeSource][1], tr(GCSourceText[Settings.GameCubeSource][2]));

	//! Settings: GameCube Mode
	Options->SetValue(Idx++, "%s", tr(GCMode[Settings.GameCubeMode]));

	//! Settings: DML + NIN + Devo Progressive Patch
	Options->SetValue(Idx++, "%s", tr(OnOffText[Settings.DMLProgPatch]));

	//! Settings: TITLE - GameCube DIOS MIOS (Lite) + Nintendont
	Options->SetValue(Idx++, " ");

	//! Settings: DML + NIN Video Mode
	Options->SetValue(Idx++, "%s", tr(DMLVideoText[Settings.DMLVideo]));

	//! Settings: DML + NIN Force Widescreen
	Options->SetValue(Idx++, "%s", tr(OnOffText[Settings.DMLWidescreen]));

	//! Settings: DML + NIN Debug
	Options->SetValue(Idx++, "%s", tr(DMLDebug[Settings.DMLDebug]));

	//! Settings: DML + NIN MultiDiscPrompt
	Options->SetValue(Idx++, "%s", tr(OnOffText[Settings.MultiDiscPrompt]));

	//! Settings: TITLE - GameCube DIOS MIOS (Lite)
	Options->SetValue(Idx++, " ");

	//! Settings: DML NMM Mode
	Options->SetValue(Idx++, "%s", tr(DMLNMMMode[Settings.DMLNMM]));

	//! Settings: DML PAD Hook
	Options->SetValue(Idx++, "%s", tr(OnOffText[Settings.DMLPADHOOK]));

	//! Settings: DML Extended No Disc
	Options->SetValue(Idx++, "%s", tr(OnOffText[Settings.DMLNoDisc2]));

	//! Settings: DML Screenshot
	Options->SetValue(Idx++, "%s", tr(OnOffText[Settings.DMLScreenshot]));

	//! Settings: DML LED Activity
	Options->SetValue(Idx++, "%s", tr(OnOffText[Settings.DMLActivityLED]));

	//! Settings: DML Japanese Patch
	Options->SetValue(Idx++, "%s", tr(OnOffText[Settings.DMLJPNPatch]));

	//! Settings: TITLE - Nintendont
	Options->SetValue(Idx++, " ");

	//! Settings: NIN Auto Boot
	Options->SetValue(Idx++, "%s", tr(OnOffText[Settings.NINAutoboot]));

	//! Settings: NIN Nincfg.bin file
	Options->SetValue(Idx++, "%s", tr(NINCfgText[Settings.NINSettings]));

	//! Settings: NIN Video Deflicker
	Options->SetValue(Idx++, "%s", tr(OnOffText[Settings.NINDeflicker]));

	//! Settings: NIN PAL50 Patch
	Options->SetValue(Idx++, "%s", tr(OnOffText[Settings.NINPal50Patch]));

	//! Settings: WiiU Widescreen
	Options->SetValue(Idx++, "%s", tr(OnOffText[Settings.NINWiiUWide]));

	//! Settings: NIN VideoScale
	if(Settings.NINVideoScale == 0)
		Options->SetValue(Idx++, "%s", tr("Auto"));
	else
	{
		Options->SetValue(Idx++, "%s", tr("Manual (40~120)"));
		Options->SetValue(Idx++, "%d", Settings.NINVideoScale);
	}

	//! Settings: NIN VideoOffset
	Options->SetValue(Idx++, "%d (-20~20)", Settings.NINVideoOffset);

	//! Settings: NIN Remove Read Speed Limiter
	Options->SetValue(Idx++, "%s", tr(OnOffText[Settings.NINRemlimit]));

	//! Settings: NIN Arcade Mode
	Options->SetValue(Idx++, "%s", tr(OnOffText[Settings.NINArcadeMode]));

	//! Settings: NIN CC Rumble
	Options->SetValue(Idx++, "%s", tr(OnOffText[Settings.NINCCRumble]));

	//! Settings: NIN Skip IPL
	Options->SetValue(Idx++, "%s", tr(OnOffText[Settings.NINSkipIPL]));

	//! Settings: NIN BBA Emulation
	Options->SetValue(Idx++, "%s", tr(OnOffText[Settings.NINBBA]));

	//! Settings: NIN BBA Net Profile
	if(Settings.NINBBAProfile == 0)
		Options->SetValue(Idx++, "%s", tr("Auto"));
	else
		Options->SetValue(Idx++, "%i", Settings.NINBBAProfile);

	//! Settings: NIN Memory Card Emulation
	Options->SetValue(Idx++, "%s", tr(NINMCText[Settings.NINMCEmulation]));

	//! Settings: NIN Memory Card Blocks Size
	Options->SetValue(Idx++, "%d", MEM_CARD_BLOCKS(Settings.NINMCSize));

	//! Settings: NIN USB-HID controller
	Options->SetValue(Idx++, "%s", tr(OnOffText[Settings.NINUSBHID]));

	//! Settings: NIN MaxPads - Number of GameCube controllers
	Options->SetValue(Idx++, "%i", Settings.NINMaxPads);

	//! Settings: NIN Native Controller
	Options->SetValue(Idx++, "%s", tr(OnOffText[Settings.NINNativeSI]));

	//! Settings: NIN LED Activity
	Options->SetValue(Idx++, "%s", tr(OnOffText[Settings.NINLED]));

	//! Settings: NIN OS Report
	Options->SetValue(Idx++, "%s", tr(OnOffText[Settings.NINOSReport]));

	//! Settings: NIN Log to file
	Options->SetValue(Idx++, "%s", tr(OnOffText[Settings.NINLog]));

	//! Settings: TITLE - Devolution
	Options->SetValue(Idx++, " ");

	//! Settings: DEVO Memory Card Emulation
	Options->SetValue(Idx++, "%s", tr(DEVOMCText[Settings.DEVOMCEmulation]));

	//! Settings: DEVO Widescreen Patch
	Options->SetValue(Idx++, "%s", tr(OnOffText[Settings.DEVOWidescreen]));

	//! Settings: DEVO Activity LED
	Options->SetValue(Idx++, "%s", tr(OnOffText[Settings.DEVOActivityLED]));

	//! Settings: DEVO F-Zero AX unlock patch
	Options->SetValue(Idx++, "%s", tr(OnOffText[Settings.DEVOFZeroAX]));

	//! Settings: DEVO Timer Fix
	Options->SetValue(Idx++, "%s", tr(OnOffText[Settings.DEVOTimerFix]));

	//! Settings: DEVO Direct Button Mapping
	Options->SetValue(Idx++, "%s", tr(OnOffText[Settings.DEVODButtons]));

	//! Settings: DEVO Crop Overscan
	Options->SetValue(Idx++, "%s", tr(OnOffText[Settings.DEVOCropOverscan]));

	//! Settings: DEVO Disc Read Delay
	Options->SetValue(Idx++, "%s", tr(OnOffText[Settings.DEVODiscDelay]));

}

int LoaderSettings::GetMenuInternal()
{
	int ret = optionBrowser->GetClickedOption();

	if (ret < 0)
		return MENU_NONE;

	int Idx = -1;

	//! Settings: Video Mode
	if (ret == ++Idx)
	{
		if (++Settings.videomode >= VIDEO_MODE_MAX) Settings.videomode = 0;
	}

	//! Settings: Dol Video Patch
	if (ret == ++Idx)
	{
		if (++Settings.videoPatchDol >= VIDEO_PATCH_DOL_MAX) Settings.videoPatchDol = 0;
	}

	//! Settings: 480p Pixel Fix Patch
	if (ret == ++Idx)
	{
		if (++Settings.patchFix480p >= MAX_ON_OFF) Settings.patchFix480p = 0;
	}

	//! Settings: Sneek Video Patch
	else if (ret == ++Idx )
	{
		if (++Settings.sneekVideoPatch >= MAX_ON_OFF) Settings.sneekVideoPatch = 0;
	}

	//! Settings: VIDTV Patch
	else if (ret == ++Idx)
	{
		if (++Settings.videopatch >= MAX_ON_OFF) Settings.videopatch = 0;
	}

	//! Settings: Deflicker Filter
	else if (ret == ++Idx)
	{
		if (++Settings.deflicker >= DEFLICKER_MAX) Settings.deflicker = 0;
	}

	//! Settings: Video Width
	else if (ret == ++Idx)
	{
		if (++Settings.videoWidth >= WIDTH_MAX) Settings.videoWidth = 0;
	}

	//! Settings: Aspect Ratio
	else if (ret == ++Idx )
	{
		if (++Settings.GameAspectRatio >= ASPECT_MAX) Settings.GameAspectRatio = 0;
	}

	//! Settings: Game Language
	else if (ret == ++Idx)
	{
		if (++Settings.language >= MAX_LANGUAGE) Settings.language = 0;
	}

	//! Settings: Patch Country Strings
	else if (ret == ++Idx)
	{
		if (++Settings.patchcountrystrings >= MAX_ON_OFF) Settings.patchcountrystrings = 0;
	}

	//! Settings: Ocarina
	else if (ret == ++Idx)
	{
		if (++Settings.ocarina >= MAX_ON_OFF) Settings.ocarina = 0;
	}

	//! Settings: Private Server
	else if (ret == ++Idx)
	{
		if (++Settings.PrivateServer >= PRIVSERV_MAX_CHOICE) Settings.PrivateServer = 0;
		Options->ClearList();
		SetOptionNames();
		SetOptionValues();
	}

	//! Settings: Custom Address
	else if (Settings.PrivateServer == PRIVSERV_CUSTOM && ret == ++Idx)
	{
		char entered[300];
		snprintf(entered, sizeof(entered), "%s", Settings.CustomAddress);
		if (OnScreenKeyboard(entered, sizeof(entered), 0, false, true))
		{
			// Only allow letters, numbers, periods and hyphens
			if (strlen(entered) <= 3 || strpbrk(entered, blocked))
				WindowPrompt(tr("Error"), tr("Please enter a valid address. Example: wiimmfi.de"), tr("OK"));
			else
				snprintf(Settings.CustomAddress, sizeof(Settings.CustomAddress), entered);
		}
	}

	//! Settings: Loaders IOS
	else if (ret == ++Idx)
	{
		if(!Settings.godmode)
			return MENU_NONE;

		char entered[4];
		snprintf(entered, sizeof(entered), "%i", Settings.LoaderIOS);
		if(OnScreenNumpad(entered, sizeof(entered)))
		{
			if(atoi(entered) == 58) // allow only IOS58 for IOS <200
				Settings.LoaderIOS = 58;
			else
				Settings.LoaderIOS = LIMIT(atoi(entered), 200, 255);

			if(NandTitles.IndexOf(TITLE_ID(1, Settings.LoaderIOS)) < 0)
			{
				WindowPrompt(tr("Attention"), tr("IOS not found on the titles list. If you are sure it is installed in this slot, ignore this warning"), tr("OK"));
			}
			else if(Settings.LoaderIOS == 254)
			{
				WindowPrompt(tr("Attention"), tr("Selected IOS is BootMii IOS. If you are sure it is not BootMii and another IOS is installed in this slot, ignore this warning"), tr("OK"));
			}
		}
	}

	//! Settings: Games IOS
	else if (ret == ++Idx)
	{
		if(!Settings.godmode)
			return MENU_NONE;
		if (++Settings.AutoIOS >= GAME_IOS_MAX) Settings.AutoIOS = GAME_IOS_AUTO;
		Options->ClearList();
		SetOptionNames();
		SetOptionValues();
	}

	//! Settings: Custom Games IOS
	else if (Settings.AutoIOS == GAME_IOS_CUSTOM && ret == ++Idx)
	{
		if(!Settings.godmode)
			return MENU_NONE;

		char entered[4];
		snprintf(entered, sizeof(entered), "%i", Settings.cios);
		if(OnScreenNumpad(entered, sizeof(entered)))
		{
			Settings.cios = LIMIT(atoi(entered), 200, 255);

			if(NandTitles.IndexOf(TITLE_ID(1, Settings.cios)) < 0)
			{
				WindowPrompt(tr("Attention"), tr("IOS not found on the titles list. If you are sure it is installed in this slot, ignore this warning"), tr("OK"));
			}
			else if(Settings.cios == 254)
			{
				WindowPrompt(tr("Attention"), tr("Selected IOS is BootMii IOS. If you are sure it is not BootMii and another IOS is installed in this slot, ignore this warning"), tr("OK"));
			}
		}
	}

	//! Settings: Quick Boot
	else if (ret == ++Idx)
	{
		if (++Settings.quickboot >= MAX_ON_OFF) Settings.quickboot = 0;
	}

	//! Settings: Block IOS Reload
	else if (ret == ++Idx )
	{
		if (++Settings.BlockIOSReload >= 3) Settings.BlockIOSReload = 0;
	}

	//! Settings: Return To
	else if (ret == ++Idx)
	{
		char tidChar[10];
		bool getChannel = TitleSelector(tidChar);
		if (getChannel)
			snprintf(Settings.returnTo, sizeof(Settings.returnTo), "%s", tidChar);
	}

	//! Settings: EmuNAND Save Mode
	else if (ret == ++Idx )
	{
		if (Settings.SDMode)
		{
			// D2X can't load a game from an SD and save to an SD at the same time
			WindowPrompt(tr("EmuNAND Error"), tr("NAND emulation is not available in SD Card mode"), tr("OK"));
			Settings.NandEmuMode = EMUNAND_OFF;
		}
		else if (Settings.AutoIOS == GAME_IOS_CUSTOM && !IosLoader::IsD2X(Settings.cios))
		{
			WindowPrompt(tr("EmuNAND Error"), tr("NAND emulation is only available with D2X cIOS"), tr("OK"));
			Settings.NandEmuMode = EMUNAND_OFF;
		}
		else if (++Settings.NandEmuMode >= EMUNAND_NEEK) Settings.NandEmuMode = EMUNAND_OFF;
	}

	//! Settings: EmuNAND Channel Mode
	else if (ret == ++Idx )
	{
		if(++Settings.NandEmuChanMode >= EMUNAND_MAX) Settings.NandEmuChanMode = EMUNAND_PARTIAL;
	}

	//! Settings: Hooktype
	else if (ret == ++Idx )
	{
		if (++Settings.Hooktype >= 8) Settings.Hooktype = 0;
	}

	//! Settings: Wiird Debugger
	else if (ret == ++Idx )
	{
		if (++Settings.WiirdDebugger >= MAX_ON_OFF) Settings.WiirdDebugger = 0;
	}

	//! Settings: Wiird Debugger Pause on Start
	else if (ret == ++Idx )
	{
		if (++Settings.WiirdDebuggerPause >= MAX_ON_OFF) Settings.WiirdDebuggerPause = 0;
	}

	//! Settings: Channel Launcher
	else if (ret == ++Idx )
	{
		if (++Settings.UseChanLauncher >= MAX_ON_OFF) Settings.UseChanLauncher = 0;
	}

	//! Settings: TITLE - GameCube Settings
	else if (ret == ++Idx)
	{
		// This one is a category title
	}

	//! Settings: GameCube Source
	else if (ret == ++Idx)
	{
		if (++Settings.GameCubeSource >= CG_SOURCE_MAX_CHOICE) Settings.GameCubeSource = 0;
	}

	//! Settings: GameCube Mode
	else if (ret == ++Idx)
	{
		if (++Settings.GameCubeMode >= CG_MODE_MAX_CHOICE) Settings.GameCubeMode = 0;
	}

	//! Settings: DML + NIN + Devo Progressive Patch
	else if (ret == ++Idx)
	{
		if (++Settings.DMLProgPatch >= MAX_ON_OFF) Settings.DMLProgPatch = 0;
	}

	//! Settings: TITLE - GameCube DM(L) + Nintendont
	else if (ret == ++Idx)
	{
		// This one is a category title
	}

	//! Settings: DML + NIN Video Mode
	else if (ret == ++Idx)
	{
		Settings.DMLVideo++;
		if(Settings.DMLVideo == DML_VIDEO_FORCE_PATCH) // Skip Force Patch
			Settings.DMLVideo++;
		if(Settings.DMLVideo >= DML_VIDEO_MAX_CHOICE) Settings.DMLVideo = 0;
	}

	//! Settings: DML + NIN Force Widescreen
	else if (ret == ++Idx)
	{
		if (++Settings.DMLWidescreen >= MAX_ON_OFF) Settings.DMLWidescreen = 0;
	}

	//! Settings: DML + NIN Debug
	else if (ret == ++Idx)
	{
		if (++Settings.DMLDebug >= 3) Settings.DMLDebug = 0;
	}

	//! Settings: DML + NIN MultiDiscPrompt
	else if (ret == ++Idx)
	{
		if (++Settings.MultiDiscPrompt >= MAX_ON_OFF) Settings.MultiDiscPrompt = 0;
	}

	//! Settings: TITLE - GameCube DIOS MIOS (Lite)
	else if (ret == ++Idx)
	{
		// This one is a category title
	}

	//! Settings: DML + NIN NMM Mode
	else if (ret == ++Idx)
	{
		if (++Settings.DMLNMM >= 3) Settings.DMLNMM = 0;
	}

	//! Settings: DML PAD Hook
	else if (ret == ++Idx)
	{
		if (++Settings.DMLPADHOOK >= MAX_ON_OFF) Settings.DMLPADHOOK = 0;
	}

	//! Settings: DML Extended No Disc
	else if (ret == ++Idx)
	{
		if (++Settings.DMLNoDisc2 >= MAX_ON_OFF) Settings.DMLNoDisc2 = 0;
	}

	//! Settings: DML Screenshot
	else if (ret == ++Idx)
	{
		if (++Settings.DMLScreenshot >= MAX_ON_OFF) Settings.DMLScreenshot = 0;
	}

	//! Settings: DML LED Activity
	else if (ret == ++Idx)
	{
		if (++Settings.DMLActivityLED >= MAX_ON_OFF) Settings.DMLActivityLED = 0;
	}

	//! Settings: DML Japanese Patch
	else if (ret == ++Idx)
	{
		if (++Settings.DMLJPNPatch >= MAX_ON_OFF) Settings.DMLJPNPatch = 0;
	}

	//! Settings: TITLE - Nintendont
	else if (ret == ++Idx)
	{
		// This one is a category title
	}

	//! Settings: NIN Auto Boot
	else if (ret == ++Idx)
	{
		if (++Settings.NINAutoboot >= MAX_ON_OFF) Settings.NINAutoboot = 0;
	}

	//! Settings: NIN Nincfg.bin file
	else if (ret == ++Idx)
	{
		if (++Settings.NINSettings > AUTO) Settings.NINSettings = 0;
	}

	//! Settings: NIN Video Deflicker
	else if (ret == ++Idx)
	{
		if (++Settings.NINDeflicker >= MAX_ON_OFF) Settings.NINDeflicker = 0;
	}

	//! Settings: NIN PAL50 Patch
	else if (ret == ++Idx)
	{
		if (++Settings.NINPal50Patch >= MAX_ON_OFF) Settings.NINPal50Patch = 0;
	}

	//! Settings: WiiU Widescreen
	else if (ret == ++Idx)
	{
		if (++Settings.NINWiiUWide >= MAX_ON_OFF) Settings.NINWiiUWide = 0;
	}

	//! Settings: NIN VideoScale
	else if (ret == ++Idx)
	{
		Settings.NINVideoScale == 0 ? Settings.NINVideoScale = 40 : Settings.NINVideoScale = 0;
		Options->ClearList();
		SetOptionNames();
		SetOptionValues();
	}
	
	else if (Settings.NINVideoScale != 0 && ret == ++Idx)
	{
		char entrie[20];
		snprintf(entrie, sizeof(entrie), "%i", Settings.NINVideoScale);
		int ret = OnScreenNumpad(entrie, sizeof(entrie));
		if(ret)
			Settings.NINVideoScale = LIMIT(atoi(entrie), 40, 120);
	}

	//! Settings: NIN VideoOffset
	else if (ret == ++Idx)
	{
		char entrie[20];
		snprintf(entrie, sizeof(entrie), "%i", Settings.NINVideoOffset);
		int ret = OnScreenNumpad(entrie, sizeof(entrie));
		if(ret)
			Settings.NINVideoOffset = LIMIT(atoi(entrie), -20, 20);
	}

	//! Settings: NIN Remove Read Speed Limiter
	else if (ret == ++Idx)
	{
		if (++Settings.NINRemlimit >= MAX_ON_OFF) Settings.NINRemlimit = 0;
	}

	//! Settings: NIN Arcade Mode
	else if (ret == ++Idx)
	{
		if (++Settings.NINArcadeMode >= MAX_ON_OFF) Settings.NINArcadeMode = 0;
	}

	//! Settings: NIN CC Rumble
	else if (ret == ++Idx)
	{
		if (++Settings.NINCCRumble >= MAX_ON_OFF) Settings.NINCCRumble = 0;
	}

	//! Settings: NIN Skip IPL
	else if (ret == ++Idx)
	{
		if (++Settings.NINSkipIPL >= MAX_ON_OFF) Settings.NINSkipIPL = 0;
	}

	//! Settings: NIN BBA Emulation
	else if (ret == ++Idx)
	{
		if (++Settings.NINBBA >= MAX_ON_OFF) Settings.NINBBA = 0;
	}

	//! Settings: NIN BBA Net Profile
	else if (ret == ++Idx)
	{
		if (++Settings.NINBBAProfile >= NIN_BBA_MAX_CHOICE) Settings.NINBBAProfile = 0;
	}

	//! Settings: NIN Memory Card Emulation
	else if (ret == ++Idx)
	{
		if (++Settings.NINMCEmulation >= NIN_MC_MAX_CHOICE) Settings.NINMCEmulation = 0;
	}

	//! Settings: NIN Memory Card Blocks Size
	else if (ret == ++Idx)
	{
		if (++Settings.NINMCSize >= 6) Settings.NINMCSize = 0;
		if (Settings.NINMCSize == 5)
			WindowPrompt(tr("Attention"), tr("Emulation of memory cards with 2043 blocs has issues with Nintendont. Use at your own risk"), tr("OK"));
	}

	//! Settings: NIN USB-HID controller
	else if (ret == ++Idx)
	{
		if (++Settings.NINUSBHID >= MAX_ON_OFF) Settings.NINUSBHID = 0;
	}

	//! Settings: NIN MaxPads - Number of Gamecube controllers
	else if (ret == ++Idx)
	{
		if (++Settings.NINMaxPads >= 5) Settings.NINMaxPads = 0;
	}

	//! Settings: NIN Native Controller
	else if (ret == ++Idx)
	{
		if (++Settings.NINNativeSI >= MAX_ON_OFF) Settings.NINNativeSI = 0;
	}

	//! Settings: NIN LED Activity
	else if (ret == ++Idx)
	{
		if (++Settings.NINLED >= MAX_ON_OFF) Settings.NINLED = 0;
	}

	//! Settings: NIN OS Report
	else if (ret == ++Idx)
	{
		if (++Settings.NINOSReport >= MAX_ON_OFF) Settings.NINOSReport = 0;
	}

	//! Settings: NIN Log to file
	else if (ret == ++Idx)
	{
		if (++Settings.NINLog >= MAX_ON_OFF) Settings.NINLog = 0;
	}

	//! Settings: TITLE - Devolution
	else if (ret == ++Idx)
	{
		// This one is a category title
	}

	//! Settings: DEVO Memory Card Emulation
	else if (ret == ++Idx)
	{
		if (++Settings.DEVOMCEmulation >= DEVO_MC_MAX_CHOICE) Settings.DEVOMCEmulation = 0;
	}

	//! Settings: DEVO Widescreen Patch
	else if (ret == ++Idx)
	{
		if (++Settings.DEVOWidescreen >= MAX_ON_OFF) Settings.DEVOWidescreen = 0;
	}

	//! Settings: DEVO Activity LED
	else if (ret == ++Idx)
	{
		if (++Settings.DEVOActivityLED >= MAX_ON_OFF) Settings.DEVOActivityLED = 0;
	}

	//! Settings: DEVO F-Zero AX unlock patch
	else if (ret == ++Idx)
	{
		if (++Settings.DEVOFZeroAX >= MAX_ON_OFF) Settings.DEVOFZeroAX = 0;
	}

	//! Settings: DEVO Timer Fix
	else if (ret == ++Idx)
	{
		if (++Settings.DEVOTimerFix >= MAX_ON_OFF) Settings.DEVOTimerFix = 0;
	}

	//! Settings: DEVO Direct Button Mapping
	else if (ret == ++Idx)
	{
		if (++Settings.DEVODButtons >= MAX_ON_OFF) Settings.DEVODButtons = 0;
	}

	//! Settings: DEVO Crop Overscan
	else if (ret == ++Idx)
	{
		if (++Settings.DEVOCropOverscan >= MAX_ON_OFF) Settings.DEVOCropOverscan = 0;
	}

	//! Settings: DEVO Disc Read Delay
	else if (ret == ++Idx)
	{
		if (++Settings.DEVODiscDelay >= MAX_ON_OFF) Settings.DEVODiscDelay = 0;
	}

	SetOptionValues();

	return MENU_NONE;
}
