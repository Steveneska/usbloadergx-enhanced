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
#include "guisettingsmenu.hpp"
#include "controls/devicehandler.hpp"
#include "settings/csettings.h"
#include "prompts/promptwindows.h"
#include "language/gettext.h"
#include "settings/settingsprompts.h"
#include "settings/gametitles.h"
#include "settings/cgamecategories.hpp"
#include "systemmenu/systemmenuresources.h"
#include "system/iosloader.h"
#include "usbloader/wbfs.h"
#include "themes/ctheme.h"
#include "utils/tools.h"

static const char * OnOffText[MAX_ON_OFF] =
{
	trNOOP( "Off" ),
	trNOOP( "On" )
};

static const char * GameInfoText[GAMEINFO_MAX] =
{
	trNOOP( "GameID" ),
	trNOOP( "Game region" ),
	trNOOP( "Both" ),
	trNOOP( "Neither" )
};

static const char * FlipXText[XFLIP_MAX][3] =
{
	{ trNOOP( "Right" ), "/", trNOOP( "Next" ) },
	{ trNOOP( "Left" ), "/", trNOOP( "Previous" ) },
	{ trNOOP( "Like " ), "System ", trNOOP( "Menu" ) },
	{ trNOOP( "Right" ), "/", trNOOP( "Previous" ) },
	{ trNOOP( "Disk " ), "Flip", trNOOP("-X") }
};

static const char * PromptButtonsText[MAX_ON_OFF] =
{
	trNOOP( "Normal" ),
	trNOOP( "Widescreen fix" ),
};

static const char * GameWindowText[3] =
{
	trNOOP( "Banner animation" ),
	trNOOP( "Rotating disc" ),
	trNOOP( "Banner on channel" ),
};

static const char * KeyboardText[KEYBOARD_MAX] =
{
	"QWERTY",
	"DVORAK",
	"QWERTZ",
	"AZERTY",
	"QWERTY 2"
};

static const char * DiscArtDownloadText[DISCARTS_MAX_CHOICE] =
{
	trNOOP( "Original/Custom" ),
	trNOOP( "Custom/Original" ),
	trNOOP( "Original only" ),
	trNOOP( "Custom only" )
};

static const char * CoversFullDownloadText[COVERSFULL_MAX_CHOICE] =
{
	trNOOP( "High quality" ),
	trNOOP( "Low quality" ),
	trNOOP( "High/Low quality" ),
	trNOOP( "Low/High quality" )
};

static const char * CoverActionText[COVER_ACTION_MAX] =
{
	trNOOP( "Download" ),
	trNOOP( "Show details" )
};

static const char *ScreensaverText[SCREENSAVER_MAX] =
{
	trNOOP( "Off" ),
	trNOOP( "3 mins" ),
	trNOOP( "5 mins" ),
	trNOOP( "10 mins" ),
	trNOOP( "20 mins" ),
	trNOOP( "30 mins" ),
	trNOOP( "1 hour" )
};

static const char * BannerFavIconText[BANNER_FAVICON_MAX_CHOICE] =
{
	trNOOP( "Off" ),
	trNOOP( "Round" ),
	trNOOP( "/\\/\\" ),
	trNOOP( "Two lines" ),
	trNOOP( "One line A" ),
	trNOOP( "One line B" )
};

static const char * HomeMenuText[HOME_MENU_MAX_CHOICE] =
{
	trNOOP( "System default" ),
	trNOOP( "Full menu" ),
	trNOOP( "Default" )
};

GuiSettingsMenu::GuiSettingsMenu()
	: SettingsMenu(tr("Interface"), &GuiOptions, MENU_NONE)
{
	int Idx = 0;
	Options->SetName(Idx++, "%s", tr("Display language" ));
	Options->SetName(Idx++, "%s", tr("Display" ));
	Options->SetName(Idx++, "%s", tr("Clock" ));
	Options->SetName(Idx++, "%s", tr("Clock font scaling" ));
	Options->SetName(Idx++, "%s", tr("Show tooltips" ));
	Options->SetName(Idx++, "%s", tr("Tooltip delay" ));
	Options->SetName(Idx++, "%s", tr("Flip X" ));
	Options->SetName(Idx++, "%s", tr("Game window style" ));
	Options->SetName(Idx++, "%s", tr("Prompt buttons" ));
	Options->SetName(Idx++, "%s", tr("Widescreen scale" ));
	Options->SetName(Idx++, "%s", tr("Display font scaling" ));
	Options->SetName(Idx++, "%s", tr("Keyboard" ));
	Options->SetName(Idx++, "%s", tr("Disc artwork type" ));
	Options->SetName(Idx++, "%s", tr("Full cover download" ));
	Options->SetName(Idx++, "%s", tr("Cover action" ));
	Options->SetName(Idx++, "%s", tr("Screensaver" ));
	Options->SetName(Idx++, "%s", tr("Remember last game" ));
	Options->SetName(Idx++, "%s", tr("Mark new games" ));
	Options->SetName(Idx++, "%s", tr("Show play count" ));
	Options->SetName(Idx++, "%s", tr("Show favorite on banner" ));
	Options->SetName(Idx++, "%s", tr("Show free space" ));
	Options->SetName(Idx++, "%s", tr("Show total games" ));
	Options->SetName(Idx++, "%s", tr("HOME Menu" ));
	Options->SetName(Idx++, "%s", tr("Use system font" ));
	Options->SetName(Idx++, "%s", tr("Virtual pointer speed" ));
	Options->SetName(Idx++, "%s", tr("Adjust overscan X" ));
	Options->SetName(Idx++, "%s", tr("Adjust overscan Y" ));

	SetOptionValues();
}

