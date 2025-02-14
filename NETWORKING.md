Networking
==========

Status check
------------
Aura has built-in connectivity diagnostic capabilities. Host a game, and join through "Local Area Network",
then type the following commands (line-by-line):

```
!owner
!checknetwork
```

After a while, Aura will let you know how people can or cannot connect to your game. That includes whether
your IPv4 and IPv6 addresses are publicly reachable, and whether any tunnels have been correctly configured.

Universal Plug and Play
-----------------------
# Why?
There is a chance that you actually have an IPv4 public address, but you haven't properly enabled 
port-forwarding to allow Aura's network traffic in. Some routers support automatic port-forwarding 
through this feature, abbreviated as UPnP.

# How to
Host a game, and join through "Local Area Network", then type the following commands (line-by-line):

```
!owner
!upnp
```

After a while, Aura will let you know how this went. Then you may run ``!checknetwork`` to confirm 
whether your IPv4 address is now reachable. If it is, then congratulations, you may now easily host 
games with no further configuration. Otherwise, see the alternatives below.

# Pros and cons:
**Pro**: It's very easy to setup.
**Con**: Not available for all networks. Even if it is, don't be too happy. Your router's UPnP capabilities 
open the door to security threats, without regard to your using them or not.

Manual port-forwarding
-----------------------
# Why?
UPnP is not a supported feature in all routers. Even if it is, it might have been disabled for security 
reasons. You might want to do disable it too, even if you succeeded at the ``Universal Plug and Play`` 
section. Note that for anything you could do with UPnP, you may also do it by logging in to the router 
and setting it up.

# How to:
The procedure is specific to each router model. You should look the instructions up in the Internet, or 
contact your Internet Service Provider (ISP) for assistance. Note that, under some network configurations 
(NAT), port-forwarding may NOT be enough to let people connect to your games. You will be able to tell 
from the ``!checknetwork`` command. In case your address is still unreachable, see the alternatives below.

# Pros and cons:
**Pro**: It's the golden standard for hosting anything on the Internet.
**Con**: Not available for all networks, and becoming rarer over the time.

IPv4 TCP Tunneling
------------------
# Why?
PvPGN servers, and players that connect to them, require that game hosts use a public IPv4 address
IPv4 address. These are scarcer and more expensive by the day. More likely than not, your computer 
is behind a NAT that prevents you from hosting games by default.

Using this exclusive feature, Aura may provide servers with a public address. Traffic at that 
address is expected to be tunneled to your own machine, thus bypassing NAT issues.

# How to
You can use [Packetriot][1] to obtain a public address. Sign up and download their client application, 
which requires running from the command line. Note that, as of February, 2024, the free tier is 
rate-limited to 1 GB / month.

## Packetriot

### Configuration
Open your console at the location where Packetriot has been downloaded, and run.

```
pktriot configure
```

Follow the onscreen instructions to setup your Packetriot client. This is required to 
link it to your account, and choosing an appropriate low-latency region. Once that's done, 
type on the console (line-by-line).

```
pktriot tunnel tcp allocate
pktriot info
```

Edit your ``config.ini`` accordingly to the displayed IPv4 address and port:

```
global_realm.custom_ip_address.enabled = yes
global_realm.custom_ip_address.value = <ADDRESS>

global_realm.custom_port.enabled = yes
global_realm.custom_port.value = <PORT>
```

Type the following command, replacing two values:

- ``<PORT>``: The same as displayed above.
- ``<net.host_port.only>``: It must match the value for ``net.host_port.only`` in your ``config.ini``. By default, 6113.

```
pktriot tunnel tcp forward --port <PORT> --destination 127.0.0.1 --dstport <net.host_port.only>
```

### Start tunneling session
Open your console at the location where Packetriot has been downloaded, and run.

```
pktriot start
```

### Pros and cons
**Pro**: All software run, and all configuration, is handled by the bot owner exclusively.
**Con**: Tunneling connections through a third party (Packetriot) server inherently increases
the network latency. In testing, I have measured 200ms RTT, but that value will vary,
depending on the distance and routing between that server, you, and any clients connected.

IPv6 TCP tunneling
------------------
# Why?
IPv4 is an outdated internet protocol. But, unfortunately, it's still the one many old software 
(and even some newer one) require. That, of course, includes Warcraft III. Yet, as of February, 
2024, [more than 40%][2] of the global Internet traffic goes through the newer IPv6 protocol, 
and the adoption rate keeps increasing as ISPs catch up with the times. Around 340 undecillion 
IPv6 addresses exist, which means that everyone gets their own (and more than one) publicly 
reachable address.

Aura is the only host bot that can host games at [your globally routable IPv6 address][3], allowing 
people to connect to it, without worries about pervasive NAT configurations.

# How to
## ??? (TODO)

### Configuration
People interested in joining your hosted games must open their console, and run:
```
node ???
```

This will open an interactive interface, that will request an IPv6 address and two ports to be entered. 
You must provide your peers with them, as displayed in the ``!checknetwork`` command. Note that the first 
port is ``net.host_port.only``, and the later is ``net.game_discovery.udp.tcp6_custom_port.value``.

This software will automatically look for games at your IPv6 address, and let your friends join through 
the "Local Area Network" menu in their game client.

# Pros and cons:
**Pro**: It's the golden standard for hosting anything on the Internet.
**Pro**: Lower latency (direct connection) than IPv4 tunneling.
**Con**: In some locations, IPv6 may not be widely deployed yet.
**Con**: Requires your peers to run additional software.

VPN software
------------
# Why?
VPNs may provide you with a new IPv4 address, which also bypasses NAT issues.

# How to:
Install the VPN software you are interested in, both in your machine, and in the machines your peers 
connect from.

Make sure that you are broadcasting games to the relevant VPN network interface. Configure the broadcast 
address in your ``config.ini``, as the value for ``net.game_discovery.udp.broadcast.address``.

Broadcast addresses for some VPN providers include:
* Hamachi: 5.255.255.255
* Radmin: 26.255.255.255

# Pros and cons:
**Pro**: After installation, connectivity with you and your peers will be easier.
**Pro**: Many VPN software alternatives exist. Search the web for [providers][4].
**Con**: Some VPN providers may not allow you to send UDP broadcasts (invisible games problem.)
**Con**: Requires everyone to perform installs with Administrative privileges.
**Con**: Some VPNs are safer (or, conversely, unsafer) than others.

[1]: https://packetriot.com/
[2]: https://www.google.com/intl/en/ipv6/statistics.html#tab=ipv6-adoption
[3]: https://api6.ipify.org/
[4]: https://www.saasworthy.com/list/vpn-software
