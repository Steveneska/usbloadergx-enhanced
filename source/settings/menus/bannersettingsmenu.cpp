/****************************************************************************
 * Copyright (C) 2012 Dimok
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ****************************************************************************/
#include <unistd.h>
#include "bannersettingsmenu.hpp"
#include "settings/csettings.h"
#include "settings/settingsprompts.h"
#include "prompts/promptwindows.h"
#include "language/gettext.h"

static const char * AnimStartText[] =
{
	trNOOP( "During zoom" ),
	trNOOP( "After zoom" ),
};

static const char * OnOffText[] =
{
	trNOOP( "Off" ),
	trNOOP( "On" )
};

BannerSettingsMenu::BannerSettingsMenu()
	: SettingsMenu(tr("Banner Animation"), &GuiOptions, MENU_NONE)
{
	int Idx = 0;
	Options->SetName(Idx++, "%s", tr("Cache BNR files" ));
	Options->SetName(Idx++, "%s", tr("Animation start" ));
	Options->SetName(Idx++, "%s", tr("Zoom duration" ));
	Options->SetName(Idx++, "%s", tr("Grid scroll speed" ));
	Options->SetName(Idx++, "%s", tr("Frame projection X offset" ));
	Options->SetName(Idx++, "%s", tr("Frame projection Y offset" ));
	Options->SetName(Idx++, "%s", tr("Frame projection width" ));
	Options->SetName(Idx++, "%s", tr("Frame projection height" ));
	Options->SetName(Idx++, "%s", tr("GameCube banner scale" ));

	SetOptionValues();
}

void BannerSettingsMenu::SetOptionValues()
{
	int Idx = 0;

	//! Settings: Cache BNR Files
	Options->SetValue(Idx++, "%s", tr(OnOffText[Settings.CacheBNRFiles]));

	//! Settings: Animation Start
	Options->SetValue(Idx++, "%s", tr(AnimStartText[Settings.BannerAnimStart]));

	//! Settings: Zoom Duration (Speed)
	Options->SetValue(Idx++, "%i %s", Settings.BannerZoomDuration, tr("frames"));

	//! Settings: Grid Scroll Speed
	Options->SetValue(Idx++, "%g %s/%s", Settings.BannerGridSpeed, tr("pixels"), tr("frame"));

	//! Settings: Frame Projection X-Offset
	Options->SetValue(Idx++, "%g %s", Settings.BannerProjectionOffsetX, tr("pixels"));

	//! Settings: Frame Projection Y-Offset
	Options->SetValue(Idx++, "%g %s", Settings.BannerProjectionOffsetY, tr("pixels"));

	//! Settings: Frame Projection Width
	Options->SetValue(Idx++, "%g %s", Settings.BannerProjectionWidth, tr("pixels"));

	//! Settings: Frame Projection Height
	Options->SetValue(Idx++, "%g %s", Settings.BannerProjectionHeight, tr("pixels"));

	//! Settings: GC Banner Scale
	Options->SetValue(Idx++, "%g", Settings.GCBannerScale);
}

int BannerSettingsMenu::GetMenuInternal()
{
	int ret = optionBrowser->GetClickedOption();

	if (ret < 0)
		return MENU_NONE;

	int Idx = -1;

	//! Settings: Cache BNR Files
	if (ret == ++Idx)
	{
		if (++Settings.CacheBNRFiles >= MAX_ON_OFF) Settings.CacheBNRFiles = 0;
	}

	//! Settings: Animation Start
	else if (ret == ++Idx)
	{
		if (++Settings.BannerAnimStart > 1) Settings.BannerAnimStart = 0;
	}

	//! Settings: Zoom Duration (Speed)
	else if (ret == ++Idx)
	{
		char entrie[20];
		snprintf(entrie, sizeof(entrie), "%i", Settings.BannerZoomDuration);
		int ret = OnScreenNumpad(entrie, sizeof(entrie));
		if(ret)
			Settings.BannerZoomDuration = atoi(entrie);
	}

	//! Settings: Grid Scroll Speed
	else if (ret == ++Idx)
	{
		char entrie[20];
		snprintf(entrie, sizeof(entrie), "%g", Settings.BannerGridSpeed);
		int ret = OnScreenNumpad(entrie, sizeof(entrie));
		if(ret)
			Settings.BannerGridSpeed = atof(entrie);
	}

	//! Settings: Frame Projection X-Offset
	else if (ret == ++Idx)
	{
		char entrie[20];
		snprintf(entrie, sizeof(entrie), "%g", Settings.BannerProjectionOffsetX);
		int ret = OnScreenNumpad(entrie, sizeof(entrie));
		if(ret)
			Settings.BannerProjectionOffsetX = atof(entrie);
	}

	//! Settings: Frame Projection Y-Offset
	else if (ret == ++Idx)
	{
		char entrie[20];
		snprintf(entrie, sizeof(entrie), "%g", Settings.BannerProjectionOffsetY);
		int ret = OnScreenNumpad(entrie, sizeof(entrie));
		if(ret)
			Settings.BannerProjectionOffsetY = atof(entrie);
	}

	//! Settings: Frame Projection Width
	else if (ret == ++Idx)
	{
		char entrie[20];
		snprintf(entrie, sizeof(entrie), "%g", Settings.BannerProjectionWidth);
		int ret = OnScreenNumpad(entrie, sizeof(entrie));
		if(ret)
			Settings.BannerProjectionWidth = atof(entrie);
	}

	//! Settings: Frame Projection Height
	else if (ret == ++Idx)
	{
		char entrie[20];
		snprintf(entrie, sizeof(entrie), "%g", Settings.BannerProjectionHeight);
		int ret = OnScreenNumpad(entrie, sizeof(entrie));
		if(ret)
			Settings.BannerProjectionHeight = atof(entrie);
	}

	//! Settings: GC Banner Scale
	else if (ret == ++Idx)
	{
		char entrie[20];
		snprintf(entrie, sizeof(entrie), "%g", Settings.GCBannerScale);
		int ret = OnScreenNumpad(entrie, sizeof(entrie));
		if(ret)
			Settings.GCBannerScale = atof(entrie);
	}

	SetOptionValues();

	return MENU_NONE;
}
