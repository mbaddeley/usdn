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
 *         Atom SDN Controller: Join and configuration application.
 * \author
 *         Michael Baddeley <m.baddeley@bristol.ac.uk>
 */
#include "contiki.h"
#include "sys/ctimer.h"

#include "net/sdn/sdn-timers.h"

#include "atom.h"

/* Log configuration */
#include "sys/log-ng.h"
#define LOG_MODULE "ATOM-CFG"
#define LOG_LEVEL LOG_LEVEL_ATOM

// HACK: To stop us sending umpteen CFGs when we are just sending cos we
//       havent heard the ACK
#define MAX_CFG_TRIES 5

/*---------------------------------------------------------------------------*/
/* Private functions  */
/*---------------------------------------------------------------------------*/
static void
handle_cfg_timer(void *ptr)
{
  /* Dereference the node data */
  atom_node_t *node = (atom_node_t *)ptr;
  if (node != NULL && (&node->ipaddr != NULL)) {
    LOG_DBG("Handling CFG timer (%s %d) [",
            ((node->cfg_id == 0) ? "RESET" : "STOP"),
            node->id);
    LOG_DBG_6ADDR(&node->ipaddr);
    LOG_DBG_("]\n");
    /* Check to see if the CFG has been updated. The node should have responded
       with a message to update us, and this would have set the cfg_id. */
    if(node->cfg_id == 0 && node->handshake.n_tries < MAX_CFG_TRIES) {
      /* Reset the timer with a new delay */
      // ctimer_reset(&node->handshake.timer);
      atom_set_handshake_timer(SDN_TMR_STATE_RESET,
                               ATOM_RESPONSE_CFG,
                               &node->handshake.timer,
                               &handle_cfg_timer,
                               node);
      /* Send another cfg response */
      atom_response_t *response = atom_response_buf_copy_to(ATOM_RESPONSE_CFG, NULL);
      uip_ipaddr_copy(&response->dest, &node->ipaddr);
      node->handshake.n_tries++;
      sb_usdn.out(NULL, response);
    } else {
      /* Stop the handshake timer. We have been acked by the node. */
      ctimer_stop(&node->handshake.timer);
    }
  } else {
    LOG_ERR("Couldn't handle hs timer, node was NULL");
  }
}

/*---------------------------------------------------------------------------*/
/* Application API */
/*---------------------------------------------------------------------------*/
static void
init(void) {
  LOG_DBG("Atom join + configuration app initialised\n");
}

/*---------------------------------------------------------------------------*/
static atom_response_t *
run(void *data)
{
  atom_node_t *node;
  uip_ipaddr_t ipaddr;

  /* Dereference the action data */
  atom_join_action_t *action = (atom_join_action_t *)data;
  /* Get the node ip */
  uip_ipaddr_copy(&ipaddr, &action->node);

  if(&action->node != NULL){
    LOG_DBG("Run JOIN for node [");
    LOG_DBG_6ADDR(&ipaddr);
    LOG_DBG_("]\n");
    /* Send a heartbeat to atom-net */
    node = atom_net_node_heartbeat(&action->node);
    if(node->cfg_id == 0) {
      /* First time we've seen this node */
      LOG_STAT("n:%d c:1\n", node->id);
      /* Node is not yet configured. Set a handshake timer */
      node->handshake.n_tries = 1;
      atom_set_handshake_timer(SDN_TMR_STATE_START,
                               ATOM_RESPONSE_CFG,
                               &node->handshake.timer,
                               &handle_cfg_timer,
                               node);
      /* Respond to node with configuration */
      return atom_response_buf_copy_to(ATOM_RESPONSE_CFG, NULL);
    } else {
      /* We don't need to respond */
      LOG_WARN("Don't need to respond...\n");
    }
  }

  return NULL;
}

/*---------------------------------------------------------------------------*/
/* Application instance */
/*---------------------------------------------------------------------------*/
struct atom_app app_join_cfg = {
  "Join + Configuration",
  ATOM_ACTION_JOIN,
  init,
  run
};
