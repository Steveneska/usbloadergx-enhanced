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
#ifndef GCGAMELOAD_SM_HPP
#define GCGAMELOAD_SM_HPP

#include "settingsmenu.hpp"
#include "settings/cgamesettings.h"

class GCGameLoadSM : public SettingsMenu
{
	public:
		GCGameLoadSM(struct discHdr *Header);
		virtual ~GCGameLoadSM();
	protected:
		void SetDefaultConfig();
		void SetOptionNames();
		void SetOptionValues();
		int GetMenuInternal();

		struct discHdr *Header;
		GameCFG GameConfig;
		OptionList GuiOptions;

		GuiText * saveBtnTxt;
		GuiImage * saveBtnImg;
		GuiButton * saveBtn;
};


#endif
