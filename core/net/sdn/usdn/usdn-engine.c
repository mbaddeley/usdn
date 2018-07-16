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
 *         uSDN Stack: The engine handles incomming and outgoing SDN messages,
 *                     using the API provided by the driver.
 * \author
 *         Michael Baddeley <m.baddeley@bristol.ac.uk>
 */
#include <stdio.h>
#include <string.h>

#include "contiki.h"
#include "contiki-net.h"
#include "net/ip/uip.h"
#include "net/ip/udp-socket.h"
#include "net/link-stats.h"
#include "sys/node-id.h"

// FIXME: These are for the packet print function
#include "net/packetbuf.h"
#include "net/mac/mac.h"

#include "sdn.h"
#include "sdn-cd.h"
#include "sdn-ft.h"
#include "sdn-conf.h"
#include "sdn-timers.h"
#include "sdn-packetbuf.h"
#include "usdn.h"

#include "net/rpl/rpl.h"

#include "net/packetbuf.h"
#include "net/mac/frame802154.h"

/* Log configuration */
#include "sys/log-ng.h"
#define LOG_MODULE "SDN-E"
#define LOG_LEVEL LOG_LEVEL_SDN

#define UIP_IP_BUF ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])

//HACK: This is for debugging so we can track the packet
static uint16_t nsu_count = 0;
static uint16_t ftq_count = 0;
static uint16_t cjoin_count = 0;

/* Controller join timer */
static struct ctimer join_timer;

static uint8_t            databuf[60];
#define USDN_BUF          ((uint8_t *)&databuf)
#define USDN_BUF_PAYLOAD  ((uint8_t *)&databuf + USDN_H_LEN)

/*---------------------------------------------------------------------------*/
/* PRINTING */
/*---------------------------------------------------------------------------*/
void
print_usdn_header(usdn_hdr_t *hdr)
{
  printf("hdr[%s n:%x f:%x]\n", USDN_CODE_STRING(hdr->typ),
                                hdr->net,
                                hdr->flow);
}

/*---------------------------------------------------------------------------*/
void
print_usdn_ftq(usdn_ftq_t *ftq)
{
  int i;
  printf("ftq[");
  printf("%d, %d, %d", ftq->tx_id, ftq->index, ftq->length);
  for(i = 0; i < ftq->length; i++) {
    printf(" %x", ((uint8_t *)ftq->data)[i]);
  }
  printf("]\n");
}

/*---------------------------------------------------------------------------*/
void
print_usdn_match(usdn_match_t *m)
{
  int i;
  printf("USDN:     M = OP:");
  if (m != NULL) {
    switch(m->operator){
      case EQ: printf("EQ "); break;
      case NOT_EQ: printf("NOT_EQ "); break;
      case GT: printf("GT "); break;
      case LT: printf("LT "); break;
      case GT_EQ: printf("GT_OR_EQ "); break;
      case LT_EQ: printf("LT_OR_EQ "); break;
      default: printf("UNKNOWN "); break;
    }
    printf("INDEX:%d LEN:%d VAL:[", m->index, m->len);
    for(i = 0; i < m->len; i++) {
      printf("%x ", m->data[i]);
    }
  }
  printf("]\n");
}

/*---------------------------------------------------------------------------*/
static void
print_usdn_action(usdn_action_t *a)
{
  int i;
  printf("USDN:     A = ");
  if(a != NULL) {
    switch(a->action){
      case SDN_FT_ACTION_ACCEPT: printf("ACCEPT "); break;
      case SDN_FT_ACTION_QUERY: printf("QUERY "); break;
      case SDN_FT_ACTION_FORWARD: printf("FORWARD "); break;
      case SDN_FT_ACTION_DROP: printf("DROP "); break;
      case SDN_FT_ACTION_MODIFY: printf("MODIFY "); break;
      case SDN_FT_ACTION_FALLBACK: printf("FALLBACK "); break;
      case SDN_FT_ACTION_SRH: printf("SRH "); break;
      case SDN_FT_ACTION_CALLBACK: printf("CALLBACK "); break;
    }
    printf("INDEX:%d LEN:%d VAL:[", a->index, a->len);
    for(i = 0; i < a->len; i++) {
      printf("%x ", a->data[i]);
    }
  }
  printf("]\n");
}

