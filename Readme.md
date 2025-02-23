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

## Installation

1. Copy the compiled `boot.dol` to the `apps/usbloader_gx` folder of your SD Card and replace any existing files
2. Optional: Install a forwarder channel for [Wii](https://github.com/wiidev/usbloadergx/raw/updates/USBLoaderGX_forwarder%5BUNEO%5D_Wii.wad) or
[vWii (Wii U)](https://github.com/wiidev/usbloadergx/raw/updates/USBLoaderGX_forwarder%5BUNEO%5D_vWii.wad) and set the return setting to `UNEO`
