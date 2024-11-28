#include <unistd.h>
#include "menus.h"
#include "GameCube/GCGames.h"
#include "GameCube/GCDumper.hpp"
#include "usbloader/usbstorage2.h"
#include "usbloader/wbfs.h"
#include "usbloader/disc.h"
#include "usbloader/GameList.h"
#include "prompts/ProgressWindow.h"
#include "prompts/GCMultiDiscMenu.h"
#include "themes/CTheme.h"
#include "utils/tools.h"
#include "system/IosLoader.h"
#include "cache/cache.hpp"
#include "settings/GameTitles.h"

#define WII_MAGIC   0x5D1C9EA3

extern int install_abort_signal;
float gamesize = 0.0f;

/****************************************************************************
 * MenuGCInstall
 ***************************************************************************/
int MenuGCInstall()
{
	GCDumper gcDumper;
	if(gcDumper.ReadDiscHeader() < 0)
	{
		WindowPrompt(tr("Disc Error"), tr("An error occurred while reading game disc"), tr("OK"));
		return MENU_DISCLIST;
	}

	std::vector<u32> installGames;

	if(gcDumper.GetDiscHeaders().size() == 0)
	{
		WindowPrompt(tr("Disc Error"), tr("No games found on disc"), tr("OK"));
		return MENU_DISCLIST;
	}
	else if(gcDumper.GetDiscHeaders().size() > 1)
	{
		//! Multi game disc, lets ask the user which games to install
		GCMultiDiscMenu gcMenu(gcDumper.GetDiscHeaders());
		gcMenu.SetAlignment(ALIGN_CENTER, ALIGN_MIDDLE);
		gcMenu.SetEffect(EFFECT_FADE, 20);
		mainWindow->SetState(STATE_DISABLED);
		mainWindow->Append(&gcMenu);

		int choice = gcMenu.ShowSelection();

		gcMenu.SetEffect(EFFECT_FADE, -20);
		while(gcMenu.GetEffect() > 0) usleep(1000);

		mainWindow->Remove(&gcMenu);
		mainWindow->SetState(STATE_DEFAULT);

		installGames = gcMenu.GetSelectedGames();

		if(choice == 0 || installGames.size() == 0)
			return MENU_DISCLIST;
	}
	else
	{
		if(!WindowPrompt(tr("Install Wizard"), tr("Continue to install game?" ), gcDumper.GetDiscHeaders().at(0).title, tr("Yes"), tr("Cancel")))
			return MENU_DISCLIST;

		installGames.push_back(0);
	}

	//! Setup dumper settings
	gcDumper.SetCompressed(Settings.GCInstallCompressed);
	gcDumper.SetCompressed(Settings.GCInstallAligned);

	//! If a different main path than the SD path is selected ask where to install
	int destination = Settings.SDMode ? 2 : 1;
	if(!Settings.SDMode && strcmp(Settings.GameCubePath, Settings.GameCubeSDPath) != 0)
		destination = WindowPrompt(tr("Install Wizard"), tr("Select game installation path to continue"), tr("USB Device"), tr("SD Card"), tr("Cancel"));
	if(!destination)
		return MENU_DISCLIST;
		
	//! Alert the user if he is dumping on SD with DIOS MIOS (USB) installed
	if(destination == 2 && IosLoader::GetMIOSInfo() == DIOS_MIOS)
	{
		if(!WindowPrompt(tr("Install Wizard"), tr("DIOS MIOS installation detected. Install game on USB device FAT32 partition or install DIOS MIOS Lite to run from SD Card"), tr("Yes"), tr("Cancel")))
			return MENU_DISCLIST;
	}

	// Load only available games from the selected device
	int oldGameCubeSource = Settings.GameCubeSource;
	Settings.GameCubeSource = destination-1;
	GCGames::Instance()->LoadAllGames();

	const char *InstallPath = destination == 1 ? Settings.GameCubePath : Settings.GameCubeSDPath;

	//! Start of install process, enable wii slot light
	wiilight(1);

	int result = 0;
	int installed_games = 0;

	for(u32 i = 0; i < installGames.size(); ++i)
	{
		//! Check if the game is already installed on SD/USB
		if(GCGames::Instance()->IsInstalled((char *)gcDumper.GetDiscHeaders().at(installGames[i]).id, gcDumper.GetDiscHeaders().at(installGames[i]).disc_no))
		{
			WindowPrompt(tr("Install Error"), tr("Game already installed:"), gcDumper.GetDiscHeaders().at(installGames[i]).title, tr("OK"));
			if(i+1 < installGames.size()) {
				continue;
			}
			else if(i == 0)
			{
				result = MENU_DISCLIST;
				break;
			}
		}

		// Check Disc2 installation format (DML 2.6+ auto-swap feature doesn't work with extracted game format)
		if(Settings.GCInstallCompressed && gcDumper.GetDiscHeaders().at(installGames[i]).disc_no == 1)
		{
			int choice = WindowPrompt(tr(gcDumper.GetDiscHeaders().at(installGames[i]).title), tr("Disc 2 needs to be installed in uncompressed format to work with DIOS MIOS Lite 2.6. Install in compressed format?"), tr("Yes"), tr("Cancel"));
			if(choice == 0)
			{
				if(i+1 < installGames.size())
				{
					continue;
				}
				else if(i == 0)
				{
					result = MENU_DISCLIST;
					break;
				}
			}
		}

		// Check if another Disc number from the same game is already installed on this device
		GCGames::Instance()->LoadAllGames(); // Refresh installed game list
		char installedGamePath[512];
		if(GCGames::Instance()->IsInstalled((char *)gcDumper.GetDiscHeaders().at(installGames[i]).id, gcDumper.GetDiscHeaders().at(installGames[i]).disc_no == 0 ? 1 : 0))
		{
			snprintf(installedGamePath, sizeof(installedGamePath), GCGames::Instance()->GetPath((char *)gcDumper.GetDiscHeaders().at(installGames[i]).id));
			char *pathPtr = strrchr(installedGamePath, '/');
			if(pathPtr) *pathPtr = 0;
		}
		else
			installedGamePath[0] = 0;

		// Game is not yet installed so let's install it
		int ret = gcDumper.InstallGame(InstallPath, installGames[i], installedGamePath);
		if(ret >= 0) {
			//! Success
			installed_games++;
		}
		else if(ret == PROGRESS_CANCELED)
		{
			result = MENU_DISCLIST;
			break;
		}
		else if(ret < 0)
		{
			//! Error occurred, ask the user what to do if there are more games to install
			if(i+1 < installGames.size())
			{
				if(!WindowPrompt(tr("Install Error"), tr("An error occured during installation. Continue with next game?"), tr("Yes"), tr("Cancel")))
				{
					result = MENU_DISCLIST;
					break;
				}
			}
			else
			{
				WindowPrompt(tr("Install Error"), tr("An error occured during game installation"), tr("Back"));
				result = MENU_DISCLIST;
				break;
			}
		}
	}

	wiilight(0);
	Settings.GameCubeSource = oldGameCubeSource;
	GCGames::Instance()->LoadAllGames();

	//! No game was installed so don't show successfully installed prompt
	if(installed_games == 0)
		return result;

	gameList.FilterList();
	isCacheCurrent();
	bgMusic->Pause();
	GuiSound instsuccess(Resources::GetFile("success.ogg"), Resources::GetFileSize("success.ogg"), Settings.sfxvolume);
	instsuccess.SetVolume(Settings.sfxvolume);
	instsuccess.SetLoop(0);
	instsuccess.Play();
	char gamesTxt[20];
	snprintf(gamesTxt, sizeof(gamesTxt), "%i %s", installed_games, tr("Games"));
	WindowPrompt(tr("Install Wizard"), tr("GameCube game installed successfully"), installGames.size() > 1 ? gamesTxt : gcDumper.GetDiscHeaders().at(installGames[0]).title, tr("OK"));
	instsuccess.Stop();
	bgMusic->Resume();

	return MENU_DISCLIST;
}