void GuiSettingsMenu::SetOptionValues()
{
	int Idx = 0;

	//! Settings: App Language
	const char * language = strrchr(Settings.language_path, '/');
	if(language)
		language += 1;
	if (!language || strcmp(Settings.language_path, "") == 0)
		Options->SetValue(Idx++, "%s", tr("Default" ));
	else
		Options->SetValue(Idx++, "%s", language);

	//! Settings: Display
	Options->SetValue(Idx++, "%s", tr( GameInfoText[Settings.sinfo] ));

	//! Settings: Clock
	if (Settings.hddinfo == CLOCK_HR12)
		Options->SetValue(Idx++, "12 %s", tr("hours" ));
	else if (Settings.hddinfo == CLOCK_HR24)
		Options->SetValue(Idx++, "24 %s", tr("hours" ));
	else if (Settings.hddinfo == OFF)
		Options->SetValue(Idx++, "%s", tr("Off" ));

	//! Settings: Clock Font Scale Factor
	Options->SetValue(Idx++, "%g", Settings.ClockFontScaleFactor);

	//! Settings: Tooltips
	Options->SetValue(Idx++, "%s", tr(OnOffText[Settings.tooltips]));

	//! Settings: Tooltip Delay
	Options->SetValue(Idx++, "%i %s", Settings.TooltipDelay, tr("ms"));

	//! Settings: Flip-X
	Options->SetValue(Idx++, "%s%s%s", tr(FlipXText[Settings.xflip][0]),
				FlipXText[Settings.xflip][1], tr( FlipXText[Settings.xflip][2] ));

	//! Settings: Game Window Mode
	Options->SetValue(Idx++, "%s", tr( GameWindowText[Settings.GameWindowMode] ));

	//! Settings: Prompts Buttons
	Options->SetValue(Idx++, "%s", tr( PromptButtonsText[Settings.wsprompt] ));

	//! Settings: Widescreen Factor
	Options->SetValue(Idx++, "%g", Settings.WSFactor);

	//! Settings: Font Scale Factor
	Options->SetValue(Idx++, "%g", Settings.FontScaleFactor);

	//! Settings: Keyboard
	Options->SetValue(Idx++, "%s", KeyboardText[Settings.keyset]);

	//! Settings: Disc Artwork Download
	Options->SetValue(Idx++, "%s", tr( DiscArtDownloadText[Settings.discart] ));

	//! Settings: Full Covers Download
	Options->SetValue(Idx++, "%s", tr( CoversFullDownloadText[Settings.coversfull] ));

	//! Settings: Cover Action
	Options->SetValue(Idx++, "%s", tr( CoverActionText[Settings.CoverAction] ));

	//! Settings: Screensaver
	Options->SetValue(Idx++, "%s", tr( ScreensaverText[Settings.screensaver] ));

	//! Settings: Remember Last Game
	Options->SetValue(Idx++, "%s", tr( OnOffText[Settings.RememberLastGame] ));

	//! Settings: Mark New Games
	Options->SetValue(Idx++, "%s", tr( OnOffText[Settings.marknewtitles] ));

	//! Settings: Show Play Count
	Options->SetValue(Idx++, "%s", tr( OnOffText[Settings.ShowPlayCount] ));

	//! Settings: Show Favorite on banner window
	Options->SetValue(Idx++, "%s", tr( BannerFavIconText[Settings.bannerFavIcon] ));

	//! Settings: Show Free Space
	Options->SetValue(Idx++, "%s", tr( OnOffText[Settings.ShowFreeSpace] ));

	//! Settings: Show Game Count
	Options->SetValue(Idx++, "%s", tr( OnOffText[Settings.ShowGameCount] ));

	//! Settings: Home Menu style
	Options->SetValue(Idx++, "%s", tr( HomeMenuText[Settings.HomeMenu] ));

	//! Settings: Use System Font
	Options->SetValue(Idx++, "%s", tr( OnOffText[Settings.UseSystemFont] ));

	//! Settings: Virtual Pointer Speed
	Options->SetValue(Idx++, "%g", Settings.PointerSpeed);

	//! Settings: Adjust Overscan X
	Options->SetValue(Idx++, "%i", Settings.AdjustOverscanX);

	//! Settings: Adjust Overscan Y
	Options->SetValue(Idx++, "%i", Settings.AdjustOverscanY);
}

