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
 *         Atom SDN Controller: An embedded SDN controller for Contiki.
 *         See "Evolving SDN for Low-Power IoT Networks", IEEE NetSoft'18
 * \author
 *         Michael Baddeley <m.baddeley@bristol.ac.uk>
 */
#include <stdio.h>
#include <string.h>

#include "contiki-net.h"

#include "sys/node-id.h"
#include "net/ip/uip.h"
#include "net/ipv6/uip-ds6.h"
#include "net/rpl/rpl.h"

#include "net/sdn/sdn.h"
#include "net/sdn/sdn-timers.h"

#include "atom.h"
#include "atom-conf.h"

/* Log configuration */
#include "sys/log-ng.h"
#define LOG_MODULE "ATOM"
#define LOG_LEVEL LOG_LEVEL_ATOM

/* UIP buffer (for debugging input) */
#include "net/sdn/usdn/usdn.h"
#define UIP_IP_BUF       ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])
#define UIP_ICMP_BUF     ((struct uip_icmp_hdr *)&uip_buf[uip_l2_l3_hdr_len])
#define UIP_USDN_BUF     ((struct usdn_hdr *)&uip_buf[uip_l2_l3_udp_hdr_len])

/* Our controller address */
uip_ipaddr_t controller_addr;

/* All applications */
const struct atom_app *all_apps[] = ATOM_APPS;
#define NUM_APPS (sizeof(all_apps) / sizeof(struct atom_app *))

/* All southbound connectors */
const struct atom_sb *all_sb[] = ATOM_SB_CONNECTORS;
#define NUM_SB (sizeof(all_sb) / sizeof(struct atom_sb *))

/*---------------------------------------------------------------------------*/
PROCESS(controller_process, "SDN Controller Process\n");

/*---------------------------------------------------------------------------*/
/* Private functions */
/*---------------------------------------------------------------------------*/
static atom_app_ptr_t *
get_apps(atom_app_ptr_t matrix[][NUM_ATOM_ACTIONS], atom_action_t *action)
{
  if(matrix[action->type][0] != NULL)
    return matrix[action->type];
  else
    return NULL;
}

/*---------------------------------------------------------------------------*/
static uint8_t
get_num_apps(atom_app_ptr_t matrix[][NUM_ATOM_ACTIONS], atom_action_t *action)
{
  int i = 0;
  atom_app_ptr_t *apps = get_apps(matrix, action);
  while(apps[i] != NULL) {
    i++;
  }
  return i;
}

/*---------------------------------------------------------------------------*/
static void
do_net_update(atom_action_t *action, void *data)
{
  int i;
  atom_node_t *node, *n;
  atom_link_t link;

  /* Dereference the action data */
  atom_netupdate_action_t *nu = (atom_netupdate_action_t *)data;

  node = &nu->node;

  if(node != NULL){
    /* Update node */
    n = atom_net_node_update(&action->src, node->cfg_id, node->rank);
    if(n != NULL && node->num_links > 0) {
      for(i = 0; i < node->num_links; i++) {
        /* Update link */
        memcpy(&link, &node->links[i], sizeof(atom_link_t));
        atom_net_link_update(n, link.dest_id, link.rssi);
      }
    }
  }
}

/*---------------------------------------------------------------------------*/
/* Controller API */
/*---------------------------------------------------------------------------*/
void
atom_init(uip_ipaddr_t *addr)
{
  int i;

  LOG_DBG("Initialising atom...\n");
  /* Initialize atom buffer */
  atom_buffer_init();
  /* Initialise the controller address */
  memcpy(&controller_addr, addr, sizeof(uip_ipaddr_t));
  /* Initialise network layer */
  atom_net_init();

  LOG_DBG("Initialising %d sb connectors...\n", NUM_SB);
  /* Initialize southbound connectors */
  for(i = 0; i < NUM_SB; i++) {
    all_sb[i]->init();
  }
  LOG_DBG("Initialising %d apps...\n", NUM_APPS);
  /* Initialize applications */
  for(i = 0; i < NUM_APPS; i++) {
    all_apps[i]->init();
  }

  /* Start the controller process */
  process_start(&controller_process, NULL);
}

/*---------------------------------------------------------------------------*/
void
atom_post(struct atom_sb *sb)
{
  /* Copy the uip buffer onto the input queue */
  LOG_DBG("Copy uip to the queue\n");
  atom_buffer_add(sb);

  // FIXME: This SHOULD be in the usdn_sb but there are issues with how the
  //        sink node tries to post stuff to the controller
  /* Spit out some stats */
  // switch(sb->type) {
  //   case ATOM_SB_TYPE_USDN:
  //     LOG_STAT("BUF %s s:%d d:%d id:%d h:%d\n",
  //               USDN_CODE_STRING(UIP_USDN_BUF->typ),
  //               UIP_IP_BUF->srcipaddr.u8[15],
  //               UIP_IP_BUF->destipaddr.u8[15],
  //               UIP_USDN_BUF->flow,
  //               uip_ds6_if.cur_hop_limit - UIP_IP_BUF->ttl + 1);
  //     break;
  //   case ATOM_SB_TYPE_RPL:
  //     LOG_STAT("BUF %s s:%d d:%d\n",
  //               RPL_CODE_STRING(UIP_ICMP_BUF->icode),
  //               UIP_IP_BUF->srcipaddr.u8[15],
  //               UIP_IP_BUF->destipaddr.u8[15]);
  //     break;
  //   default:
  //     LOG_ERR("Unknown SB type\n");
  // }

  /* Poll ourselves as quickly as possible */
  LOG_DBG("Poll the atom controller process\n");
  process_poll(&controller_process);
}

