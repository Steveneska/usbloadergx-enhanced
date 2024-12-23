/****************************************************************************
 * Copyright (C) 2011
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
#ifndef CATEGORY_PROMPT_HPP_
#define CATEGORY_PROMPT_HPP_

#include "gui/gui_checkboxbrowser.hpp"

class CategoryPrompt : public GuiWindow, public sigslot::has_slots<>
{
	public:
		CategoryPrompt(const std::string &title);
		virtual ~CategoryPrompt();
		int Show();
		bool categoriesChanged() const { return changed; }
	protected:
		void markChanged() { changed = true; }
		sigslot::signal0<> browserRefresh;
		sigslot::signal0<> resetChanges;
		GuiCheckboxBrowser *browser;
	private:
		bool changed;

		GuiImageData *bgImgData;
		GuiImageData *browserImgData;
		GuiImageData *btnOutline;
		GuiImageData *addImgData;
		GuiImageData *deleteImgData;
		GuiImageData *editImgData;

		GuiImage *browserImg;
		GuiImage *bgImg;
		GuiImage *addImg;
		GuiImage *deleteImg;
		GuiImage *editImg;
		GuiImage *backImg;
		GuiImage *saveImg;

		GuiButton *backBtn;
		GuiButton *homeButton;
		GuiButton *addButton;
		GuiButton *deleteButton;
		GuiButton *editButton;
		GuiButton *saveButton;

		GuiText *titleTxt;
		GuiText *addTxt;
		GuiText *deleteTxt;
		GuiText *editTxt;
		GuiText *backTxt;
		GuiText *saveTxt;

		GuiTrigger trigA;
		GuiTrigger trigB;
		GuiTrigger trigHome;
		GuiTrigger trigPlus;
		GuiTrigger trigMinus;
		GuiTrigger trig1;
};

#endif
