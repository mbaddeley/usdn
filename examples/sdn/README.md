μSDN - Low-Power Wireless SDN for Contriki-NG
==

### Intro
This repo hosts the source code of μSDN, that we published in the NetSoft 2018 conference.

### About
μSDN is has been developed to provide an open source platform to deliver SDN on 6LoWPAN IEEE 802.15.4-2012 networks. The version here is currently not compatible with TSCH, though we have previously tried it out with our own 6TiSCH implementation for contiki (check out the paper NFV-SDN 2017 paper here).

Alongside μSDN itself, we provide an embedded SDN controller, *Atom*, as well as a flow generator for testing purposes, *Multiflow*.

Please note, this is an academic exercise and a fairly large codebase, so there are many things in μSDN which may not have been done as cleanly or transparently as I may have liked (though I have tried to clean it up somewhat). I've tried to give a brief overview of all the features and modules here, and you can find the paper and slides within at the top level, but if you find yourself getting lost then I'm happy to answer any questions you may have.

### Getting Started
To get you going you can find some Cooja examples in:

 */examples/sdn/..*

### Further Development
Unfortunately I've moved on from working on μSDN, but if you wish to add to it/fix anything then feel free to create a pull request.

[usdn_v1.1] - 15/03/19
### Minor Bug Fixes

[usdn_v1.0] - 28/06/18
----------------
### Where is everything?
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

### Known Issues
- Lots ;) Just ask if you have problems and I'll try to help as best I can.
