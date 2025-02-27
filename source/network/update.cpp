/***************************************************************************
 * Copyright (C) 2009
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
 *
 * update.cpp
 *
 * Update operations
 * for Wii-Xplorer 2009
 ***************************************************************************/
#include <stdio.h>
#include <string.h>
#include <ogcsys.h>
#include <string>

#include "gecko.h"
#include "zipfile.h"
#include "https.h"
#include "networkops.h"
#include "settings/csettings.h"
#include "settings/gametitles.h"
#include "language/gettext.h"
#include "language/updatelanguage.h"
#include "utils/showerror.h"
#include "prompts/promptwindows.h"
#include "prompts/progresswindow.h"
#include "fileoperations/fileops.h"
#include "xml/gametdb.hpp"
#include "sys.h"
#include "svnrev.h"

/****************************************************************************
 * Checking if an Update is available
 ***************************************************************************/
int DownloadFileToPath(const char *url, const char *dest)
{
	const char *filename = strrchr(url, '/') + 1;
	ProgressCancelEnable(true);
	StartProgress(tr("Downloading"), 0, filename, true, true);

	struct download file = {};
	file.show_progress = true;
	downloadfile(url, &file);
	if (file.size > 0)
	{
		FILE *savefile = fopen(dest, "wb");
		if (!savefile)
		{
			MEM2_free(file.data);
			ShowError(tr("Could not save file to destination"));
			return -7;
		}
		fwrite(file.data, 1, file.size, savefile);
		fclose(savefile);
		MEM2_free(file.data);
	}
	ProgressStop();
	ProgressCancelEnable(false);
	return file.size;
}

static bool CheckNewGameTDBVersion(const char *url)
{
	gprintf("Checking GameTDB database version\n");
	struct download file = {};
	file.gametdbcheck = true;
	downloadfile(url, &file);

	if (file.gametdbcheck <= 0)
		return false;

	std::string Filepath(Settings.titlestxt_path);
	if (Filepath.back() != '/')
		Filepath += '/';
	Filepath += "wiitdb.xml";

	GameTDB XML_DB;

	if (!XML_DB.OpenFile(Filepath.c_str()))
		return true; //! If no file exists we need the file

	u64 ExistingVersion = XML_DB.GetGameTDBVersion();
	XML_DB.CloseFile();

	gprintf("GameTDB local database: %llu GameTDB online database: %llu\n", ExistingVersion, file.gametdbcheck);

	return (ExistingVersion != file.gametdbcheck);
}

int UpdateGameTDB()
{
	if (CheckNewGameTDBVersion(Settings.URL_GameTDB) == false)
	{
		gprintf("GameTDB database is already up to date\n");
		return -2;
	}

	gprintf("Updating database\n");

	std::string ZipPath(Settings.titlestxt_path);
	if (ZipPath.back() != '/')
		ZipPath += '/';
	ZipPath += "wiitdb.zip";

	int filesize = DownloadFileToPath(Settings.URL_GameTDB, ZipPath.c_str());

	if (filesize <= 0)
		return -1;

	ZipFile zFile(ZipPath.c_str());

	bool result = zFile.ExtractAll(Settings.titlestxt_path);

	//! The zip file is not needed anymore so we can remove it
	remove(ZipPath.c_str());

	//! Reload all titles and reload cached titles because the file changed now.
	GameTitles.Reset();
	GameTitles.LoadTitlesFromGameTDB(Settings.titlestxt_path);
	return (result ? filesize : -1);
}

static void UpdateIconPng()
{
	char iconpath[200];
	struct download file = {};
	downloadfile("https://raw.githubusercontent.com/wiidev/usbloadergx/updates/icon.png", &file);
	if (file.size > 0)
	{
		snprintf(iconpath, sizeof(iconpath), "%sicon.png", Settings.ConfigPath);
		FILE *pfile = fopen(iconpath, "wb");
		if (pfile)
		{
			fwrite(file.data, 1, file.size, pfile);
			fclose(pfile);
		}
		MEM2_free(file.data);
	}
}

static void UpdateMetaXml()
{
	char xmlpath[200];
	struct download file = {};
	downloadfile("https://raw.githubusercontent.com/wiidev/usbloadergx/updates/meta.xml", &file);
	if (file.size > 0)
	{
		snprintf(xmlpath, sizeof(xmlpath), "%smeta.xml", Settings.ConfigPath);
		FILE *pfile = fopen(xmlpath, "wb");
		if (pfile)
		{
			fwrite(file.data, 1, file.size, pfile);
			fclose(pfile);
		}
		MEM2_free(file.data);
	}
}