/*---------------------------------------------------------------------------*/
void
print_usdn_fts(usdn_fts_t *fts)
{
  printf("fts[");
  printf("id:%d dflt:%d :: \n", fts->tx_id, fts->is_default);
  print_usdn_match(&fts->m);
  print_usdn_action(&fts->a);
}


/*---------------------------------------------------------------------------*/
void
print_usdn_nsu(usdn_nsu_t *nsu)
{
  int i;
  usdn_nsu_link_t *link;
  printf("nsu[cfg:%d, r:%d nl:%d", nsu->cfg_id, nsu->rank, nsu->num_links);
  if(nsu->num_links > 0) {
    for(i = 0; i < nsu->num_links; i++) {
      link = &nsu->links[i];
      printf(" :: %x %x", link->nbr_id, link->rssi);
    }
  }
  printf("]\n");
}

/*---------------------------------------------------------------------------*/
/* Callback */
/*---------------------------------------------------------------------------*/
// FIXME: Redo this so it's a callback function / interrupt
#if UIP_CONF_IPV6_SDN && SDN_SICSLOWPAN_CALLBACK
void
usdn_engine_mac_callback(void *data, int status)
{
  /* Dereference the uSDN header */
  usdn_hdr_t *hdr = (usdn_hdr_t *)data;

  /* OUT type src dest txid mac_satus*/
  LOG_STAT("MAC %s f:%d ms:%s\n",
            USDN_CODE_STRING(hdr->typ),
            hdr->flow,
            MAC_TX_STATE_STRING[status]);
}
#endif

/*---------------------------------------------------------------------------*/
/* Message Handling In */
/*---------------------------------------------------------------------------*/
static void
cack_input(void *data)
{
  LOG_DBG("Parsing CACK...\n");

  /* TODO: usdn CACK */

}

static void
cnack_input(void *data)
{
  LOG_DBG("Parsing CNACK...\n");

  /* TODO: usdn CNACK */

}

/*---------------------------------------------------------------------------*/
// FIXME: This works, HOWEVER, it's not really something that should be done
//        in the engine.  We should really be creating the table entries in the
//        driver. The FTS should be using the driver API to set FT entries.
static void
fts_input(void *data) {
  usdn_fts_t *fts = (usdn_fts_t *)data;

  LOG_DBG("Parsing FTSET...\n");
  /* Create the actual entry in the table */
  sdn_ft_match_rule_t *m = sdn_ft_create_match(fts->m.operator,
                                               fts->m.index,
                                               fts->m.len,
                                               fts->m.req_ext,
                                               &fts->m.data);
  sdn_ft_action_rule_t *a = sdn_ft_create_action(fts->a.action,
                                                 fts->a.index,
                                                 fts->a.len,
                                                 &fts->a.data);
  sdn_ft_create_entry(FLOWTABLE, m, a, SDN_CONF.ft_lifetime, fts->is_default);

  // print_usdn_fts(fts);

#if SDN_CONF_RETRY_AFTER_QUERY
  /* Check for a buffered packet */
  LOG_DBG("Retry buffered packet with tx_id: %d\n", fts->tx_id);
  SDN_DRIVER.retry(fts->tx_id);
#endif
}