int GuiSettingsMenu::GetMenuInternal()
{
	int ret = optionBrowser->GetClickedOption();

	if (ret < 0)
		return MENU_NONE;

	int Idx = -1;

	//! Settings: App Language
	if (ret == ++Idx)
	{
		if (!Settings.godmode)
		{
			WindowPrompt(tr("Parental Control"), tr("Permission denied. Unlock console to access settings"), tr("OK"));
			return MENU_NONE;
		}
		SetEffect(EFFECT_FADE, -20);
		while (GetEffect() > 0) usleep(100);
		HaltGui();
		if(parentElement)
		{
			((GuiWindow *) parentElement)->Remove(this);
			((GuiWindow *) parentElement)->SetState(STATE_DISABLED);
		}
		ResumeGui();

		int returnhere = 1;
		while (returnhere == 1)
			returnhere = MenuLanguageSelect();

		if (returnhere == 2)
		{
			// Language changed. Reload game titles with new lang code.
			GameTitles.Reset();
			GameTitles.LoadTitlesFromGameTDB(Settings.titlestxt_path);
			return MENU_SETTINGS;
		}

		HaltGui();
		if(parentElement)
		{
			((GuiWindow *) parentElement)->Append(this);
			((GuiWindow *) parentElement)->SetState(STATE_DEFAULT);
		}
		SetEffect(EFFECT_FADE, 20);
		ResumeGui();
		while (GetEffect() > 0) usleep(100);
	}

	// Settings: Display
	else if (ret == ++Idx)
	{
		if (++Settings.sinfo >= GAMEINFO_MAX) Settings.sinfo = 0;
	}

	// Settings: Clock
	else if (ret == ++Idx)
	{
		if (++Settings.hddinfo >= CLOCK_MAX) Settings.hddinfo = 0; //CLOCK
	}

	// Settings: Clock Font Scale Factor
	else if (ret == ++Idx)
	{
		char entrie[20];
		snprintf(entrie, sizeof(entrie), "%g", Settings.ClockFontScaleFactor);
		int ret = OnScreenNumpad(entrie, sizeof(entrie));
		if(ret)
			Settings.ClockFontScaleFactor = LIMIT(atof(entrie), 0.01f, 1.5f);
	}

	// Settings: Tooltips
	else if (ret == ++Idx)
	{
		if (++Settings.tooltips >= MAX_ON_OFF) Settings.tooltips = 0;
	}

	// Settings: Tooltip Delay
	else if (ret == ++Idx)
	{
		char entrie[20];
		snprintf(entrie, sizeof(entrie), "%i", Settings.TooltipDelay);
		int ret = OnScreenNumpad(entrie, sizeof(entrie));
		if(ret)
			Settings.TooltipDelay = atoi(entrie);
	}

	// Settings: Flip-X
	else if (ret == ++Idx)
	{
		if (++Settings.xflip >= XFLIP_MAX) Settings.xflip = 0;
	}

	// Settings: Game Window Mode
	else if (ret == ++Idx)
	{
		if (++Settings.GameWindowMode >= 3) Settings.GameWindowMode = 0;

		if(Settings.GameWindowMode != GAMEWINDOW_DISC && !SystemMenuResources::Instance()->IsLoaded()) {
			WindowPrompt(tr("Error"), tr("Banners are only available with AHBPROT. Install a newer version of Homebrew Channel to use this feature"), tr("OK"));
			Settings.GameWindowMode = GAMEWINDOW_DISC;
		}
	}

	// Settings: Prompts Buttons
	else if (ret == ++Idx)
	{
		if (++Settings.wsprompt >= MAX_ON_OFF) Settings.wsprompt = 0;
	}

	// Settings: Widescreen Factor
	else if (ret == ++Idx)
	{
		char entrie[20];
		snprintf(entrie, sizeof(entrie), "%g", Settings.WSFactor);
		int ret = OnScreenNumpad(entrie, sizeof(entrie));
		if(ret)
			Settings.WSFactor = LIMIT(atof(entrie), 0.01f, 1.5f);
	}

	// Settings: Font Scale Factor
	else if (ret == ++Idx)
	{
		char entrie[20];
		snprintf(entrie, sizeof(entrie), "%g", Settings.FontScaleFactor);
		int ret = OnScreenNumpad(entrie, sizeof(entrie));
		if(ret)
			Settings.FontScaleFactor = LIMIT(atof(entrie), 0.01f, 1.5f);
	}

	// Settings: Keyboard
	else if (ret == ++Idx)
	{
		if (++Settings.keyset >= KEYBOARD_MAX) Settings.keyset = 0;
	}

	// Settings: Disc Artwork Download
	else if (ret == ++Idx)
	{
		if (++Settings.discart >= DISCARTS_MAX_CHOICE) Settings.discart = 0;
	}

	// Settings: Full Covers Download
	else if (ret == ++Idx)
	{
		if (++Settings.coversfull >= COVERSFULL_MAX_CHOICE) Settings.coversfull = 0;
	}

	// Settings: Cover Action
	else if (ret == ++Idx)
	{
		if (++Settings.CoverAction >= COVER_ACTION_MAX) Settings.CoverAction = 0;
	}

	// Settings: Screensaver
	else if (ret == ++Idx)
	{
		if (++Settings.screensaver >= SCREENSAVER_MAX) Settings.screensaver = 0;

		SetWPADTimeout();
	}

	// Settings: Remember Last Game
	else if (ret == ++Idx)
	{
		if (++Settings.RememberLastGame >= MAX_ON_OFF) Settings.RememberLastGame = 0;
	}

	// Settings: Mark New Games
	else if (ret == ++Idx)
	{
		if (++Settings.marknewtitles >= MAX_ON_OFF) Settings.marknewtitles = 0;
	}

	// Settings: Show Play Count
	else if (ret == ++Idx)
	{
		if (++Settings.ShowPlayCount >= MAX_ON_OFF) Settings.ShowPlayCount = 0;
	}

	// Settings: Show favorite on banner window
	else if (ret == ++Idx)
	{
		if (++Settings.bannerFavIcon >= BANNER_FAVICON_MAX_CHOICE) Settings.bannerFavIcon = 0;
	}

	// Settings: Show Free Space
	else if (ret == ++Idx)
	{
		if (++Settings.ShowFreeSpace >= MAX_ON_OFF) Settings.ShowFreeSpace = 0;
	}

	// Settings: Show Game Count
	else if (ret == ++Idx)
	{
		if (++Settings.ShowGameCount >= MAX_ON_OFF) Settings.ShowGameCount = 0;
	}

	// Settings: Home Menu Style
	else if (ret == ++Idx)
	{
		if (++Settings.HomeMenu >= HOME_MENU_MAX_CHOICE) Settings.HomeMenu = 0;
	}

	// Settings: Use System Font
	else if (ret == ++Idx)
	{
		if (++Settings.UseSystemFont >= MAX_ON_OFF) Settings.UseSystemFont = 0;

		HaltGui();
		Theme::LoadFont(Settings.ConfigPath);
		ResumeGui();

		if(Settings.FontScaleFactor == 1.0f && Settings.UseSystemFont == ON)
			Settings.FontScaleFactor = 0.8f;
		else if(Settings.FontScaleFactor == 0.8f && Settings.UseSystemFont == OFF)
			Settings.FontScaleFactor = 1.0f;
	}

	// Settings: Virtual Pointer Speed
	else if (ret == ++Idx)
	{
		char entrie[20];
		snprintf(entrie, sizeof(entrie), "%g", Settings.PointerSpeed);
		int ret = OnScreenNumpad(entrie, sizeof(entrie));
		if(ret)
			Settings.PointerSpeed = atof(entrie);
	}

	// Settings: Adjust Overscan X
	else if (ret == ++Idx)
	{
		char entrie[20];
		snprintf(entrie, sizeof(entrie), "%i", Settings.AdjustOverscanX);
		int ret = OnScreenNumpad(entrie, sizeof(entrie));
		if(ret)
		{
			Settings.AdjustOverscanX = atoi(entrie);
			AdjustOverscan(Settings.AdjustOverscanX, Settings.AdjustOverscanY);
		}
	}

	// Settings: Adjust Overscan Y
	else if (ret == ++Idx)
	{
		char entrie[20];
		snprintf(entrie, sizeof(entrie), "%i", Settings.AdjustOverscanY);
		int ret = OnScreenNumpad(entrie, sizeof(entrie));
		if(ret)
		{
			Settings.AdjustOverscanY = atoi(entrie);
			AdjustOverscan(Settings.AdjustOverscanX, Settings.AdjustOverscanY);
		}
	}


	SetOptionValues();

	return MENU_NONE;
}
