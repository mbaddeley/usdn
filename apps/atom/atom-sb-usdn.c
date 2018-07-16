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
 *         Atom SDN Controller: Southbound uSDN connector.
 * \author
 *         Michael Baddeley <m.baddeley@bristol.ac.uk>
 */
#include <stdio.h>
#include <string.h>

#include "contiki-net.h"

#include "sys/node-id.h"

#include "net/ip/uip.h"
#include "net/sdn/sdn-conf.h"
#include "net/sdn/usdn/usdn.h"

#include "atom.h"

/* Log configuration */
#include "sys/log-ng.h"
#define LOG_MODULE "ATOM"
#define LOG_LEVEL LOG_LEVEL_ATOM

/* UIP buffer (for debugging input) */
#define UIP_IP_BUF       ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])
#define UIP_USDN_BUF     ((struct usdn_hdr *)&uip_buf[uip_l2_l3_udp_hdr_len])

/* Controller buffer */
#define cbuf_l3_udp_hdr_len     (UIP_IPUDPH_LEN + c_ext_len)
#define cbuf_l3_udp_sdn_hdr_len (cbuf_l3_udp_hdr_len + USDN_H_LEN)
#define C_IP_BUF                ((struct uip_ip_hdr *)&c_buf[UIP_LLH_LEN])
#define C_USDN_HDR              ((struct usdn_hdr *)&c_buf[cbuf_l3_udp_hdr_len])
#define C_USDN_PAYLOAD          ((void *)&c_buf[cbuf_l3_udp_sdn_hdr_len])

/* Outgoing packet buffer */
static uint8_t              output_buf[80];
#define C_USDN_OUT          ((uint8_t *)&output_buf)
#define C_USDN_OUT_PAYLOAD  ((uint8_t *)&output_buf + USDN_H_LEN)

/* udp connection */
static struct simple_udp_connection udp;

/*---------------------------------------------------------------------------*/
usdn_hdr_t *
usdn_set_header(void *buf, uint8_t net, uint8_t type, uint8_t flow)
{
  usdn_hdr_t *hdr = (usdn_hdr_t *)buf;
  hdr->net = net;
  hdr->typ = type;
  hdr->flow = flow;

  return hdr;
}

/*---------------------------------------------------------------------------*/
/* Message Handling In */
/*---------------------------------------------------------------------------*/
static atom_action_t *
nsu_input(void) {
  int i;
  usdn_nsu_t *nsu = (usdn_nsu_t *)C_USDN_PAYLOAD;

  atom_action_type_t action_type = ATOM_ACTION_NETUPDATE;
  atom_netupdate_action_t action_data;

  LOG_DBG("NSU There are %d link updates for node [%u]",
           nsu->num_links,
           C_IP_BUF->srcipaddr.u8[15]);

  /* Get node info */
  uip_ipaddr_copy(&action_data.node.ipaddr, &C_IP_BUF->srcipaddr);
  action_data.node.cfg_id = nsu->cfg_id;
  action_data.node.rank = nsu->rank;
  /* Get link info */
  action_data.node.num_links = nsu->num_links;
  // action_data.n_links = nsu->num_links;
  for(i = 0; i < nsu->num_links; i++) {
    action_data.node.links[i].dest_id = nsu->links[i].nbr_id;
    action_data.node.links[i].rssi = nsu->links[i].rssi;
  }

  return atom_action_buf_copy_to(action_type, &action_data);
}

/*---------------------------------------------------------------------------*/
static atom_action_t *
cjoin_input(void)
{
  atom_action_type_t action_type = ATOM_ACTION_JOIN;
  atom_join_action_t action_data;

  /* Get node address */
  uip_ipaddr_copy(&action_data.node, &C_IP_BUF->srcipaddr);

  return atom_action_buf_copy_to(action_type, &action_data);
}


/*---------------------------------------------------------------------------*/
static atom_action_t *
ftq_input(void)
{
  /* Get the ftq packet */
  usdn_ftq_t *ftq = (usdn_ftq_t *)C_USDN_PAYLOAD;

  atom_action_type_t action_type = ATOM_ACTION_ROUTING;
  atom_routing_action_t action_data;

  action_data.tx_id = ftq->tx_id;
  uip_ipaddr_copy(&action_data.src, &C_IP_BUF->srcipaddr);
  uip_ipaddr_copy(&action_data.dest, (uip_ip6addr_t *)ftq->data);

  // TODO: Work out what type of query this is

  LOG_DBG("FTQ receive FTQ with tx_id = %d\n", ftq->tx_id);
  LOG_DBG("FTQ src [");
  LOG_DBG_6ADDR(&action_data.src);
  LOG_DBG_("]\n");
  LOG_DBG("FTQ dest [");
  LOG_DBG_6ADDR(&action_data.dest);
  LOG_DBG_("]\n");

  return atom_action_buf_copy_to(action_type, &action_data);
}

