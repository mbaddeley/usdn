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
 *    without specific prior written permission.
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
 *         Multiflow: A flow generator for uSDN testing.
 * \author
 *         Michael Baddeley <m.baddeley@bristol.ac.uk>
 */

#ifndef mflow_H_
#define mflow_H_

#include "contiki.h"

#ifdef CONF_NUM_APPS
#define USDN_CONF_MAX_APPS CONF_NUM_APPS
#else
#define USDN_CONF_MAX_APPS 1
#endif

typedef struct mflow mflow_t;

/* usdn app connection state */
typedef enum {
  mflow_STATE_DISCONNECTED,
  mflow_STATE_CONNECTING,
  mflow_STATE_CONNECTED,
  mflow_STATE_SENDING,
  mflow_STATE_ERROR
} mflow_state_t;

/* Connection callback (event/input) */
typedef void (* mflow_rx_callback_t)(mflow_t *app,
                                        const uip_ipaddr_t *source_addr,
                                        uint16_t src_port,
                                        const uip_ipaddr_t *dest_addr,
                                        uint16_t dest_port,
                                        const uint8_t *data, uint16_t datalen);
/* Sendtimer callback */
typedef void (* mflow_timer_callback_t)(mflow_t *app);

/* Application data structure */
struct mflow {
  struct mflow *next;

  /* callbacks */
  mflow_rx_callback_t receive_callback;
  mflow_timer_callback_t timer_callback;
  /* app settings */
  uint8_t           id;
  uint16_t          seq;
  float             brdelay;
  float             brmin;
  float             brmax;
  struct ctimer     sendtimer;
  /* udp connection */
  uip_ipaddr_t remote_addr;
  uint16_t lport, rport;
  struct uip_udp_conn *udp;
  /* State */
  mflow_state_t state;
  /* Pointer to client process */
  void *process;
};


/*---------------------------------------------------------------------------*/
/* usdn-udp API */
/*---------------------------------------------------------------------------*/
mflow_t *mflow_new(uint8_t id, uip_ipaddr_t *remote_port,
                         uint16_t lport, uint16_t rport,
                         float brdelay, float brmin, float brmax,
                         mflow_rx_callback_t receive_callback,
                         mflow_timer_callback_t timer_callback);
void mflow_send(mflow_t *app,
                   void *data,
                   uint8_t datalen);
void mflow_new_sendinterval(mflow_t *a, uint8_t delay);
uint8_t mflow_num_flows(void);
void mflow_init(void);

#endif /* mflow_H_ */
