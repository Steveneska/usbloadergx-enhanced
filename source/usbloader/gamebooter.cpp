/****************************************************************************
 * Copyright (C) 2012-2015 Cyan
 * Copyright (C) 2011 Dimok
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
#include <algorithm>

#include "menu/menus.h"
#include "menu/wdmmenu.hpp"
#include "mload/mload.h"
#include "mload/mload_modules.h"
#include "system/iosloader.h"
#include "libs/libruntimeiospatch/runtimeiospatch.h"
#include "controls/devicehandler.hpp"
#include "channels/channels.h"
#include "usbloader/disc.h"
#include "usbloader/apploader.h"
#include "usbloader/usbstorage2.h"
#include "usbloader/wdvd.h"
#include "usbloader/gamelist.h"
#include "settings/cgamesettings.h"
#include "settings/settingsenums.h"
#include "usbloader/frag.h"
#include "usbloader/wbfs.h"
#include "usbloader/playlog.h"
#include "usbloader/mountgamepartition.h"
#include "usbloader/alternatedoloffsets.h"
#include "gamecube/gcgames.h"
#include "settings/newtitles.h"
#include "network/wiinnertag.h"
#include "patches/patchcode.h"
#include "patches/gamepatches.h"
#include "patches/wip.h"
#include "patches/bca.h"
#include "banner/openingbnr.hpp"
#include "wad/nandtitle.h"
#include "menu/menus.h"
#include "memory/memory.h"
#include "utils/stringtools.h"
#include "homebrewboot/boothomebrew.h"
#include "gamebooter.hpp"
#include "nandemu.h"
#include "savepath.h"
#include "sys.h"
#include "fileoperations/fileops.h"
#include "prompts/progresswindow.h"
#include "neek.hpp"
#include "lstub.h"
#include "xml/gametdb.hpp"
#include "usbloader/sdhc.h"
#include "wad/nandtitle.h"

/* GCC 11 false positives */
#if __GNUC__ > 10
#pragma GCC diagnostic ignored "-Warray-bounds"
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif

// appentrypoint has to be global because of asm
u32 AppEntrypoint = 0;

extern bool isWiiVC; // in sys.cpp
extern u32 hdd_sector_size[2];
extern u8 sdhc_mode_sd;
extern std::vector<struct d2x> d2x_list;
extern "C"
{
	syssram *__SYS_LockSram();
	u32 __SYS_UnlockSram(u32 write);
	u32 __SYS_SyncSram(void);
	extern void __exception_closeall();
}

// Check if a game or channel is incompatible with some patches
bool GameBooter::exclude_game(u8 *gameid, bool skipChannels)
{
    if (memcmp(gameid, "RPW", 3) == 0 || memcmp(gameid, "SPX", 3) == 0 ||
        memcmp(gameid, "R3D", 3) == 0 || memcmp(gameid, "SDV", 3) == 0 ||
        memcmp(gameid, "STN", 3) == 0 || memcmp(gameid, "S7S", 3) == 0 ||
        memcmp(gameid, "SDUP41", 6) == 0 || memcmp(gameid, "SDUE41", 6) == 0 ||
        memcmp(gameid, "SDUX41", 6) == 0 || memcmp(gameid, "SD2", 3) == 0 ||
        memcmp(gameid, "SXD", 3) == 0 || memcmp(gameid, "REX", 3) == 0)
    {
        return true;
    }
    if (!skipChannels && (memcmp(gameid, "HAAA", 4) == 0 || memcmp(gameid, "HAYK", 4) == 0 ||
        memcmp(gameid, "HAYC", 4) == 0))
    {
        return true;
    }
    return false;
}

int GameBooter::BootGCMode(struct discHdr *gameHdr)
{
	// Check the settings
	GameCFG *game_cfg = GameSettings.GetGameCFG(gameHdr->id);
	u8 GCMode = game_cfg->GameCubeMode == INHERIT ? Settings.GameCubeMode : game_cfg->GameCubeMode;

	// Devolution
	if (GCMode == GC_MODE_DEVOLUTION)
		return BootDevolution(gameHdr);

	// Nintendont
	if (GCMode == GC_MODE_NINTENDONT)
		return BootNintendont(gameHdr);

	// DIOS MIOS (Lite) and QuadForce
	int currentMIOS = IosLoader::GetMIOSInfo();
	if (currentMIOS == DIOS_MIOS || currentMIOS == DIOS_MIOS_LITE || currentMIOS == QUADFORCE || currentMIOS == QUADFORCE_USB)
		return BootDIOSMIOS(gameHdr);

	// MIOS or Wiigator cMIOS
	if (gameHdr->type == TYPE_GAME_GC_DISC)
	{
		ExitApp();
		gprintf("\nLoading BC for GameCube");
		WII_Initialize();
		return WII_LaunchTitle(0x0000000100000100ULL);
	}

	WindowPrompt(tr("Error"), tr("Install a GameCube loader or select a different GameCube mode to launch games from USB device or SD Card"), tr("OK"));

	return 0;
}

u32 GameBooter::BootPartition(char *dolpath, u8 videoselected, u8 alternatedol, u32 alternatedoloffset)
{
	gprintf("Booting partition IOS %u r%u\n", IOS_GetVersion(), IOS_GetRevision());
	entry_point p_entry;
	s32 ret;
	u64 offset;

	/* Find game partition offset */
	ret = Disc_FindPartition(&offset);
	if (ret < 0)
		return 0;

	/* Open specified partition */
	u32 Tmd_Buffer[0x4A00] ATTRIBUTE_ALIGN(32);
	ret = WDVD_OpenPartition(offset, Tmd_Buffer);
	if (ret < 0)
		return 0;

	/* Setup low memory */
	Disc_SetLowMem();

	/* Setup video mode */
	Disc_SelectVMode(videoselected, false, NULL, NULL);

	/* Run apploader */
	ret = Apploader_Run(&p_entry, dolpath, alternatedol, alternatedoloffset);

	if (ret < 0)
		return 0;

	return (u32)p_entry;
}

void GameBooter::SetupAltDOL(u8 *gameID, u8 &alternatedol, u32 &alternatedoloffset)
{
	if (alternatedol == ALT_DOL_ON_LAUNCH)
	{
		alternatedol = ALT_DOL_FROM_GAME;
		alternatedoloffset = WDMMenu::GetAlternateDolOffset();
	}
	else if (alternatedol == ALT_DOL_DEFAULT)
	{
		alternatedol = ALT_DOL_FROM_GAME;
		alternatedoloffset = defaultAltDol((char *)gameID);
	}

	if (alternatedol == ALT_DOL_FROM_GAME && alternatedoloffset == 0)
		alternatedol = OFF;
}

void GameBooter::SetupNandEmu(u8 NandEmuMode, const char *NandEmuPath, struct discHdr &gameHeader)
{
	if (NandEmuMode && strchr(NandEmuPath, '/'))
	{
		int partition = -1;

		//! Create save game path and title.tmd for not existing saves
		CreateSavePath(&gameHeader, NandEmuPath);

		gprintf("Enabling %s NAND emulation on: %s\n", NandEmuMode == 2 ? "Full" : "Partial", NandEmuPath);
		Set_FullMode(NandEmuMode == 2);
		Set_Path(strchr(NandEmuPath, '/'));

		//! Unmount devices to flush data before activating NAND Emu
		if (strncmp(NandEmuPath, "usb", 3) == 0)
		{
			//! Set which partition to use (USB only)
			partition = atoi(NandEmuPath + 3) - 1;
			Set_Partition(DeviceHandler::PartitionToPortPartition(partition));
			DeviceHandler::Instance()->UnMount(USB1 + partition);
		}
		else
		{
			//! Can't play from an SD card while saving to an SD card with bases other than 56 & 57
			DeviceHandler::Instance()->UnMountSD();
		}

		Enable_Emu(strncmp(NandEmuPath, "usb", 3) == 0 ? EMU_USB : EMU_SD);

		//! Mount USB to start game, SD is not required
		if (strncmp(NandEmuPath, "usb", 3) == 0)
			DeviceHandler::Instance()->Mount(USB1 + partition);
	}
}

int GameBooter::SetupDisc(struct discHdr &gameHeader)
{
	if (gameHeader.type == TYPE_GAME_WII_DISC)
	{
		gprintf("Loading DVD\n");
		return Disc_Open();
	}

	int ret = -1;

	if (IosLoader::IsWaninkokoIOS() && IOS_GetRevision() < 18)
	{
		gprintf("Disc_SetUSB...");
		ret = Disc_SetUSB(gameHeader.id);
		gprintf("%d\n", ret);
		if (ret < 0)
			return ret;
	}
	else
	{
		gprintf("Loading fragment list...");
		ret = get_frag_list(gameHeader.id);
		gprintf("%d\n", ret);
		if (ret < 0)
			return ret;
		ret = set_frag_list(gameHeader.id, Settings.SDMode);
		if (ret < 0)
			return ret;
		gprintf("%s set to game\n", Settings.SDMode ? "SD" : "USB");
	}

	gprintf("Disc_Open()...");
	ret = Disc_Open();
	gprintf("%d\n", ret);

	return ret;
}

void GameBooter::ShutDownDevices(int gameUSBPort)
{
	bool usbconnected = false;
	if (DeviceHandler::Instance()->USB0_Inserted() || DeviceHandler::Instance()->USB1_Inserted())
		usbconnected = true;

	gprintf("Shutting down devices...\n");
	//! Flush all caches and close up all devices
	WBFS_CloseAll();
	DeviceHandler::DestroyInstance();

	//! Shadow mload - Only needed on some games with Hermes v5.1 (Check is inside the function)
	shadow_mload();

	// Reset USB port because device handler changes it for cache flushing
	if (Settings.USBPort == 2)
		USBStorage2_SetPort(gameUSBPort);
	USBStorage2_Deinit();
	if (usbconnected)
		USB_Deinitialize();
}

