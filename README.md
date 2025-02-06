Aura
====

Fork of Josko's aura-bot
------------------------

This is a fork of Josko's with the following changes:
* Support for 1.28.5 according to [this issue comment](https://github.com/Josko/aura-bot/issues/72#issuecomment-387125132)
* Added game owner and root admin protection so they can't be kicked or bullied by regular admins
* Show commands to all players when a command is given
* Added startnow command
* Newest PG readable StormLib added

Overview
--------

Aura is a Warcraft III hosting-bot based on GHost++ by Varlock. It's a complete
overhaul with speed and efficiency in mind and packed with fewer dependencies.

Removed features from GHost++:
* No MySQL support
* No autohost
* No admin game
* No language.cfg
* No W3MMD support
* No replay saving
* No save/load games
* No BNLS support
* No boost required

Other changes:
* Uses C++14
* Single-threaded
* Has a Windows 64-bit build
* Uses SQLite and a different database organization.
* Tested on OS X (see [Building -> OS X](#os-x) for detailed requirements)
* A lot of code removed, about 1 MB smaller binary on Linux
* Updated libraries: StormLib, SQLite, zlib
* Connects to and can be controlled via IRC
* Using aggressive optimizations
* Up to 11 fakeplayers can be added.
* Uses DotA stats automagically on maps with 'DotA' in the filename
* Auto spoofcheck in private games on PvPGNs
* More commands added either ingame or bnet
* Checked with various tools such as clang-analyzer and cppcheck

Multi-platform
--------------

The bot runs on little-endian Linux (32-bit and 64-bit), Windows (32-bit and 64-bit) and OS X (64-bit Intel CPU) machines.

USING DOCKER
------

## Installation

### 1. Install Docker

If you haven't installed Docker yet, install it by running:

```shell
curl -sSL https://get.docker.com | sh
sudo usermod -aG docker $(whoami)
exit
```
And log in again.

### 2. Run Bot Easy

To automatically install & run aura-bot, simply run:

```shell
docker run \
  --name aura-bot \
  --env SERVER=server.eurobattle.net \
  --env BOT_USER_NAME=<YOUR_BOT_USER_NAME> \
  --env BOT_PASSWORD=<YOUR_BOT_PASSWORD> \
  --env ROOT_ADMINS=<YOUR_USER_NAME> <USER_NAME_ADMIN2> \
  -v ./aura-bot:/app/data \
  -p 6113-6114:6113-6114/udp \
  -p 6113-6114:6113-6114/tcp \
  --restart on-failure:5 \
  nhatnhat011/aura-bot:latest
```

Docker compose 
```yaml
services:
  aura-bot:
    image: nhatnhat011/aura-bot:latest
    container_name: aura-bot
    environment:
      #- SCRIPT_PATH= # (Option) Default: ./aura.cfg
      #- BOT_VIRTUAL_NAME= # (Option) Default: |cFFFF0000Aura
      #- HOST_PORT= # (Option) Default: 6113
      #- RECONNECT_PORT= # (Option) Default: 6114
      - SERVER=server.eurobattle.net # Required to run. Default: server.eurobattle.net
      #- SERVER_PORT= # (Option) Default port 6112
      #- SERVER_ALIAS= # (Option) Default EuroBattle
      - BOT_USER_NAME= # Required to run
      - BOT_PASSWORD= # Required to run
      - ROOT_ADMINS= # Required to control bots
      #- WAR_VERSION= # (Option) Default: 28
    volumes:
      - ./aura-bot:/app/data
    ports:
      - 6113-6114:6113-6114/udp
      - 6113-6114:6113-6114
    restart: on-failure:5
    logging:
      options:
        max-size: "5m"
        max-file: "2"
```

Building
--------

### Windows

Windows users must use VS2015 or later. Visual Studio 2015 Community edition works.
Neccessary .sln and .vcxproj files are provided. Before building, choose the Release configuration and Win32 or x64 as the platform.
The binary shall be generated in the `..\aura-bot\aura\Release` folder.

Note: When installing Visual Studio select in the `Desktop development with C++` category the `Windows 8.1 SDK` or `Windows 10 SDK` (depending on your OS version), and, if running with VS2017 or newer, also the `VC++ 2015.3 v140 toolset for desktop (x86, x64)`.

### Linux

Linux users will probably need some packages for it to build:

* Debian/Ubuntu -- `apt-get install git build-essential m4 libgmp3-dev cmake libbz2-dev zlib1g-dev`
* Arch Linux -- `pacman -S base-devel cmake`

#### Steps

For building StormLib execute the following commands (line by line):

	cd ~/aura-bot/StormLib/
	mkdir build
	cd build
	cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_DYNAMIC_MODULE=1 ..
	make
	sudo make install

Continue building bncsutil:

	cd ~/aura-bot/bncsutil/src/bncsutil/
	make
	sudo make install

Then proceed to build Aura:

	cd ~/aura-bot/
	make
	sudo make install

**Note**: gcc version needs to be 5 or higher along with a compatible libc.

**Note**: clang needs to be 3.6 or higher along with ld gold linker (ie. package binutils-gold for ubuntu)

**Note**: StormLib installs itself in `/usr/local/lib` which isn't in PATH by default
on some distros such as Arch or CentOS.

### OS X

#### Requirements

* OSX ≥10.9, possibly even higher for necessary C++14 support. It is verified to work and tested on OSX 10.11.
* Latest available Xcode for your platform and/or the Xcode Command Line Tools.
One of these might suffice, if not just get both.
* A recent `libgmp`.

You can use [Homebrew](http://brew.sh/) to get `libgmp`. When you are at it, you can also use it to install StormLib instead of compiling it on your own:

	brew install gmp
	brew install stormlib   # optional

Now proceed by following the [steps for Linux users](#steps) and omit StormLib in case you installed it using `brew`.


Configuring
-----------

Modify the `aura.cfg` file to configure the bot to your wishes.

Credits
-------

* Varlock -- the author of the GHost++ bot
* Argon- -- suggestions, code, bug fixes, testing and OS X support
* Joakim -- testing and bug reports
* PhillyPhong -- testing and bug reports

Contributing
------------

That would be lovely.

1. Fork it.
2. Create a branch (`git checkout -b my_aura`)
3. Commit your changes (`git commit -am "Fixed a crash when using GProxy++"`)
4. Push to the branch (`git push origin my_aura`)
5. Create an [Issue][1] with a link to your branch or make Pull Request
6. Enjoy a beer and wait

[1]: https://github.com/Josko/aura-bot/issues