/*---------------------------------------------------------------------------*/
static void
cfg_input(void *data) {
  usdn_cfg_t *cfg = (usdn_cfg_t *)data;
  LOG_DBG("Setting SDN Configuration...\n");

#if WITH_SDN_STATS
  /* We have been configured */
  LOG_STAT("cfg:1\n");
#endif

  // TODO: Put this in config and call a function
  /* Update Config */
  SDN_CONF.sdn_net = cfg->sdn_net;
  SDN_CONF.cfg_id = cfg->cfg_id;
  SDN_CONF.hops = uip_ds6_if.cur_hop_limit - UIP_IP_BUF->ttl + 1;
  SDN_CONF.ft_lifetime = cfg->ft_lifetime;
  SDN_CONF.query_full = cfg->query_full;
  SDN_CONF.query_idx = cfg->query_idx;
  SDN_CONF.query_len = cfg->query_len;

  /* Update controller */
  DEFAULT_CONTROLLER->update_period = cfg->update_period;

  /* Update RPL */
  // rpl_sdn_set_instance_properties(cfg->rpl_dio_interval,
  //                                 cfg->rpl_dfrt_lifetime);

  if(DEFAULT_CONTROLLER->state == CTRL_CONNECTING) {
    /* Setup a new controller connection */
    sdn_cd_set_state(CTRL_CONNECTED_NEW, DEFAULT_CONTROLLER);
  } else {
    /* Refresh the controller connection */
    sdn_cd_set_state(CTRL_CONNECTED, DEFAULT_CONTROLLER);
  }

  /* Immediately update/ack the controller (to say we have been configured) */
  SDN_ENGINE.controller_update(SDN_TMR_STATE_IMMEDIATE);
}
/*---------------------------------------------------------------------------*/
/* Message Handling Out */
/*---------------------------------------------------------------------------*/
static void
send(sdn_controller_t *c, uint8_t length, void *data)
{
  usdn_hdr_t *hdr = (usdn_hdr_t *)data;
  LOG_DBG("Sending %s len:%d\n", USDN_CODE_STRING(hdr->typ), length);
  if(c != NULL) {
    /* Output some info and send */
    LOG_STAT("OUT %s s:%d d:%d id:%d\n",
             USDN_CODE_STRING(hdr->typ),
             uip_lladdr.addr[7],
             c->ipaddr.u8[15],
             hdr->flow);
    /* Send to the controller */
    SDN_ADAPTER.send(c, length, data);
  } else {
    LOG_ERR("Controller was NULL!");
  }
}

/*---------------------------------------------------------------------------*/
static usdn_hdr_t *
set_header(void *buf, uint8_t net, uint8_t type, uint8_t flow)
{
  usdn_hdr_t *hdr = (usdn_hdr_t *)buf;
  hdr->net = net;
  hdr->typ = type;
  hdr->flow = flow;

  return hdr;
}

/*---------------------------------------------------------------------------*/
static usdn_nsu_t *
nsu_output(void *buf)
{
  uint8_t i;
  usdn_nsu_t      *nsu;
  uip_ds6_nbr_t   *nbr;
  usdn_nsu_link_t link;

  nsu = (usdn_nsu_t *)buf;

  /* Set node information */
  nsu->cfg_id = SDN_CONF.cfg_id;
  rpl_dag_t *dag = rpl_get_any_dag();
  if(dag != NULL) {
      nsu->rank = DAG_RANK(dag->rank, dag->instance) - 1;
  }

  /* Set link information, itterating over all neighbours */
  i = 0;
  nbr = nbr_table_head(ds6_neighbors);
  while(nbr != NULL) {
    /* Neighbor stats */
    link.nbr_id = sdn_node_id_from_ipaddr(&nbr->ipaddr);
    /* Link stats */
    const struct link_stats *stats = sdn_get_nbr_link_stats(nbr);
    link.rssi = stats->rssi;
    memcpy(&nsu->links->rssi, &link.rssi, sizeof(int16_t));
    // link.etx = stats->etx;
    // memcpy(&nsu->links->etx, &link.etx, sizeof(int16_t));
    // link.is_fresh = sdn_get_nbr_link_is_fresh(nbr);
    /* Copy the link into the buffer */
    memcpy(&nsu->links[i], &link, sizeof(usdn_nsu_link_t));
    /* Move on to next neighbor link */
    nbr = nbr_table_next(ds6_neighbors, nbr);
    i++;
  }

  /* Set total number of links */
  nsu->num_links = i;

  /* Return the payload length */
  return nsu;
}

/*---------------------------------------------------------------------------*/
static usdn_ftq_t *
ftq_output(void *buf, uint8_t tx_id, uint16_t datalen, void *data)
{
  usdn_ftq_t *ftq = (usdn_ftq_t *)buf;
  /* Set FTQ */
  ftq->tx_id = tx_id;
  ftq->index = 0;
  ftq->length = datalen;
  memcpy(&ftq->data, (uint8_t *)data, datalen);

  return ftq;
}