int GameBooter::BootGame(struct discHdr *gameHdr)
{
	if (!gameHdr)
		return -1;

	struct discHdr gameHeader;
	memcpy(&gameHeader, gameHdr, sizeof(struct discHdr));

	gprintf("Boot Game: %s (%.6s)\n", gameHeader.title, gameHeader.id);

	// Load the HBC from NAND instead of from the homebrew browser
	if (memcmp(gameHeader.id, "JODI", 4) == 0)
		Sys_BackToLoader();

	if (Settings.Wiinnertag)
		Wiinnertag::TagGame((const char *)gameHeader.id);

	if (gameHeader.type == TYPE_GAME_GC_IMG || gameHeader.type == TYPE_GAME_GC_DISC || gameHdr->type == TYPE_GAME_GC_EXTRACTED)
		return BootGCMode(&gameHeader);

	//! Setup game configuration from game settings. If no game settings exist use global/default.
	GameCFG *game_cfg = GameSettings.GetGameCFG(gameHeader.id);
	u8 videoChoice = game_cfg->video == INHERIT ? Settings.videomode : game_cfg->video;
	u8 videoPatchDolChoice = game_cfg->videoPatchDol == INHERIT ? Settings.videoPatchDol : game_cfg->videoPatchDol;
	u8 patchFix480pChoice = game_cfg->patchFix480p == INHERIT ? Settings.patchFix480p : game_cfg->patchFix480p;
	u8 aspectChoice = game_cfg->aspectratio == INHERIT ? Settings.GameAspectRatio : game_cfg->aspectratio;
	u8 languageChoice = game_cfg->language == INHERIT ? Settings.language : game_cfg->language;
	u8 ocarinaChoice = game_cfg->ocarina == INHERIT ? Settings.ocarina : game_cfg->ocarina;
	u8 PrivServChoice = game_cfg->PrivateServer == INHERIT ? Settings.PrivateServer : game_cfg->PrivateServer;
	const char *customAddress = game_cfg->CustomAddress.size() == 0 ? Settings.CustomAddress : game_cfg->CustomAddress.c_str();
	u8 viChoice = game_cfg->vipatch == INHERIT ? Settings.videopatch : game_cfg->vipatch;
	u8 deflicker = game_cfg->deflicker == INHERIT ? Settings.deflicker : game_cfg->deflicker;
	u8 sneekChoice = game_cfg->sneekVideoPatch == INHERIT ? Settings.sneekVideoPatch : game_cfg->sneekVideoPatch;
	s32 iosChoice = game_cfg->ios == INHERIT ? Settings.cios : game_cfg->ios;
	u8 autoIOS = game_cfg->autoios == INHERIT ? Settings.AutoIOS : game_cfg->autoios;
	u8 countrystrings = game_cfg->patchcountrystrings == INHERIT ? Settings.patchcountrystrings : game_cfg->patchcountrystrings;
	u8 alternatedol = game_cfg->loadalternatedol;
	u32 alternatedoloffset = game_cfg->alternatedolstart;
	u8 reloadblock = game_cfg->iosreloadblock == INHERIT ? Settings.BlockIOSReload : game_cfg->iosreloadblock;
	u8 Hooktype = game_cfg->Hooktype == INHERIT ? Settings.Hooktype : game_cfg->Hooktype;
	u8 WiirdDebugger = game_cfg->WiirdDebugger == INHERIT ? Settings.WiirdDebugger : game_cfg->WiirdDebugger;
	u16 videoWidth = game_cfg->videoWidth == INHERIT ? Settings.videoWidth : game_cfg->videoWidth;
	u64 returnToChoice = strlen(Settings.returnTo) > 0 ? (game_cfg->returnTo ? NandTitles.FindU32(Settings.returnTo) : 0) : 0;
	u8 NandEmuMode = OFF;
	const char *NandEmuPath = game_cfg->NandEmuPath.size() == 0 ? Settings.NandEmuPath : game_cfg->NandEmuPath.c_str();
	if (gameHeader.type == TYPE_GAME_WII_IMG && !Settings.SDMode)
		NandEmuMode = game_cfg->NandEmuMode == INHERIT ? Settings.NandEmuMode : game_cfg->NandEmuMode;
	if (gameHeader.type == TYPE_GAME_EMUNANDCHAN)
	{
		NandEmuMode = game_cfg->NandEmuMode == INHERIT ? Settings.NandEmuChanMode : game_cfg->NandEmuMode;
		NandEmuPath = game_cfg->NandEmuPath.size() == 0 ? Settings.NandEmuChanPath : game_cfg->NandEmuPath.c_str();
	}
	
	// boot neek for Wii games and EmuNAND channels only
	if (NandEmuMode == EMUNAND_NEEK && (gameHeader.type == TYPE_GAME_WII_IMG || gameHeader.type == TYPE_GAME_EMUNANDCHAN))
		return BootNeek(&gameHeader);

	if (languageChoice == CONSOLE_DEFAULT)
	{
		std::string Filepath(Settings.titlestxt_path);
		if (Filepath.back() != '/')
			Filepath += '/';
		Filepath += "wiitdb.xml";

		GameTDB XML_DB;
		if (XML_DB.OpenFile(Filepath.c_str()))
		{
			std::string gameLangs;
			if (XML_DB.GetLanguages((char *)gameHeader.id, gameLangs))
			{
				// Check if the game supports the system language (CONF_GetLanguage returns 0-9)
				std::string sysLangs[] = {"JA", "EN", "DE", "FR", "ES", "IT", "NL", "ZHCN", "ZHTW", "KO"};
				if (gameLangs.find(sysLangs[CONF_GetLanguage()]) == std::string::npos)
				{
					// Use whatever is the first supported language
					std::string lang = gameLangs.substr(0, gameLangs.find(","));
					for (u32 i = 0; i < sizeof(sysLangs) / sizeof(sysLangs[0]); i++)
					{
						if (sysLangs[i] == lang)
						{
							gprintf("Changed language to %s\n", lang.c_str());
							languageChoice = i;
							break;
						}
					}
				}
			}
			XML_DB.CloseFile();
		}
	}

	if (autoIOS == GAME_IOS_AUTO && d2x_list.size())
	{
		s32 requestedIOS = 0;
		if (gameHeader.type == TYPE_GAME_NANDCHAN)
			requestedIOS = Channels::GetRequestedIOS(gameHeader.tid, NULL);
		else if (gameHeader.type == TYPE_GAME_EMUNANDCHAN)
			requestedIOS = Channels::GetRequestedIOS(gameHeader.tid, NandEmuPath);
		else if (gameHeader.type == TYPE_GAME_WII_IMG)
		{
			wbfs_disc_t *d = WBFS_OpenDisc(gameHeader.id);
			if (d)
			{
				void *titleTMD = NULL;
				int tmd_size = wbfs_extract_file(d, (char *)"TMD", &titleTMD);
				if (titleTMD != NULL)
				{
					if (tmd_size > 0x18B)
						requestedIOS = *((u8 *)titleTMD + 0x18B);
					free(titleTMD);
				}
				WBFS_CloseDisc(d);
			}
		}
		else if (gameHeader.type == TYPE_GAME_WII_DISC)
		{
			u64 offset;
			if (Disc_FindPartition(&offset) >= 0)
			{
				u32 Tmd_Buffer[0x4A00] ATTRIBUTE_ALIGN(32);
				if (WDVD_OpenPartition(offset, Tmd_Buffer) >= 0)
				{
					tmd *tmd_dvd = (tmd *)SIGNATURE_PAYLOAD(Tmd_Buffer);
					requestedIOS = tmd_dvd->sys_version;
					WDVD_ClosePartition();
				}
			}
		}

		if (requestedIOS)
		{
			// Remove cIOS duplicates
			// This is done here so that IsD2X() always has the complete list
			for (auto cios = d2x_list.begin(); cios != d2x_list.end();)
			{
				if (cios->duplicate)
				{
					gprintf("Duplicate IOS: %d in slot %d removed\n", cios->base, cios->slot);
					cios = d2x_list.erase(cios);
				}
				else
					++cios;
			}

			bool sdEmuNAND = false;
			gprintf("Requested IOS: %d\n", requestedIOS);
			// Workaround for SpongeBobs Boating Bash
			if (memcmp(gameHeader.id, "SBV", 3) == 0)
			{
				// Check if we don't have a cIOS with base IOS 53
				if (!IosLoader::GetD2XIOS(requestedIOS))
				{
					if (isWiiU())
						requestedIOS = 58;
					else
						// Saves will go to NAND in SD card mode if using base 38
						requestedIOS = IosLoader::GetD2XIOS(58) ? 58 : 38;
					gprintf("Applied SpongeBob workaround\n");
				}
			}
			// The d2x cIOS can only save to SD cards with bases 56-60
			else if ((strncmp(NandEmuPath, "sd", 2) == 0 && NandEmuMode > EMUNAND_OFF) || Settings.SDMode)
			{
				for (auto cios = d2x_list.begin(); cios != d2x_list.end();)
				{
					if (cios->base < 56 || cios->base > 60)
					{
						cios = d2x_list.erase(cios);
						sdEmuNAND = true;
					}
					else
						++cios;
				}
			}

			// Check if there's any cIOS options remaining
			if (d2x_list.size())
			{
				// Check for a D2X cIOS with the requested base IOS
				int slot = IosLoader::GetD2XIOS(requestedIOS);
				if (slot)
					iosChoice = slot;
				else
				{
					// Nothing found, so try the closest match
					// e.g. if we've got 55, 57 & 58 and a game requests 56 we'll use 57
					auto cios = std::lower_bound(d2x_list.begin(), d2x_list.end(), requestedIOS, [](const d2x &x, const int &y)
												{ return x.base < y; });
					// Check if the requested IOS is greater than what's available
					if (cios == d2x_list.end())
					{
						requestedIOS = d2x_list.back().base;
						iosChoice = d2x_list.back().slot;
					}
					else
					{
						requestedIOS = cios->base;
						iosChoice = cios->slot;
					}
					gprintf("Next best IOS: %d%s\n", requestedIOS, sdEmuNAND ? " (restricted to 56-60)" : "");
				}
				gprintf("Boot with IOS: %d base %d\n", iosChoice, requestedIOS);
			}
		}
	}
	AppCleanUp();

	gprintf("Settings.partition: %d\n", Settings.partition);

	s32 ret = -1;

	//! Remember game's USB port
	int partition = gameList.GetPartitionNumber(gameHeader.id);
	int usbport = DeviceHandler::PartitionToUSBPort(partition);

	//! Prepare alternate dol settings
	SetupAltDOL(gameHeader.id, alternatedol, alternatedoloffset);

	//! Reload game settings cIOS for this game
	if (iosChoice != IOS_GetVersion())
	{
		gprintf("Reloading into game cIOS: %i...\n", iosChoice);
		IosLoader::LoadGameCios(iosChoice);
		if (MountGamePartition(false) < 0)
			return -1;
	}
	//! Boot with custom SD code, otherwise the game ID won't match
	else if (sdhc_mode_sd)
	{
		DeviceHandler::Instance()->UnMountSD();
		sdhc_mode_sd = 0;
		DeviceHandler::Instance()->MountSD();
	}

	//! Modify Wii Message Board to display the game starting here (before NAND Emu)
	if (Settings.PlaylogUpdate)
	{
		// enable isfs permission if using IOS+AHB or Hermes v4
		if (IOS_GetVersion() < 200 || (IosLoader::IsHermesIOS() && IOS_GetRevision() == 4))
		{
			gprintf("Patching IOS%d...\n", IOS_GetVersion());
			if (IosPatch_RUNTIME(true, false, false, false, false) == ERROR_PATCH)
				gprintf("Patching %sIOS%d failed!\n", IOS_GetVersion() >= 200 ? "c" : "", IOS_GetVersion());
		}

		BNRInstance::Instance()->Load(&gameHeader);
		Playlog_Update((char *)gameHeader.id, BNRInstance::Instance()->GetIMETTitle(CONF_GetLanguage()));
	}

	if (PrivServChoice == PRIVSERV_CUSTOM)
		gprintf("Custom address: %s\n", customAddress);

	//! Load wip codes
	load_wip_code(gameHeader.id);

	//! Load Ocarina codes
	if (ocarinaChoice)
		ocarina_load_code(Settings.Cheatcodespath, gameHeader.id);

	//! Disable private server for games that still have official servers.
	if (memcmp(gameHeader.id, "SC7", 3) == 0 || memcmp(gameHeader.id, "RJA", 3) == 0 ||
		memcmp(gameHeader.id, "SM8", 3) == 0 || memcmp(gameHeader.id, "SZB", 3) == 0 || memcmp(gameHeader.id, "R9J", 3) == 0)
	{
		PrivServChoice = PRIVSERV_OFF; // Private server patching causes error 20100
	}

	//! Force hooktype if not selected but Ocarina is enabled
	if (ocarinaChoice && Hooktype == OFF)
		Hooktype = 1;

	//! Load gameconfig.txt even if ocarina disabled
	if (Hooktype)
		LoadGameConfig(Settings.Cheatcodespath);

	//! Setup NAND emulation
	if (!exclude_game(gameHeader.id, true))
		SetupNandEmu(NandEmuMode, NandEmuPath, gameHeader);

	//! Setup disc stuff if we load a game
	if (gameHeader.tid == 0)
	{
		//! Setup disc in cIOS and open it
		ret = SetupDisc(gameHeader);
		if (ret < 0)
			Sys_BackToLoader();

		//! Load BCA data for the game
		gprintf("Loading BCA data...");
		ret = do_bca_code(Settings.BcaCodepath, gameHeader.id);
		gprintf("%d\n", ret);
	}

	if (IosLoader::IsHermesIOS(iosChoice))
	{
		if (reloadblock == ON)
		{
			//! Setup IOS reload block
			enable_ES_ioctlv_vector();
			if (Settings.SDMode)
			{
				if (gameList.GetGameFSSD() == PART_FS_WBFS)
					mload_close();
			}
			else
			{
				if (gameList.GetGameFS(gameHeader.id) == PART_FS_WBFS)
					mload_close();
			}
		}
	}
	else if (IosLoader::IsD2X(iosChoice))
	{
		// Open ES file descriptor for the d2x patches
		static char es_fs[] ATTRIBUTE_ALIGN(32) = "/dev/es";
		int es_fd = IOS_Open(es_fs, 0);
		if (es_fd >= 0)
		{
			// IOS Reload Block
			if (reloadblock != OFF)
				BlockIOSReload(es_fd, iosChoice);
			// Check if new patch method for return to works otherwise old method will be used
			if (PatchNewReturnTo(es_fd, returnToChoice) >= 0)
				returnToChoice = 0; // Patch successful, no need for old method

			// Close ES file descriptor
			IOS_Close(es_fd);
		}
	}

	//! Now we can free up the memory used by the game/channel lists
	gameList.clear();
	Channels::DestroyInstance();

	//! Load main.dol or alternative dol into memory, start the game apploader and get game entrypoint
	if (gameHeader.tid == 0)
	{
		gprintf("Game Boot\n");
		AppEntrypoint = BootPartition(Settings.dolpath, videoChoice, alternatedol, alternatedoloffset);
		// Reading of game is done we can close devices now
		ShutDownDevices(usbport);
	}
	else
	{
		//! shutdown now and avoid later crashes with free if memory gets overwritten by channel
		ShutDownDevices(DeviceHandler::PartitionToUSBPort(std::max(atoi(NandEmuPath + 3) - 1, 0)));
		gprintf("Channel Boot\n");
		/* Setup video mode */
		Disc_SelectVMode(videoChoice, false, NULL, NULL);
		// Load dol
		AppEntrypoint = Channels::LoadChannel(gameHeader.tid);
	}

	//! No entrypoint found...back to HBC/SystemMenu
	if (AppEntrypoint == 0)
	{
		gprintf("AppEntryPoint is 0, something went wrong\n");
		WDVD_ClosePartition();
		Sys_BackToLoader();
	}

	//! Do all the game patches
	gprintf("Applying game patches...\n");

	//! Now this code block is responsible for the private server patch
	//! and the gecko code handler loading

	//! If a server other than Wiimmfi is selected, do the normal patching
	//! If Wiimmfi is selected for other games than MKWii, do normal patching as well
	//! If Wiimmfi is selected for MKWii, skip normal patching (PRIVSERV_OFF)
	//! and let the new code in do_new_wiimmfi() handle the complete server patch

	//! Also, the new Wiimmfi server patch should be loaded into memory after
	//! the code handler and the cheat codes.

	if (PrivServChoice != PRIVSERV_WIIMMFI || memcmp(gameHeader.id, "RMC", 3) != 0)
	{
		//! Either the server is not Wiimmfi, or, if it is Wiimmfi, the game isn't MKWii - patch the old way
		gamepatches(videoChoice, videoPatchDolChoice, aspectChoice, languageChoice, countrystrings, viChoice,
					deflicker, sneekChoice, Hooktype, videoWidth, returnToChoice, PrivServChoice, customAddress);
	}
	else
	{
		//! Wiimmfi patch for Mario Kart Wii - patch with PRIVSERV_OFF and handle all the patching within do_new_wiimmfi()
		gamepatches(videoChoice, videoPatchDolChoice, aspectChoice, languageChoice, countrystrings, viChoice,
					deflicker, sneekChoice, Hooktype, videoWidth, returnToChoice, PRIVSERV_OFF, customAddress);
	}

	//! Load Code handler if needed
	load_handler(Hooktype, WiirdDebugger, Settings.WiirdDebuggerPause);

	//! Apply the 480p fix (enabled by default).
	//! This needs to be done after the call to gamepatches(), after loading any code handler.
	//! Can (and should) be done before Wiimmfi patching, can't be done in gamepatches() itself.
	//! Exclude Prince of Persia: The Forgotten Sands and a few games that use MetaFortress
	if (patchFix480pChoice && !exclude_game(gameHeader.id))
		PatchFix480p();

	//! If we're NOT on Wiimmfi, patch the known RCE vulnerability in MKWii.
	//! Wiimmfi will handle that on its own through the update payload.
	//! This will also patch error 23400 for a couple games that still have official servers.
	if (PrivServChoice != PRIVSERV_WIIMMFI)
		patch_error_codes(gameHeader.id);

	//! New Wiimmfi patch should be loaded last, after the codehandler, just before the call to the entry point
	if (PrivServChoice == PRIVSERV_WIIMMFI && memcmp(gameHeader.id, "RMC", 3) == 0)
	{
		// all the cool new Wiimmfi stuff:
		switch (do_new_wiimmfi())
		{
		case 0:
			gprintf("Wiimmfi patch for Mario Kart Wii successful.\n");
			break;
		case -1:
			gprintf("Could not determine game region for Wiimmfi patch - make sure the fourth char of the ID is one of [PEJK].\n");
			break;
		case -2:
			gprintf("This image is already patched for Wiimmfi, no need to do so again.\n");
			break;
		}
	}

	//! Jump to the entrypoint of the game - the last function of the USB Loader
	gprintf("Jumping to game entrypoint: 0x%08X.\n", AppEntrypoint);
	return Disc_JumpToEntrypoint(Hooktype, WDMMenu::GetDolParameter());
}

