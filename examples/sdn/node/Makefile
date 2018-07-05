all: sdn-udp-client

CONTIKI = ../../..

CFLAGS += -DPROJECT_CONF_H=\"../project-conf.h\"

include ../Makefile

################ START of SDN Configuration
ifeq ($(SDN),1)
    # TODO
endif

include $(CONTIKI)/Makefile.include
