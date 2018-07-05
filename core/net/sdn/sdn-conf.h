/*
 * Copyright (c) 2018, Toshiba Research Europe Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *     without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */
/**
 * \file
 *         uSDN Core: SDN configuration for nodes.
 * \author
 *         Michael Baddeley <m.baddeley@bristol.ac.uk>
 */
#ifndef SDN_CONF_H_
#define SDN_CONF_H_

#include "net/ip/uip.h"

#include "sdn.h"

#include <stdint.h>

/*---------------------------------------------------------------------------*/
/* SDN behaviour configuration */
/*---------------------------------------------------------------------------*/
/* Force update to the controller on JOIN */
#ifndef SDN_CONF_FORCE_UPDATE
#define SDN_CONF_FORCE_UPDATE               0
#endif
/* Refresh flowtable entry liftimers when entry is hit */
#ifndef SDN_CONF_REFRESH_LIFETIME_ON_HIT
#define SDN_CONF_REFRESH_LIFETIME_ON_HIT       1
#endif
/* Send the FULL packet to controller in a ftq (needs SICSLOWPAN_CONF_FRAG)*/
#ifndef SDN_CONF_QUERY_FULL_PACKET
#define SDN_CONF_QUERY_FULL_PACKET          0
#endif
/* When receiving a FTS, try and send the packect that's in the query buffer */
#ifndef SDN_CONF_RETRY_AFTER_QUERY
#define SDN_CONF_RETRY_AFTER_QUERY          0
#endif

/*---------------------------------------------------------------------------*/
/* Default settings for SDN configuration data structure */
/*---------------------------------------------------------------------------*/
#ifndef SDN_CONF_DEFAULT_NET
#define SDN_CONF_DEFAULT_NET                1
#endif

#ifdef SDN_CONF_FT_LIFETIME
#define SDN_FT_LIFETIME                          CLOCK_SECOND * SDN_CONF_FT_LIFETIME
#else
#define SDN_FT_LIFETIME                          0xFFFF  // Infinite
#endif

#ifndef SDN_CONF_QUERY_INDEX
#define SDN_CONF_QUERY_INDEX                uip_dst_index
#endif

#ifndef SDN_CONF_QUERY_LENGTH
#define SDN_CONF_QUERY_LENGTH               16
#endif

/* Controller configuration settings */
#ifndef SDN_CONF_CONTROLLER_IP
#define SDN_CONF_CONTROLLER_IP(addr)        uip_ip6addr(addr, 0xaaaa, 0, 0, 0, 0, 0, 0, 1);
#endif
#ifndef SDN_CONF_CONTROLLER_UPDATE_PERIOD
#define SDN_CONF_CONTROLLER_UPDATE_PERIOD   600
#endif
#ifndef SDN_CONF_CONTROLLER_INIT_STATE
#define SDN_CONF_CONTROLLER_INIT_STATE      CTRL_CONNECTING
#endif
#ifndef SDN_CONF_CONTROLLER_CONN_TYPE
#define SDN_CONF_CONTROLLER_CONN_TYPE       SDN_CONN_TYPE_USDN
#endif
#ifndef SDN_CONF_CONTROLLER_CONN_DATA
#define SDN_CONF_CONTROLLER_CONN_DATA       {1234 >> 8, 1234 << 8, \
                                             4321 >> 8, 4321 << 8}
#endif
#ifndef SDN_CONF_CONTROLLER_CONN_LENGTH
#define SDN_CONF_CONTROLLER_CONN_LENGTH     4
#endif
#ifndef SDN_CONF_CONTROLLER_CONN_LENGTH_MAX
#define SDN_CONF_CONTROLLER_CONN_LENGTH_MAX 10
#endif

/*---------------------------------------------------------------------------*/
/* SDN configuration data */
/*---------------------------------------------------------------------------*/
/* SDN Configuration data structure */
typedef struct sdn_configuration {
  /* general configuration */
  uint8_t       sdn_net;              /* virtual network id */
  uint8_t       cfg_id;               /* sdn configuration id */
  uint8_t       hops;                 /* hops from controller */
  /* flowtable configuration */
  clock_time_t  ft_lifetime;          /* time to live for flowtable entries */
  uint8_t       query_full;           /* query part of or full packet */
  uint8_t       query_idx;            /* start index to query */
  uint8_t       query_len;            /* length to query */
  /* rpl configuration */
  uint8_t       rpl_dio_interval;     /* RPL_DIO_INTERVAL_MIN */
  uint8_t       rpl_dfrt_lifetime;    /* RPL_DEFAULT_LIFETIME */
} sdn_cfg_t;

/* Allow the configuration to be accessed globally */
extern sdn_cfg_t SDN_CONF;

/* Node Configuration API. */
void sdn_conf_init(void);
void sdn_conf_print(void);

#endif /* SDN_CONF */
/** @} */
