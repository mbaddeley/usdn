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
 *         uSDN Core: Extension headers for source routing.
 * \author
 *         Michael Baddeley <m.baddeley@bristol.ac.uk>
 */
#include "net/ip/uip.h"
#include "net/ipv6/uip-ds6.h"

#include "net/rpl/rpl.h"
#include "net/rpl/rpl-ns.h"
#include "net/rpl/rpl-private.h"

#include "net/sdn/sdn.h"

/* Log configuration */
#include "sys/log-ng.h"
#define LOG_MODULE "SDN-EXT"
#define LOG_LEVEL LOG_LEVEL_SDN

#include <limits.h>
#include <string.h>

#define UIP_IP_BUF                ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])
#define UIP_EXT_BUF               ((struct uip_ext_hdr *)&uip_buf[uip_l2_l3_hdr_len])
#define UIP_RH_BUF                ((struct uip_routing_hdr *)&uip_buf[uip_l2_l3_hdr_len])
#define UIP_RPL_SRH_BUF           ((struct uip_sdn_srh_hdr *)&uip_buf[uip_l2_l3_hdr_len + RPL_RH_LEN])

/*---------------------------------------------------------------------------*/
int
sdn_ext_insert_srh(sdn_srh_route_t *route)
{
  uip_ipaddr_t ipaddr;
  uint8_t temp_len;
  uint8_t path_len;
  uint8_t ext_len;
  uint8_t cmpri, cmpre; /* Masking fields in the SRH */
  uint8_t *hop_ptr;
  uint8_t padding;
  sdn_node_id_t id;
  int i;

  LOG_DBG("INSERT SRH with dest [");
  LOG_DBG_6ADDR(&UIP_IP_BUF->destipaddr);
  LOG_DBG_("]\n");

  /* Compute path length and compression factors */
  /* Take cmpr from the sdn_srh_route */
  cmpri = route->cmpr;
  cmpre = route->cmpr;

  LOG_DBG("Route with len %d\n", route->length);
  i = 1; id = route->nodes[i]; path_len = 0;
  while(i < route->length) {
    LOG_DBG("SRH Hop %d\n", id);
    id = route->nodes[++i];
    path_len++;
  }

  /* Extension header length: fixed headers + (n-1) * (16-ComprI) + (16-ComprE)*/
  ext_len = RPL_RH_LEN + RPL_SRH_LEN
      + (path_len - 1) * (16 - cmpre)
      + (16 - cmpri);

  padding = ext_len % 8 == 0 ? 0 : (8 - (ext_len % 8));
  ext_len += padding;

  LOG_DBG("SRH Path len: %u, ComprI %u, ComprE %u, ext len %u (padding %u)\n",
          path_len, cmpri, cmpre, ext_len, padding);

  /* Check if there is enough space to store the extension header */
  if(uip_len + ext_len > UIP_BUFSIZE) {
    LOG_ERR("INSERT Packet too long: can't add source routing header (%u bytes)\n", ext_len);
    return 1;
  }

  /* Move existing ext headers and payload uip_ext_len further */
  memmove(uip_buf + uip_l2_l3_hdr_len + ext_len,
      uip_buf + uip_l2_l3_hdr_len, uip_len - UIP_IPH_LEN);
  memset(uip_buf + uip_l2_l3_hdr_len, 0, ext_len);

  /* Insert source routing header */
  UIP_RH_BUF->next = UIP_IP_BUF->proto;
  UIP_IP_BUF->proto = UIP_PROTO_ROUTING;

  /* Initialize IPv6 Routing Header */
  UIP_RH_BUF->len = (ext_len - 8) / 8;
  UIP_RH_BUF->routing_type = SDN_RH_TYPE_SRH;
  UIP_RH_BUF->seg_left = path_len;

  /* Initialize SDN Source Routing Header */
  UIP_RPL_SRH_BUF->cmpr = (cmpri << 4) + cmpre;
  UIP_RPL_SRH_BUF->pad = padding << 4;

  /* Get preceeding SRH address bytes based on the current destination addr */
  uip_ipaddr_copy(&ipaddr, &UIP_IP_BUF->destipaddr);

  /* Initialize addresses field (the actual source route).
   * From last to first. */
  hop_ptr = ((uint8_t *)UIP_RH_BUF) + ext_len - padding; /* Pointer where to write the next hop compressed address */
  for(i = route->length - 1; i > 0; i--) {
    id = route->nodes[i];
    /* Compute hop ipaddr from node id */
    memcpy(&ipaddr.u8[15], &id, sizeof(sdn_node_id_t));
    // LOG_DBG("INSERT ID is %d, Next Node is [", id);
    // LOG_DBG_6ADDR(&ipaddr);
    // LOG_DBG_("]\n");
    hop_ptr -= (16 - cmpri);
    memcpy(hop_ptr, ((uint8_t*)&ipaddr) + cmpri, 16 - cmpri);
  }
  // LOG_DBG("INSERT Current Dest is [");
  // LOG_DBG_6ADDR(&UIP_IP_BUF->destipaddr);
  // LOG_DBG_("]\n");
  // LOG_DBG("INSERT Next HOP is [");
  // LOG_DBG_6ADDR(&ipaddr);
  // LOG_DBG_("]\n");
  /* The next hop is placed as the current IPv6 destination */
  uip_ipaddr_copy(&UIP_IP_BUF->destipaddr, &ipaddr);

  /* In-place update of IPv6 length field */
  temp_len = UIP_IP_BUF->len[1];
  UIP_IP_BUF->len[1] += ext_len;
  if(UIP_IP_BUF->len[1] < temp_len) {
    UIP_IP_BUF->len[0]++;
  }

  uip_ext_len = ext_len;
  uip_len += ext_len;

  return 1;
}