/*---------------------------------------------------------------------------*/
static usdn_nsu_t *
cjoin_output(void *buf)
{
  /* Currently a CJOIN is essentially a NSU with a different header... */
  return nsu_output(buf);
}

/*---------------------------------------------------------------------------*/
/* Timer Handling */
/*---------------------------------------------------------------------------*/
static void
handle_join_timer(void *ptr)
{
  sdn_controller_t *c = (sdn_controller_t *)ptr;
  LOG_DBG("JOIN Handling timer...\n");

  if(c != NULL && !sdn_connected()) {
    set_header(USDN_BUF,
               SDN_CONF.sdn_net,
               USDN_MSG_CODE_CJOIN,
               cjoin_count++);
    usdn_nsu_t *nsu = cjoin_output(USDN_BUF_PAYLOAD);

    uint8_t packet_length = USDN_H_LEN + nsu_length(nsu);
    send(c, packet_length, USDN_BUF);
    ctimer_reset(&join_timer);
  } else {
    LOG_ERR("JOIN We seem to have already joined...\n");
  }
}

/*---------------------------------------------------------------------------*/
static void
handle_update_timer(void *ptr)
{
  sdn_controller_t *c = (sdn_controller_t *)ptr;
  LOG_DBG("NSU Handling NSU timer...\n");

  if(c != NULL) {
    // usdn_hdr_t *hdr =
    set_header(USDN_BUF,
               SDN_CONF.sdn_net,
               USDN_MSG_CODE_NSU,
               nsu_count++);
    usdn_nsu_t *nsu = nsu_output(USDN_BUF_PAYLOAD);

    // print_usdn_header(hdr);
    // print_usdn_nsu(nsu);
    // LOG_DBG("NSU Length: %d, Send Length:%d\n", nsu_length(nsu), USDN_H_LEN + nsu_length(nsu));

    uint8_t packet_length = USDN_H_LEN + nsu_length(nsu);
    send(c, packet_length, USDN_BUF);
    /* If the conf has set the period to 0 then turn off updates */
    if (c->update_period == 0) {
      SDN_ENGINE.controller_update(SDN_TMR_STATE_STOP);
    } else {
      SDN_ENGINE.controller_update(SDN_TMR_STATE_RESET);
    }
  } else {
    LOG_ERR("NSU No controller to send to!");
  }
}

/*---------------------------------------------------------------------------*/
/* SDN Engine Implementation */
/*---------------------------------------------------------------------------*/
static void
init(void)
{
  /* TODO */
}

/*---------------------------------------------------------------------------*/
static void
in(void *data, uint8_t length, void *ptr)
{
  usdn_hdr_t *hdr = (usdn_hdr_t *)data;
  /* IN type src txid hops */
  LOG_STAT("IN %s s:%d d:%d id:%d h:%d\n",
            USDN_CODE_STRING(hdr->typ),
            UIP_IP_BUF->srcipaddr.u8[sizeof(UIP_IP_BUF->srcipaddr.u8) - 1],
            UIP_IP_BUF->destipaddr.u8[sizeof(UIP_IP_BUF->destipaddr.u8) - 1],
            hdr->flow,
            uip_ds6_if.cur_hop_limit - UIP_IP_BUF->ttl + 1);

  /* Handle the message based on type */
  switch(hdr->typ) {
    case USDN_MSG_CODE_CACK:
      cack_input(data);
      break;
    case USDN_MSG_CODE_CNACK:
      cnack_input(data);
      break;
    case USDN_MSG_CODE_FTS:
      fts_input(data + USDN_H_LEN);
      break;
    case USDN_MSG_CODE_CFG:
      cfg_input(data + USDN_H_LEN);
      break;
    default:
      LOG_ERR("IN Unknown message type!");
      break;
  }
}

