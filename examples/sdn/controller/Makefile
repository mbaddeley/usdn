print-%:
	@echo '$*=$($*)'

all: sdn-controller

CONTIKI = ../../..

CFLAGS += -DPROJECT_CONF_H=\"../project-conf.h\"

# include common makefile
include ../Makefile

################ START of SDN Configuration
ifeq ($(SDN), 1)
	  # Atom controller application
    APPS += atom
		# Controller conf path
    CFLAGS += -DCONTROLLER_CONF_PATH=\"controller-conf.h\"
endif

# include contiki makefile
include $(CONTIKI)/Makefile.include
