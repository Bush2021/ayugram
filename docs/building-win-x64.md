# Build instructions for Windows 64-bit

- [Prepare folder](#prepare-folder)
- [Install third party software](#install-third-party-software)
- [Clone source code and prepare libraries](#clone-source-code-and-prepare-libraries)
- [Build the project](#build-the-project)

## Prepare folder

The build is done in **Visual Studio 2026** with **10.0.26200.0** SDK version.

Choose an empty folder for the future build, for example **D:\\TBuild**. It will be named ***BuildPath*** in the rest of this document. Create two folders there, ***BuildPath*\\ThirdParty** and ***BuildPath*\\Libraries**.

All commands (if not stated otherwise) will be launched from **x64 Native Tools Command Prompt for VS 2026.bat** (should be in **Start Menu > Visual Studio 2026** menu folder). Pay attention not to use any other Command Prompt.

## Install third party software

* Download **Python 3.14** installer from [https://www.python.org/downloads/](https://www.python.org/downloads/) and install it with adding to PATH.
* Download **Git** installer from [https://git-scm.com/download/win](https://git-scm.com/download/win) and install it.

## Clone source code and prepare libraries

Open **x64 Native Tools Command Prompt for VS 2026.bat**, go to ***BuildPath*** and run

    git clone --recursive https://github.com/Bush2021/ayugram.git ayugram
    ayugram\Telegram\build\prepare\win.bat

## Build the project

Go to ***BuildPath*\\ayugram\\Telegram** and run

    configure.bat x64 -D TDESKTOP_API_ID=2040 -D TDESKTOP_API_HASH=b18441a1ff607e10a989891a5462e627 -D DESKTOP_APP_DISABLE_AUTOUPDATE=ON -D DESKTOP_APP_NO_PDB=ON

* Open ***BuildPath*\\ayugram\\out\\Telegram.sln** in Visual Studio 2022
* Select Telegram project and press Build > Build Telegram (Debug and Release configurations)
* The result AyuGram.exe will be located in **D:\TBuild\ayugram\out\Debug** (and **Release**)