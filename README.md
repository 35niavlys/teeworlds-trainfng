Trainfng is a Teeworlds mod created from [ddnet](https://ddnet.tw).
The aim of this mod is to be able to train easily to the [fng](https://github.com/fstd/teeworlds) mod.

Cloning
-------

To clone this repository with full history and external libraries (~350 MB):

    git clone --recursive https://github.com/35niavlys/teeworlds-trainfng

To clone this repository with full history when you have the necessary libraries on your system already (~220 MB):

    git clone https://github.com/35niavlys/teeworlds-trainfng

To clone this repository with history since we moved the libraries to https://github.com/35niavlys/teeworlds-trainfng-libs (~40 MB):

    git clone --shallow-exclude=included-libs https://github.com/35niavlys/teeworlds-trainfng

To clone the libraries if you have previously cloned DDNet without them:

    git submodule update --init --recursive

Building on Linux and macOS
---------------------------

To compile DDNet yourself, execute the following commands in the source root:

    mkdir build
    cd build
    cmake ..
    make

DDNet requires additional libraries, that are bundled for the most common platforms (Windows, Mac, Linux, all x86 and x86\_64). The bundled libraries are now in the ddnet-libs submodule.

You can install the required libraries on your system, `touch CMakeLists.txt` and CMake will use the system-wide libraries by default. You can install all required dependencies and CMake on Debian or Ubuntu like this:

    sudo apt install cmake git libcurl4-openssl-dev libfreetype6-dev libglew-dev libogg-dev libopus-dev libopusfile-dev libpnglite-dev libsdl2-dev libwavpack-dev python

Or on Arch Linux like this (Arch Linux does not package `pnglite`, not even in AUR):

    sudo pacman -S --needed cmake curl freetype2 git glew opusfile sdl2 wavpack python

If you have the libraries installed, but still want to use the bundled ones instead, you can do so by removing your build directory and re-running CMake with `-DPREFER_BUNDLED_LIBS=ON`, e.g. `cmake -DPREFER_BUNDLED_LIBS=ON ..`.

MySQL (or MariaDB) support in the server is not included in the binary releases but it can be built by specifying `-DMYSQL=ON`, like `cmake -DMYSQL=ON ..`. It requires `libmariadbclient-dev`, `libmysqlcppconn-dev` and `libboost-dev`, which are also bundled for the common platforms.

Note that the bundled MySQL libraries might not work properly on your system. If you run into connection problems with the MySQL server, for example that it connects as root while you chose another user, make sure to install your system libraries for the MySQL client and C++ connector. Make sure that the CMake configuration summary says that it found MySQL libs that were not bundled (no "using bundled libs").

Building on Windows with Visual Studio
--------------------------------------

Download and install some version of [Microsoft Visual Studio](https://www.visualstudio.com/) (as of writing, MSVS Community 2017) with **C++ support**, install [Python 3](https://www.python.org/downloads/) **for all users** and install [CMake](https://cmake.org/download/#latest).

Start CMake and select the source code folder (where DDNet resides, the directory with `CMakeLists.txt`). Additionally select a build folder, e.g. create a build subdirectory in the source code directory. Click "Configure" and select the Visual Studio generator (it should be pre-selected, so pressing "Finish" will suffice). After configuration finishes and the "Generate" reactivates, click it. When that finishes, click "Open Project". Visual Studio should open. You can compile the DDNet client by right-clicking the DDNet project (not the solution) and select "Select as StartUp project". Now you should be able to compile DDNet by clicking the green, triangular "Run" button.

Cross-compiling on Linux to Windows x86/x86\_64
-----------------------------------------------

Install MinGW cross-compilers of the form `i686-w64-mingw32-gcc` (32 bit) or
`x86_64-w64-mingw32-gcc` (64 bit). This is probably the hard part. ;)

Then add `-DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/mingw64.toolchain` to the
**initial** CMake command line.

Cross-compiling on Linux to macOS
---------------------------------

Install [osxcross](https://github.com/tpoechtrager/osxcross), then add
`-DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/darwin.toolchain` and
`-DCMAKE_OSX_SYSROOT=/path/to/osxcross/target/SDK/MacOSX10.11.sdk/` to the
**initial** CMake command line.

Install `dmg` and `hfsplus` from
[libdmg-hfsplus](https://github.com/mozilla/libdmg-hfsplus) and `newfs_hfs`
from
[diskdev\_cmds](http://pkgs.fedoraproject.org/repo/pkgs/hfsplus-tools/diskdev_cmds-540.1.linux3.tar.gz/0435afc389b919027b69616ad1b05709/diskdev_cmds-540.1.linux3.tar.gz)
to unlock the `package_dmg` target that outputs a macOS disk image.
