#!/bin/sh
SCRIPT_PATH=${SCRIPT_PATH:-"./aura.cfg"}
BOT_VIRTUAL_NAME=${BOT_VIRTUAL_NAME:-"|cFFFF0000Aura"}
HOST_PORT=${HOST_PORT:-'6113'}
RECONNECT_PORT=${RECONNECT_PORT:-'6114'}
SERVER=${SERVER:-"server.eurobattle.net"}
SERVER_PORT=${SERVER_PORT:-'6112'}
SERVER_ALIAS=${SERVER_ALIAS:-"EuroBattle"}
BOT_USER_NAME=${BOT_USER_NAME:-"aura"}
BOT_PASSWORD=${BOT_PASSWORD:-"aura"}
ROOT_ADMINS=${ROOT_ADMINS:-"W3_Beta"}
WAR_VERSION=${WAR_VERSION:-'28'}

if [ ! -e "$SCRIPT_PATH" ]; then
	cp "/app/aura_example.cfg" "$SCRIPT_PATH"
fi

if [ ! -d "/app/data/wc3" ]; then
	cp -r "/app/wc3" "/app/data/wc3"
fi

if [ ! -d "/app/data/mapcfgs" ]; then
	cp -r "/app/mapcfgs" "/app/data/mapcfgs"
fi

if [ ! -d "/app/data/maps" ]; then
	mkdir "/app/data/maps"
fi

if ! cat "$SCRIPT_PATH" 2>/dev/null | grep -q 'bot_virtualhostname'; then
	echo "bot_virtualhostname = $BOT_VIRTUAL_NAME" >> "$SCRIPT_PATH"
fi
if ! cat "$SCRIPT_PATH" 2>/dev/null | grep -q 'bot_hostport'; then
	echo "bot_hostport = $HOST_PORT" >> "$SCRIPT_PATH"
fi
if ! cat "$SCRIPT_PATH" 2>/dev/null | grep -q 'bot_reconnectport'; then
	echo "bot_reconnectport = $RECONNECT_PORT" >> "$SCRIPT_PATH"
fi
if ! cat "$SCRIPT_PATH" 2>/dev/null | grep -q 'bnet_server'; then
	echo "bnet_server = $SERVER" >> "$SCRIPT_PATH"
fi
if ! cat "$SCRIPT_PATH" 2>/dev/null | grep -q 'bnet_serverport'; then
	echo "bnet_serverport = $SERVER_PORT" >> "$SCRIPT_PATH"
fi
if ! cat "$SCRIPT_PATH" 2>/dev/null | grep -q 'bnet_serveralias'; then
	echo "bnet_serveralias = $SERVER_ALIAS" >> "$SCRIPT_PATH"
fi
if ! cat "$SCRIPT_PATH" 2>/dev/null | grep -q 'bnet_username'; then
	echo "bnet_username = $BOT_USER_NAME" >> "$SCRIPT_PATH"
fi
if ! cat "$SCRIPT_PATH" 2>/dev/null | grep -q 'bnet_password'; then
	echo "bnet_password = $BOT_PASSWORD" >> "$SCRIPT_PATH"
fi
if ! cat "$SCRIPT_PATH" 2>/dev/null | grep -q 'bnet_rootadmins'; then
	echo "bnet_rootadmins = $ROOT_ADMINS" >> "$SCRIPT_PATH"
fi
if ! cat "$SCRIPT_PATH" 2>/dev/null | grep -q 'bnet_custom_war3version'; then
	echo "bnet_custom_war3version = $WAR_VERSION" >> "$SCRIPT_PATH"
fi

/app/aura++ $SCRIPT_PATH