/*---------------------------------------------------------------------------*/
/* FIXME: This is currently not used as we are using DAO messages to join */
static void
controller_join(sdn_tmr_state_t state)
{
  sdn_controller_t *c = DEFAULT_CONTROLLER;
  int join_period = SDN_RANDOM_JOIN_DELAY();
  LOG_DBG("Set CJOIN timer (%s)...\n", SDN_TIMER_STRING(state));
  if(c != NULL) { /* Defensive coding */
    switch(state) {
      case SDN_TMR_STATE_STOP:
        ctimer_stop(&join_timer);
        break;
      case SDN_TMR_STATE_START:
      case SDN_TMR_STATE_RESET:
        ctimer_set(&join_timer, join_period, handle_join_timer, c);
        LOG_DBG("Join delay %d.%02lu\n", (int)(join_period/CLOCK_SECOND),
          (int)(100L * (join_period/CLOCK_SECOND)) - ((int)(join_period/CLOCK_SECOND)*100L));
        break;
      case SDN_TMR_STATE_IMMEDIATE:
        handle_join_timer(c);
        break;
      default:
        LOG_ERR("CJOIN Unknown state!");
    }
  } else {
    LOG_ERR("CJOIN Default controller is NULL!");
  }
}

/*---------------------------------------------------------------------------*/
static void
controller_update(sdn_tmr_state_t state)
{
  sdn_controller_t *c = DEFAULT_CONTROLLER;
  int period;

  LOG_DBG("Set NSU timer (%s)...\n", SDN_TIMER_STRING(state));
  if(c != NULL) { /* Defensive coding */
    switch(state) {
      case SDN_TMR_STATE_STOP:
        ctimer_stop(&c->update_timer);
        break;
      case SDN_TMR_STATE_START:
      case SDN_TMR_STATE_RESET:
        if(!c->update_period) {
          LOG_ERR("Update period is 0, not setting NSU");
        } else {
          period = (c->update_period * CLOCK_SECOND) + SDN_RANDOM_NSU_DELAY();
          LOG_DBG("Setting NSU %d ticks in future\n", period);
          ctimer_set(&c->update_timer, period, handle_update_timer, c);
        }
        break;
      case SDN_TMR_STATE_IMMEDIATE:
        handle_update_timer(c);
        break;
      default:
        LOG_ERR("NSU Unknown state!");
    }
  } else {
    LOG_ERR("NSU Default controller is NULL!");
  }
}

/*---------------------------------------------------------------------------*/
static void
controller_query(void *data)
{
  sdn_controller_t *c = DEFAULT_CONTROLLER;
  sdn_bufpkt_t *p = (sdn_bufpkt_t *)data;

  // FIXME: There is a bit of confusion between flows and tx_id
  LOG_DBG("QUERY Send query with tx_id (%d)\n", p->id);

  if(c != NULL) {
    set_header(USDN_BUF,
               SDN_CONF.sdn_net,
               USDN_MSG_CODE_FTQ,
               ++ftq_count);
    usdn_ftq_t *ftq = ftq_output(USDN_BUF_PAYLOAD,
                                 p->id, p->buf_len, &p->packet_buf);
    // print_usdn_ftq(ftq);
    // LOG_DBG("FTQ Length: %d, Send Length:%d\n",
    //        ftq_length(ftq), USDN_H_LEN + ftq_length(ftq));
    uint8_t packet_length = USDN_H_LEN + ftq_length(ftq);
    send(c, packet_length, USDN_BUF);
  } else {
    LOG_ERR("Controller was NULL");
  }
}

/*---------------------------------------------------------------------------*/
void
controller_query_data(void *data, uint8_t len)
{
  sdn_controller_t *c = DEFAULT_CONTROLLER;

  LOG_DBG("QUERY Send query with tx_id (%d)\n", ftq_count);

  if(c != NULL) {
    set_header(USDN_BUF,
               SDN_CONF.sdn_net,
               USDN_MSG_CODE_FTQ,
               ++ftq_count);
    usdn_ftq_t *ftq = ftq_output(USDN_BUF_PAYLOAD, ftq_count, len, data);
    uint8_t packet_length = USDN_H_LEN + ftq_length(ftq);
    send(c, packet_length, USDN_BUF);
  } else {
    LOG_ERR("Controller was NULL");
  }
}

/*---------------------------------------------------------------------------*/
/* Engine API */
/*---------------------------------------------------------------------------*/
const struct sdn_engine usdn_engine = {
  init,
  in,
  controller_join,
  controller_update,
  controller_query
};
