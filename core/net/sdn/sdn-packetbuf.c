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
 *         uSDN Core: Packet buffer to store manage packets during the
 *                    controller query/response process.
 * \author
 *         Michael Baddeley <m.baddeley@bristol.ac.uk>
 */
#include <string.h>
#include <stdio.h>

#include "contiki.h"
#include "lib/memb.h"
#include "lib/list.h"
#include "lib/random.h"

#include "net/ip/uip.h"

#include "sdn.h"
#include "sdn-conf.h"
#include "sdn-packetbuf.h"

/* Log configuration */
#include "sys/log-ng.h"
#define LOG_MODULE "SDN-BUF"
#define LOG_LEVEL LOG_LEVEL_SDN

#define ID_MAX   255
static int current_id = 0;
#define generate_id() (++current_id % ID_MAX)

/*---------------------------------------------------------------------------*/
static void
packet_timedout(void *ptr)
{
  sdn_bufpkt_t *p = ptr;
  LOG_DBG("TIMEOUT Packet timed out! p=%p, id=%d\n", p, p->id);
  sdn_pbuf_free(p);
}

/*---------------------------------------------------------------------------*/
sdn_bufpkt_t *
sdn_pbuf_allocate(struct memb *memb, list_t list, clock_time_t lifetime, uint8_t *id)
{
  sdn_bufpkt_t *p = NULL;
  p = memb_alloc(memb);
  if(p == NULL) {
    LOG_ERR("Failed to alloc a packet\n");
    return NULL;
  }
  if (id != NULL) {
    p->id = *id;
  } else {
    /* Set the id based on the list size */
    p->id = generate_id();
  }
  /* Add the packet to the list and keep a pointer to the list in the packet */
  list_add(list, p);
  p->memb = memb;
  p->list = list;
  /* Set the packet lifetimer */
  ctimer_set(&p->lifetimer, lifetime, packet_timedout, p);
  LOG_DBG("Added a packet to buf (%p) (id=%d)!\n", p, p->id);
  return p;
}

/*---------------------------------------------------------------------------*/
void
sdn_pbuf_free(sdn_bufpkt_t* p)
{
  if(p != NULL) {
    ctimer_stop(&p->lifetimer);
    list_remove(p->list, p);
    LOG_DBG("Removed packet (%p) (id=%d) from buf!\n", p, p->id);
    int res = memb_free(p->memb, p);
    if (res !=0){
      LOG_ERR("Failed to dealloc a packet. Reference count: %d",res);
    }
  }
}

/*---------------------------------------------------------------------------*/
uint8_t *
sdn_pbuf_buf(sdn_bufpkt_t *p)
{
  return p != NULL? p->packet_buf: NULL;
}

/*---------------------------------------------------------------------------*/
uint16_t
sdn_pbuf_buflen(sdn_bufpkt_t *p)
{
  return p != NULL? p->buf_len: 0;
}

/*---------------------------------------------------------------------------*/
uint8_t
sdn_pbuf_extlen(sdn_bufpkt_t *p)
{
  return p != NULL? p->ext_len: 0;
}

/*---------------------------------------------------------------------------*/
void
sdn_pbuf_set(sdn_bufpkt_t *p, uint8_t *buf, uint16_t buf_len, uint8_t ext_len)
{
  if(p != NULL) {
    memcpy(&p->packet_buf, buf, buf_len);
    p->buf_len = buf_len;
    p->ext_len = ext_len;
  }
  LOG_DBG("Set packet (%p) (id=%d, len=%d, ext=%d)!",
           p, p->id, p->buf_len, p->ext_len);
}

/*---------------------------------------------------------------------------*/
sdn_bufpkt_t *
sdn_pbuf_find(list_t list, uint8_t id)
{
  sdn_bufpkt_t *p;
  for(p = list_head(list); p != NULL; p = list_item_next(p)) {
    if(p->id == id) {
      return p;
    }
  }
  LOG_ERR("RETRY Couldn't find packet with id=%d\n", id);
  return NULL;
}

