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
#include <stdio.h>
#include <string.h>

#include "lib/memb.h"
#include "lib/list.h"
#include "sys/node-id.h"

#include "net/ip/uip.h"
#include "net/ipv6/uip-nd6.h"
#include "net/ipv6/uip-ds6-nbr.h"
#include "net/ip/uip-udp-packet.h"

#include "net/rpl/rpl.h"

#include "sdn.h"
#include "sdn-conf.h"
#include "sdn-timers.h"
#include "sdn-cd.h"

/* Log configuration */
#include "sys/log-ng.h"
#define LOG_MODULE "SDN-CD"
#define LOG_LEVEL LOG_LEVEL_SDN

#undef LOG_WITH_ANNOTATE
#define LOG_WITH_ANNOTATE 1

#if (LOG_LEVEL >= DEBUG_LEVEL)
static const char *CONTROLLER_STATE_STRING[] = {
  "NONE", "CONNECTING", "CONNECTED_NEW", "CONNECTED_FRESH",
  "CONNECTED_STALE", "DISCONNECTED", "SEARCHING"
};
#endif

/*---------------------------------------------------------------------------*/
/* Allocate Controller Tables */
/*---------------------------------------------------------------------------*/
LIST(controllerlist);
MEMB(controller_memb, sdn_controller_t, SDN_MAX_CONTROLLERS);

/*---------------------------------------------------------------------------*/
/* Declare Default Controller */
/*---------------------------------------------------------------------------*/
sdn_controller_t *DEFAULT_CONTROLLER;

/*---------------------------------------------------------------------------*/
/* Memory Functions */
/*---------------------------------------------------------------------------*/
static sdn_controller_t *
controller_alloc()
{
  sdn_controller_t *c;
  c = memb_alloc(&controller_memb);
  memset(c, 0, sizeof(sdn_controller_t));
  if(c == NULL) {
    LOG_ERR("Failed to allocate a controller, memb full!\n");
  }
  return c;
}

/*---------------------------------------------------------------------------*/
static void
controller_free(sdn_controller_t *c)
{
  list_remove(controllerlist, c);
  int res = memb_free(&controller_memb, c);
  if (res !=0){
    LOG_ERR("Failed to free a Controller. Ref cnt: %d\n", res);
  }
}

/*---------------------------------------------------------------------------*/
/* List Functions */
/*---------------------------------------------------------------------------*/
sdn_controller_t *
sdn_cd_add(uip_ipaddr_t *ipaddr)
{
  sdn_controller_t *c;

  for(c = list_head(controllerlist); c != NULL; c = list_item_next(c)) {
    if(uip_ipaddr_cmp(&c->ipaddr, &ipaddr)) {
      LOG_ERR("Controller already in list!\n");
      return NULL;
    }
  }
  c = controller_alloc();
  if(c != NULL) {
    /* Set controller fields */
    uip_ipaddr_copy(&c->ipaddr, ipaddr);
    /* Add controller to list */
    list_add(controllerlist, c);
    LOG_DBG("Added controller at [");
    LOG_DBG_6ADDR(&c->ipaddr);
    LOG_DBG_("]\n");
    /* Set default controller to point to new controller */
    DEFAULT_CONTROLLER = c;
  } else {
    LOG_ERR("Couldn't add controller\n");
  }

  return c;
}

/*---------------------------------------------------------------------------*/
void
sdn_cd_rm(sdn_controller_t *c)
{
  sdn_controller_t *tmp;
  LOG_DBG("Removing controller at [");
  LOG_DBG_6ADDR(&c->ipaddr);
  LOG_DBG_("]\n");
  for(tmp = list_head(controllerlist); tmp != NULL; tmp = list_item_next(tmp)) {
    if(uip_ipaddr_cmp(&tmp->ipaddr, &c->ipaddr)) {
      /* If we're removing a controller that is our default, we need to handle
         this */
      if(uip_ipaddr_cmp(&DEFAULT_CONTROLLER->ipaddr, &c->ipaddr)) {
        DEFAULT_CONTROLLER = NULL;
      }
      /* Remove controller from list, then free memory. */
      list_remove (controllerlist, tmp);
      controller_free(tmp);
      LOG_DBG("Controller REMOVED\n");
    }
  }
}

/*---------------------------------------------------------------------------*/
sdn_controller_t *
sdn_cd_lookup(uip_ipaddr_t *ipaddr)
{
  sdn_controller_t *c;
  for(c = list_head(controllerlist); c != NULL; c = list_item_next(c)) {
    if(uip_ipaddr_cmp(&c->ipaddr, ipaddr)) {
      return c;
    }
  }
  return NULL;
}

