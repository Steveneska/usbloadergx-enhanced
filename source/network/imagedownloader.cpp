/****************************************************************************
 * Copyright (C) 2011 Dimok
 * Copyright (C) 2012 Cyan
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
#include <gccore.h>
#include <string.h>
#include "imagedownloader.h"
#include "network/networkops.h"
#include "prompts/promptwindows.h"
#include "prompts/progresswindow.h"
#include "prompts/checkboxprompt.hpp"
#include "fileoperations/fileops.h"
#include "settings/csettings.h"
#include "settings/gametitles.h"
#include "language/gettext.h"
#include "usbloader/getmissinggamefiles.hpp"
#include "utils/stringtools.h"
#include "usbloader/gamelist.h"
#include "wstring.hpp"
#include "gecko.h"

#define VALID_IMAGE(x) (!(x->size == 36864 || x->size <= 1024 || x->size == 7386 || x->size <= 1174 || x->size == 4446 || x->data == NULL))

void ImageDownloader::DownloadImages()
{
	bool showBanner = (Settings.LoaderMode & MODE_GCGAMES);

	int choice = CheckboxWindow(tr( "Cover Downloader" ), 0, tr("3D Covers"), tr("2D Covers"), tr("Full Covers"), tr("Disc Artwork" ), showBanner ? tr( "Custom Banners" ) : 0, 0, showBanner ? 0x1F : 0xF); // Ask for download choice
	if (choice == 0 || choice == CheckedNone)
		return;

	ImageDownloader Downloader;
	Downloader.SetChoices(choice);
	Downloader.Start();
}

void ImageDownloader::Start()
{
	gprintf("CoverDownload start - choices: %04X\n", choices);

	MissingImagesCount = 0;
	FindMissingImages();

	if(MissingImagesCount == 0)
	{
		WindowPrompt(tr("Cover Downloader"), tr("No images missing"), tr("OK"));
		return;
	}

	u32 TotalDownloadCount = MissingImagesCount;

	if (WindowPrompt(tr("Cover Downloader"), tr("Found"), fmt(tr("%i missing images"), TotalDownloadCount), tr("Yes"), tr("No")) == 0)
		return;

	if (!IsNetworkInit() && !NetworkInitPrompt())
	{
		gprintf("Network connection could not be initialized\n");
		return;
	}

	ProgressCancelEnable(true);

	DownloadProcess(TotalDownloadCount);

	ProgressCancelEnable(false);

	ProgressStop();

	if(MissingImagesCount == 0)
		WindowPrompt(tr("Cover Downloader"), tr("Images downloaded successfully"), tr("OK"));
	else
	{
		int res = WindowPrompt(tr("Cover Downloader" ), fmt(tr("%i images could not be found"), MissingImagesCount), tr("Save List"), tr("OK"));
		if(res)
			CreateCSVLog();
	}
}

void ImageDownloader::FindMissingImages()
{
	wString oldFilter(gameList.GetCurrentFilter());

	// Make sure that all games are added to the gamelist
	gameList.LoadUnfiltered();

	if(choices & CheckedBox1)
		FindMissing(Settings.covers_path, Settings.URL_Covers3D, NULL, tr("Downloading 3D covers"), NULL, ".png");

	if(choices & CheckedBox2)
		FindMissing(Settings.covers2d_path, Settings.URL_Covers2D, NULL, tr("Downloading 2D covers"), NULL, ".png");

	if(choices & CheckedBox3)
	{
		const char * downloadURL = (Settings.coversfull == COVERSFULL_HQ || Settings.coversfull == COVERSFULL_HQ_LQ ) ? Settings.URL_CoversFullHQ : Settings.URL_CoversFull;
		const char * progressTitle = (Settings.coversfull == COVERSFULL_HQ || Settings.coversfull == COVERSFULL_HQ_LQ ) ? tr("Downloading full covers") : tr("Downloading full covers");
		const char * backupURL = (Settings.coversfull == COVERSFULL_HQ_LQ || Settings.coversfull == COVERSFULL_LQ_HQ) ? ((Settings.coversfull == COVERSFULL_HQ_LQ) ? Settings.URL_CoversFull : Settings.URL_CoversFullHQ) : NULL;
		const char * backupProgressTitle = (Settings.coversfull == COVERSFULL_HQ_LQ || Settings.coversfull == COVERSFULL_LQ_HQ) ? ((Settings.coversfull == COVERSFULL_HQ_LQ) ? tr("Downloading full covers") : tr("Downloading full covers")) : NULL;
		FindMissing(Settings.coversFull_path, downloadURL, backupURL, progressTitle, backupProgressTitle, ".png");
	}

	if(choices & CheckedBox4)
	{
		const char * downloadURL = (Settings.discart == DISCARTS_ORIGINALS || Settings.discart == DISCARTS_ORIGINALS_CUSTOMS ) ? Settings.URL_Discs : Settings.URL_DiscsCustom;
		const char * progressTitle = (Settings.discart == DISCARTS_ORIGINALS || Settings.discart == DISCARTS_ORIGINALS_CUSTOMS ) ? tr("Downloading original disc artwork") : tr("Downloading custom disc artwork");
		const char * backupURL = (Settings.discart == DISCARTS_ORIGINALS_CUSTOMS || Settings.discart == DISCARTS_CUSTOMS_ORIGINALS) ? ((Settings.discart == DISCARTS_ORIGINALS_CUSTOMS) ? Settings.URL_DiscsCustom : Settings.URL_Discs) : NULL;
		const char * backupProgressTitle = (Settings.discart == DISCARTS_ORIGINALS_CUSTOMS || Settings.discart == DISCARTS_CUSTOMS_ORIGINALS) ? ((Settings.discart == DISCARTS_ORIGINALS_CUSTOMS) ? tr("Downloading custom disc artwork") : tr("Downloading original disc artwork")) : NULL;
		FindMissing(Settings.disc_path, downloadURL, backupURL, progressTitle, backupProgressTitle, ".png");
	}

	if(choices & CheckedBox5)
	{
		FindMissing(Settings.BNRCachePath, Settings.URL_Banners, NULL, tr("Downloading custom banners"), NULL, ".bnr");
	}

	// Bring the game list back to it's old state
	gameList.FilterList(oldFilter.c_str());
}

void ImageDownloader::FindMissing(const char *writepath, const char *downloadURL, const char *backupURL, const char *progressTitle, const char *backupProgressTitle, const char *fileExt)
{
	if (!CreateSubfolder(writepath))
	{
		WindowPrompt(tr("File Error" ), fmt("%s %s", tr("An error occurred while creating directory"), writepath), tr("OK"));
		return;
	}

	std::vector<std::string> MissingFilesList;
	GetMissingGameFiles(writepath, fileExt, MissingFilesList);
	int size = MissingImages.size();
	MissingImages.resize(size+MissingFilesList.size());

	for(u32 i = 0, n = size; i < MissingFilesList.size(); ++i, ++n)
	{
		MissingImages[n].gameID = MissingFilesList[i];
		MissingImages[n].downloadURL = downloadURL;
		MissingImages[n].backupURL = backupURL;
		MissingImages[n].writepath = writepath;
		MissingImages[n].progressTitle = progressTitle;
		MissingImages[n].backupProgressTitle = backupProgressTitle;
		MissingImages[n].fileExt = fileExt;
	}

		MissingImagesCount += MissingFilesList.size();
}

int ImageDownloader::DownloadProcess(int TotalDownloadCount)
{
	char progressMsg[270];

	for(u32 i = 0, pos = 0; i < MissingImages.size(); ++i, ++pos)
	{
		if(ProgressCanceled())
			break;

		if(strcmp(MissingImages[i].fileExt, ".bnr") == 0)
		{
			char *path = strchr(MissingImages[i].downloadURL + ((strncmp(Settings.URL_Banners, "https://", 8) == 0) ? 8 : 7), '/');
			int domainlength = path - Settings.URL_Banners;
			char domain[domainlength + 1];
			strlcpy(domain, Settings.URL_Banners, domainlength + 1);
			snprintf(progressMsg, sizeof(progressMsg), "%s : %s.bnr", domain, MissingImages[i].gameID.c_str());
		}
		else
		{
			char *path = strchr(MissingImages[i].downloadURL + ((strncmp(MissingImages[i].downloadURL, "https://", 8) == 0) ? 8 : 7), '/');
			int domainlength = path - MissingImages[i].downloadURL;
			char domain[domainlength + 1];
			strlcpy(domain, MissingImages[i].downloadURL, domainlength + 1);
			snprintf(progressMsg, sizeof(progressMsg), "%s : %s.png", domain, MissingImages[i].gameID.c_str());
		}

		ShowProgress(MissingImages[i].progressTitle, fmt("%i %s", TotalDownloadCount - pos, tr("files left" )), progressMsg, pos, TotalDownloadCount);

		if(MissingImages[i].gameID.size() < 3)
			continue;

		gprintf("Searching for %s%s\n", MissingImages[i].gameID.c_str(), MissingImages[i].fileExt);
		struct download file = {};
		DownloadImage(MissingImages[i].downloadURL, MissingImages[i].gameID.c_str(), MissingImages[i].fileExt, &file);
		if(file.size <= 0)
		{
			if(MissingImages[i].backupURL)
			{
				gprintf("Trying backup URL\n");
				MissingImages[i].downloadURL = MissingImages[i].backupURL;
				MissingImages[i].backupURL = NULL;
				MissingImages[i].progressTitle = MissingImages[i].backupProgressTitle;
				--i;
				--pos;
			}
			continue;
		}

		char imgPath[200];
		snprintf(imgPath, sizeof(imgPath), "%s/%s%s", MissingImages[i].writepath, MissingImages[i].gameID.c_str(), MissingImages[i].fileExt);

		FILE *pfile = fopen(imgPath, "wb");
		if (pfile != NULL)
		{
			fwrite(file.data, 1, file.size, pfile);
			fclose(pfile);
			MissingImagesCount--;
		}
		MEM2_free(file.data);
		gprintf(" - Saved %s%s\n", MissingImages[i].gameID.c_str(), MissingImages[i].fileExt);

		//! Remove the image from the vector since it's done
		MissingImages.erase(MissingImages.begin()+i);
		--i;
	}

	return MissingImages.size();
}

void ImageDownloader::DownloadImage(const char *url, const char *gameID, const char *fileExt, struct download *file)
{
	char region[3];
	char downloadURL[512];

	if(strcmp(fileExt, ".bnr") == 0)
	{
		snprintf(downloadURL, sizeof(downloadURL), "%s%s.bnr", url, gameID);
		gprintf(" - Trying: %s\n", downloadURL);
		downloadfile(downloadURL, file);
		if(file->size > 132 && IsValidBanner(file->data)) // 132 = IMET magic location in the banner with u8 header
			return;

		snprintf(downloadURL, sizeof(downloadURL), "%s%.3s.bnr", url, gameID);
		gprintf(" - Trying ID3: %s\n", downloadURL);
		
		downloadfile(downloadURL, file);
		if(file->size > 132 && IsValidBanner(file->data))
			return;

		gprintf(" - Not found\n");
		return;
	}

	// Try to find PAL covers matching the loaders language
	switch (gameID[3])
	{
		case 'P': // Europe
		case 'D': // Germany
		case 'F': // France
		case 'H': // Netherlands
		case 'I': // Italy
		case 'L': // Japanese import to Europe
		case 'M': // American import to Europe
		case 'R': // Russia
		case 'S': // Spain
		case 'U': // Australia
		case 'V': // Scandinavia
		case 'X': // Europe / USA special releases
		case 'Y': // Europe / USA special releases
		case 'Z': // Europe / USA special releases
			sprintf(region, "%.2s", Settings.db_language);
			break;
		case 'E': // US
		case 'N': // Japanese import to US
			sprintf(region, "US");
			break;
		case 'J': // Japan
			sprintf(region, "JA");
			break;
		case 'K': // Korea
		case 'Q': // Japanese import to Korea 
		case 'T': // American import to Korea
			sprintf(region, "KO");
			break; 
		case 'W': // Taiwan / Hong Kong / Macau
			sprintf(region, "ZH");
			break;
		default:  // Custom games?
			sprintf(region, "EN");
	}
	sprintf(downloadURL, "%s%s/%s.png", url, region, gameID);
	gprintf(" - Trying: %s\n", downloadURL);
	downloadfile(downloadURL, file);
	if (VALID_IMAGE(file))
		return;

	// Try to find covers matching our systems language
	std::vector<std::string> v;
	char syslang[3] = {0};
	switch (CONF_GetLanguage())
	{
		case CONF_LANG_GERMAN:
			sprintf(syslang, "DE");
			break;
		case CONF_LANG_FRENCH:
			sprintf(syslang, "FR");
			break;
		case CONF_LANG_SPANISH:
			sprintf(syslang, "SE");
			break;
		case CONF_LANG_ITALIAN:
			sprintf(syslang, "IT");
			break;
		case CONF_LANG_DUTCH:
			sprintf(syslang, "NL");
			break;
	}
	if (syslang[0] != '\0' && strncmp(syslang, region, 2) != 0)
	{
		sprintf(downloadURL, "%s%s/%s.png", url, syslang, gameID);
		gprintf(" - Trying: %s\n", downloadURL);
		downloadfile(downloadURL, file);
		if (VALID_IMAGE(file))
			return;
	}

	// Try to find covers matching the games region e.g. SGWD7K
	char gameregion[3] = {0};
	switch (gameID[3])
	{
		case 'D': // Germany
			sprintf(gameregion, "DE");
			break;
		case 'F': // France
			sprintf(gameregion, "FR");
			break;
		case 'H': // Netherlands
			sprintf(gameregion, "NL");
			break;
		case 'I': // Italy
			sprintf(gameregion, "IT");
			break;
		case 'R': // Russia
			sprintf(gameregion, "RU");
			break;
		case 'S': // Spain
			sprintf(gameregion, "ES");
			break;
		case 'U': // Australia
			sprintf(gameregion, "AU");
			break;
		case 'V': // Scandinavia
			sprintf(gameregion, "DK");
			break;
	}
	if (gameregion[0] != '\0' && strncmp(gameregion, region, 2) != 0 && strncmp(gameregion, syslang, 2) != 0)
	{
		sprintf(downloadURL, "%s%s/%s.png", url, gameregion, gameID);
		gprintf(" - Trying: %s\n", downloadURL);
		downloadfile(downloadURL, file);
		if (VALID_IMAGE(file))
			return;
	}

	// Might be a special US release
	if (gameID[3] == 'X' || gameID[3] == 'Y' || gameID[3] == 'Z')
	{
		if (strncmp(region, "US", 2) != 0)
		{
			sprintf(downloadURL, "%sUS/%s.png", url, gameID);
			gprintf(" - Trying: %s\n", downloadURL);
			downloadfile(downloadURL, file);
			if (VALID_IMAGE(file))
				return;
		}
	}

	// The game might only have an English cover available
	if (strncmp(region, "EN", 2) != 0)
	{
		sprintf(downloadURL, "%sEN/%s.png", url, gameID);
		gprintf(" - Trying: %s\n", downloadURL);
		downloadfile(downloadURL, file);
		if (VALID_IMAGE(file))
			return;
	}

	// Try Finnish and Swedish
	if (gameID[3] == 'V')
	{
		snprintf(downloadURL, sizeof(downloadURL), "%sFI/%s.png", url, gameID);
		gprintf(" - Trying: %s\n", downloadURL);
		downloadfile(downloadURL, file);
		if (VALID_IMAGE(file))
			return;

		snprintf(downloadURL, sizeof(downloadURL), "%sSE/%s.png", url, gameID);
		gprintf(" - Trying: %s\n", downloadURL);
		downloadfile(downloadURL, file);
		if (VALID_IMAGE(file))
			return;
	}

	// Final attempt
	snprintf(downloadURL, sizeof(downloadURL), "%sother/%s.png", url, gameID);
	gprintf(" - Trying: %s\n", downloadURL);
	downloadfile(downloadURL, file);
	if (VALID_IMAGE(file))
		return;

	gprintf(" - Not found\n");
}

void ImageDownloader::CreateCSVLog()
{
	char path[200];
	snprintf(path, sizeof(path), "%s/missingimages.csv", Settings.ConfigPath);

	FILE *f = fopen(path, "wb");
	if(!f) return;

	const char *ImageType = "3D Cover";

	fprintf(f, "\"ID\",\"Name\",\"ImageType\"\n");

	for (u32 i = 0; i < MissingImages.size(); ++i)
	{
		if(MissingImages[i].downloadURL == Settings.URL_Covers3D)
		{
			ImageType = "3D Cover";
		}
		else if(MissingImages[i].downloadURL == Settings.URL_Covers2D)
		{
			ImageType = "2D Cover";
		}
		else if(MissingImages[i].downloadURL == Settings.URL_CoversFullHQ)
		{
			ImageType = "Full HQ Cover";
		}
		else if(MissingImages[i].downloadURL == Settings.URL_CoversFull)
		{
			ImageType = "Full LQ Cover";
		}
		else if(MissingImages[i].downloadURL == Settings.URL_Discs)
		{
			ImageType = "Original Discart";
		}
		else if(MissingImages[i].downloadURL == Settings.URL_DiscsCustom)
		{
			ImageType = "Custom Discart";
		}
		else if(MissingImages[i].downloadURL == Settings.URL_Banners)
		{
			ImageType = "Custom Banner";
		}

		fprintf(f, "\"%s\",\"%s\",\"%s\"\n", MissingImages[i].gameID.c_str(), GameTitles.GetTitle(MissingImages[i].gameID.c_str()), ImageType);
		gprintf("\"%s\",\"%s\",\"%s\"\n", MissingImages[i].gameID.c_str(), GameTitles.GetTitle(MissingImages[i].gameID.c_str()), ImageType);
	}

	fclose(f);
}

bool ImageDownloader::IsValidBanner(char *banner)
{
	if(!((*(u32*)(banner+64)) == 'IMET'))
	{
		if(!((*(u32*)(banner+128)) == 'IMET')) // With U8Archive header
			return false;
	}
	return true;
}
