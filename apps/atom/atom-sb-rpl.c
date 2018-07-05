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
 *         Atom SDN Controller: Southbound RPL connector.
 * \author
 *         Michael Baddeley <m.baddeley@bristol.ac.uk>
 */
#include <stdio.h>
#include <string.h>

#include "contiki-net.h"
#include "sys/node-id.h"
#include "net/ip/uip.h"
#include "net/rpl/rpl-private.h"

#include "atom.h"

/* Log configuration */
#include "sys/log-ng.h"
#define LOG_MODULE "ATOM"
#define LOG_LEVEL LOG_LEVEL_ATOM

/* Controller buffer */
#define cbuf_l2_l3_hdr_len (UIP_LLH_LEN + UIP_IPH_LEN + c_ext_len)
#define C_IP_BUF           ((struct uip_ip_hdr *)&c_buf[UIP_LLH_LEN])
#define C_ICMP_BUF         ((struct uip_icmp_hdr *)&c_buf[cbuf_l2_l3_hdr_len])

/*---------------------------------------------------------------------------*/
static atom_action_t *
dao_input(void)
{
  /* Atom can use the dao message as a join request from a node */
  atom_action_type_t action_type = ATOM_ACTION_JOIN;
  atom_join_action_t action_data;

  /* Get node address */
  uip_ipaddr_copy(&action_data.node, &C_IP_BUF->srcipaddr);

  return atom_action_buf_copy_to(action_type, &action_data);
}

/*---------------------------------------------------------------------------*/
static void
init(void)
{
  LOG_INFO("Atom sb rpl connector initialised\n");
}

/*---------------------------------------------------------------------------*/
static atom_action_t *
in(void) {
  struct uip_icmp_hdr *hdr = C_ICMP_BUF;

  if(hdr != NULL && hdr->type == ICMP6_RPL) {
      /* Print out some stats */
    LOG_STAT("IN %s s:%d d:%d\n",
            RPL_CODE_STRING(hdr->icode),
            C_IP_BUF->srcipaddr.u8[15],
            C_IP_BUF->destipaddr.u8[15]);

    /* Convert the input into an action */
    atom_action_t *action = NULL;
    switch(hdr->icode) {
      case RPL_CODE_DAO:
        action = dao_input();
        break;
      default:
        LOG_ERR("Unknown rpl icmp (%d) from [", hdr->icode);
        LOG_ERR_6ADDR(&C_IP_BUF->srcipaddr);
        LOG_ERR_("]\n");
        break;
    }

    /* Copy the address of the instigating node */
    if(action != NULL) {
      uip_ipaddr_copy(&action->src, &C_IP_BUF->srcipaddr);
      LOG_INFO("Action src [");
      LOG_INFO_6ADDR(&action->src);
      LOG_INFO_("]\n");
    }

    /* Return the action */
    LOG_DBG("Calling apps with action %s", ACTION_STRING(action->type));
    return action;
  }

  /* ICMP is null or it's not RPL ICMP type */
  LOG_ERR("ICMP_BUF NULL (%p) / Unknown ICMP type (%x)\n", hdr, hdr->type);
  return NULL;
}

/*---------------------------------------------------------------------------*/
static void
out(atom_action_t *action, atom_response_t *response)
{
  /* TODO: Ideally, we would respond to a DAO with controller configuration
           information. However, there is currently not really a good way
           to do this. Really, the initial controller info (ip addr, sb info)
           needs to be an extention of DIO messages, so we aren't sending
           multihop messages downwards to every single node (N.B. This is
           where Atomic-SDN comes in...) */
  LOG_INFO("RPL SB send response to");
  LOG_INFO_6ADDR(&response->dest);
  LOG_INFO_("]\n");

  switch(response->type) {
    case ATOM_RESPONSE_CFG:
      LOG_DBG("RPL SB response CFG\n");
      /* Send out a CFG on the usdn SB */
      sb_usdn.out(action, response);
      break;
    default:
      LOG_ERR("RPL SB unknown response type\n");
      break;
  }
}

/*---------------------------------------------------------------------------*/
struct atom_sb sb_rpl = {
  ATOM_SB_TYPE_RPL,
  init,
  in,
  out,
  ATOM_APP_MATRIX_RPL
};