// /*---------------------------------------------------------------------------*/
// void
// atom_send(struct atom_sb *sb, atom_response_t *response)
// {
//
// }

/*---------------------------------------------------------------------------*/
void
atom_set_handshake_timer(sdn_tmr_state_t state,
                         uint8_t type,
                         struct ctimer *timer,
                         void (*callback)(void *),
                         void *data)
{
  int delay;
  LOG_DBG("%s %s hs timer\n", SDN_TIMER_STRING(state), HANDSHAKE_STRING(type));

  /* Get delay based on type */
  switch (type) {
    case ATOM_RESPONSE_CFG:
      delay = ATOM_RANDOM_CFG_HS_DELAY();
      break;
    default:
      LOG_ERR("Unknown handshake type\n");
      return;
  }
  /* Set timer */
  switch(state) {
    case SDN_TMR_STATE_STOP:
      ctimer_stop(timer);
      break;
    case SDN_TMR_STATE_START:
    case SDN_TMR_STATE_RESET:
      ctimer_set(timer, delay, callback, data);
      LOG_DBG("Handshake delay %d.%02lu\n", (int)(delay/CLOCK_SECOND),
        (int)(100L * (delay/CLOCK_SECOND)) - ((int)(delay/CLOCK_SECOND)*100L));
      break;
    case SDN_TMR_STATE_IMMEDIATE:
      callback(data);
      break;
    default:
      LOG_ERR("Unknown state!\n");
      return;
  }
}

/*---------------------------------------------------------------------------*/
void
atom_run(struct atom_sb *sb)
{
  int i;

  /* Set initial response to null */
  atom_response_t *response = NULL;
  /* Get the action from the sb */
  atom_action_t *action = sb->in();

  LOG_DBG("Running %s action\n", ACTION_STRING(action->type));

  if(action != NULL) {
    /* Get the apps for that action */
    LOG_DBG("Get %s applications\n", ACTION_STRING(action->type));
    atom_app_ptr_t *apps = get_apps(sb->app_matrix, action);

    /* Check we actally have some apps to run */
    if(apps != NULL) {
      uint8_t n_apps = get_num_apps(sb->app_matrix, action);
      LOG_DBG("There are %d applications\n", n_apps);
      /* Run the apps and get the response */
      for(i = 0; i < n_apps; i++) {
        /* Check to see if we are running the right action on the right app type */
        LOG_DBG("Trying to run app %s\n", apps[i]->name);
        if(action->type == apps[i]->action_type) {
          response = apps[i]->run(action->data);

          // TODO: Configurable logic so we can play around with what app outputs

          if(response != NULL) {
            // Break on first successful result
            break;
          }
        } else {
          LOG_WARN("Action type [%s] not handled by APP [%s])\n",
                   ACTION_STRING(action->type),
                   ACTION_STRING(apps[i]->action_type));
        }
      }
      /* Send response to sb */
      if(response != NULL) {
        // FIXME: This needs reviewed. Who decides where the response should go?
        uip_ipaddr_copy(&response->dest, &action->src);
        sb->out(action, response);
        return;
      } else {
        LOG_DBG("No response from apps.\n");
        return;
      }
    }

    // LOG_ERR("No apps to run!\n");

    // HACK: Treat netupdates as special for now...
    if(action->type == ATOM_ACTION_NETUPDATE){
      LOG_DBG("Calling network update app\n");
      do_net_update(action, action->data);
      return;
    }

  } else {
    LOG_ERR("Action was NULL!\n");
  }
}

/*---------------------------------------------------------------------------*/
/* PROCESS */
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(controller_process, ev, data)
{
  atom_msg_t *m;

  PROCESS_BEGIN();

  LOG_DBG("Atom SDN controller process started!\n");

  while(1) {
    /* Wait for event */
    PROCESS_YIELD();

    LOG_DBG("START ***************************** \n");
    if(ev == PROCESS_EVENT_POLL) {
      LOG_DBG("Received poll event\n");
      /* Get the first packet on the queue and put it on the cbuf */
      m = atom_buffer_head();
      /* Run atom apps for input */
      atom_run(m->sb);
      /* Free the message from the input queue */
      atom_buffer_remove(m);
      /* Clear action/response buffers */
      atom_action_buf_clear();
      atom_response_buf_clear();
    } else {
      LOG_ERR("Unknown event (0x%x) :(\n", ev);
    }
    LOG_DBG("END ******************************* \n");
  }

  PROCESS_END();
}