int GameBooter::BootDIOSMIOS(struct discHdr *gameHdr)
{
	const char *RealPath = GCGames::Instance()->GetPath((const char *)gameHdr->id);

	GameCFG *game_cfg = GameSettings.GetGameCFG(gameHdr->id);
	s8 languageChoice = game_cfg->language == INHERIT ? Settings.language - 1 : game_cfg->language;
	u8 ocarinaChoice = game_cfg->ocarina == INHERIT ? Settings.ocarina : game_cfg->ocarina;
	u8 multiDiscChoice = Settings.MultiDiscPrompt;
	u8 dmlVideoChoice = game_cfg->DMLVideo == INHERIT ? Settings.DMLVideo : game_cfg->DMLVideo;
	u8 dmlProgressivePatch = game_cfg->DMLProgPatch == INHERIT ? Settings.DMLProgPatch : game_cfg->DMLProgPatch;
	u8 dmlNMMChoice = game_cfg->DMLNMM == INHERIT ? Settings.DMLNMM : game_cfg->DMLNMM;
	u8 dmlActivityLEDChoice = game_cfg->DMLActivityLED == INHERIT ? Settings.DMLActivityLED : game_cfg->DMLActivityLED;
	u8 dmlPADHookChoice = game_cfg->DMLPADHOOK == INHERIT ? Settings.DMLPADHOOK : game_cfg->DMLPADHOOK;
	u8 dmlNoDisc2Choice = game_cfg->DMLNoDisc2 == INHERIT ? Settings.DMLNoDisc2 : game_cfg->DMLNoDisc2;
	u8 dmlWidescreenChoice = game_cfg->DMLWidescreen == INHERIT ? Settings.DMLWidescreen : game_cfg->DMLWidescreen;
	u8 dmlScreenshotChoice = game_cfg->DMLScreenshot == INHERIT ? Settings.DMLScreenshot : game_cfg->DMLScreenshot;
	u8 dmlJPNPatchChoice = game_cfg->DMLJPNPatch == INHERIT ? Settings.DMLJPNPatch : game_cfg->DMLJPNPatch;
	u8 dmlDebugChoice = game_cfg->DMLDebug == INHERIT ? Settings.DMLDebug : game_cfg->DMLDebug;

	int currentMIOS = IosLoader::GetMIOSInfo();
	char LoaderName[15];
	if (currentMIOS == DIOS_MIOS)
		snprintf(LoaderName, sizeof(LoaderName), "DIOS MIOS");
	else if (currentMIOS == DIOS_MIOS_LITE)
		snprintf(LoaderName, sizeof(LoaderName), "DIOS MIOS Lite");
	else if (currentMIOS == QUADFORCE)
		snprintf(LoaderName, sizeof(LoaderName), "QuadForce");
	else if (currentMIOS == QUADFORCE_USB)
		snprintf(LoaderName, sizeof(LoaderName), "QuadForce_USB");

	// DIOS MIOS
	if (currentMIOS == DIOS_MIOS || currentMIOS == QUADFORCE_USB)
	{
		// Check Main GameCube Path location
		if (strncmp(Settings.GameCubePath, "sd", 2) == 0 || strncmp(DeviceHandler::PathToFSName(Settings.GameCubePath), "FAT", 3) != 0)
		{
			WindowPrompt(tr("Device Error"), fmt(tr("GameCube games path not set to USB device FAT32 partition"), LoaderName), tr("OK"));
			return -1;
		}

		// Check current game location
		if (strncmp(RealPath, "sd", 2) == 0)
		{
			WindowPrompt(tr("Device Error"), fmt(tr("%s can only run GameCube games located on a USB device FAT32 partition"), LoaderName), tr("OK"));
			// Todo: Add here copySD2USB.
			return -1;
		}

		// Check if the partition is the first primary partition on the drive
		bool found = false;
		int USB_partNum = DeviceHandler::PathToDriveType(Settings.GameCubePath) - USB1;
		int USBport_partNum = DeviceHandler::PartitionToPortPartition(USB_partNum);
		int usbport = DeviceHandler::PartitionToUSBPort(USB_partNum);
		PartitionHandle *usbHandle = DeviceHandler::Instance()->GetUSBHandleFromPartition(USB_partNum);
		for (int partition = 0; partition <= USBport_partNum; partition++)
		{
			if (usbHandle->GetPartitionTableType(partition) != MBR)
				continue;

			if (partition == USBport_partNum)
			{
				found = true;
				break;
			}
		}
		if (!found)
		{
			WindowPrompt(tr("Device Error"), fmt(tr("%s can only run GameCube games located on a USB device first FAT32 partition"), LoaderName), tr("OK"));
			return -1;
		}

		// Check HDD sector size. Only 512 bytes/sector is supported by DIOS MIOS
		if (hdd_sector_size[usbport] != BYTES_PER_SECTOR)
		{
			WindowPrompt(tr("Device Error"), fmt(tr("%s cannot run GameCube games on a USB device partition with 512 bytes/sector"), LoaderName), tr("OK"));
			return -1;
		}

		if (usbHandle->GetPartitionClusterSize(usbHandle->GetLBAStart(USBport_partNum)) > 32768)
		{
			WindowPrompt(tr("Device Error"), fmt(tr("%s cannot run GameCube games on a USB device partition with less than 32KB/cluster"), LoaderName), tr("OK"));
			return -1;
		}
	}

	// DIOS MIOS Lite
	else if (currentMIOS == DIOS_MIOS_LITE || currentMIOS == QUADFORCE)
	{
		if (((gameHdr->type == TYPE_GAME_GC_IMG) || (gameHdr->type == TYPE_GAME_GC_EXTRACTED)) && strncmp(RealPath, "usb", 3) == 0)
		{
			if (!GCGames::Instance()->CopyUSB2SD(gameHdr))
				return -1;

			RealPath = GCGames::Instance()->GetPath((const char *)gameHdr->id);
		}
	}

	// Check DIOS MIOS config for specific versions
	if (currentMIOS != QUADFORCE && currentMIOS != QUADFORCE_USB)
	{
		if (IosLoader::GetDMLVersion() < DML_VERSION_DML_1_2)
		{
			WindowPrompt(tr("Loader Error"), tr("DIOS MIOS Lite version too old. Install DIOS MIOS Lite 1.2 or newer"), tr("OK"));
			return -1;
		}
		if (dmlWidescreenChoice && IosLoader::GetDMLVersion() < DML_VERSION_DM_2_1) // DML Force Widescreen setting : added in DM v2.1+, config v1.
		{
			if (Settings.DMLWidescreen) // Display the warning only if set as Global setting. Individual game setting is not displayed.
				WindowPrompt(tr("Attention"), tr("Force Widescreen requires DIOS MIOS 2.1 or newer. This setting will be ignored"), tr("OK"));
			dmlWidescreenChoice = OFF;
		}
		if (dmlNoDisc2Choice && (IosLoader::GetDMLVersion() < DML_VERSION_DM_2_2_2 || IosLoader::GetDMLVersion() > DML_VERSION_DML_2_2_1)) // DML NoDisc+ setting : Added in DM 2.2 upate 2, config v2, removed in DM(L) v2.3
		{
			if (Settings.DMLNoDisc2) // Display the warning only if set as Global setting. Individual game setting is not displayed.
				WindowPrompt(tr("Attention"), tr("No Disc+ requires DIOS MIOS 2.2 Update 2. This setting will be ignored"), tr("OK"));
			dmlNoDisc2Choice = false;
		}
	}

	// Check Ocarina and cheat file location. the .gct file need to be located on the same partition than the game.
	if (gameHdr->type != TYPE_GAME_GC_DISC && ocarinaChoice && strcmp(DeviceHandler::GetDevicePrefix(RealPath), DeviceHandler::GetDevicePrefix(Settings.Cheatcodespath)) != 0)
	{
		char path[255], destPath[255];
		int res = -1;
		snprintf(path, sizeof(path), "%s%.6s.gct", Settings.Cheatcodespath, (char *)gameHdr->id);
		snprintf(destPath, sizeof(destPath), "%s:/DMLTemp.gct", DeviceHandler::GetDevicePrefix(RealPath));

		gprintf("DML: Copying %s to %s \n", path, destPath);
		res = CopyFile(path, destPath);
		if (res < 0)
		{
			gprintf("DML: Couldn't copy the file. ret %d. Ocarina Disabled\n", res);
			RemoveFile(destPath);
			ocarinaChoice = false;
		}
	}

	// Check if game has multi Discs
	bool bootDisc2 = false;
	if (multiDiscChoice && gameHdr->type != TYPE_GAME_GC_DISC && gameHdr->disc_no == 0 && currentMIOS != QUADFORCE)
	{
		char disc2Path[255];
		snprintf(disc2Path, sizeof(disc2Path), "%s", RealPath);
		char *pathPtr = strrchr(disc2Path, '/');
		if (pathPtr)
			*pathPtr = 0;
		snprintf(disc2Path + strlen(disc2Path), sizeof(disc2Path) - strlen(disc2Path), "/disc2.iso");
		if (CheckFile(disc2Path))
		{
			int choice = WindowPrompt(gameHdr->title, tr("Select which disc to launch"), tr("Disc 1"), tr("Disc 2"), tr("Cancel"));
			if (choice == 0)
				return -1;
			else if (choice == 2)
				bootDisc2 = true;
		}
	}

	const char *gcPath = strchr(RealPath, '/');
	if (!gcPath)
		gcPath = "";

	char gamePath[255];
	snprintf(gamePath, sizeof(gamePath), "%s", gcPath);

	if (bootDisc2)
	{
		char *pathPtr = strrchr(gamePath, '/');
		if (pathPtr)
			*pathPtr = 0;
		snprintf(gamePath + strlen(gamePath), sizeof(gamePath) - strlen(gamePath), "/disc2.iso");
	}

	ExitApp();

	// Game ID
	memcpy((u8 *)Disc_ID, gameHdr->id, 6);
	DCFlushRange((u8 *)Disc_ID, 6);

	// *(vu32*)0xCC003024 |= 7; // DML 1.1- only?

	DML_CFG *dml_config = (DML_CFG *)DML_CONFIG_ADDRESS;
	memset(dml_config, 0, sizeof(DML_CFG));

	// Magic and version for DML
	dml_config->Magicbytes = DML_MAGIC;
	dml_config->Version = IosLoader::GetDMLVersion() >= DML_VERSION_DM_2_2 ? 0x00000002 : 0x00000001;

	// Select disc source
	if ((gameHdr->type == TYPE_GAME_GC_IMG) || (gameHdr->type == TYPE_GAME_GC_EXTRACTED))
	{
		dml_config->Config |= DML_CFG_GAME_PATH;
		strncpy(dml_config->GamePath, gamePath, sizeof(dml_config->GamePath));
		// Extended NoDisc patch
		if (dmlNoDisc2Choice && IosLoader::GetDMLVersion() >= DML_VERSION_DM_2_2_2 && IosLoader::GetDMLVersion() < DML_VERSION_DML_2_3m)
			dml_config->Config |= DML_CFG_NODISC2; // used by v2.2 update2 as an Extended NoDisc patching

		gprintf("DML: Loading game %s\n", dml_config->GamePath);
	}
	else
	{
		dml_config->Config |= DML_CFG_BOOT_DISC;
	}

	// setup cheat and path
	if (ocarinaChoice)
	{
		// Check if the .gct folder is on the same partition than the game, if not load the temporary .gct file.
		if (strcmp(DeviceHandler::GetDevicePrefix(RealPath), DeviceHandler::GetDevicePrefix(Settings.Cheatcodespath)) == 0)
		{
			const char *CheatPath = strchr(Settings.Cheatcodespath, '/');
			if (!CheatPath)
				CheatPath = "";
			snprintf(dml_config->CheatPath, sizeof(dml_config->CheatPath), "%s%.6s.gct", CheatPath, (char *)gameHdr->id);
		}
		else if (gameHdr->type != TYPE_GAME_GC_DISC)
		{
			snprintf(dml_config->CheatPath, sizeof(dml_config->CheatPath), "DMLTemp.gct");
		}

		dml_config->Config |= DML_CFG_CHEATS | DML_CFG_CHEAT_PATH;
		gprintf("DML: Loading cheat %s\n", dml_config->CheatPath);
	}

	// other DML configs
	if (dmlPADHookChoice)
		dml_config->Config |= DML_CFG_PADHOOK;
	if (dmlActivityLEDChoice)
		dml_config->Config |= DML_CFG_ACTIVITY_LED;
	if (dmlNMMChoice)
		dml_config->Config |= dmlNMMChoice == ON ? DML_CFG_NMM : DML_CFG_NMM_DEBUG;
	if (dmlDebugChoice)
		dml_config->Config |= dmlDebugChoice == ON ? DML_CFG_DEBUGGER : DML_CFG_DEBUGGER | DML_CFG_DEBUGWAIT;
	if (dmlWidescreenChoice)
		dml_config->Config |= DML_CFG_FORCE_WIDE;
	if (dmlScreenshotChoice)
	{
		dml_config->Config |= DML_CFG_SCREENSHOT;
		dml_config->Config |= DML_CFG_PADHOOK;
	}
	if (bootDisc2 && IosLoader::GetDMLVersion() >= DML_VERSION_DM_2_6_0)
		dml_config->Config |= DML_CFG_BOOT_DISC2;

	// Setup Video Mode
	if (dmlVideoChoice == DML_VIDEO_NONE) // No video mode
	{
		dml_config->VideoMode = DML_VID_NONE;
	}
	else
	{
		if (dmlVideoChoice == DML_VIDEO_AUTO) // Auto select video mode
		{
			dml_config->VideoMode = DML_VID_DML_AUTO;
			Disc_SelectVMode(VIDEO_MODE_DISCDEFAULT, false, NULL, NULL);
		}
		else // Force user choice
		{
			Disc_SelectVMode(dmlVideoChoice - 1, false, &dml_config->VideoMode, NULL);
			if (!(dml_config->VideoMode & DML_VID_DML_AUTO))
				dml_config->VideoMode |= DML_VID_FORCE;
		}
		Disc_SetVMode();
	}

	if (dmlProgressivePatch)
		dml_config->VideoMode |= DML_VID_PROG_PATCH;

	DCFlushRange(dml_config, sizeof(DML_CFG));
	memcpy((u8 *)DML_CONFIG_ADDRESS_V1_2, dml_config, sizeof(DML_CFG));
	DCFlushRange((u8 *)DML_CONFIG_ADDRESS_V1_2, sizeof(DML_CFG));

	// print the config set for DML
	gprintf("DML: setup configuration 0x%X\n", dml_config->Config);
	gprintf("DML: setup video mode 0x%X\n", dml_config->VideoMode);

	// Set Sram flags
	bool progressive = (dml_config->VideoMode & DML_VID_FORCE_PROG) || (dml_config->VideoMode & DML_VID_PROG_PATCH);
	PatchSram(languageChoice, true, progressive);

	/* NTSC-J Patch */ // Thanks to Fix94
	u8 *diskid = (u8 *)Disc_ID;
	if (dmlJPNPatchChoice && diskid[3] == 'J')
		*HW_PPCSPEED = 0x0002A9E0;

	gprintf("\nLoading BC for GameCube\n");
	WII_Initialize();
	return WII_LaunchTitle(0x0000000100000100ULL);
}

