Building
--------

Get the source code from the project [repository][1] at Gitlab.

### Windows

Windows users must use VS2019 or later. Visual Studio 2019 Community edition works.

#### Steps

- Open `aura.sln` with VS2019.
- Choose the ``Release`` configuration, and Win32 or x64 as the platform.
- Compile the solution.
- Find the generated binary in the `.msvc\Release` folder.

**Note**: When installing Visual Studio select in the `Desktop development with C++` category the `Windows 8.1 SDK` or `Windows 10 SDK` 
(depending on your OS version), and, if running with VS2019 or newer, also the `MSVC v142 - VS 2019 C++ x64/x86 build tools (v14.29)`.

**Note**: If you have trouble getting some components to build, you may use the ``ReleaseLite`` configuration instead. Alternatively, 
you may manually disable troublesome components in the project ``"Configuration Properties"``. Find instructions below. [3] 

### Linux

Linux users will probably need some packages for it to build:

* Debian/Ubuntu -- `apt-get install git build-essential m4 libgmp3-dev libssl-dev cmake libbz2-dev zlib1g-dev libcurl4-openssl-dev curl`
* Arch Linux -- `pacman -S base-devel cmake libssl-dev libgmp3-dev curl libssl-dev`

#### Steps

For building StormLib execute the following commands (line by line):

	cd aura-bot/deps/StormLib
	mkdir build
	cd build
	cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=1 ..
	make
	sudo make install

Next, build bncsutil:

	cd ../..
	cd bncsutil/src/bncsutil
	make
	sudo make install

Continue building miniupnpc

	cd ../../..
	cd miniupnpc
	make
	sudo make install

  (Or disable it by setting an environment variable: ``export AURALINKMINIUPNP=0``)

Afterwards, C++ Requests

	cd ../..
	git clone https://github.com/libcpr/cpr.git cpr-src
	cd cpr-src
	mkdir build
	cd build
	cmake .. -DCPR_USE_SYSTEM_CURL=ON -DBUILD_SHARED_LIBS=1
	cmake --build . --parallel
	cmake --install .

  (Or disable it by setting an environment variable: ``export AURALINKCPR=0``)

Optionally, D++ for Discord integration. Note that this step can take around half an hour.

	cd ../../
	git clone https://github.com/brainboxdotcc/DPP.git dpp-src
	cd dpp-src
	mkdir build
	cd build
	cmake .. -DBUILD_SHARED_LIBS=ON -DBUILD_VOICE_SUPPORT=OFF -DDPP_BUILD_TEST=OFF
	cmake --build . -j4
	make install

  (Or disable it by setting an environment variable: ``export AURALINKDPP=0``)

Then, proceed to build Aura:

	cd ../../..
	make

Now you can run Aura by executing `./aura` or install it to your PATH using `sudo make install`.

**Note**: gcc version needs to be 8 or higher along with a compatible libc.

**Note**: clang needs to be 5 or higher along with ld gold linker (ie. package binutils-gold for ubuntu),
clang lower than 16 requires -std=c++17

**Note**: StormLib installs itself in `/usr/local/lib` which isn't in PATH by default
on some distros such as Arch or CentOS.

### OS X

#### Requirements

* OSX ≥10.15 (Catalina) or higher.
* Latest available Xcode for your platform and/or the Xcode Command Line Tools.
One of these might suffice, if not just get both.
* A recent `libgmp`.

You can use [Homebrew](http://brew.sh/) to get `libgmp`. When you are at it, you can also use it to install StormLib instead of compiling it on your own:

	brew install gmp
	brew install stormlib   # optional

Now proceed by following the [steps for Linux users](#steps) and omit StormLib in case you installed it using `brew`.

### Optional components

When using Makefile and setting the appropriate environment variables to disable components, as described in the
Linux build steps, this section will be automatically taken care of.

When using MSVC, follow these steps to disable components.

- **C++ Requests** (CPR) is disabled with the preprocessor directive ``DISABLE_CPR``. The following linked libraries must be removed:

  Windows: cpr.lib;libcurl.lib;Crypt32.lib;Wldap32.lib

- **MiniUPnP** is disabled with the preprocessor directive ``DISABLE_MINIUPNP``. The following linked libraries must be removed:

  Windows: miniupnpc.lib
  
- **D++** is disabled with the preprocessor directive ``DISABLE_DPP``. The following linked libraries must be removed:

  Windows: dpp.lib

[1]: https://gitlab.com/ivojulca/aura-bot
[2]: https://github.com/libcpr/cpr
[3]: https://gitlab.com/ivojulca/aura-bot/BUILDING.md?ref_type=heads#optional-components