/*---------------------------------------------------------------------------*/
static uint8_t
sdn_pbuf_cmp_buf(sdn_bufpkt_t *p1, sdn_bufpkt_t *p2)
{
  int i;
  if(p1->buf_len >= p2->buf_len) {
    for(i = 0; i < p2->buf_len; i++) {
      if(p1->packet_buf[i] != p2->packet_buf[i]) {
        // LOG_TRC("P1 & P2 NOT_EQUAL\n");
        return 0;
      }
    }
    // LOG_TRC("P1 & P2 EQUAL\n");
    return 1;
  }
  LOG_ERR("P1 len > P2 len. Cannot compare!\n");
  return 0;
}

/*---------------------------------------------------------------------------*/
static uint8_t
sdn_pbuf_cmp_buf_idx(sdn_bufpkt_t *p1,
                          sdn_bufpkt_t *p2,
                          uint8_t index, uint8_t len)
{
  int i;

  for(i = index; i < (index+len); i++) {
    if(p1->packet_buf[i] != p2->packet_buf[i]) {
      // LOG_TRC("P1 & P2 NOT_EQUAL %d %d\n", index, len);
      return 0;
    }
  }
  // LOG_TRC("P1 & P2 EQUAL\n");
  print_sdn_pbuf_packet(p1, SDN_PB_PRINT_DFLT);
  print_sdn_pbuf_packet(p2, SDN_PB_PRINT_DFLT);
  return 1;
}

/*---------------------------------------------------------------------------*/
sdn_bufpkt_t *
sdn_pbuf_contains(list_t list, uint8_t *buf, uint16_t buf_len, uint8_t *index, uint8_t *len)
{
 sdn_bufpkt_t *p;
 static sdn_bufpkt_t q;
 /* Set the packet for comparing */
 sdn_pbuf_set(&q, buf, buf_len, 0);
 /* Search through the buffer and compare packets */
 for(p = list_head(list); p != NULL; p = list_item_next(p)) {
   if((index != NULL) && (len != NULL)) {
     /* Compare a portion of the packets */
     if(sdn_pbuf_cmp_buf_idx(p, &q, *index, *len)) {
       // LOG_TRC("Packet already in buffer (id=%d)\n", p->id);
       return p;
     }
   } else {
     /* Compare the whole packet */
     if(sdn_pbuf_cmp_buf(p, &q)) {
       // LOG_TRC("Contains PACKET\n");
       return p;
     }
   }
 }
 /* If there is no match then return NULL*/
 return NULL;
}

/*---------------------------------------------------------------------------*/
int
sdn_pbuf_length(list_t list)
{
  return list_length(list);
}

/*---------------------------------------------------------------------------*/
void
print_sdn_pbuf_packet(sdn_bufpkt_t *p, sdn_pb_print_level_t level)
{
  int i;
  int idx_udp = UIP_IPH_LEN + UIP_LLH_LEN;
  int idx_udp_pld = UIP_IPUDPH_LEN + UIP_LLH_LEN;

  LOG_DBG("packet=%p | id=%d | timer=(%ld) data=[", p, p->id,
           timer_remaining(&p->lifetimer.etimer.timer));
  switch(level)
  {
    case SDN_PB_PRINT_UDP:
      /* UDP Header */
      LOG_DBG("UDP_H=[");
      for(i = 0; i < (idx_udp_pld - idx_udp); i++) {
        LOG_DBG(" %x\n", ((unsigned char *)&p->packet_buf[idx_udp])[i]);
      }
      LOG_DBG("] UDP_P=[");
      /* UDP PLD */
      for(i = 0; i < (p->buf_len - idx_udp_pld); i++) {
        LOG_DBG(" %x\n", ((unsigned char *)&p->packet_buf[idx_udp_pld])[i]);
      }
      break;
    default:
      for(i = 0; i < p->buf_len; i++) {
        LOG_DBG(" %x\n", ((unsigned char *)&p->packet_buf)[i]);
      }
      break;
  }

  LOG_DBG(" ]\n");

}

/*---------------------------------------------------------------------------*/
void
print_sdn_pbuf(list_t list)
{
  sdn_bufpkt_t *p;
  LOG_DBG("PACKETBUF: =================== Packetbuf\n");
  for(p = list_head(list); p != NULL; p = list_item_next(p)) {
    print_sdn_pbuf_packet(p, SDN_PB_PRINT_DFLT);
  }
  LOG_DBG("PACKETBUF: =================== (%d) Packets\n", list_length(list));
}

/** @} */