/****************************************************************************
 * MenuInstall
 ***************************************************************************/
int MenuInstall()
{
	gprintf("\nMenuInstall()\n");

	static struct discHdr headerdisc ATTRIBUTE_ALIGN( 32 );

	Disc_SetUSB(NULL);

	int ret, choice = 0;

	ret = DiscWait(tr( "Install Wizard"), tr("Insert a game disc to begin"), tr("Cancel"), 0, 0);
	if (ret < 0)
	{
		WindowPrompt(tr("Disc Error"), tr("An error occurred while reading game disc"), tr("Back"));
		return MENU_DISCLIST;
	}
	ret = Disc_Open();
	if (ret < 0)
	{
		WindowPrompt(tr("Disc Error"), tr("An error occured while opening game disc"), tr("Back"));
		return MENU_DISCLIST;
	}

	memset(&headerdisc, 0, sizeof(struct discHdr));

	Disc_ReadHeader(&headerdisc);

	if ((headerdisc.magic != WII_MAGIC) && (headerdisc.gc_magic != GCGames::MAGIC))
	{
		choice = WindowPrompt(tr("Disc Error"), tr("Disc in tray is not a Wii or a GameCube disc"), tr("OK"), tr("Back"));
		if (choice == 1)
			return MenuInstall();
		else
			return MENU_DISCLIST;
	}

	if(headerdisc.gc_magic == GCGames::MAGIC)
	{
		return MenuGCInstall();
	}

	ret = WBFS_CheckGame(headerdisc.id);
	if (ret)
	{
		WindowPrompt(tr("Install Wizard"), headerdisc.title, tr("game disc already installed"), tr("Back"));
		return MENU_DISCLIST;
	}

	f32 freespace, used;

	WBFS_DiskSpace(&used, &freespace);
	gamesize = (float) WBFS_EstimeGameSize();

	char gametxt[strlen(headerdisc.title) + 16];
	snprintf(gametxt, sizeof(gametxt), "%s : %.2fGB", headerdisc.title, gamesize/GB_SIZE);

	wiilight(1);
	choice = WindowPrompt(tr("Install Wizard"), tr("Continue to install game?" ), gametxt, tr("OK"), tr("Cancel"));

	if (choice == 1)
	{
		sprintf(gametxt, "%s", tr("Installing" ));

		if (gamesize/GB_SIZE > freespace)
		{
			char errortxt[50];
			sprintf(errortxt, "%s: %.2fGB, %s: %.2fGB", tr("Game Size" ), gamesize/GB_SIZE, tr("Free Space" ), freespace);
			WindowPrompt(tr("Device Error"), tr("Not enough free space on storage device" ), errortxt, tr("OK"));
		}
		else
		{
			StartProgress(gametxt, headerdisc.title, 0, true, true);
			ProgressCancelEnable(true);
			ret = WBFS_AddGame();
			ProgressCancelEnable(false);
			ProgressStop();
			wiilight(0);
			if (install_abort_signal)
			{
				WindowPrompt(tr("Install Wizard"), tr("Game installation canceled"), tr("OK"));
			}
			else if (ret != 0)
			{
				WindowPrompt(tr("Error"), tr("Something went wrong"), tr("Back"));
			}
			else
			{
				ShowProgress(tr("Install Wizard"), tr("Installation complete. Refreshing game list"), headerdisc.title, gamesize, gamesize, true, true);
				gameList.ReadGameList(); // Get the entries again
				gameList.FilterList();
				isCacheCurrent();
				bgMusic->Pause();
				GuiSound instsuccess(Resources::GetFile("success.ogg"), Resources::GetFileSize("success.ogg"), Settings.sfxvolume);
				instsuccess.SetVolume(Settings.sfxvolume);
				instsuccess.SetLoop(0);
				instsuccess.Play();
				WindowPrompt(tr("Install Wizard"), tr("Game disc installed successfully"), headerdisc.title, tr("OK"));
				instsuccess.Stop();
				bgMusic->Resume();
			}
		}
	}

	// Turn off the WiiLight
	wiilight(0);
	gamesize = 0.0f;

	return MENU_DISCLIST;
}
