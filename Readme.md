<p align="center"><a href="https://github.com/Steveneska/usbloadergx-enhanced/" title="USB Loader GX"><img src="data/web/logo.png"></a></p>

## Description

USB Loader GX Enhanced is a backup manager for the Nintendo Wii based on the original [USB Loader GX](https://github.com/wiidev/usbloadergx),
with bug fixes and a revamped user interface to:

- Play games from a USB storage device or an SD Card
- Launch other homebrew apps
- Create backups of your original game discs
- Use cheats codes in games and a whole lot more

## Building

The easiest way to build the project is to start a codespace from the repository with Visual Studio Code. Once created, open a Terminal window
and run the following:

```bash
make clean && make
```

## Quick Installation

1. Copy the compiled `boot.dol` to the `apps/usbloadergx` folder of your SD Card and replace any existing files
2. Install the [d2x Custom IOS Installer](https://wii.hacks.guide/assets/files/d2x-cios-installer.zip) by extracting the `apps` folder from the
downloaded achive to the root of your SD Card
3. Install the required custom IOS by setting everything to match the table below. `Slot 248 Base 38 IOS` is only compatible with Wii systems.
4. Optional: Download the `wiitdb.xml` file by selecting the update option within the loader Settings menu
5. Optional: Install a forwarder channel for [Wii](https://github.com/wiidev/usbloadergx/raw/updates/USBLoaderGX_forwarder%5BUNEO%5D_Wii.wad) or
[vWii (Wii U)](https://github.com/wiidev/usbloadergx/raw/updates/USBLoaderGX_forwarder%5BUNEO%5D_vWii.wad) and then set the return to setting to `UNEO`

## cIOS Reference

Patched IOS enable custom functionalities of loaders like USB Loader GX. Generally they will extend a Wii with little to no downsides.
The configuration below should improves compatibility on Wii and vWii (Wii U).

| File name          | IOS Slot | IOS Base |  IOS Revision  |    Compatibility     |
| :----------------- | :------: | :------: | :------------: | :------------------: |
| IOS38-64-v4123.wad | Slot 248 | Base  38 | Revision 65535 |       Wii Only       |
| IOS56-64-v5661.wad | Slot 249 | Base  56 | Revision 65535 | Wii and vWii (Wii U) |
| IOS57-64-v5918.wad | Slot 250 | Base  57 | Revision 65535 | Wii and vWii (Wii U) |
| IOS58-64-v6175.wad | Slot 251 | Base  58 | Revision 65535 | Wii and vWii (Wii U) |
