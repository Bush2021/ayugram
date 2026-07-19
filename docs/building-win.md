# Build instructions for Windows

- [Prepare folder](#prepare-folder)
- [Install third party software](#install-third-party-software)
- [Choose architecture and initialize terminal](#choose-architecture-and-initialize-terminal)
- [Clone source code and prepare libraries](#clone-source-code-and-prepare-libraries)
- [Build the project](#build-the-project)

## Prepare folder

The build uses the MSVC toolchain from **Visual Studio 2026** with **10.0.26100.0** SDK version and CMake with Ninja.

Choose an empty folder for the future build, for example **D:\\TBuild**. It will be named ***BuildPath*** in the rest of this document. Create two folders there, ***BuildPath*\\ThirdParty** and ***BuildPath*\\Libraries**.

The default modern toolset from Visual Studio 2026 (`v145`) does not support Windows 7, so for Telegram Desktop you must use `-vcvars_ver=14.44` (`v144.4`, based on `v143` with Windows 7 support).

## Install third party software

* Download **Python 3.14** installer from [https://www.python.org/downloads/](https://www.python.org/downloads/) and install it with adding to PATH.
* Download **Git** installer from [https://git-scm.com/download/win](https://git-scm.com/download/win) and install it.
* In the Visual Studio Installer, install the Desktop development with C++ workload, including CMake tools for Windows and Ninja.

## Choose architecture and initialize terminal

Before preparing libraries and running build commands, initialize the Visual Studio environment for your target architecture.
The default modern toolset from Visual Studio 2026 (`v145`) does not support Windows 7, so for Telegram Desktop you must use `-vcvars_ver=14.44` (`v144.4`, based on `v143` with Windows 7 support).

For `win` (32-bit):

    %comspec% /k "C:\Program Files\Microsoft Visual Studio\18\Enterprise\VC\Auxiliary\Build\vcvars32.bat" -vcvars_ver=14.44

For `win64` (64-bit):

    %comspec% /k "C:\Program Files\Microsoft Visual Studio\18\Enterprise\VC\Auxiliary\Build\vcvars64.bat" -vcvars_ver=14.44

Run both `Clone source code and prepare libraries` and `Build the project` sections in the terminal initialized with one of the commands above.

## Clone source code and prepare libraries

In the initialized terminal, go to ***BuildPath*** and run

    git clone --recursive https://github.com/Bush2021/ayugram.git
    ayugram\Telegram\build\prepare\win.bat

## Build the project

Go to ***BuildPath*\\ayugram\\Telegram** and run:

For `win` (32-bit):

    configure.bat -G "Ninja Multi-Config" -D TDESKTOP_API_ID=2040 -D TDESKTOP_API_HASH=b18441a1ff607e10a989891a5462e627 -D DESKTOP_APP_DISABLE_AUTOUPDATE=ON

For `win64` (64-bit):

    configure.bat -G "Ninja Multi-Config" -D TDESKTOP_API_ID=2040 -D TDESKTOP_API_HASH=b18441a1ff607e10a989891a5462e627 -D DESKTOP_APP_DISABLE_AUTOUPDATE=ON

Build the Release version:

    cmake --build ..\out --config Release --target Telegram --parallel

For a Debug build, replace `Release` with `Debug`. The result `AyuGram.exe` will be located in **D:\TBuild\ayugram\out\Release** or **D:\TBuild\ayugram\out\Debug**.

After the first configuration, source-only changes normally need only the `cmake --build` command. Re-run `configure.bat` when changing CMake options or CMake files. A build directory configured with the Visual Studio generator cannot be reused with Ninja; remove and reconfigure `out` when switching generators.

If you encounter issue like `error C1090: PDB API call failed, error code '12'` on Release build, apply the following patch in `tdesktop/cmake` folder (via pwsh or manually):

```diff
@'
diff --git a/options_win.cmake b/options_win.cmake
index c2d66cf..ccceb53 100644
--- a/options_win.cmake
+++ b/options_win.cmake
@@ -32,6 +32,7 @@ if (MSVC)
       /utf-8
       /W4
       /MP     # Enable multi process build.
+        /FS
       /EHsc   # Catch C++ exceptions only, extern C functions never throw a C++ exception.
       /w15038 # wrong initialization order
       /w14265 # class has virtual functions, but destructor is not virtual
@@ -64,7 +65,7 @@ if (MSVC)
   INTERFACE
       $<$<CONFIG:Debug>:/NODEFAULTLIB:LIBCMT>
       $<$<AND:$<CONFIG:Debug>,$<BOOL:${build_win64}>>:/DEBUG:FASTLINK>
-        $<$<NOT:$<AND:$<CONFIG:Debug>,$<BOOL:${build_win64}>>>:$<IF:$<STREQUAL:$<GENEX_EVAL:
$<TARGET_PROPERTY:MSVC_DEBUG_INFORMATION_FORMAT>>,ProgramDatabase>,/DEBUG,/DEBUG:NONE>>
+        $<$<NOT:$<AND:$<CONFIG:Debug>,$<BOOL:${build_win64}>>>:$<IF:$<BOOL:$<GENEX_EVAL:
$<TARGET_PROPERTY:MSVC_DEBUG_INFORMATION_FORMAT>>>,/DEBUG,/DEBUG:NONE>>
       $<$<NOT:$<CONFIG:Debug>>:/OPT:REF>
       /INCREMENTAL:NO
       /DEPENDENTLOADFLAG:0x800
diff --git a/variables.cmake b/variables.cmake
index d6ac6c5..b2f492a 100644
--- a/variables.cmake
+++ b/variables.cmake
@@ -21,7 +21,9 @@ if (DESKTOP_APP_SPECIAL_TARGET STREQUAL ""
endif()

set(CMAKE_CXX_SCAN_FOR_MODULES OFF CACHE BOOL "")
-set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "ProgramDatabase" CACHE STRING "")
+set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT
+    "$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>$<$<CONFIG:Release,MinSizeRel>:Embedded>"
+    CACHE STRING "" FORCE)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "")
option(DESKTOP_APP_TEST_APPS "Build test apps, development only." OFF)
option(DESKTOP_APP_LOTTIE_DISABLE_RECOLORING "Disable recoloring of lottie animations." OFF)
'@ | git -C cmake apply -
```