int GameBooter::BootDevolution(struct discHdr *gameHdr)
{
	const char *RealPath = GCGames::Instance()->GetPath((const char *)gameHdr->id);
	const char *LoaderName = "Devolution";

	GameCFG *game_cfg = GameSettings.GetGameCFG(gameHdr->id);
	s8 languageChoice = game_cfg->language == INHERIT ? Settings.language - 1 : game_cfg->language;
	u8 devoProgressivePatch = game_cfg->DMLProgPatch == INHERIT ? Settings.DMLProgPatch : game_cfg->DMLProgPatch;
	u8 devoMCEmulation = game_cfg->DEVOMCEmulation == INHERIT ? Settings.DEVOMCEmulation : game_cfg->DEVOMCEmulation;
	u8 devoActivityLEDChoice = game_cfg->DEVOActivityLED == INHERIT ? Settings.DEVOActivityLED : game_cfg->DEVOActivityLED;
	u8 devoWidescreenChoice = game_cfg->DEVOWidescreen == INHERIT ? Settings.DEVOWidescreen : game_cfg->DEVOWidescreen;
	u8 devoFZeroAXChoice = game_cfg->DEVOFZeroAX == INHERIT ? Settings.DEVOFZeroAX : game_cfg->DEVOFZeroAX;
	u8 devoTimerFixChoice = game_cfg->DEVOTimerFix == INHERIT ? Settings.DEVOTimerFix : game_cfg->DEVOTimerFix;
	u8 devoDButtonsChoice = game_cfg->DEVODButtons == INHERIT ? Settings.DEVODButtons : game_cfg->DEVODButtons;
	u8 devoCropOverscanChoice = game_cfg->DEVOCropOverscan == INHERIT ? Settings.DEVOCropOverscan : game_cfg->DEVOCropOverscan;
	u8 devoDiscDelayChoice = game_cfg->DEVODiscDelay == INHERIT ? Settings.DEVODiscDelay : game_cfg->DEVODiscDelay;
	u64 returnToChoice = strlen(Settings.returnTo) > 0 ? (game_cfg->returnTo ? NandTitles.FindU32(Settings.returnTo) : 0) : 0;

	if (gameHdr->type == TYPE_GAME_GC_DISC)
	{
		WindowPrompt(tr("Loader Error"), tr("Set GameCube mode to MIOS to run GameCube games from disc"), tr("OK"));
		return -1;
	}

	if (gameHdr->type == TYPE_GAME_GC_EXTRACTED)
	{
		WindowPrompt(tr("Disc Error"), fmt(tr("%s is compatible with GameCube game images in ISO format only"), LoaderName), tr("OK"));
		return -1;
	}

	if (!CheckAHBPROT())
	{
		WindowPrompt(tr("Loader Error"), fmt(tr("%s requires AHB access. Relaunch USB Loader GX from Homebrew Channel or with an updated forwarder"), LoaderName), tr("OK"));
		return -1;
	}

	if (strncmp(DeviceHandler::PathToFSName(RealPath), "FAT", 3) != 0)
	{
		WindowPrompt(tr("Device Error"), fmt(tr("GameCube games path not set to USB device FAT32 partition"), LoaderName), tr("OK"));
		return -1;
	}

	// Check if Devolution is available
	u8 *loader_bin = NULL;
	int DEVO_version = 0;
	char DEVO_loader_path[110];
	snprintf(DEVO_loader_path, sizeof(DEVO_loader_path), "%sloader.bin", Settings.DEVOLoaderPath);
	FILE *f = fopen(DEVO_loader_path, "rb");
	if (f)
	{
		fseek(f, 0, SEEK_END);
		u32 size = ftell(f);
		rewind(f);
		loader_bin = (u8 *)MEM2_alloc(size);
		if (!loader_bin)
		{
			fclose(f);
			WindowPrompt(tr("Loader Error"), tr("An error occurred while loading Devolution loader.bin file"), tr("OK"));
			return -1;
		}
		fread(loader_bin, 1, size, f);

		// Read Devolution version
		char version[5];
		fseek(f, 23, SEEK_SET);
		fread(version, 1, 4, f);
		char *ptr = strchr(version, ' ');
		if (ptr)
			*ptr = 0;
		else
			version[4] = 0;
		DEVO_version = atoi(version);

		fclose(f);
	}
	else
	{
		WindowPrompt(tr("Loader Error"), tr("Devolution loader.bin file not found in Devolution path"), tr("OK"));
		return -1;
	}

	// Devolution config
	DEVO_CFG *devo_config = (DEVO_CFG *)0x80000020;

	char disc1[100];
	char disc2[100];
	bool multiDisc = false;
	char DEVO_memCard[100];
	snprintf(disc1, sizeof(disc1), "%s", RealPath);

	snprintf(disc2, sizeof(disc2), "%s", RealPath);
	char *pathPtr = strrchr(disc2, '/');
	if (pathPtr)
		*pathPtr = 0;
	snprintf(disc2 + strlen(disc2), sizeof(disc2) - strlen(disc2), "/disc2.iso");
	if (CheckFile(disc2))
		multiDisc = true;

	snprintf(DEVO_memCard, sizeof(DEVO_memCard), "%s", RealPath); // Set memory card folder to Disc1 folder
	char *ptr = strrchr(DEVO_memCard, '/');
	if (ptr)
		*ptr = 0;

	// Make sure the directory exists
	char devoPath[20];
	snprintf(devoPath, sizeof(devoPath), "%s:/apps/gc_devo", DeviceHandler::GetDevicePrefix(RealPath));
	CreateSubfolder(devoPath);

	// Get the starting cluster (and device ID) for the ISO file 1
	struct stat st1;
	stat(disc1, &st1);

	// Get the starting cluster for the ISO file 2
	struct stat st2;
	if (multiDisc)
		stat(disc2, &st2);

	// setup Devolution
	memset(devo_config, 0, sizeof(*devo_config));
	devo_config->signature = DEVO_SIG;
	devo_config->version = DEVO_CONFIG_VERSION;
	devo_config->device_signature = st1.st_dev == 'WISD' ? 'SD' : 'SB'; // Set device type.
	devo_config->disc1_cluster = st1.st_ino; // set starting cluster for first disc ISO file
	if (multiDisc)
		devo_config->disc2_cluster = st2.st_ino; // set starting cluster for second disc ISO file

	// Devolution configs
	// use wifi logging if USB gecko is not found in slot B
	// devo_config->options |= DEVO_CFG_WIFILOG; // removed on Tueidj request
	if (devoWidescreenChoice && DEVO_version >= 188)
		devo_config->options |= DEVO_CFG_WIDE;
	if (!devoActivityLEDChoice && DEVO_version >= 142)
		devo_config->options |= DEVO_CFG_NOLED; // ON by default
	if (devoFZeroAXChoice && DEVO_version >= 196)
		devo_config->options |= DEVO_CFG_FZERO_AX;
	if (devoTimerFixChoice && DEVO_version >= 196)
		devo_config->options |= DEVO_CFG_TIMER_FIX;
	if (devoDButtonsChoice && DEVO_version >= 200)
		devo_config->options |= DEVO_CFG_D_BUTTONS;
	if (devoCropOverscanChoice && DEVO_version >= 234)
		devo_config->options |= DEVO_CFG_CROP_OVERSCAN;
	if (devoDiscDelayChoice && DEVO_version >= 234)
		devo_config->options |= DEVO_CFG_DISC_DELAY;
	//	devo_config->options |= DEVO_CFG_PLAYLOG; // Playlog setting managed by USBLoaderGX features menu

	if (devoProgressivePatch && DEVO_version >= 266)
	{
		devo_config->options |= DEVO_CFG_FORCE_480P;
		devo_config->options |= DEVO_CFG_FORCE_576P;
	}

	// check memory card
	if (devoMCEmulation == DEVO_MC_OFF)
	{
		devo_config->memcard_cluster = 0;
		snprintf(DEVO_memCard, sizeof(DEVO_memCard), "Original");
	}
	else
	{
		if (devoMCEmulation == DEVO_MC_INDIVIDUAL)
		{
			snprintf(DEVO_memCard + strlen(DEVO_memCard), sizeof(DEVO_memCard) - strlen(DEVO_memCard), "/memcard_%.6s.bin", (const char *)gameHdr->id);
		}
		else if (devoMCEmulation == DEVO_MC_REGIONAL)
		{
			snprintf(DEVO_memCard, sizeof(DEVO_memCard), "%s:/apps/gc_devo/memcard_%c.bin", DeviceHandler::GetDevicePrefix(RealPath), gameHdr->id[3]);
		}
		else // same for all games
		{
			snprintf(DEVO_memCard, sizeof(DEVO_memCard), "%s:/apps/gc_devo/memcard.bin", DeviceHandler::GetDevicePrefix(RealPath));
		}

		// check if file doesn't exist or is less than 512KB (59 Blocks)
		struct stat st;
		if (stat(DEVO_memCard, &st) == -1 || st.st_size < 1 << 19)
		{
			// need to enlarge or create it
			FILE *f = fopen(DEVO_memCard, "wb");
			if (f)
			{
				// make it 16MB
				ShowProgress(tr("Please wait"), 0, 0);
				gprintf("Resizing memcard file...\n");
				fseek(f, (16 << 20) - 1, SEEK_SET);
				fputc(0, f);
				fclose(f);
				if (stat(DEVO_memCard, &st) == -1 || st.st_size < 1 << 19)
				{
					// it still isn't big enough. Give up.
					st.st_ino = 0;
				}
				ProgressStop();
			}
			else
			{
				// couldn't open or create the memory card file
				st.st_ino = 0;
			}
		}
		devo_config->memcard_cluster = st.st_ino;
	}

	// read 32 bytes of disc 1 to the start of MEM1
	FILE *iso_file = fopen(disc1, "rb");
	if (!iso_file)
	{
		WindowPrompt(tr("File Error"), tr("Disc image file not found"), tr("OK"));
		return -1;
	}
	u8 *lowmem = (u8 *)0x80000000;
	fread(lowmem, 1, 32, iso_file);
	fclose(iso_file);

	// setup video mode
	Disc_SelectVMode(0, true, NULL, NULL);
	Disc_SetVMode();

	// Set sram flags
	PatchSram(languageChoice, false, false);

	// flush disc ID and Devolution config out to memory
	DCFlushRange(lowmem, 64);

	ExitApp();
	IosLoader::ReloadIosKeepingRights(58); // reload IOS 58 with AHBPROT rights

	gprintf("DEVO: Loading game: %s\n", disc1);
	gprintf("DEVO: Memory Card: %s\n\n", DEVO_memCard);
	gprintf("%.72s", (const char *)loader_bin + 4);

	if (returnToChoice)
	{
		loadStub();
		Set_Stub(returnToChoice);
	}

	u32 cpu_isr;
	SYS_ResetSystem(SYS_SHUTDOWN, 0, 0);
	_CPU_ISR_Disable(cpu_isr);
	__exception_closeall();
	LAUNCH_DEVO();
	_CPU_ISR_Restore(cpu_isr);
	return 0;
}

