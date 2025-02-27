#include <dirent.h>
#include <unistd.h>

#include "fileoperations/fileops.h"
#include "controls/devicehandler.hpp"
#include "wad/nandtitle.h"
#include "system/iosloader.h"
#include "menu/menus.h"
#include "wpad.h"
#include "usbloader/wbfs.h"
#include "usbloader/gamelist.h"
#include "settings/gametitles.h"
#include "xml/gametdb.hpp"
#include "utils/showerror.h"
#include "cache/cache.hpp"

static int FindGamePartition()
{
	int partCount = DeviceHandler::GetUSBPartitionCount();

	// Loop through all WBFS partitions first to check them in case IOS249 Rev < 18
	for(int i = 0; i < partCount; ++i)
	{
		if(DeviceHandler::GetFilesystemType(USB1+i) != PART_FS_WBFS)
			continue;

		if (WBFS_OpenPart(i) == 0)
		{
			GameTitles.SortTitleList();
			Settings.partition = i;
			return 0;
		}
	}

	int firstValidPartition = -1;

	if(IosLoader::IsWaninkokoIOS() && NandTitles.VersionOf(TITLE_ID(1, IOS_GetVersion())) < 18)
		return -1;

	// Loop through FAT/NTFS/EXT partitions, and find the first partition with games on it (if there is one)
	for(int i = 0; i < partCount; ++i)
	{
		if(DeviceHandler::GetFilesystemType(USB1+i) != PART_FS_NTFS &&
			DeviceHandler::GetFilesystemType(USB1+i) != PART_FS_FAT &&
			DeviceHandler::GetFilesystemType(USB1+i) != PART_FS_EXT)
		{
			continue;
		}

		if (WBFS_OpenPart(i) != 0)
			continue;

		u32 count;
		// Get the game count
		WBFS_GetCount(i, &count);

		if (count > 0)
		{
			GameTitles.SortTitleList();
			Settings.partition = i;
			return 0;
		}

		if(firstValidPartition < 0)
			firstValidPartition = i;

		WBFS_Close(i);
	}

	if(firstValidPartition >= 0)
	{
		GameTitles.SortTitleList();
		Settings.partition = firstValidPartition;
		return 0;
	}

	return -1;
}

static int FindGamePartitionSD()
{
	// Check for a WBFS partition first in case IOS249 Rev < 18
	if (DeviceHandler::GetFilesystemType(0) == PART_FS_WBFS)
	{
		if (WBFS_OpenPart(0) == 0)
		{
			GameTitles.SortTitleList();
			Settings.partition = 0;
			return 0;
		}
	}

	if (IosLoader::IsWaninkokoIOS() && NandTitles.VersionOf(TITLE_ID(1, IOS_GetVersion())) < 18)
		return -1;

	// Check if it's a FAT/NTFS/EXT partition and if it's got games on it
	if (DeviceHandler::GetFilesystemType(0) == PART_FS_NTFS || DeviceHandler::GetFilesystemType(0) == PART_FS_FAT || DeviceHandler::GetFilesystemType(0) == PART_FS_EXT)
	{
		if (WBFS_OpenPart(0) == 0)
		{
			u32 count;
			// Get the game count
			WBFS_GetCount(0, &count);
			if (count > 0)
			{
				GameTitles.SortTitleList();
				Settings.partition = 0;
				return 0;
			}
			WBFS_Close(0);
		}
	}

	return -1;
}

static int PartitionChoice()
{
	int ret = -1;

	int choice = WindowPrompt(tr("Device Error"), tr("No supported partition found. Select an option below to continue or switch to Channel mode"), tr("Select"), tr("Format"), tr("Channel" ));
	if (choice == 0)
	{
		Settings.LoaderMode = MODE_NANDCHANNELS;
		return 0;
	}
	else if(choice == 1)
	{
		int part_num = SelectPartitionMenu();
		if(part_num >= 0)
		{
			if(IosLoader::IsWaninkokoIOS() && NandTitles.VersionOf(TITLE_ID(1, IOS_GetVersion())) < 18 &&
				(DeviceHandler::GetFilesystemType(USB1+part_num) == PART_FS_NTFS ||
				DeviceHandler::GetFilesystemType(USB1+part_num) == PART_FS_FAT ||
				DeviceHandler::GetFilesystemType(USB1+part_num) == PART_FS_EXT))
			{
				WindowPrompt(tr("Attention"), tr("You are trying to select a partition while running an old version of cIOS 249. Continue at your own risk"), tr("OK"));
			}

			ret = WBFS_OpenPart(part_num);

			Settings.partition = part_num;
			Settings.Save();
		}
	}
	else if(choice == 2)
	{
		while(ret < 0 || ret == -666)
		{
			int part_num = SelectPartitionMenu();
			if(part_num >= 0)
				ret = FormatingPartition(tr("Formatting partition"), part_num);
		}
	}

	return ret;
}

/****************************************************************************
 * MountGamePartition
 ***************************************************************************/
int MountGamePartition(bool ShowGUI)
{
	s32 ret = -1;
	gprintf("MountGamePartition()\n");

	s32 wbfsinit = WBFS_Init(Settings.SDMode ? WBFS_DEVICE_SDHC : WBFS_DEVICE_USB);

	if (wbfsinit < 0)
	{
		if (Settings.LoaderMode & MODE_WIIGAMES)
		{
			if (ShowGUI)
				ShowError("%s %s", tr("USB storage device not initialized"), tr("Switching to Channel mode"));

			Settings.LoaderMode &= ~MODE_WIIGAMES;
			Settings.LoaderMode |= MODE_NANDCHANNELS;
		}
	}
	else
	{
		if (Settings.SDMode)
			ret = WBFS_OpenPart(Settings.partition);
		else if (Settings.MultiplePartitions)
			ret = WBFS_OpenAll();
		else if (!Settings.FirstTimeRun)
			ret = WBFS_OpenPart(Settings.partition);

		if (Settings.LoaderMode & MODE_WIIGAMES)
		{
			if (ret < 0)
				ret = Settings.SDMode ? FindGamePartitionSD() : FindGamePartition();

			if (ret < 0)
			{
				if (ShowGUI && !Settings.SDMode)
					PartitionChoice();
				else
					Settings.LoaderMode = MODE_NANDCHANNELS;
			}
		}
	}

	gprintf("\tDisc_Init\n");
	ret = Disc_Init();
	if (ret < 0)
	{
		if (ShowGUI)
			WindowPrompt(tr("System Error"), tr("An error occurred while initializing DIP module"), tr("OK"));
		Sys_LoadMenu();
	}

	if (ShowGUI && Settings.CacheCheck && !isCacheCurrent())
		ResetGameHeaderCache();

	gameList.LoadUnfiltered();

	return ret;
}
