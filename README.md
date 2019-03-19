μSDN - Low-Power Wireless SDN for Contiki.
===

Intro
---
This repo hosts the source code of μSDN, which we published in the NetSoft 2018 conference ([paper](evolving-sdn-for-low-power-iot-networks.pdf),  [slides](netsoft20181.pdf)). μSDN is also in the process of being ported to Contiki-NG (which will supersede this version), you can check out the μSDN-NG repo progess [here](https://github.com/mbaddeley/usdn-ng).

Publications
---
[M. Baddeley, R. Nejabati, G. Oikonomou, M. Sooriyabandara, and D. Simeonidou, “Evolving SDN  for Low-Power IoT Networks,” in 2018 IEEE Conference on Network Softwarization (NetSoft), June 2018.](https://arxiv.org/pdf/1809.07296.pdf)

[M. Baddeley, R. Nejabati, G. Oikonomou, S. Gormus, M. Sooriyabandara and D. Simeonidou, "Isolating SDN control traffic with layer-2 slicing in 6TiSCH industrial IoT networks," 2017 IEEE Conference on Network Function Virtualization and Software Defined Networks (NFV-SDN), Berlin, 2017, pp. 247-251.](https://arxiv.org/pdf/1809.06624.pdf)

About
---
μSDN is has been developed to provide an open source platform to deliver SDN on 6LoWPAN IEEE 802.15.4-2012 networks. The version here is currently not compatible with TSCH, though we have previously tried it out with our own 6TiSCH implementation for contiki (check out the NFV-SDN 2017 paper [here](https://www.researchgate.net/publication/321733914_Isolating_SDN_control_traffic_with_layer-2_slicing_in_6TiSCH_industrial_IoT_networks)).

Alongside μSDN itself, we provide an embedded SDN controller, *Atom*, as well as a flow generator for testing purposes, *Multiflow*.

Please note, this is an academic exercise and a fairly large codebase, so there are many things in μSDN which may not have been done as cleanly or transparently as I may have liked (though I have tried to clean it up somewhat). I've tried to give a brief overview of all the features and modules here, and you can find the paper and slides within at the top level, but if you find yourself getting lost then I'm happy to answer any questions you may have.

Getting Started
---

**IMPORTANT** You'll also need to install the 20-bit mspgcc compiler.

For instructions on how to compile this please click [here](https://github.com/contiki-os/contiki/wiki/MSP430X)

For a pre-compiled version for Ubuntu-64 please click [here](https://github.com/pksec/msp430-gcc-4.7.3)

Some people have had issues trying to install this compiler, so if you're new to Contiki or Linux in general then I'd recommend doing the following:

- Use a clean Ubuntu64 installation, don't use Instant Contiki. Contiki is included as part of uSDN and it's not necessary to have a separate Contiki repo.
- Use the precompiled msp430-gcc version above. You literally just need to extract it to a folder of your choice and then add it to your path `export PATH=$PATH:<uri-to-your-mspgcc>`. Once you have done this your path should look something like this:

```
echo $PATH
/home/mike/Compilers/mspgcc-.7.3/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games:/snap/bin:/usr/lib/jvm/java-8-oracle/bin:/usr/lib/jvm/java-8-oracle/db/bin:/usr/lib/jvm/java-8-oracle/jre/bin
```

- NB There is only *ONE* msp430-gcc compiler in the path. If there are two you need to remove the old one.
- Check the mspgcc version (`msp430-gcc --version`) it should be 4.7.3.

```
msp430-gcc (GCC) 4.7.3 20130411 (mspgcc dev 20120911)
Copyright (C) 2012 Free Software Foundation, Inc.
This is free software; see the source for copying conditions. There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
```
You are now ready move on to the next stage! If you haven't properly set it up to use the 20-bit mspgcc then it will not compile!!!

Because of the size of the stack, if you're testing in Cooja you'll need to compile for exp5438 motes (*there is a Makefile.target which should handle this for you*). Please note that you'll need to run make in *both* sdn/controller and sdn/node as I haven't set it up to do both in the higher level directory.

```
  cd usdn/examples/sdn/controller/
  make clean & make
  cd ..
  cd node/
  make clean & make
```

To get you going you can find some Cooja examples in:

 **usdn/examples/sdn/..**

There is a handy compile script in there that can be used to compile both the controller and node:

```
./compile.sh MULTIFLOW=1 NUM_APPS=1 FLOWIDS=1 TXNODES=8 RXNODES=10 DELAY=0 BRMIN=5 BRMAX=5 NSUFREQ=600 FTLIFETIME=300 FTREFRESH=1 FORCENSU=1 LOG_LEVEL_SDN=LOG_LEVEL_DBG LOG_LEVEL_ATOM=LOG_LEVEL_DBG
```

uSDN Make Args:
- NSUFREQ - Frequency of node state updates to the controller (seconds)
- FTLIFETIME - Lifetime of flowtable entries (seconds)
- FTREFRESH - Refresh flowtable entries on a match (0/1)
- FORCENSU - Immediately send a NSU to the controller on join (0/1)
- LOG_LEVEL_SDN - Set the uSDN log level (0 - 5)
- LOG_LEVEL_ATOM - Set the Atom controller log level (0 - 5)

Multiflow Make Args:
- MULTIFLOW - Turn on multiflow (0/1)
- NUM_APPS - Number of flows (N)
- FLOWIDS - Id for each flow ([0,1,2...])
- TXNODES - Transmission nodes ([18,12...] 0 is ALL)
- RXNODES - Receiving nodes ([1,2...])
- DELAY   - Delay each transmission (seconds)
- BRMIN   - Minimum bitrate (seconds)
- BRMAX   - Maximum bitrate (seconds)

Further Development
---
Future μSDN development will merge with μSDN-NG, based on the newer (and maintained) Contiki-NG.

Where is everything?
---
- Core: */core/net/sdn/*
- Stack: */core/net/sdn/usdn/*
- Atom: */apps/atom/*
- Multiflow: */apps/multiflow/*

### Overview of optimization features
- Fragmentation elimination.
- Configurable controller update frequency.
- Configurable flowtable lifetimes.
- Flowtable matches on packet index + byte array.
- Source routed control packets.
- Throttling of repeated control packets.
- Refreshing of regularly used flowtable entries.
- SDN node configuration.
- Control packet buffer.

### Core
- sdn.c

  Main SDN process.

- sdn-cd.c

  Controller discovery and join services.

- sdn-conf.c

  SDN node configuration.

- sdn-ft.c

  SDN flowtable(s).

- sdn-ext-header.c

  Extension headers for source routing.

- sdn-packetbuf.c

  Buffer to allow storing of packets whilst nodes query the controller.

- sdn-timer.h

  Timer configuration and macros.

- sdn-stats.c

  SDN statistics. This also replaces the powertrace application from Contiki.

### Stack
- usdn-driver.c

  The driver interfaces with the flowtable to provide a basic API to allow specific functions, such as fowarding packets based on source/destination, aggregating packets, dropping packets, etc.

- usdn-engine.c

  The engine handles incomming and outgoing SDN messages, using the API provided by the driver.

- usdn-adapter.c

  The adapter provides a connection interface to the controller for a specific protocol. Currently this is for the μSDN protocol.

### ATOM Controller
- atom.c

  Main process for atom.

- atom-buffer.c

  Buffer for incomming and outgoing packets.

- atom-net.c

  Keeps tracks of and abstracts the network state for the contrller. My original idea for this was that the controller should be able to configure the network so that nodes update the view with the required metric, rather than all metrics, in order to keep the memory requirements down.

- atom-sb-xx.c

  Atom allows you to create multiple southbound connectors. Currently we have a μSDN and RPL connector implemented.

- atom-app-net-xx.c

  Network applications.

- atom-app-route-xx.c

  Routing applications.

- atom-app-join-xx.c

  Join/association applications.

### Still to be Implemented...
- Perform multiple flowtable entries on packets
- Port to Contiki-NG (waiting for exp5438 motes to be supported)

### Known Issues
- Lots ;) Just ask if you have problems and I'll try to help as best I can.

---
[usdn_v1.2] - 19/03/19
---
- Added compilation script so you can compile in one command: `./compile.sh ADD_MAKEFILE_ARGS_HERE*`
- Added a redirect example: *usdn-redirect.csc*
- Modified *uip6.c* and *usdn-driver.c* so that RPL source routing packets are no longer automatically ignored by the SDN.
- Changed Atom controller to use shortest path routing by default rather than RPL-NS routing!

---
[usdn_v1.1] - 17/07/18
---
- Turned on default FT lifetimes.
- Fixed issue where SDN was turned off in the Makefile (oops!).
- Added hopcount to Atom ingress messages.
- Added the NetSoft 2018 paper and slides.
- Added Makefile.target so you don't need to type in target each time.
- Fixes for automatic simulation through ContikiPy.

---
[usdn_v1.0] - 28/06/18
---
- Initial commit.