int GameBooter::BootNintendont(struct discHdr *gameHdr)
{
	char RealPath[100];
	if (gameHdr->type == TYPE_GAME_GC_DISC)
		snprintf(RealPath, sizeof(RealPath), "di");
	else
		snprintf(RealPath, sizeof(RealPath), "%s", GCGames::Instance()->GetPath((const char *)gameHdr->id));

	const char *LoaderName = "Nintendont";

	GameCFG *game_cfg = GameSettings.GetGameCFG(gameHdr->id);
	s8 languageChoice = game_cfg->language == INHERIT ? Settings.language - 1 : game_cfg->language;
	u8 ocarinaChoice = game_cfg->ocarina == INHERIT ? Settings.ocarina : game_cfg->ocarina;
	u8 multiDiscChoice = Settings.MultiDiscPrompt;
	u8 ninVideoChoice = game_cfg->DMLVideo == INHERIT ? Settings.DMLVideo : game_cfg->DMLVideo;
	u8 ninProgressivePatch = game_cfg->DMLProgPatch == INHERIT ? Settings.DMLProgPatch : game_cfg->DMLProgPatch;
	u8 ninDeflickerChoice = game_cfg->NINDeflicker == INHERIT ? Settings.NINDeflicker : game_cfg->NINDeflicker;
	u8 ninWidescreenChoice = game_cfg->DMLWidescreen == INHERIT ? Settings.DMLWidescreen : game_cfg->DMLWidescreen;
	u8 ninMCEmulationChoice = game_cfg->NINMCEmulation == INHERIT ? Settings.NINMCEmulation : game_cfg->NINMCEmulation;
	u8 ninMCSizeChoice = game_cfg->NINMCSize == INHERIT ? Settings.NINMCSize : game_cfg->NINMCSize;
	u8 ninAutobootChoice = Settings.NINAutoboot;
	u8 ninSettingsChoice = Settings.NINSettings;
	u8 ninUSBHIDChoice = game_cfg->NINUSBHID == INHERIT ? Settings.NINUSBHID : game_cfg->NINUSBHID;
	u8 ninMaxPadsChoice = game_cfg->NINMaxPads == INHERIT ? Settings.NINMaxPads : game_cfg->NINMaxPads;
	u8 ninNativeSIChoice = game_cfg->NINNativeSI == INHERIT ? Settings.NINNativeSI : game_cfg->NINNativeSI;
	u8 ninWiiUWideChoice = game_cfg->NINWiiUWide == INHERIT ? Settings.NINWiiUWide : game_cfg->NINWiiUWide;
	u8 ninLEDChoice = game_cfg->NINLED == INHERIT ? Settings.NINLED : game_cfg->NINLED;
	u8 ninDebugChoice = game_cfg->DMLDebug == INHERIT ? Settings.DMLDebug : game_cfg->DMLDebug;
	u8 ninOSReportChoice = game_cfg->NINOSReport == INHERIT ? Settings.NINOSReport : game_cfg->NINOSReport;
	u8 ninLogChoice = game_cfg->NINLog == INHERIT ? Settings.NINLog : game_cfg->NINLog;
	u8 ninVideoScale = game_cfg->NINVideoScale == INHERIT ? Settings.NINVideoScale : game_cfg->NINVideoScale;
	u8 ninVideoOffset = game_cfg->NINVideoOffset == INHERIT - 20 ? Settings.NINVideoOffset : game_cfg->NINVideoOffset;
	u8 ninPal50PatchChoice = game_cfg->NINPal50Patch == INHERIT ? Settings.NINPal50Patch : game_cfg->NINPal50Patch;
	u8 ninRemlimitChoice = game_cfg->NINRemlimit == INHERIT ? Settings.NINRemlimit : game_cfg->NINRemlimit;
	u8 ninArcadeModeChoice = game_cfg->NINArcadeMode == INHERIT ? Settings.NINArcadeMode : game_cfg->NINArcadeMode;
	u8 ninCCRumbleChoice = game_cfg->NINCCRumble == INHERIT ? Settings.NINCCRumble : game_cfg->NINCCRumble;
	u8 ninSkipIPLChoice = game_cfg->NINSkipIPL == INHERIT ? Settings.NINSkipIPL : game_cfg->NINSkipIPL;
	u8 ninBBAChoice = game_cfg->NINBBA == INHERIT ? Settings.NINBBA : game_cfg->NINBBA;
	u8 ninBBAProfileChoice = game_cfg->NINBBAProfile == INHERIT ? Settings.NINBBAProfile : game_cfg->NINBBAProfile;

	const char *ninLoaderPath = game_cfg->NINLoaderPath.size() == 0 ? Settings.NINLoaderPath : game_cfg->NINLoaderPath.c_str();

	if (!CheckAHBPROT())
	{
		WindowPrompt(tr("Loader Error"), fmt(tr("%s requires AHB access. Relaunch USB Loader GX from Homebrew Channel or with an updated forwarder"), LoaderName), tr("OK"));
		return -1;
	}

	// Check if Nintendont boot.dol is available
	char NIN_loader_path[255];
	if (strncmp(RealPath, "usb", 3) == 0) // Nintendont r39 only
	{
		snprintf(NIN_loader_path, sizeof(NIN_loader_path), "%sloaderusb.dol", ninLoaderPath);
		if (!CheckFile(NIN_loader_path))
			snprintf(NIN_loader_path, sizeof(NIN_loader_path), "%sbootusb.dol", ninLoaderPath);
	}
	if (strncmp(RealPath, "sd", 2) == 0 || !CheckFile(NIN_loader_path))
	{
		snprintf(NIN_loader_path, sizeof(NIN_loader_path), "%sloader.dol", ninLoaderPath);
		if (!CheckFile(NIN_loader_path))
			snprintf(NIN_loader_path, sizeof(NIN_loader_path), "%sboot.dol", ninLoaderPath);
	}
	if (!CheckFile(NIN_loader_path))
	{
		// Nintendont boot.dol not found
		WindowPrompt(tr("Loader Error"), tr("Nintendont boot.dol file not found in Nintendont path"), tr("OK"));
		return -1;
	}
	gprintf("NIN: Loader path = %s \n", NIN_loader_path);
	gprintf("NIN: Game path   = %s \n", RealPath);

	// Check Nintendont version
	u32 NIN_cfg_version = NIN_CFG_VERSION;
	char NINVersion[7] = "";
	u32 NINRev = 0;
	bool NINArgsboot = false;
	NINRev = nintendontVersion(Settings.NINLoaderPath, NINVersion, sizeof(NINVersion));
	if (NINRev > 0) // Version available since 3.324
	{
		gprintf("NIN: Nintendont revision = %d \n", NINRev);
		NINArgsboot = true; //	no need to check argsboot string, 3.324+ supports it.
	}
	else
	{
		char NINBuildDate[21] = "";
		if (nintendontBuildDate(Settings.NINLoaderPath, NINBuildDate))
		{
			// Current build date
			struct tm time;
			strptime(NINBuildDate, "%b %d %Y %H:%M:%S", &time);
			const time_t NINLoaderTime = mktime(&time);

			// Alpha0.1
			strptime("Sep 20 2013 15:27:01", "%b %d %Y %H:%M:%S", &time);
			if (NINLoaderTime == mktime(&time))
			{
				WindowPrompt(tr("Loader Error"), tr("USB Loader GX Rev.1218 required for Nintendont Alpha 0.1. Update Nintendont boot.dol file"), tr("OK"));
				return -1;
			}

			// r01 - r40
			strptime("Mar 30 2014 12:33:44", "%b %d %Y %H:%M:%S", &time); // r42 - NIN_CFG_VERSION = 2
			if (NINLoaderTime < mktime(&time))
			{
				gprintf("Nintendont r01 - r40 detected. Using CFG version 0x00000001\n");
				NIN_cfg_version = 1;

				strptime("Mar 29 2014 10:49:31", "%b %d %Y %H:%M:%S", &time); // r39
				if (NINLoaderTime < mktime(&time) && strncmp(RealPath, "usb", 3) == 0)
				{
					if (WindowPrompt(tr("Attention"), tr("Nintendont version does not support games on USB device"), tr("Continue"), tr("Cancel")) == 0)
						return -1;
				}
			}

			// v1.01 - v1.134
			strptime("Aug  5 2014 22:38:21", "%b %d %Y %H:%M:%S", &time); // v1.135 - NIN_CFG_VERSION = 3
			if (NINLoaderTime < mktime(&time) && NIN_cfg_version != 1)
			{
				gprintf("Nintendont v1.01 - v1.134 detected. Using CFG version 0x00000002\n");
				NIN_cfg_version = 2;
				// no need to fake NIN_CFG struct size, the size is checked in nintendont only since v1.143
			}
			else if (NINLoaderTime >= mktime(&time))
				NINRev = 135;

			// v2.200 to 2.207
			strptime("Nov  6 2014.17:33:30", "%b %d %Y %H:%M:%S", &time); // v1.208
			if (ninAutobootChoice && NINLoaderTime < mktime(&time))
			{
				strptime("Oct 31 2014 21:14:47", "%b %d %Y %H:%M:%S", &time); // v1.200
				if (NINLoaderTime >= mktime(&time))
				{
					WindowPrompt(tr("Attention"), tr("Nintendont version not fully supported. Disabling game auto boot"), tr("OK"));
					ninAutobootChoice = OFF;
				}
			}

			// v2.259 - disc support
			strptime("Dec 23 2014 17:28:56", "%b %d %Y %H:%M:%S", &time); // v1.259
			if (gameHdr->type == TYPE_GAME_GC_DISC && NINLoaderTime < mktime(&time))
			{
				WindowPrompt(tr("Loader Error"), tr("Set GameCube mode to MIOS to run GameCube games from disc"), tr("OK"));
				return -1;
			}

			// v3.304 - Controller.ini is now optional
			strptime("Feb 23 2015 05:32:16", "%b %d %Y %H:%M:%S", &time); // v3.304
			if (NINLoaderTime >= mktime(&time))
			{
				NINRev = 304;
			}

			// checks argsboot
			if (ninAutobootChoice)
			{
				u8 *buffer = NULL;
				u32 filesize = 0;
				if (LoadFileToMem(NIN_loader_path, &buffer, &filesize))
				{
					for (u32 i = 0; i < filesize; i += 0x10)
					{
						if ((*(u32 *)(buffer + i)) == 'args' && (*(u32 *)(buffer + i + 4)) == 'boot')
						{
							gprintf("NIN: argsboot found at %08x, using arguments instead of Nincfg.bin\n", i);
							NINArgsboot = true;
							break;
						}
					}
					free(buffer);
				}
			}
		}
		else
		{
			int choice = WindowPrompt(tr("Attention"), tr("Nintendont boot.dol file could not be verified. Launch anyway?"), tr("Yes"), tr("Cancel"));
			if (choice == 0)
				return -1;
		}
	}

	// needed since v3.354 CFG v4 to still work with old CFG version
	if (NINRev >= 135 && NINRev < 354)
		NIN_cfg_version = 3;
	else if (NINRev >= 354 && NINRev < 358)
		NIN_cfg_version = 4;
	else if (NINRev >= 358 && NINRev < 368)
		NIN_cfg_version = 5;
	else if (NINRev >= 368 && NINRev < 424)
		NIN_cfg_version = 6;
	else if (NINRev >= 424 && NINRev < 431)
		NIN_cfg_version = 7;
	else if (NINRev >= 431 && NINRev < 487)
		NIN_cfg_version = 8;
	else if (NINRev >= 487)
		NIN_cfg_version = 9;

	// Check USB device
	if (gameHdr->type != TYPE_GAME_GC_DISC && strncmp(RealPath, "usb", 3) == 0)
	{
		// Check Main GameCube Path location
		if (strncmp(DeviceHandler::PathToFSName(Settings.GameCubePath), "FAT", 3) != 0)
		{
			WindowPrompt(tr("Device Error"), fmt(tr("%s can only run GameCube games located on a USB device FAT32 partition"), LoaderName), tr("OK"));
			return -1;
		}

		// Check the partition type
		int USB_partNum = DeviceHandler::PathToDriveType(Settings.GameCubePath) - USB1;					// Get partition number across all mounted device
		int USBport_partNum = DeviceHandler::PartitionToPortPartition(USB_partNum);						// Get partition position from corresponding USB port
		PartitionHandle *usbHandle = DeviceHandler::Instance()->GetUSBHandleFromPartition(USB_partNum); // Open a handle on used USB port

		// GPT and EBR 0x0F support added on v3.400, primary type was required on old version.
		if (NINRev < 400 && usbHandle->GetPartitionTableType(USBport_partNum) != MBR)
		{
			WindowPrompt(tr("Device Error"), fmt(tr("%s can only run GameCube games located on a USB device first FAT32 partition"), LoaderName), tr("OK"));
			return -1;
		}

		// Extended type EBR 0x05 was added in 4.406, only type 0x0F was working from 400 to 405
		if (NINRev > 400 && NINRev < 406 && usbHandle->GetPartitionTableType(USBport_partNum) == EBR && usbHandle->GetPartitionType(USBport_partNum) != 0x0F)
		{
			WindowPrompt(tr("Device Error"), tr("GameCube games partition table not compatible. Update Nintendont"), tr("OK"));
			return -1;
		}

		// check if the partition is the first FAT32 of the drive. ExFAT was added to nintendont 4.x but USBLoaderGX can't list games so no need to check that format.
		bool found = false;
		for (int partition = 0; partition <= USBport_partNum; partition++)
		{
			if (strncmp(usbHandle->GetFSName(partition), "FAT", 3) != 0)
				continue;

			if (partition == USBport_partNum)
			{
				found = true;
				break;
			}
		}
		if (!found)
		{
			WindowPrompt(tr("Device Error"), fmt(tr("%s can only run GameCube games located on a USB device first FAT32 partition"), LoaderName), tr("OK"));
			return -1;
		}
	}

	// Set used device when launching game from disc
	if (gameHdr->type == TYPE_GAME_GC_DISC)
	{
		if (Settings.GameCubeSource >= GC_SOURCE_AUTO && strncmp(Settings.GameCubePath, "usb", 3) == 0)
		{
			if (WindowPrompt("Game Loader", tr("Select device to use for Nintendont files"), tr("SD Card"), tr("USB Device")) == 1)
				snprintf(RealPath, sizeof(RealPath), "%s:/", DeviceHandler::GetDevicePrefix(Settings.GameCubeSDPath));
			else
				snprintf(RealPath, sizeof(RealPath), "%s:/", DeviceHandler::GetDevicePrefix(Settings.GameCubePath));
		}
		else if (Settings.GameCubeSource == GC_SOURCE_MAIN)
		{
			snprintf(RealPath, sizeof(RealPath), "%s:/", DeviceHandler::GetDevicePrefix(Settings.GameCubePath));
		}
		else
			snprintf(RealPath, sizeof(RealPath), "%s:/", DeviceHandler::GetDevicePrefix(Settings.GameCubeSDPath));
	}

	// Check Ocarina and cheat file location. the .gct file need to be located on the same partition than the game.
	if (ocarinaChoice && strcmp(DeviceHandler::GetDevicePrefix(RealPath), DeviceHandler::GetDevicePrefix(Settings.Cheatcodespath)) != 0)
	{
		char path[255], destPath[255];
		int res = -1;
		snprintf(path, sizeof(path), "%s%.6s.gct", Settings.Cheatcodespath, (char *)gameHdr->id);
		snprintf(destPath, sizeof(destPath), "%s:/NINTemp.gct", DeviceHandler::GetDevicePrefix(RealPath));

		gprintf("NIN: Copying %s to %s \n", path, destPath);
		res = CopyFile(path, destPath);
		if (res < 0)
		{
			gprintf("NIN: Couldn't copy the file. ret %d. Ocarina Disabled\n", res);
			RemoveFile(destPath);
			ocarinaChoice = false;
		}
	}

	// Check kenobiwii.bin
	if (NINRev < 336 && (ocarinaChoice || (ninDebugChoice && !isWiiU())))
	{
		char kenobiwii_path[30];
		snprintf(kenobiwii_path, sizeof(kenobiwii_path), "%s:/sneek/kenobiwii.bin", DeviceHandler::GetDevicePrefix(RealPath));
		if (!CheckFile(kenobiwii_path))
		{
			// try to copy kenobiwii from the other device
			if (strcmp(Settings.GameCubePath, Settings.GameCubeSDPath) != 0)
			{
				char kenobiwii_srcpath[30];

				snprintf(kenobiwii_srcpath, sizeof(kenobiwii_srcpath), "%s:/sneek/kenobiwii.bin", strncmp(RealPath, "usb", 3) == 0 ? "sd" : DeviceHandler::GetDevicePrefix(Settings.GameCubePath));
				gprintf("kenobiwii source path = %s \n", kenobiwii_srcpath);
				if (CheckFile(kenobiwii_srcpath))
				{
					if (CopyFile(kenobiwii_srcpath, kenobiwii_path) < 0)
					{
						gprintf("NIN: Couldn't copy %s to %s.\n", kenobiwii_srcpath, kenobiwii_path);
						RemoveFile(kenobiwii_path);
						if (WindowPrompt(tr("Attention"), fmt(tr("%s file is required to use Ocarina with %s"), kenobiwii_path, LoaderName), tr("Continue"), tr("Cancel")) == 0)
							return -1;
					}
				}
				else
				{
					gprintf("kenobiwii source path = %s Not found.\n", kenobiwii_srcpath);
					if (WindowPrompt(tr("Attention"), fmt(tr("%s file is required to use Ocarina with %s"), kenobiwii_path, LoaderName), tr("Continue"), tr("Cancel")) == 0)
						return -1;
				}
			}
			else
			{
				gprintf("kenobiwii path = %s Not found.\n", kenobiwii_path);
				if (WindowPrompt(tr("Attention"), fmt(tr("%s file is required to use Ocarina with %s"), kenobiwii_path, LoaderName), tr("Continue"), tr("Cancel")) == 0)
					return -1;
			}
		}
	}

	// Check controller.ini
	if (ninUSBHIDChoice)
	{
		// Check controller.ini file in priority, then controllers folder, for compatibility with older nintendont versions.
		char controllerini_path[30];
		snprintf(controllerini_path, sizeof(controllerini_path), "%s:/controller.ini", DeviceHandler::GetDevicePrefix(RealPath));
		if (!CheckFile(controllerini_path) && strcmp(Settings.GameCubePath, Settings.GameCubeSDPath) != 0)
		{
			// try to copy controller.ini from the other device
			char controllerini_srcpath[30];
			snprintf(controllerini_srcpath, sizeof(controllerini_srcpath), "%s:/controller.ini", strncmp(RealPath, "usb", 3) == 0 ? "sd" : DeviceHandler::GetDevicePrefix(Settings.GameCubePath));
			gprintf("Controller.ini source path = %s \n", controllerini_srcpath);
			if (CheckFile(controllerini_srcpath))
			{
				if (CopyFile(controllerini_srcpath, controllerini_path) < 0)
				{
					gprintf("NIN: Couldn't copy %s to %s.\n", controllerini_srcpath, controllerini_path);
					RemoveFile(controllerini_path);
					if (NINRev < 304) // HID is always enabled and controller.ini optional since r304
					{
						if (WindowPrompt(tr("Attention"), fmt(tr("%s file is required to use other HID controllers with %s"), controllerini_path, LoaderName), tr("Continue"), tr("Cancel")) == 0)
							return -1;
					}
				}
			}
			else // check controllers folder if no controller.ini found on root.
			{

				// Check gamepath:/controllers/ folder
				snprintf(controllerini_path, sizeof(controllerini_path), "%s:/controllers/", DeviceHandler::GetDevicePrefix(RealPath));
				if (!CheckFile(controllerini_path) && strcmp(Settings.GameCubePath, Settings.GameCubeSDPath) != 0)
				{
					// try to copy controllers folder from the other device
					char controllerini_srcpath[30];
					snprintf(controllerini_srcpath, sizeof(controllerini_srcpath), "%s:/controllers/", strncmp(RealPath, "usb", 3) == 0 ? "sd" : DeviceHandler::GetDevicePrefix(Settings.GameCubePath));
					gprintf("Controllers folder source path = %s \n", controllerini_srcpath);
					if (CheckFile(controllerini_srcpath))
					{
						if (CopyDirectory(controllerini_srcpath, controllerini_path) < 0)
						{
							gprintf("NIN: Couldn't copy %s to %s.\n", controllerini_srcpath, controllerini_path);
							RemoveDirectory(controllerini_path);
						}
					}
					else if (NINRev < 304)
					{
						snprintf(controllerini_path, sizeof(controllerini_path), "%s:/controller.ini", DeviceHandler::GetDevicePrefix(RealPath));
						if (WindowPrompt(tr("Attention"), fmt(tr("%s file is required to use other HID controllers with %s"), controllerini_path, LoaderName), tr("Continue"), tr("Cancel")) == 0)
							return -1;
					}
				}
			}
		}
	}

	// Check if game has multi Discs
	bool bootDisc2 = false;
	if (multiDiscChoice && gameHdr->type != TYPE_GAME_GC_DISC && gameHdr->disc_no == 0)
	{
		char disc2Path[255];
		snprintf(disc2Path, sizeof(disc2Path), "%s", RealPath);
		char *pathPtr = strrchr(disc2Path, '/');
		if (pathPtr)
			*pathPtr = 0;
		snprintf(disc2Path + strlen(disc2Path), sizeof(disc2Path) - strlen(disc2Path), "/disc2.iso");
		if (CheckFile(disc2Path))
		{
			int choice = WindowPrompt(gameHdr->title, tr("Select which disc to launch"), tr("Disc 1"), tr("Disc 2"), tr("Cancel"));
			if (choice == 0)
				return -1;
			else if (choice == 2)
				bootDisc2 = true;
		}
	}
	const char *gcPath = strchr(RealPath, '/');
	if (!gcPath)
		gcPath = "";

	char gamePath[255];
	snprintf(gamePath, sizeof(gamePath), "%s", gcPath);

	if (bootDisc2)
	{
		char *pathPtr = strrchr(gamePath, '/');
		if (pathPtr)
			*pathPtr = 0;
		snprintf(gamePath + strlen(gamePath), sizeof(gamePath) - strlen(gamePath), "/disc2.iso");
	}

	if (gameHdr->type == TYPE_GAME_GC_DISC)
	{
		snprintf(gamePath, sizeof(gamePath), "di");
	}

	// Nintendont Config file settings
	NIN_CFG *nin_config = NULL;
	nin_config = (NIN_CFG *)MEM2_alloc(sizeof(NIN_CFG));
	if (!nin_config)
	{
		gprintf("Not enough memory to create nincfg.bin file.\n");
		WindowPrompt(tr("File Error"), tr("An error occurred while writing Nintendont config file"), tr("OK"));
		return -1;
	}

	memset(nin_config, 0, sizeof(NIN_CFG));

	// Magic and CFG_Version for Nintendont
	nin_config->Magicbytes = NIN_MAGIC;
	nin_config->Version = NIN_cfg_version;

	// Game path
	strncpy(nin_config->GamePath, gamePath, sizeof(nin_config->GamePath));

	// setup cheat and path
	if (ocarinaChoice)
	{
		// Check if the .gct folder is on the same partition than the game, if not load the temporary .gct file.
		if (strcmp(DeviceHandler::GetDevicePrefix(RealPath), DeviceHandler::GetDevicePrefix(Settings.Cheatcodespath)) == 0)
		{
			const char *CheatPath = strchr(Settings.Cheatcodespath, '/');
			if (!CheatPath)
				CheatPath = "";
			snprintf(nin_config->CheatPath, sizeof(nin_config->CheatPath), "%s%.6s.gct", CheatPath, (char *)gameHdr->id);
		}
		else
		{
			snprintf(nin_config->CheatPath, sizeof(nin_config->CheatPath), "/NINTemp.gct");
		}

		nin_config->Config |= NIN_CFG_CHEATS | NIN_CFG_CHEAT_PATH;
		gprintf("NIN: Loading cheat %s\n", nin_config->CheatPath);
	}

	// Set other settings
	if (ninDebugChoice && !isWiiU()) // only on Wii
		nin_config->Config |= ninDebugChoice == ON ? NIN_CFG_DEBUGGER : NIN_CFG_DEBUGGER | NIN_CFG_DEBUGWAIT;
	if (ninMCEmulationChoice)
		nin_config->Config |= NIN_CFG_MEMCARDEMU;
	if (ninWidescreenChoice)
		nin_config->Config |= NIN_CFG_FORCE_WIDE;
	if (ninProgressivePatch)
	{
		nin_config->Config |= NIN_CFG_FORCE_PROG;
		nin_config->VideoMode |= NIN_VID_PROG;
	}
	if (ninAutobootChoice)
		nin_config->Config |= NIN_CFG_AUTO_BOOT;
	if (ninUSBHIDChoice)
		nin_config->Config |= NIN_CFG_HID; // auto enabled by nintendont v2.152 and less on vWii
	if (ninOSReportChoice)
		nin_config->Config |= NIN_CFG_OSREPORT;
	if (strncmp(RealPath, "usb", 3) == 0)
		nin_config->Config |= NIN_CFG_USB; // r40+
	if (ninLEDChoice)
		nin_config->Config |= NIN_CFG_LED; // r45+
	if (ninLogChoice)
		nin_config->Config |= NIN_CFG_LOG; // v1.109+
	if (ninMCEmulationChoice == NIN_MC_MULTI)
		nin_config->Config |= NIN_CFG_MC_MULTI; // v1.135+
	if (ninNativeSIChoice)
		nin_config->Config |= NIN_CFG_NATIVE_SI; // v2.189+
	if (ninWiiUWideChoice)
		nin_config->Config |= NIN_CFG_WIIU_WIDE; // v2.258+
	if (ninArcadeModeChoice)
		nin_config->Config |= NIN_CFG_ARCADE_MODE; // v4.424+ Triforce Arcade Mode
	if (ninCCRumbleChoice)
		nin_config->Config |= NIN_CFG_CC_RUMBLE; // v4.431+ Classic Controller Rumble
	if (ninSkipIPLChoice)
		nin_config->Config |= NIN_CFG_SKIP_IPL; // v4.435+ Skip Gamecube BIOS

	// Max Pads
	nin_config->MaxPads = ninMaxPadsChoice; // NIN_CFG_VERSION 2 r42

	// GameID for MCEmu
	memcpy(&nin_config->GameID, gameHdr->id, 4); // NIN_CFG_VERSION 2 r83

	// GameID for Video mode DiscDefault
	memcpy((u8 *)Disc_ID, gameHdr->id, 6);
	DCFlushRange((u8 *)Disc_ID, 6);

	// Memory Card Emulation Blocs size with NIN_CFG v3
	if (NIN_cfg_version == 3)
		nin_config->MemCardBlocks = ninMCSizeChoice; // NIN_CFG_VERSION 3 v1.135
	// Memory Card Emulation Blocs size + Aspect ratio with NIN_CFG v4
	else if (NIN_cfg_version >= 4)
	{
		nin_config->MemCardBlocksV4 = ninMCSizeChoice; // NIN_CFG_VERSION 4 v3.354
		nin_config->VideoScale = ninVideoScale;		   // v3.354+
		nin_config->VideoOffset = ninVideoOffset;	   // v3.354+
	}

	// Remove data read speed limiter
	if (NIN_cfg_version >= 5 && ninRemlimitChoice)
		nin_config->Config |= NIN_CFG_REMLIMIT;

	// BBA emulation
	if (NIN_cfg_version >= 9 && ninBBAChoice)
		nin_config->Config |= NIN_CFG_BBA_EMU; // v6.487+

	// BBA network profile
	if (NIN_cfg_version >= 9 && ninBBAChoice && !isWiiU())
		nin_config->NetworkProfile = ninBBAProfileChoice; // v6.487+

	// Setup Video Mode
	if (ninVideoChoice == DML_VIDEO_NONE) // No video mode changes
	{
		nin_config->VideoMode = NIN_VID_NONE;
	}
	else
	{
		if (ninVideoChoice == DML_VIDEO_AUTO || ninVideoChoice == DML_VIDEO_FORCE_DISCDEFAULT) // Auto select video mode
		{
			Disc_SelectVMode(VIDEO_MODE_DISCDEFAULT, false, NULL, &nin_config->VideoMode);
			nin_config->VideoMode = NIN_VID_AUTO;
		}
		else // Force user choice
		{
			Disc_SelectVMode(ninVideoChoice - 1, false, NULL, &nin_config->VideoMode);
			if (nin_config->VideoMode & NIN_VID_FORCE_MASK)
				nin_config->VideoMode |= NIN_VID_FORCE;

			if (ninDeflickerChoice)
				nin_config->VideoMode |= NIN_VID_FORCE_DF; // v2.208+

			if (ninPal50PatchChoice)
				nin_config->VideoMode |= NIN_VID_PATCH_PAL50; // v3.368+

			if (nin_config->VideoMode & NIN_VID_PROG)
				nin_config->Config |= NIN_CFG_FORCE_PROG; // Set Force_PROG bit in Config
		}
		Disc_SetVMode();
	}

	gprintf("NIN: Active device %s\n", nin_config->Config & NIN_CFG_USB ? "USB" : "SD");
	gprintf("NIN: config 0x%08x\n", nin_config->Config);
	gprintf("NIN: Video mode 0x%08x\n", nin_config->VideoMode);

	// Set game language setting
	if (languageChoice >= GC_ENGLISH && languageChoice <= GC_DUTCH)
	{
		nin_config->Language = languageChoice;
	}
	else // console default or other languages
	{
		nin_config->Language = NIN_LAN_AUTO;
		if (CONF_GetLanguage() >= CONF_LANG_ENGLISH && CONF_GetLanguage() <= CONF_LANG_DUTCH)
		{
			nin_config->Language = CONF_GetLanguage() - 1;
		}
	}
	gprintf("NIN: Language 0x%08x \n", nin_config->Language);

	// if WiiVC, force creation and use of nincfg.bin file to fix a nintendont bug if HID is connected before launching it.
	if (isWiiVC)
	{
		ninSettingsChoice = ON;
		NINArgsboot = OFF;
	}

	// Delete existing nincfg.bin files
	if (ninSettingsChoice == OFF)
	{
		char NINCfgPath[17];

		// Nintendont loader partition
		snprintf(NINCfgPath, sizeof(NINCfgPath), "%s:/nincfg.bin", DeviceHandler::GetDevicePrefix(NIN_loader_path));
		RemoveFile(NINCfgPath);

		// game partition
		if (strncmp(NINCfgPath, RealPath, 4) != 0)
		{
			snprintf(NINCfgPath, sizeof(NINCfgPath), "%s:/nincfg.bin", DeviceHandler::GetDevicePrefix(RealPath));
			RemoveFile(NINCfgPath);
		}
	}
	else if (ninSettingsChoice == ON || !NINArgsboot)
	{
		// Nintendont Config file path
		char NINCfgPath[17];
		snprintf(NINCfgPath, sizeof(NINCfgPath), "%s:/nincfg.bin", DeviceHandler::GetDevicePrefix(NIN_loader_path));
		gprintf("NIN: Cfg path : %s \n", NINCfgPath);

		// write config file to nintendont's partition root.
		FILE *fp = fopen(NINCfgPath, "wb");
		if (fp)
		{
			fwrite(nin_config, sizeof(char), sizeof(NIN_CFG), fp);
			fclose(fp);
		}
		else
		{
			gprintf("Could not open NINCfgPath in write mode");
			int choice = WindowPrompt(tr("Attention"), tr("USB Loader GX could not write Nintendont config file. Launch Nintendont anyway?"), tr("Yes"), tr("Cancel"));
			if (choice == 0)
				return -1;
		}

		// Copy Nintendont Config file to game path
		if (strncmp(NINCfgPath, RealPath, 2) != 0)
		{
			char NINDestPath[17];
			snprintf(NINDestPath, sizeof(NINDestPath), "%s:/nincfg.bin", DeviceHandler::GetDevicePrefix(RealPath));
			gprintf("NIN: Copying %s to %s...", NINCfgPath, NINDestPath);
			if (CopyFile(NINCfgPath, NINDestPath) < 0)
			{
				gprintf("\nError: Couldn't copy %s to %s.\n", NINCfgPath, NINDestPath);
				RemoveFile(NINDestPath);
				if (WindowPrompt(tr("Attention"), tr("USB Loader GX could not write Nintendont config file. Launch Nintendont anyway?"), tr("Yes"), tr("Cancel")) == 0)
					return -1;
			}
			gprintf("done\n");
		}
	}

	if (NINArgsboot)
	{
		// initialize homebrew and arguments
		u8 *buffer = NULL;
		u32 filesize = 0;
		LoadFileToMem(NIN_loader_path, &buffer, &filesize);
		if (!buffer)
		{
			return -1;
		}
		FreeHomebrewBuffer();
		CopyHomebrewMemory(buffer, 0, filesize);

		AddBootArgument(NIN_loader_path);
		AddBootArgument((char *)nin_config, sizeof(NIN_CFG));

		// Launch Nintendont
		return !(BootHomebrewFromMem() < 0);
	}
	else
	{
		// Launch Nintendont
		return !(BootHomebrew(NIN_loader_path) < 0);
	}
}

