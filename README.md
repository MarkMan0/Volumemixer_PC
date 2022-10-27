# Volumemixer_PC

The PC application of the VolumeMixer project


## Project Structure

The project is using CMake, and MSVC compiler. MFC and ATL libraries are needed to be installed from Visual Studio, to be able to build the project.

### Libraries

+ ComEnum - list COM ports and some of their properties, uses `SetupAPI.h`
+ CommSupervisor - CRC32 algorithm and helper classes to create/decode messages
+ SerialPortWrapper - Simplify working with COM ports in windows
+ VolumeAPI - retrieve info about audio sessions

### Examples
Helper executables to use-test/demo certain parts of the project

### MixerClient
The main executable of the project. 

The client will detect, when the board is disconnected, and will automatically look for a port, which has the board. The correct port is detected, by examining the *bus reported device description* property of the COM port. After that, the program will wait for commands from the board.

This close-search-open is done in a state machine, to reduce if-else clutter, and to allow for easy expansion in the future.

### Dependencies
MFC and ATL libraries are needed and used, these should be installed in *Visual Studio Installer*. No other external library is required.

[Magick](https://imagemagick.org/index.php) needs to be available on the PATH. It's used to convert .ico files to PNG.


### Building
If the dependencies are met, the project should build. Only MSVC compiler is supported.

### Formatting
A `.clang_format` file is included with the project, along with a `.pre-commit-config.yaml`. [pre-commit](https://pre-commit.com/) should be enabled, to only allow formatted commits into the repo.


## TODO
These only concern this application, not the whole project.

1. Register for device notification, to detect when a usb is inserted/removed, and not check periodically
1. Register for notification on volume change, and don't query each time
1. make this a windows service, so it will be able to run in the background
1. do the .ico to .png conversion inside the program, and don't call magick.