static int ApplicationDownload(void)
{
	std::string DownloadURL;
	int newrev = 0;
	int currentrev = atoi(GetRev());

	struct download file = {};
#ifdef FULLCHANNEL
	downloadfile("https://raw.githubusercontent.com/wiidev/usbloadergx/updates/update_wad.txt", &file);
#else
	downloadfile("https://raw.githubusercontent.com/wiidev/usbloadergx/updates/update_dol.txt", &file);
#endif

	if (file.size > 0)
	{
		// First line of the text file is the revisionc
		newrev = atoi((char *)file.data);
		// 2nd line of the text file is the url
		char *ptr = strchr((char *)file.data, '\n');
		while (ptr && (*ptr == '\r' || *ptr == '\n' || *ptr == ' '))
			ptr++;
		while (ptr && *ptr != '\0' && *ptr != '\r' && *ptr != '\n')
		{
			DownloadURL.push_back(*ptr);
			ptr++;
		}

		MEM2_free(file.data);
	}

	if (newrev <= currentrev)
	{
		WindowPrompt(tr("Update Wizard"), tr("No new updates available"), tr("OK"));
		return 0;
	}

	bool update_error = false;
	char tmppath[250];

#ifdef FULLCHANNEL
	snprintf(tmppath, sizeof(tmppath), "%s/ULNR.wad", Settings.BootDevice);
#else
	char realpath[250];
	snprintf(realpath, sizeof(realpath), "%sboot.dol", Settings.ConfigPath);
	snprintf(tmppath, sizeof(tmppath), "%sboot.tmp", Settings.ConfigPath);
#endif

	int ret = DownloadFileToPath(DownloadURL.c_str(), tmppath);
	if (ret < 1024 * 1024)
	{
		remove(tmppath);
		WindowPrompt(tr("Update Error"), tr("An error occurred while downloading update"), tr("OK"));
		update_error = true;
	}
	else
	{
#ifdef FULLCHANNEL
		FILE *wadFile = fopen(tmppath, "rb");
		if (!wadFile)
		{
			update_error = true;
			WindowPrompt(tr("Update Error"), tr("An error occurred while updating USB Loader GX"), tr("OK"));
			return -1;
		}

		int error = Wad_Install(wadFile);
		if (error)
		{
			update_error = true;
			ShowError(tr("Update installation failed with error %i"), error);
		}
		else
			WindowPrompt(tr("Update Wizard"), tr("USB Loader GX updated successfully"), tr("OK"));

		RemoveFile(tmppath);
#else
		gprintf("%s\n%s\n", realpath, tmppath);
		RemoveFile(realpath);
		if (!RenameFile(tmppath, realpath))
			update_error = true;
#endif
	}

	if (update_error)
	{
		ShowError(tr("An error occurred while downloading update"));
		return -1;
	}

	UpdateIconPng();
	UpdateMetaXml();
	UpdateGameTDB();

	WindowPrompt(tr("Update Wizard"), tr("Restarting USB Loader GX"), 0, 0, 0, 0, 150);
	RebootApp();

	return 0;
}

int UpdateApp()
{
	if (!IsNetworkInit() && !NetworkInitPrompt())
	{
		WindowPrompt(tr("Network Error"), tr("An error occurred while initializing network connection"), tr("OK"));
		return -1;
	}

	int choice = WindowPrompt(tr("Update Wizard"), tr("Select an option to continue"), "USB Loader GX", tr("Database"), tr("Language Files"), tr("Cancel"));
	if (choice == 0)
		return 0;

	if (choice == 1)
	{
		return ApplicationDownload();
	}
	else if (choice == 2)
	{
		int gameTDB = UpdateGameTDB();
		if (gameTDB == -2)
		{
			WindowPrompt(tr("Update Wizard"), tr("GameTDB database file is already up to date"), tr("OK"));
			return 1;
		}
		else if (gameTDB == -1)
		{
			WindowPrompt(tr("Update Error"), tr("An error occurred while updating GameTDB database"), tr("OK"));
			return 1;
		}
		else
		{
			WindowPrompt(tr("Update Wizard"), tr("GameTDB database updated successfully"), tr("OK"));
			return 1;
		}
	}
	else if (choice == 3)
	{
		if (UpdateLanguageFiles() > 0)
			WindowPrompt(tr("Update Wizard"), tr("Language files updated successfully"), tr("OK"));
	}

	return 1;
}