int GameBooter::BootNeek(struct discHdr *gameHdr)
{
	struct discHdr gameHeader;
	memcpy(&gameHeader, gameHdr, sizeof(struct discHdr));

	GameCFG *game_cfg = GameSettings.GetGameCFG(gameHdr->id);
	u8 ocarinaChoice = game_cfg->ocarina == INHERIT ? Settings.ocarina : game_cfg->ocarina;
	u64 returnToChoice = game_cfg->returnTo;
	const char *NandEmuPath = game_cfg->NandEmuPath.size() == 0 ? Settings.NandEmuChanPath : game_cfg->NandEmuPath.c_str();
	bool autoboot = true;
	bool NK2O_isInstalled = false;
	char tempPath[100] = "";
	int ret = -1;

	// Check all settings first before loading kernel

	// Check kernel.bin
	int neekMode = neekIsNeek2o(NandEmuPath); // -1 = kernel.bin not found, 0 = neek, 1 = neek2o
	if (neekMode == -1)
	{
		WindowPrompt(tr("EmuNAND Error"), tr("An error occurred while loading Neek kernel"), tr("OK"));
		return -1;
	}
	if (neekMode == 0)
	{
		if (WindowPrompt(tr("Attention"), tr("Current Neek files are not Neek2o. Disabling game auto boot"), tr("Continue"), tr("Cancel")) == 0)
			return -1;
		autoboot = false;
	}

	// Set current EmuNAND path as default for neek2o.
	if (neekMode == 1)
	{
		ret = neek2oSetNAND(NandEmuPath);
		gprintf("NEEK: Setting EmuNAND in nandcfg.bin : %d \n", ret);
		if (ret < 0)
		{
			WindowPrompt(tr("EmuNAND Error"), tr("Neek NAND path not found on device"), tr("OK"));
			return -1;
		}
	}

	// check and prepare EmuNAND path for neek
	char neekNandPath[256] = "";
	neekPathFormat(neekNandPath, NandEmuPath, sizeof(neekNandPath));

	// check if the NAND path is compatible with current neek mode.
	if (neekMode == 0 && strlen(neekNandPath) > 0)
	{
		WindowPrompt(tr("EmuNAND Error"), tr("Neek2o is required to load EmuNAND from sub-folders"), tr("OK"));
		return -1;
	}

	// Check if EmuNAND path is on SD
	if (neekMode == 1 && isWiiU() && strncmp(NandEmuPath, "sd", 2) == 0) // neek2o on SD is not supported with the vWii leaked version of neek2o. Users could use it on Wii too, but they should be using r96.
	{
		if (WindowPrompt(tr("Attention"), tr("EmuNAND channel path on SD Card is not supported by Neek2o. Setup Uneek2o instead"), tr("Continue"), tr("Cancel")) == 0)
			return -1;
	}

	// check partition compatibility - TODO : confirm incompatibility with each check

	// Check if EmuNAND partition is on USB devices
	if (strncmp(NandEmuPath, "usb", 3) == 0)
	{
		// Todo: add uStealth'd HDD check here, might need neek version detection too.

		// Check partition format // Assume SD is always FAT32
		if (strncmp(DeviceHandler::PathToFSName(NandEmuPath), "FAT", 3) != 0)
		{
			WindowPrompt(tr("EmuNAND Error"), tr("Set EmuNAND channel path to USB device FAT32 partition to use Neek"), tr("OK"));
			return -1;
		}

		// Check if the partition is the first primary partition on the drive - TODO : verify if it also needs to be the first partition of the drive.
		bool found = false;
		int USB_partNum = DeviceHandler::PathToDriveType(NandEmuPath) - USB1;
		int USBport_partNum = DeviceHandler::PartitionToPortPartition(USB_partNum);
		int usbport = DeviceHandler::PartitionToUSBPort(USB_partNum);
		PartitionHandle *usbHandle = DeviceHandler::Instance()->GetUSBHandleFromPartition(USB_partNum);
		for (int partition = 0; partition <= USBport_partNum; partition++)
		{
			if (usbHandle->GetPartitionTableType(partition) != MBR)
				continue;

			if (partition == USBport_partNum)
			{
				found = true;
				break;
			}
		}
		if (!found)
		{
			WindowPrompt(tr("EmuNAND Error"), tr("Set EmuNAND channel path to USB device first primary partition to use Neek"), tr("OK"));
			return -1;
		}

		// Check HDD sector size. Only 512 bytes/sector is supported by neek?
		if (neekMode == 0 && hdd_sector_size[usbport] != BYTES_PER_SECTOR) // neek2o supports 3TB+ HDD
		{
			WindowPrompt(tr("EmuNAND Error"), tr("Neek can only run on a 512 bytes/sector USB device partition"), tr("OK"));
			return -1;
		}
	}

	// Set ocarina file.
	if (ocarinaChoice)
	{
		if (WindowPrompt(tr("Attention"), tr("Ocarina is not supported with Neek2o. Launch game anyway?"), tr("Continue"), tr("Cancel")) == 0)
			return -1;
	}

	if (!returnToChoice)
	{
		// delete residual "return to" file if last shutdown was unclean.
		snprintf(tempPath, sizeof(tempPath), "%s:/sneek/reload.sys", DeviceHandler::GetDevicePrefix(NandEmuPath));
		if (CheckFile(tempPath))
			RemoveFile(tempPath);
	}
	else
	{
		snprintf(tempPath, sizeof(tempPath), "%s/title/00010001/4e4b324f/content/title.tmd", NandEmuPath);
		if (CheckFile(tempPath))
			NK2O_isInstalled = true;
	}

	// Every checks passed successfully. Continue execution.

	// Load neek kernel.bin
	if (neekLoadKernel(NandEmuPath) == false)
	{
		WindowPrompt(tr("EmuNAND Error"), tr("An error occurred while loading Neek kernel"), tr("OK"));
		return -1;
	}

	// all is good so far, exit the loader, set the settings and boot neek.
	ExitApp();

	// Set Neek2o settings
	NEEK_CFG *neek_config = (NEEK_CFG *)NEEK_CONFIG_ADDRESS;
	memset(neek_config, 0, sizeof(NEEK_CFG));

	// Magic and version for Neek2o
	neek_config->magic = NEEK_MAGIC;

	// Set NAND path
	snprintf(neek_config->nandpath, sizeof(neek_config->nandpath), "%s", neekNandPath);
	neek_config->config |= NCON_EXT_NAND_PATH; // specify a NAND path in case default NAND set in nandcfg.bin failed
	// neek_config->config |= NCON_HIDE_EXT_PATH;  // set NAND path as temporary (attention: "return to" loads channel from the default NAND path)

	// Set TitleID to return to
	if (autoboot && returnToChoice)
	{
		// Todo : allow user to select the channel to return to.
		if (NK2O_isInstalled)
		{
			neek_config->returnto = TITLE_ID(0x00010001, 'NK2O'); // Currently forced to NK2O user channel
			neek_config->config |= NCON_EXT_RETURN_TO;			  //  enable "return to" patch
		}

		if (isWiiU())
		{
			neek_config->returnto = TITLE_ID(0x00010002, 'HCVA'); // Currently forced to "Return to WiiU" system channel
			neek_config->config |= NCON_EXT_RETURN_TO;			  //  enable "return to" patch
		}
	}

	// Set GameID - Channels
	if (autoboot && gameHeader.type == TYPE_GAME_EMUNANDCHAN)
		neek_config->titleid = gameHeader.tid;

	// Set GameID - Wii ISO
	else if (autoboot && (gameHeader.type == TYPE_GAME_WII_IMG || gameHeader.type == TYPE_GAME_WII_DISC)) // This autoobot method doesn't work in neek2o r96
	{
		neek_config->gamemagic = 0x5d1c9ea3;	   // Wii game
		neek_config->gameid = (u32)gameHeader.id;  // wbfs GameID4 to autoboot
		neek_config->config |= NCON_EXT_BOOT_GAME; // Boot di Game
	}

	// Set GameID - GameCube ISO
	else if (autoboot && (gameHeader.type == TYPE_GAME_GC_IMG || gameHdr->type == TYPE_GAME_GC_EXTRACTED)) // not implemented yet
	{
		neek_config->gamemagic = 0xC2339F3D;	   // gamecube games
		neek_config->gameid = (u32)gameHeader.id;  // GameCube GameID4 to autoboot
		neek_config->config |= NCON_EXT_BOOT_GAME; // Boot di Game

		// set DML setttings in Neek config2
		// see how to boot neek for DM/L games
	}

	// set a custom di folder
	// snprintf(neek_config->dipath, sizeof(neek_config->dipath), "/sneek/vwii"); 	// Set path for di.bin and diconfig.bin
	// neek_config->config |= NCON_EXT_DI_PATH; 										// Use custom di path

	DCFlushRange(neek_config, sizeof(NEEK_CFG));

	gprintf("NEEK: Settings:");
	hexdump((u8 *)NEEK_CONFIG_ADDRESS, sizeof(NEEK_CFG));

	if (neekBoot() == -1)
		Sys_BackToLoader();
	return 0;
}