/*---------------------------------------------------------------------------*/
/* Message Handling Out */
/*---------------------------------------------------------------------------*/
uint8_t
cack_output(uint8_t net_id, uint8_t flow, void *buf)
{
  usdn_set_header(buf, net_id, USDN_MSG_CODE_CACK, flow);

  return USDN_H_LEN;
}

/*---------------------------------------------------------------------------*/
static uint8_t
cnack_output(uint8_t net_id, uint8_t flow, void *buf)
{
  usdn_set_header(buf, net_id, USDN_MSG_CODE_CNACK, flow);

  return USDN_H_LEN;
}


/*---------------------------------------------------------------------------*/
// FIXME: This header is different from the other output functions
static uint8_t
fts_output(uint8_t id, uip_ipaddr_t *dest, void *data)
{
  /* Set the usdn header */
  usdn_set_header(C_USDN_OUT, 0, USDN_MSG_CODE_FTS, id);
  /* Set the usdn payload */
  usdn_fts_t *fts = (usdn_fts_t *)C_USDN_OUT_PAYLOAD;

  // TODO: Work out what type of query it was

  /* Routing */
  fts->tx_id = id;
  fts->m.operator = EQ;
  fts->m.index = uip_dst_index;
  fts->m.len = sizeof(uip_ipaddr_t);
  fts->m.req_ext = 0;
  memcpy(&fts->m.data, dest, fts->m.len);
  fts->a.action = SDN_FT_ACTION_SRH;
  fts->a.index = 0;
  fts->a.len = srh_route_length(((sdn_srh_route_t *)data));
  memcpy(&fts->a.data, (sdn_srh_route_t *)data, 16);

  /* Set this to be the default flowtable entry */
  // FIXME: Obviosuly we don't want this happening every time, and it Should
  //        depend upon the application...
#if SDN_CONF_DEFAULT_FT_ENTRY
  fts->is_default = 1;
#else
  fts->is_default = 0;
#endif /* SDN_CONF_DEFAULT_FT_ENTRY */

  // Debug
  // print_usdn_fts(fts);

  return USDN_H_LEN + fts_length(fts);
}

/*---------------------------------------------------------------------------*/
static uint8_t
cfg_output(uint8_t net_id, uint8_t flow, void *buf) {
  /* Set the usdn header */
  usdn_set_header(C_USDN_OUT, net_id, USDN_MSG_CODE_CFG, flow);
  /* Set the usdn payload */
  usdn_cfg_t *cfg = (usdn_cfg_t *)C_USDN_OUT_PAYLOAD;
  /* Set fields as specified in usdn.h */
  cfg->sdn_net =           SDN_CONF_DEFAULT_NET;
  cfg->cfg_id =            1; // TODO: Modes of operation
  cfg->ft_lifetime =            SDN_FT_LIFETIME;
  cfg->query_full =        SDN_CONF_QUERY_FULL_PACKET;
  cfg->query_idx =         SDN_CONF_QUERY_INDEX;
  cfg->query_len =         SDN_CONF_QUERY_LENGTH;
  cfg->update_period =     SDN_CONF_CONTROLLER_UPDATE_PERIOD;
  cfg->rpl_dio_interval =  32; // FIXME: Hardcoded (2^16 = 65.536s)
  cfg->rpl_dfrt_lifetime = 120; // FIXME: Hardcoded (120mins)

  return USDN_H_LEN + cfg_length(cfg);
}

/*---------------------------------------------------------------------------*/
/* Southbound connector API */
/*---------------------------------------------------------------------------*/
// FIXME: This isn't currently used as the controller is held at a virtual
//        address
/* UDP callback function. Calls the 'post' function of the controller which
   copies over the uip_buf to a queue, and keeps a reference to this sb
   connector */
static void
usdn_receive(struct simple_udp_connection *c,
             const uip_ipaddr_t *source_addr,
             uint16_t source_port,
             const uip_ipaddr_t *dest_addr,
             uint16_t dest_port,
             const uint8_t *data, uint16_t datalen)
{
  /* Spit out some stats */
  uint8_t hops = uip_ds6_if.cur_hop_limit - UIP_IP_BUF->ttl + 1;
  LOG_STAT("BUF %s s:%d d:%d id:%d h:%d\n",
          USDN_CODE_STRING(UIP_USDN_BUF->typ),
          UIP_IP_BUF->srcipaddr.u8[15],
          UIP_IP_BUF->destipaddr.u8[15],
          UIP_USDN_BUF->flow,
          hops);
  /* Post to the controller queue */
  atom_post(&sb_usdn);
}

