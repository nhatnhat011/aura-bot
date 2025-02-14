SHELL = /bin/sh
SYSTEM = $(shell uname)
ARCH = $(shell uname -m)
INSTALL_DIR = /usr

ifndef CC
	CC = gcc
endif

ifndef CXX
	CXX = g++
endif

CCFLAGS += -fno-builtin
CXXFLAGS += -g -std=c++17 -pipe -pthread -Wall -Wextra -fno-builtin -fno-rtti
DFLAGS = -DNDEBUG
OFLAGS = -O3 -flto
LFLAGS += -pthread -L. -Llib/ -L/usr/local/lib/ -Ldeps/bncsutil/src/bncsutil/ -lgmp -lbz2 -lz -lstorm -lbncsutil

ifeq ($(AURASTATIC), 1)
	LFLAGS += -static
endif

ifeq ($(AURALINKMINIUPNP), 0)
	CXXFLAGS += -DDISABLE_MINIUPNP
else
	ifeq ($(AURASTATIC), 1)
		LFLAGS += -lminiupnpc
	else
		LFLAGS += -lminiupnpc
	endif
endif

ifeq ($(AURALINKCPR), 0)
	CXXFLAGS += -DDISABLE_CPR
else
	ifeq ($(AURASTATIC), 1)
		LFLAGS += -lcpr
	else
		LFLAGS += -lcpr
	endif
endif

ifeq ($(AURALINKDPP), 0)
	CXXFLAGS += -DDISABLE_DPP
else
	ifeq ($(AURASTATIC), 1)
		LFLAGS += -ldpp
	else
		LFLAGS += -ldpp
	endif
endif

ifeq ($(ARCH),x86_64)
	CCFLAGS += -m64
	CXXFLAGS += -m64
endif

ifeq ($(SYSTEM),Darwin)
	INSTALL_DIR = /usr/local
	CXXFLAGS += -stdlib=libc++
	CC = clang
	CXX = clang++
	DFLAGS += -D__APPLE__
else
	LFLAGS += -lrt
endif

ifeq ($(SYSTEM),FreeBSD)
	DFLAGS += -D__FREEBSD__
endif

ifeq ($(SYSTEM),SunOS)
	DFLAGS += -D__SOLARIS__
	LFLAGS += -lresolv -lsocket -lnsl
endif

CCFLAGS += $(OFLAGS) -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION -I.
CXXFLAGS += $(OFLAGS) $(DFLAGS) -I. -Ilib/ -Ideps/bncsutil/src/ -Ideps/StormLib/src/ -Ideps/miniupnpc/include/ -Icpr-src/include/ -Idpp-src/include/

OBJS = lib/csvparser/csvparser.o \
			 lib/crc32/crc32.o \
			 lib/sha1/sha1.o \
			 src/protocol/bnet_protocol.o \
			 src/protocol/game_protocol.o \
			 src/protocol/gps_protocol.o \
			 src/protocol/vlan_protocol.o \
			 src/config/config.o \
			 src/config/config_bot.o \
			 src/config/config_realm.o \
			 src/config/config_commands.o \
			 src/config/config_game.o \
			 src/config/config_irc.o \
			 src/config/config_discord.o \
			 src/config/config_net.o \
			 src/auradb.o \
			 src/bncsutil_interface.o \
			 src/file_util.o \
			 src/os_util.o \
			 src/map.o \
			 src/packed.o \
			 src/save_game.o \
			 src/socket.o \
			 src/connection.o \
			 src/net.o \
			 src/realm.o \
			 src/realm_chat.o \
			 src/async_observer.o \
			 src/game_seeker.o \
			 src/game_user.o \
			 src/game_setup.o \
			 src/game_slot.o \
			 src/game_virtual_user.o \
			 src/game.o \
			 src/aura.o \
			 src/cli.o \
			 src/command.o \
			 src/discord.o \
			 src/irc.o \
			 src/stats.o \
			 src/w3mmd.o \

COBJS = lib/sqlite3/sqlite3.o

PROG = aura

all: $(OBJS) $(COBJS) $(PROG)
	@echo "Used CFLAGS: $(CXXFLAGS)"

$(PROG): $(OBJS) $(COBJS)
	@$(CXX) -o aura $(OBJS) $(COBJS) $(CXXFLAGS) $(LFLAGS)
	@echo "[BIN] $@ created."
	@strip "$(PROG)"
	@echo "[BIN] Stripping the binary."

clean:
	@rm -f $(OBJS) $(COBJS) $(PROG)
	@echo "Binary and object files cleaned."

install:
	@install -d "$(DESTDIR)$(INSTALL_DIR)/bin"
	@install $(PROG) "$(DESTDIR)$(INSTALL_DIR)/bin/$(PROG)"
	@echo "Binary $(PROG) installed to $(DESTDIR)$(INSTALL_DIR)/bin"

$(OBJS): %.o: %.cpp
	@$(CXX) -o $@ $(CXXFLAGS) -c $<
	@echo "[$(CXX)] $@"

$(COBJS): %.o: %.c
	@$(CC) -o $@ $(CCFLAGS) -c $<
	@echo "[$(CC)] $@"

clang-tidy:
	@for file in $(OBJS); do \
		clang-tidy "src/$$(basename $$file .o).cpp" -fix -checks=* -header-filter=src/* -- $(CXXFLAGS) $(DFLAGS); \
	done;
