/****************************************************************************
 * Copyright (C) 2014 Cyan
 * Copyright (C) 2011 Dimok
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
#include <sys/statvfs.h>
#include "harddrivesm.hpp"
#include "controls/devicehandler.hpp"
#include "settings/csettings.h"
#include "settings/meta.h"
#include "prompts/promptwindows.h"
#include "language/gettext.h"
#include "usbloader/gamelist.h"
#include "usbloader/wbfs.h"
#include "prompts/progresswindow.h"
#include "settings/gametitles.h"
#include "system/iosloader.h"
#include "wad/nandtitle.h"
#include "utils/tools.h"
#include "fileoperations/fileops.h"
#include "gecko.h"
#include "sys.h"

static const char * OnOffText[] =
{
	trNOOP( "Off" ),
	trNOOP( "On" )
};

static const char * InstallToText[] =
{
	trNOOP( "None" ),
	trNOOP( "GAMEID_Gamename" ),
	trNOOP( "Gamename [GAMEID]" )
};

static const char * SplitSizeText[] =
{
	trNOOP( "No" ),
	trNOOP( "Every 2GB" ),
	trNOOP( "Every 4GB" ),
};

static inline bool IsValidPartition(int fs_type, int cios)
{
	if (IosLoader::IsWaninkokoIOS(cios) && NandTitles.VersionOf(TITLE_ID(1, cios)) < 18)
	{
		return fs_type == PART_FS_WBFS;
	}
	else
	{
		return fs_type == PART_FS_WBFS || fs_type == PART_FS_FAT || fs_type == PART_FS_NTFS || fs_type == PART_FS_EXT;
	}
}

HardDriveSM::HardDriveSM()
	: SettingsMenu(tr("Storage Device"), &GuiOptions, MENU_NONE)
{
	int Idx = 0;
	Options->SetName(Idx++, "%s", tr("Device partition" ));
	Options->SetName(Idx++, "%s", tr("Multiple partitions" ));
	if (strncmp(Settings.ConfigPath, "sd", 2) == 0)
		Options->SetName(Idx++, "%s", tr("SD Card mode" ));
	Options->SetName(Idx++, "%s", tr("USB port" ));
	Options->SetName(Idx++, "%s", tr("Mount USB at launch" ));
	Options->SetName(Idx++, "%s", tr("Install directory" ));
	Options->SetName(Idx++, "%s", tr("Split game disc image" ));
	Options->SetName(Idx++, "%s", tr("Game partitions" ));
	Options->SetName(Idx++, "%s", tr("Compress game install" ));
	Options->SetName(Idx++, "%s", tr("32K align game install" ));
	Options->SetName(Idx++, "%s", tr("Sync FAT32 free space" ));

	OldSettingsPartition = Settings.partition;
	OldSettingsMultiplePartitions = Settings.MultiplePartitions;
	OldSettingsSDMode = Settings.SDMode;
	NewSettingsUSBPort = Settings.USBPort;
	oldSettingsUSBAutoMount = Settings.USBAutoMount;

	SetOptionValues();
}

HardDriveSM::~HardDriveSM()
{
	gprintf("SD Card mode %i\n", Settings.SDMode);
	//! if partition has changed, Reinitialize it
	if (Settings.partition != OldSettingsPartition ||
		Settings.MultiplePartitions != OldSettingsMultiplePartitions ||
		Settings.USBPort != NewSettingsUSBPort ||
		Settings.USBAutoMount != oldSettingsUSBAutoMount ||
		Settings.SDMode != OldSettingsSDMode)
	{
		if(!Settings.SDMode)
		{
			WBFS_CloseAll();

			if(Settings.USBPort != NewSettingsUSBPort)
			{
				DeviceHandler::Instance()->UnMountAllUSB();
				Settings.USBPort = NewSettingsUSBPort;
				DeviceHandler::Instance()->MountAllUSB();

				if(Settings.partition >= DeviceHandler::GetUSBPartitionCount())
					Settings.partition = 0;

				// Set -1 to edit meta.xml arguments
				NewSettingsUSBPort = -1;
			}

			WBFS_Init(Settings.SDMode ? WBFS_DEVICE_SDHC : WBFS_DEVICE_USB);
			if(Settings.MultiplePartitions)
				WBFS_OpenAll();
			else
				WBFS_OpenPart(Settings.partition);

			//! Reload the new game titles
			gameList.ReadGameList();
			gameList.LoadUnfiltered();
		}
		
		if(oldSettingsUSBAutoMount != Settings.USBAutoMount || NewSettingsUSBPort == -1 || OldSettingsSDMode != Settings.SDMode)
		{
			// Edit meta.xml arguments
			editMetaArguments();
			gprintf("USB Loader GX launch updated\n");
		}
		if(OldSettingsSDMode != Settings.SDMode)
		{
			Settings.NandEmuMode = EMUNAND_OFF;
			RemoveDirectory(Settings.GameHeaderCachePath);
			RebootApp();
		}
	}
}

void HardDriveSM::SetOptionValues()
{
	int Idx = 0;

	//! Settings: Game/Install Partition
	PartitionHandle *handle;
	int checkPart = 0;
	if (!Settings.SDMode)
	{
		handle = DeviceHandler::Instance()->GetUSBHandleFromPartition(Settings.partition);
		checkPart = DeviceHandler::PartitionToPortPartition(Settings.partition);
	}
	else
		handle = DeviceHandler::Instance()->GetSDHandle();

	//! Get the partition name and it's size in GB's
	if (handle)
		Options->SetValue(Idx++, "%s (%.2fGB)", handle->GetFSName(checkPart), handle->GetSize(checkPart)/GB_SIZE);
	else
		Options->SetValue(Idx++, tr("Not initialized"));

	//! Settings: Multiple Partitions
	Options->SetValue(Idx++, "%s", tr( OnOffText[Settings.MultiplePartitions] ));

	//! Settings: SD Card Mode
	if (strncmp(Settings.ConfigPath, "sd", 2) == 0)
		Options->SetValue(Idx++, "%s", tr( OnOffText[Settings.SDMode] ));

	//! Settings: USB Port
	if(NewSettingsUSBPort == 2)
		Options->SetValue(Idx++, tr("Both"));
	else
		Options->SetValue(Idx++, "%i", NewSettingsUSBPort);

	//! Settings: Auto Mount USB at launch
	Options->SetValue(Idx++, "%s", tr( OnOffText[Settings.USBAutoMount] ));

	//! Settings: Install directories
	Options->SetValue(Idx++, "%s", tr( InstallToText[Settings.InstallToDir] ));

	//! Settings: Game Split Size
	Options->SetValue(Idx++, "%s", tr( SplitSizeText[Settings.GameSplit] ));

	//! Settings: Install partitions
	if(Settings.InstallPartitions == ONLY_GAME_PARTITION)
		Options->SetValue(Idx++, "%s", tr("Game partition only"));
	else if(Settings.InstallPartitions == ALL_PARTITIONS)
		Options->SetValue(Idx++, "%s", tr("All"));
	else if(Settings.InstallPartitions == REMOVE_UPDATE_PARTITION)
		Options->SetValue(Idx++, "%s", tr("Remove update"));

	//! Settings: GC Install Compressed
	Options->SetValue(Idx++, "%s", tr( OnOffText[Settings.GCInstallCompressed] ));

	//! Settings: GC Install 32K Aligned
	Options->SetValue(Idx++, "%s", tr( OnOffText[Settings.GCInstallAligned] ));

	//! Settings: Sync FAT32 FS Info
	Options->SetValue(Idx++, " ");
}

int HardDriveSM::GetMenuInternal()
{
	int ret = optionBrowser->GetClickedOption();

	if (ret < 0)
		return MENU_NONE;

	int Idx = -1;

	//! Settings: Game/Install Partition
	if (ret == ++Idx)
	{
		PartitionHandle *handle; 
		if (Settings.SDMode)
		{
			handle = DeviceHandler::Instance()->GetSDHandle();
		}
		else
		{
			// Init the USB device if mounted after launch
			handle = DeviceHandler::Instance()->GetUSBHandleFromPartition(Settings.partition);
			if (handle == NULL)
				DeviceHandler::Instance()->MountAllUSB(true);
		}

		// Select the next valid partition, even if that's the same one
		int fs_type = 0;
		int ios = IOS_GetVersion();
		int retries = 20;
		do
		{
			if (Settings.SDMode)
			{
				Settings.partition = 0;
				fs_type = DeviceHandler::GetFilesystemType(SD);
			}
			else
			{
				Settings.partition = (Settings.partition + 1) % DeviceHandler::GetUSBPartitionCount();
				fs_type = DeviceHandler::GetFilesystemType(USB1+Settings.partition);
			}
		}
		while (!IsValidPartition(fs_type, ios) && --retries > 0);

		if (fs_type == PART_FS_FAT && Settings.GameSplit == GAMESPLIT_NONE)
			Settings.GameSplit = GAMESPLIT_4GB;
	}

	//! Settings: Multiple Partitions
	else if (ret == ++Idx)
	{
		if (++Settings.MultiplePartitions >= MAX_ON_OFF) Settings.MultiplePartitions = 0;
	}

	//! Settings: SD Card Mode
	else if (strncmp(Settings.ConfigPath, "sd", 2) == 0 && ret == ++Idx)
	{
		if (++Settings.SDMode >= MAX_ON_OFF) Settings.SDMode = 0;
	}

	//! Settings: USB Port
	else if (ret == ++Idx)
	{
		if(!IosLoader::IsHermesIOS() && !IosLoader::IsD2X())
		{
			WindowPrompt(tr("Device Error"), tr("Changing USB port is only supported with Hermes cIOS"), tr("OK"));
			NewSettingsUSBPort = 0;
			Settings.USBPort = 0;
		}

		else if (++NewSettingsUSBPort >= 3) // 2 = both ports
			NewSettingsUSBPort = 0;
	}

	//! Settings: Auto mount USB at launch
	else if (ret == ++Idx)
	{
		if (++Settings.USBAutoMount >= MAX_ON_OFF) Settings.USBAutoMount = 0;
	}

	//! Settings: Install directories
	else if (ret == ++Idx)
	{
		if (++Settings.InstallToDir >= INSTALL_TO_MAX) Settings.InstallToDir = 0;
	}

	//! Settings: Game Split Size
	else if (ret == ++Idx)
	{
		if (++Settings.GameSplit >= GAMESPLIT_MAX)
		{
			if (DeviceHandler::GetFilesystemType(Settings.SDMode ? SD : USB1+Settings.partition) == PART_FS_FAT)
				Settings.GameSplit = GAMESPLIT_2GB;
			else
				Settings.GameSplit = GAMESPLIT_NONE;
		}
	}

	//! Settings: Install partitions
	else if (ret == ++Idx)
	{
		switch(Settings.InstallPartitions)
		{
			case ONLY_GAME_PARTITION:
				Settings.InstallPartitions = ALL_PARTITIONS;
				break;
			case ALL_PARTITIONS:
				Settings.InstallPartitions = REMOVE_UPDATE_PARTITION;
				break;
			default:
			case REMOVE_UPDATE_PARTITION:
				Settings.InstallPartitions = ONLY_GAME_PARTITION;
				break;
		}
	}

	//! Settings: GC Install Compressed
	else if (ret == ++Idx)
	{
		if (++Settings.GCInstallCompressed >= MAX_ON_OFF) Settings.GCInstallCompressed = 0;
	}

	//! Settings: GC Install 32K Aligned
	else if (ret == ++Idx)
	{
		if (++Settings.GCInstallAligned >= MAX_ON_OFF) Settings.GCInstallAligned = 0;
	}

	//! Settings: Sync FAT32 FS Info
	else if (ret == ++Idx )
	{
		int choice = WindowPrompt(tr("Free Space Sync"), tr("Synchronize free space sectors information for all FAT32 partitions?"), tr("Yes"), tr("Cancel"));
		if (choice)
		{
			StartProgress(tr("Synchronizing"), tr("Please wait"), 0, false, false);
			int partCount = Settings.SDMode ? 1 : DeviceHandler::GetUSBPartitionCount();
			for (int i = 0; i < partCount; ++i)
			{
				ShowProgress(i, partCount);
				if (DeviceHandler::GetFilesystemType(Settings.SDMode ? SD : USB1+i) == PART_FS_FAT)
				{
					PartitionHandle *handle;
					if (Settings.SDMode)
						handle = DeviceHandler::Instance()->GetSDHandle();
					else
						handle = DeviceHandler::Instance()->GetUSBHandleFromPartition(i);
					if (!handle)
						continue;
					struct statvfs stats;
					char drive[20];
					snprintf(drive, sizeof(drive), "%s:/", handle->MountName(i));
					memset(&stats, 0, sizeof(stats));
					memcpy(&stats.f_flag, "SCAN", 4);
					statvfs(drive, &stats);
				}
			}
			ProgressStop();
		}
	}

	SetOptionValues();

	return MENU_NONE;
}
