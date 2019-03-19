#----------------------------------------------------------------------------#
# Common Makefile Configuration #
#----------------------------------------------------------------------------#
print-%:
	@echo '$*=$($*)'

CONTIKI = ../../..

################# START of CONTIKI Configuration
CONTIKI_WITH_IPV6=1

# https://github.com/contiki-os/contiki/wiki/Reducing-Contiki-OS-firmware-size
CFLAGS += -ffunction-sections
LDFLAGS += -Wl,--gc-sections,--undefined=_reset_vector__,--undefined=InterruptVectors,--undefined=_copy_data_init__,--undefined=_clear_bss_init__,--undefined=_end_of_init__

# Reduce the size as much as possible
# CFLAGS += -DUIP_CONF_TCP=0

# Linker Optimization
SMALL=1

# MAC settting
MAC ?= CSMA
ifeq ($(MAC),CONTIKIMAC)
    CFLAGS += -DNETSTACK_CONF_RDC=contikimac_driver
    CFLAGS += -DNETSTACK_CONF_MAC=csma_driver
endif
ifeq ($(MAC),CSMA)
    CFLAGS += -DNETSTACK_CONF_RDC=nullrdc_driver
    CFLAGS += -DNETSTACK_CONF_MAC=csma_driver
endif
ifeq ($(MAC),NULLMAC)
    CFLAGS += -DNETSTACK_CONF_RDC=nullrdc_driver
    CFLAGS += -DNETSTACK_CONF_MAC=nullmac_driver
endif

# Don't need llsec
CFLAGS += -DNETSTACK_CONF_LLSEC=nullsec_driver

# Check for RPL and RPL_MODE setting in cmd line vars
RPL ?= 1
RPL_MODE ?= NS
ifeq ($(RPL),1)
    CONTIKI_WITH_RPL=1
    ifeq ($(RPL_MODE), NS)
        CFLAGS += -DRPL_MODE_NS=1
    else
        CFLAGS += -DRPL_MODE_NS=0
    endif
else
    CONTIKI_WITH_RPL=0
endif

################ Start of Multiflow Configuration
# Server Reply
REPLY ?= 0
ifneq ($(REPLY),)
    CFLAGS += -DSERVER_REPLY=$(REPLY)
endif

# Multiflow Application
MULTIFLOW ?= 1
ifeq ($(MULTIFLOW),1)
CFLAGS += -DBUILD_WITH_MULTIFLOW=1
APPS += multiflow
# Number of client applications
NUM_APPS ?= 1
ifneq ($(NUM_APPS),)
    CFLAGS += -DCONF_NUM_APPS=$(NUM_APPS)
endif
# Application Ports
ifneq ($(APP_LPORT)$(APP_RPORT),)
    CFLAGS += -DUDP_LPORT=$(APP_LPORT)
    CFLAGS += -DUDP_RPORT=$(APP_RPORT)
endif
# Bit-rate
ifneq ($(BRMIN)$(BRMAX)$(DELAY),)
    CFLAGS += -DCONF_APP_BR_DELAY=$(DELAY)
    CFLAGS += -DCONF_APP_BR_MIN=$(BRMIN)
    CFLAGS += -DCONF_APP_BR_MAX=$(BRMAX)
endif
# Sending node
ifneq ($(TXNODES),)
    CFLAGS += -DCONF_TX_NODES=$(TXNODES)
endif
# Receiving node
ifneq ($(RXNODES),)
    CFLAGS += -DCONF_RX_NODES=$(RXNODES)
endif
endif # MULTIFLOW

################ End of Multiflow Configuration


################ START of SDN Configuration

# Check for SDN setting in cmd line vars
SDN ?= 1
ifeq ($(SDN), 1)
    CFLAGS += -DUIP_CONF_IPV6_SDN=1
    MODULES += core/net/sdn core/net/sdn/usdn
else
    CFLAGS += -DUIP_CONF_IPV6_SDN=0
endif

# SDN configuration
ifneq ($(FTSRETRY),)
    CFLAGS += -DSDN_CONF_RETRY_AFTER_QUERY=$(FTSRETRY)
endif
ifneq ($(NSUFREQ),)
    CFLAGS += -DSDN_CONF_CONTROLLER_UPDATE_PERIOD=$(NSUFREQ)
endif
ifneq ($(FTLIFETIME),)
    CFLAGS += -DSDN_CONF_FT_LIFETIME=$(FTLIFETIME)
endif

# Overhead reduction and simulation hacks
ifneq ($(FORCENSU),)
    CFLAGS += -DSDN_CONF_FORCE_UPDATE=$(FORCENSU)
endif
ifneq ($(FTREFRESH),)
    CFLAGS += -DSDN_CONF_REFRESH_LIFETIME_ON_HIT=$(FTREFRESH)
endif

# Node statistics (not just for SDN...)
WITH_SDN_STATS ?= 1
ifeq ($(WITH_SDN_STATS),1)
    PROJECTDIRS += ../../../core/net/sdn
    PROJECT_SOURCEFILES += sdn-stats.c
    CFLAGS += -DWITH_SDN_STATS=1 -DSDN_STATS_PERIOD=60
    POWER ?= 0
ifeq ($(POWER),1)
    CFLAGS += -DCONTIKIMAC_CONF_COMPOWER=1 -DWITH_COMPOWER=1
    CFLAGS += -DSDN_STATS_CONF_POWER=1
endif
endif

# Logging levels
ifneq ($(LOG_LEVEL_SDN),)
    CFLAGS += -DLOG_CONF_LEVEL_SDN=$(LOG_LEVEL_SDN)
endif
ifneq ($(LOG_LEVEL_ATOM),)
    CFLAGS += -DLOG_CONF_LEVEL_ATOM=$(LOG_LEVEL_ATOM)
endif