/*---------------------------------------------------------------------------*/
/* Controller Discovery */
/*---------------------------------------------------------------------------*/
#if SDN_CONF_JOIN_ICMP
static void
start_controller_search_callback() {
  if(DEFAULT_CONTROLLER == NULL) {
    LOG_INFO("Starting Controller solicitation process\n");
    sdn_tmr_cis(SDN_TMR_STATE_START);
  }
}
#endif /* SDN_CONF_JOIN_ICMP */

/*---------------------------------------------------------------------------*/
/* Controller Setup and Maintainance */
/*---------------------------------------------------------------------------*/
static void
connecting(void)
{
#if SDN_CONF_JOIN_ICMP
  /* If we have a join timer, then start it */
  SDN_ENGINE.controller_join(SDN_TMR_STATE_START);
#endif
}

/*---------------------------------------------------------------------------*/
static void
new(sdn_controller_t *c)
{
#if (SDN_CONTROLLER_TYPE == SDN_CONTROLLER_NONE)
  /* Add controller src/dest to whitelist as to piggy-back on RPL */
  // LOG_DBG("Add ACCEPT on src [");
  // LOG_DBG_6ADDR(&c->ipaddr);
  // LOG_DBG_("]\n");
  // LOG_DBG("Add ACCEPT on dest [");
  // LOG_DBG_6ADDR(&c->ipaddr);
  // LOG_DBG_("]\n");
  // SDN_DRIVER.add_accept_on_src(FLOWTABLE, &c->ipaddr);
  // SDN_DRIVER.add_accept_on_dest(FLOWTABLE, &c->ipaddr);
#endif
}

/*---------------------------------------------------------------------------*/
static void
connected(void)
{
  /* Start timer for sending state updates to controller */
  SDN_ENGINE.controller_update(SDN_TMR_STATE_START);
#if SDN_CONF_JOIN_ICMP
  /* If we have a join timer, then stop it */
  SDN_ENGINE.controller_join(SDN_TMR_STATE_STOP);
#endif
}

/*---------------------------------------------------------------------------*/
static void
disconnected(void)
{
  /* Stop timer for sending state updates to controller */
  SDN_ENGINE.controller_update(SDN_TMR_STATE_STOP);
}

/*---------------------------------------------------------------------------*/
void
sdn_cd_set_state(ctrl_state_t state, sdn_controller_t *c)
{
  ctrl_state_t new_state = state;
  LOG_DBG("Set controller state [%s]\n",  CONTROLLER_STATE_STRING[state]);
  if(c != NULL) {
    /* Do some processing before setting the state */
    switch(state) {
      case CTRL_NONE:
        LOG_ANNOTATE("#L %u 0; \n", c->ipaddr.u8[sizeof(uip_ipaddr_t) - 1]);
        break;
      case CTRL_CONNECTING:
        connecting();
        break;
      case CTRL_CONNECTED_NEW:
        new(c);
        /* Set ourselves as connected, we just had to start some timers */
        new_state = CTRL_CONNECTED;
      case CTRL_CONNECTED:
        LOG_ANNOTATE("#L %u 1; green\n", c->ipaddr.u8[sizeof(uip_ipaddr_t) - 1]);
        connected();
        break;
      case CTRL_DISCONNECTED:
        LOG_INFO("c:0\n");
        disconnected();
        break;
      default:
        LOG_ERR("Unknown state!\n");
        break;
    }

    /* Actually set the state */
    c->state = new_state;

  } else {
    LOG_ERR("sdn_cd_set_state NULL controller!\n");
  }
}

/*---------------------------------------------------------------------------*/
sdn_controller_t *
sdn_cd_init_default_controller(void) {
  uip_ipaddr_t ipaddr;
  LOG_INFO("Initialising a default controller...\n");
  SDN_CONF_CONTROLLER_IP(&ipaddr);
  sdn_controller_t *c = sdn_cd_add(&ipaddr);
  if(c != NULL) {
    c->update_period = SDN_CONF_CONTROLLER_UPDATE_PERIOD;
    c->conn_type = SDN_CONF_CONTROLLER_CONN_TYPE;
    c->conn_length = SDN_CONF_CONTROLLER_CONN_LENGTH;
    uint16_t data[2] = {1234, 4321};
    memcpy(&c->conn_data, &data, c->conn_length);
    /* Register the controller connection */
    SDN_ADAPTER.register_controller(c);
    /* Set the controller initial state */
    sdn_cd_set_state(SDN_CONF_CONTROLLER_INIT_STATE, c);
  } else {
    LOG_ERR("Could not add a new controller\n");
  }

  return c;
}

/*---------------------------------------------------------------------------*/
void
sdn_cd_init(void)
{
  /* Initialise controller list memory */
  list_init(controllerlist);
  memb_init(&controller_memb);

  /* DEFAULT_CONTROLLER intially NULL */
  DEFAULT_CONTROLLER = NULL;
}