void GameBooter::PatchSram(int language, bool patchVideoMode, bool progressive)
{
	syssram *sram = __SYS_LockSram();

	// Setup language flag
	if (language >= GC_ENGLISH && language <= GC_DUTCH)
	{
		sram->lang = language;
	}
	else // console default
	{
		sram->lang = GC_ENGLISH;
		if (CONF_GetLanguage() >= CONF_LANG_ENGLISH && CONF_GetLanguage() <= CONF_LANG_DUTCH)
		{
			sram->lang = CONF_GetLanguage() - 1;
		}
	}
	gprintf("Sram: Language set to 0x%02x\n", sram->lang);

	// Setup Video mode flags
	if (patchVideoMode)
	{
		if (progressive)
			sram->flags |= 0x80; // set progressive flag
		else
			sram->flags &= 0x7F; // clear progressive flag

		if (*Video_Mode == VI_NTSC)
		{
			sram->flags &= ~1; // Clear bit 0 to set the video mode to NTSC
			sram->ntd &= 0xBF; // clear pal60 flag
		}
		else
		{
			sram->flags |= 1;  // Set bit 0 to set the video mode to PAL
			sram->ntd |= 0x40; // set pal60 flag
		}

		gprintf("Sram: flags set to 0x%02x\n", sram->flags);
		gprintf("Sram: ntd set to 0x%02x\n", sram->ntd);
	}

	__SYS_UnlockSram(1); // 1 -> write changes
	while (!__SYS_SyncSram())
		usleep(100);
}