/*---------------------------------------------------------------------------*/
static void
init(void)
{
  /* Initialize UDP connection */
  simple_udp_register(&udp, ATOM_USDN_LPORT, NULL,
                      ATOM_USDN_RPORT, usdn_receive);
  LOG_INFO("Atom sb usdn connector initialised\n");
}

/*---------------------------------------------------------------------------*/
static atom_action_t *
in(void) {

  /* Print out some stats */
  usdn_hdr_t *hdr = C_USDN_HDR;
  LOG_STAT("IN %s s:%d d:%d id:%d h:%u\n",
          USDN_CODE_STRING(hdr->typ),
          C_IP_BUF->srcipaddr.u8[15],
          C_IP_BUF->destipaddr.u8[15],
          hdr->flow,
          c_hops);

  // TODO: net id
  // TODO: flow id

  /* Convert the input into an action */
  atom_action_t *action = NULL;
  switch(hdr->typ) {
    case USDN_MSG_CODE_CJOIN:
      action = cjoin_input();
      break;
    case USDN_MSG_CODE_NSU:
      action = nsu_input();
      break;
    case USDN_MSG_CODE_FTQ:
      action = ftq_input();
      break;
    default:
      LOG_ERR("Unknown usn msg (%u) from [", hdr->typ);
      LOG_ERR_6ADDR(&C_IP_BUF->srcipaddr);
      LOG_ERR_("]\n");
      break;
  }

  /* Copy the address of the instigating node */
  if(action != NULL) {
    uip_ipaddr_copy(&action->src, &C_IP_BUF->srcipaddr);
    LOG_DBG("Action src [");
    LOG_DBG_6ADDR(&action->src);
    LOG_DBG_("]\n");
  }

  LOG_DBG("Calling apps with action %s\n", ACTION_STRING(action->type));
  return action;
}

/*---------------------------------------------------------------------------*/
static void
out(atom_action_t *action, atom_response_t *response)
{
  atom_routing_action_t *routing_action;
  /* Send length */
  uint8_t s_len = 0;
  /* Set the usdn header */
  usdn_hdr_t *hdr = (usdn_hdr_t *)output_buf;

  LOG_DBG("uSDN SB send response to");
  LOG_DBG_6ADDR(&response->dest);
  LOG_DBG_("]\n");

  /* Call output functions based on the response type to set the payload */
  switch(response->type) {
    LOG_DBG("%s response\n", RESPONSE_STRING(response->type));
    case ATOM_RESPONSE_ROUTING:
      if(action != NULL) {
        /* Dereference the action */
        routing_action = (atom_routing_action_t *)&action->data;
        // TODO: How do we know this is action->dest? How do we know it's an FTS EQ?
        s_len = fts_output(routing_action->tx_id, &routing_action->dest, response->data);
        break;
      }
    case ATOM_RESPONSE_ACK:
      s_len = cack_output(SDN_CONF_DEFAULT_NET, response->id, C_USDN_OUT);
      break;
    case ATOM_RESPONSE_NACK:
      s_len = cnack_output(SDN_CONF_DEFAULT_NET, response->id, C_USDN_OUT);
      break;
    case ATOM_RESPONSE_CFG:
      s_len = cfg_output(SDN_CONF_DEFAULT_NET, response->id, C_USDN_OUT);
      break;
    default:
      LOG_ERR("uSDN SB unknown response type");
      break;
  }

  /* Is there data to send? */
  if(s_len > 0) {
    /* Spit out some stats */
    LOG_STAT("OUT %s s:%d d:%d id:%d\n",
               USDN_CODE_STRING(hdr->typ),
               node_id,
               response->dest.u8[15],
               hdr->flow);
#if (SDN_CONTROLLER_TYPE == SDN_CONTROLLER_ATOM)
  /* If we are the controller and we are sending to ourselves
     then send straight to the engine */
  if(uip_ds6_is_my_addr(&response->dest)) {
    LOG_DBG("Sending to SDN_ENGINE\n");
    SDN_ENGINE.in(output_buf, s_len, NULL);
    return;
  }
#endif
  /* Send usdn packet over udp */
  simple_udp_sendto(&udp,
                    &output_buf,
                    s_len,
                    &response->dest);
  } else {
    LOG_ERR("Error in OUT");
  }
}

/*---------------------------------------------------------------------------*/
struct atom_sb sb_usdn = {
  ATOM_SB_TYPE_USDN,
  init,
  in,
  out,
  ATOM_APP_MATRIX_USDN
};
