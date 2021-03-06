# vClick 2.0.0 released
 

This version introduces new features, lots of improvements and numerous bug fixes. 

The source and binaries can be downloaded from: <https://github.com/tarmoj/vclick/releases/tag/v2.0.0>.


### New in version 2.0.0:

* **NB! The default OSC port has beeen changed!  **In version 1 client was listening OSC messages on (and server sending to) fixed port 87878,  now *the port can be set in settings of both server and client*. **The default port is now 57878**. Bigger port numbers (above 65535) are not supported on MacOS any more. **If you use v2 and v1 clients/servers together, make sure that the traffic happens on port 87878!**

* **Server's look and layout is changed** (ported to QtQuick 2 using Material style).

* Ability to use **several score files** (ListView), easy switching between them.

* Feature to **send time (like stopwatch) to clients**, also with a playing soundfile (useful for example for for playing a fixed media piece).

* **Remote control from clients to server** -  it is possible now to start/stop, set bars, start time etc from client.

* Easier way to set most basic Csound **options (Sound/No sound)**.

* **Experimental Android build of server**.

<br>


### Fixes

* Fixed crash if there is not enough fields in tempo line.

* Fixed one memory leak.

* Better websocket connection from clients to server.

* Better layout for delay and intrements row (client). More improvements on layout.

* Updated Windows version to build with MSVC 2017.

* iOS build complies with iOs 12.

* Csound v6.12 used as engine in Server


