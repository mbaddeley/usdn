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
 *         uSDN Core: Controller discovery and join services.
 * \author
 *         Michael Baddeley <m.baddeley@bristol.ac.uk>
 */
#ifndef SDN_CD_H_
#define SDN_CD_H_

#include "net/ip/uip.h"
#include "net/ip/udp-socket.h"

#include "net/sdn/sdn.h"
#include "net/sdn/sdn-conf.h"

#ifdef SDN_CONF_MAX_CONTROLLERS
#define SDN_MAX_CONTROLLERS SDN_CONF_MAX_CONTROLLERS
#else
#define SDN_MAX_CONTROLLERS                      1
#endif /* SDN_CONF_MAX_CONTROLLERS */

/*---------------------------------------------------------------------------*/
/* Logical Representation of SDN Controller */
/*---------------------------------------------------------------------------*/

typedef enum ctrl_state {
  CTRL_NONE,
  CTRL_CONNECTING,
  CTRL_CONNECTED_NEW,
  CTRL_CONNECTED,
  CTRL_DISCONNECTED,
} ctrl_state_t;

/* SDN Controller Object */
struct sdn_controller {
  struct sdn_controller     *next;
  uip_ipaddr_t              ipaddr;
  struct ctimer             update_timer;
  uint16_t                  update_period;
  ctrl_state_t              state;           /* Controller status */
  sdn_conn_type_t           conn_type;
  uint8_t                   conn_length;
  uint8_t                   conn_data[SDN_CONF_CONTROLLER_CONN_LENGTH_MAX];
};

/* Pointer to the current default controller */
extern sdn_controller_t *DEFAULT_CONTROLLER;

/*---------------------------------------------------------------------------*/
/* SDN Controller Discovery API */
/*---------------------------------------------------------------------------*/
void              sdn_cd_init(void);
sdn_controller_t *sdn_cd_init_default_controller(void);
void              sdn_cd_set_state(ctrl_state_t state, sdn_controller_t *c);
sdn_controller_t *sdn_cd_add(uip_ipaddr_t *ipaddr);
void              sdn_cd_rm(sdn_controller_t *c);
sdn_controller_t *sdn_cd_lookup(uip_ipaddr_t *ipaddr);

#endif /* SDN_CD_H_ */
