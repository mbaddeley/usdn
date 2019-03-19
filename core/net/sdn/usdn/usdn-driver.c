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
 *         uSDN Stack: The driver interfaces with the flowtable to provide a
 *                     basic API to allow specific functions, such as fowarding
 *                     packets based on source/destination, aggregating
 *                     packets, dropping packets, etc.
 * \author
 *         Michael Baddeley <m.baddeley@bristol.ac.uk>
 */
#include "contiki.h"
#include "contiki-net.h"
#include "net/ip/uip.h"
#include "net/ipv6/uip-icmp6.h"

#include "net/rpl/rpl.h"

#include "net/sdn/sdn.h"
#include "net/sdn/sdn-ft.h"
#include "net/sdn/sdn-conf.h"
#include "net/sdn/sdn-cd.h"
#include "net/sdn/sdn-packetbuf.h"

#include "net/sdn/usdn/usdn.h"

#if (SDN_CONTROLLER_TYPE == SDN_CONTROLLER_ATOM)
#include "atom.h"
#endif

/* Log configuration */
#include "sys/log-ng.h"
#define LOG_MODULE "SDN-D"
#define LOG_LEVEL LOG_LEVEL_SDN

/* Buffer packets while wating for controller instructions */
MEMB(sdn_pbuf_memb, sdn_bufpkt_t, SDN_PACKET_BUF_LEN);
LIST(sdn_pbuf_list);

/* UIP Send length for when we're clearing the out buffer */
extern uint16_t uip_slen;

/* uIPv6 Pointers */
#define UIP_BUF          ((uint8_t *)&uip_buf[UIP_LLH_LEN])
#define UIP_IP_BUF       ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])
#define UIP_ICMP_BUF     ((struct uip_icmp_hdr *)&uip_buf[uip_l2_l3_hdr_len])
#define UIP_UDP_BUF      ((struct uip_udp_hdr *)&uip_buf[uip_l2_l3_hdr_len])

#define UIP_USDN_PAYLOAD ((uint8_t *)&uip_buf[uip_l2_l3_udp_sdn_hdr_len])

#define UIP_EXT_HDR_OPT_RPL_BUF    ((struct uip_ext_hdr_opt_rpl *)&uip_buf[uip_l2_l3_hdr_len])

/*---------------------------------------------------------------------------*/
/* Engine Funtions */
/*---------------------------------------------------------------------------*/
void
usdn_add_fwd_on_src(uip_ipaddr_t *src, uip_ipaddr_t *nexthop) {
  sdn_ft_match_rule_t *m = sdn_ft_create_match(EQ,
                                               uip_src_index,
                                               sizeof(uip_ipaddr_t),
                                               0,
                                               src);
  sdn_ft_action_rule_t *a = sdn_ft_create_action(SDN_FT_ACTION_FORWARD,
                                                 0,
                                                 sizeof(uip_ipaddr_t),
                                                 nexthop);
  /* If we match on the src address, we perform the forwarding action. Index
     of the action is 0 as the forwarding action will give us the nexthop, not
     modify the datagram */
  LOG_DBG("Adding FWD on SRC entry to FLOWTABLE\n");
  sdn_ft_create_entry(FLOWTABLE, m, a, SDN_CONF.ft_lifetime, 0);
}

/*---------------------------------------------------------------------------*/
void
usdn_add_fwd_on_dest(uip_ipaddr_t *dest, uip_ipaddr_t *nexthop) {
  sdn_ft_match_rule_t *m = sdn_ft_create_match(EQ,
                                               uip_dst_index,
                                               sizeof(uip_ipaddr_t),
                                               0,
                                               dest);
  sdn_ft_action_rule_t *a = sdn_ft_create_action(SDN_FT_ACTION_FORWARD,
                                                 uip_dst_index,
                                                 sizeof(uip_ipaddr_t),
                                                 nexthop);
  /* If we match on the dest address, we perform a forwarding action. Index
     of the action is 0 as the forwarding action will give us the nexthop, not
     modify the datagram */
  LOG_DBG("Adding FWD on DEST entry to FLOWTABLE\n");
  sdn_ft_create_entry(FLOWTABLE, m, a, SDN_CONF.ft_lifetime, 0);
}

/*---------------------------------------------------------------------------*/
void
usdn_add_fwd_on_dest_infinite(uip_ipaddr_t *dest, uip_ipaddr_t *nexthop) {
  sdn_ft_match_rule_t *m = sdn_ft_create_match(EQ,
                                               uip_dst_index,
                                               sizeof(uip_ipaddr_t),
                                               0,
                                               dest);
  sdn_ft_action_rule_t *a = sdn_ft_create_action(SDN_FT_ACTION_FORWARD,
                                                 uip_dst_index,
                                                 sizeof(uip_ipaddr_t),
                                                 nexthop);
  LOG_DBG("Adding FWD on DEST (INFINITE) entry to FLOWTABLE\n");
  sdn_ft_create_entry(FLOWTABLE, m, a, SDN_FT_INFINITE_LIFETIME, 0);
}

/*---------------------------------------------------------------------------*/
void
usdn_add_fwd_on_flow(flowtable_id_t id, uint8_t flow, uip_ipaddr_t *nexthop) {
  sdn_ft_match_rule_t *m = sdn_ft_create_match(EQ,
                                               usdn_flow_index,
                                               sizeof(uint8_t),
                                               1, /* need ext_len */
                                               &flow);
  sdn_ft_action_rule_t *a = sdn_ft_create_action(SDN_FT_ACTION_FORWARD,
                                                 uip_dst_index,
                                                 sizeof(uip_ipaddr_t),
                                                 nexthop);
  LOG_DBG("Adding FWD on FLOW to FLOWTABLE\n");
  sdn_ft_create_entry(id, m, a, SDN_CONF.ft_lifetime, 0);
}

/*---------------------------------------------------------------------------*/
void
usdn_add_srh_on_dest(flowtable_id_t id, uip_ipaddr_t *dest, sdn_node_id_t *path, uint8_t len) {
  /* Populate the route struct */
  sdn_srh_route_t route;
  route.cmpr = 15;
  route.length = len;
  memcpy(&route.nodes, path, len);
  /* Work out the length of the route data */
  uint8_t length = sizeof(route.cmpr) + sizeof(route.length) +
                   (route.length * sizeof(sdn_node_id_t));
  sdn_ft_match_rule_t *m = sdn_ft_create_match(EQ,
                                               uip_dst_index,
                                               sizeof(uip_ipaddr_t),
                                               0,
                                               dest);
  sdn_ft_action_rule_t *a = sdn_ft_create_action(SDN_FT_ACTION_SRH,
                                                 0,
                                                 length,
                                                 &route);

  LOG_DBG("Adding SRH on DEST entry to FLOWTABLE\n");
  sdn_ft_create_entry(id, m, a, SDN_CONF.ft_lifetime, 0);
}

/*---------------------------------------------------------------------------*/
void
usdn_add_fallback_on_dest(flowtable_id_t id, uip_ipaddr_t *dest) {
  sdn_ft_match_rule_t *m = sdn_ft_create_match(EQ,
                                               uip_dst_index,
                                               sizeof(uip_ipaddr_t),
                                               0,
                                               dest);
  sdn_ft_action_rule_t *a = sdn_ft_create_action(SDN_FT_ACTION_FALLBACK, 0, 0, NULL);
  /* If we match on the dest address, we send to the fallback interface */
  LOG_DBG("Adding FALLBACK on DEST entry to FLOWTABLE\n");
  sdn_ft_create_entry(id, m, a, SDN_FT_INFINITE_LIFETIME, 0);
}

/*---------------------------------------------------------------------------*/
void
usdn_add_accept_on_src(flowtable_id_t id, uip_ipaddr_t *src) {
  sdn_ft_match_rule_t *m = sdn_ft_create_match(EQ,
                                               uip_src_index,
                                               sizeof(uip_ipaddr_t),
                                               0,
                                               src);
  sdn_ft_action_rule_t *a = sdn_ft_create_action(SDN_FT_ACTION_ACCEPT, 0, 0, NULL);
  /* If we match on the dest address, we deliver the packet to upper layers */
  LOG_DBG("Adding ACCEPT on SRC entry to FLOWTABLE\n");
  sdn_ft_create_entry(id, m, a, SDN_FT_INFINITE_LIFETIME, 0);
}

/*---------------------------------------------------------------------------*/
void
usdn_add_accept_on_dest(flowtable_id_t id, uip_ipaddr_t *dest) {
  sdn_ft_match_rule_t *m = sdn_ft_create_match(EQ,
                                               uip_dst_index,
                                               sizeof(uip_ipaddr_t),
                                               0,
                                               dest);
  sdn_ft_action_rule_t *a = sdn_ft_create_action(SDN_FT_ACTION_ACCEPT, 0, 0, NULL);
  /* If we match on the dest address, we deliver the packet to upper layers */
  LOG_DBG("Adding ACCEPT on DEST entry to FLOWTABLE\n");
  sdn_ft_create_entry(id, m, a, SDN_FT_INFINITE_LIFETIME, 0);
}

/*---------------------------------------------------------------------------*/
void
usdn_add_accept_on_icmp6_type(flowtable_id_t id, uint8_t type) {
  sdn_ft_match_rule_t *m = sdn_ft_create_match(EQ,
                                               uip_icmp_type_index,
                                               sizeof(uint8_t),
                                               1,
                                               &type);
  sdn_ft_action_rule_t *a = sdn_ft_create_action(SDN_FT_ACTION_ACCEPT, 0, 0, NULL);

  LOG_DBG("Adding ACCEPT on ICMP6_TYPE (inf lftime) to FLOWTABLE\n");
  sdn_ft_create_entry(id, m, a, SDN_FT_INFINITE_LIFETIME, 0);
}

/*---------------------------------------------------------------------------*/
void
usdn_add_do_callback_on_dest(flowtable_id_t id,
                             uip_ipaddr_t *dest,
                             sdn_ft_callback_action_ipaddr_t callback){
  sdn_ft_match_rule_t *m = sdn_ft_create_match(EQ,
                                               uip_dst_index,
                                               sizeof(uip_ipaddr_t),
                                               0,
                                               dest);
  sdn_ft_action_rule_t *a = sdn_ft_create_action(SDN_FT_ACTION_CALLBACK,
                                                 0,
                                                 sizeof(void*),
                                                 callback);
  /* If we match on the dest address, we deliver the packet to upper layers */
  LOG_DBG("Adding CALLBACK on DEST entry to FLOWTABLE\n");
  sdn_ft_create_entry(id, m, a, SDN_FT_INFINITE_LIFETIME, 0);
}

/*---------------------------------------------------------------------------*/
/* Memory Functions */
/*---------------------------------------------------------------------------*/
static void
copy_buf_packet_to_uip(sdn_bufpkt_t *p) {
  /* Copy over the packet buffer onto the sdn buf */
  LOG_DBG("COPY packet (len=%d) (ext=%d) to uip_buf (len=%d) (ext=%d)",
    sdn_pbuf_buflen(p), sdn_pbuf_extlen(p), uip_len, uip_ext_len);
  uip_len = sdn_pbuf_buflen(p);
  uip_ext_len = sdn_pbuf_extlen(p);
  memcpy(&uip_buf, sdn_pbuf_buf(p), uip_len);
}

/*---------------------------------------------------------------------------*/
/* Action Handler Functions */
/*---------------------------------------------------------------------------*/
/* Performs the SDN forwrding operation. N.B. If nh_nbr is NULL then
   Network Engine will BROADCAST */
static void
sdn_fwd(uip_ds6_nbr_t *nbr)
{

  // If we aren't the source, then decrement ttl
  if(uip_ds6_addr_lookup(&UIP_IP_BUF->srcipaddr) == NULL) {
    UIP_IP_BUF->ttl--;
  }

  /* See if there is a srh nexthop */
  uip_ipaddr_t *nexthop = NULL;
  if(uip_ext_len > 0) {
    LOG_DBG("FORWARD ext_length is %d\n", uip_ext_len);
    rpl_process_srh_header();
    uip_ipaddr_t ipaddr;
    if(rpl_srh_get_next_hop(&ipaddr)) {
      nexthop = &ipaddr;
      nbr = uip_ds6_nbr_lookup(nexthop);
      if (nbr == NULL) {
        LOG_ERR("FORWARD Could not find NBR\n");
        return;
      }
    }
  }

  LOG_DBG("Forwarding packet from [");
  LOG_DBG_6ADDR(&UIP_IP_BUF->srcipaddr);
  LOG_DBG_("] to [");
  LOG_DBG_6ADDR(&nbr->ipaddr);
  LOG_DBG_("]\n");

  const uip_lladdr_t *localdest = uip_ds6_nbr_get_ll(nbr);
  if (localdest == NULL) {
    LOG_WARN("FORWARD localdest is null\n");
  }
  tcpip_output(uip_ds6_nbr_get_ll(nbr));
}

/*---------------------------------------------------------------------------*/
static void
sdn_fallback(void) {
#if (SDN_CONTROLLER_TYPE == SDN_CONTROLLER_ATOM)
    atom_post(&sb_usdn);
#else
    LOG_ERR("We are not configured as a SDN controller node\n");
#endif /* (SDN_CONTROLLER_TYPE == SDN_CONTROLLER_ATOM) */
}

/*---------------------------------------------------------------------------*/
static sdn_bufpkt_t *
buffer_packet(void) {
  sdn_bufpkt_t *p = NULL;
  /* Buffer the packet currently in the sdn_buf. Set new lifetimer. */
  p = sdn_pbuf_allocate(&sdn_pbuf_memb, sdn_pbuf_list,
                        SDN_PACKETBUF_LIFETIME, NULL);
  if(p != NULL) {
    LOG_ANNOTATE("#A p=%d/%d\n", list_length(sdn_pbuf_list), SDN_PACKET_BUF_LEN);
    sdn_pbuf_set(p, UIP_BUF, uip_len, uip_ext_len);
    LOG_DBG("Packet added to buffer (id=%d)", p->id);
    LOG_DBG_(" dest was [");
    LOG_DBG_6ADDR(&UIP_IP_BUF->destipaddr);
    LOG_DBG_("]\n");
  } else {
    LOG_ERR("Failed to BUFFER packet\n");
    return NULL;
  }
  return p;
}

/*---------------------------------------------------------------------------*/
sdn_bufpkt_t *
sdn_query(void) {
  sdn_bufpkt_t *p = NULL;

/* Buffer the packet currently in the sdn_buf so we can retry when we get
   a response from the controller */
  p = buffer_packet();

  /* Check if we are querying the full packet, or only part of it */
  if (!SDN_CONF.query_full) {
    sdn_bufpkt_t temp;
    if(p != NULL) {
      /* Set the id to the same as the buf packet if we want to retry it*/
      temp.id = p->id;
    } else {
      temp.id = 0;
    }

    //FIXME: Need to put this in conf
    /* sdn-packetbuf doesn't use the Link Layer Header, so we need to offset */
    sdn_pbuf_set(&temp,
                 UIP_BUF + (SDN_CONF.query_idx - UIP_LLH_LEN),
                 SDN_CONF.query_len, uip_ext_len);
    // PRINTFNC(TRACE_LEVEL, print_sdn_pbuf_packet(&temp, SDN_PB_PRINT_DFLT));
    /* Send a query to the controller */
    LOG_DBG("QUERY send query with tx_id (%d)\n", temp.id);
    SDN_ENGINE.controller_query(&temp);
  } else {
    /* Send a query to the controller */
    LOG_DBG("QUERY send query with tx_id (%d)\n", p->id);
    SDN_ENGINE.controller_query(p);
  }

  return p;
}

/*---------------------------------------------------------------------------*/
static int
action_handler(sdn_ft_action_rule_t *action_rule, uint8_t *data)
{
  uip_ds6_nbr_t *nbr = NULL;

  /* If there is no entry then assume the uip stack knows how to process
     the packet, so accept it */
  if(action_rule == NULL) {
    goto accept;
  }

  /* Check what action we should be performing*/
  switch (action_rule->action) {
    case SDN_FT_ACTION_ACCEPT:
      goto accept;
    case SDN_FT_ACTION_DROP:
      goto drop;
    case SDN_FT_ACTION_QUERY:
      goto query;
    case SDN_FT_ACTION_FORWARD:
      goto forward;
    case SDN_FT_ACTION_MODIFY:
      goto modify;
    case SDN_FT_ACTION_FALLBACK:
      goto fallback;
    case SDN_FT_ACTION_SRH:
      goto srh;
    case SDN_FT_ACTION_CALLBACK:
      goto callback;
    default:
      goto accept;
  }

  /*  Controller */
  query:
    LOG_DBG("ACTION_HANDLER Controller...\n");
    // TODO:  Query Controller
    goto drop;

  /* Forward to Neighbor */
  forward:
    LOG_DBG("ACTION_HANDLER Forwarding ...\n");
    /* Get the forwarding addr from the action and check to see if we have
       that neighbor */
    // TODO: Should we check uip_ds6_is_addr_onlink?
    nbr = uip_ds6_nbr_lookup((uip_ipaddr_t *)action_rule->data);
    if(nbr != NULL) {
      sdn_fwd(nbr);
      goto drop;
    }
    LOG_ERR("ACTION_HANDLER No ds6 nbr!\n");
    goto drop;

  /* Modify the packet */
  modify:
    LOG_DBG("ACTION_HANDLER Modify packet...\n");
    // TODO: Modify
    goto accept;

  /* Send to the fallback interface */
  fallback:
    LOG_DBG("ACTION_HANDLER Send packet to FALLBACK...\n");
    sdn_fallback();
    goto drop;

  /* Insert Source Routing Header (SRH) */
  srh:
    LOG_DBG("ACTION_HANDLER Insert SRH...\n");
    sdn_srh_route_t srh;
    sdn_get_action_data_srh(action_rule, &srh);
    sdn_ext_insert_srh(&srh);
    sdn_fwd(NULL);
    goto drop;

  callback:
    LOG_DBG("ACTION_HANDLER Do CALLBACK FUNCTION...\n");
    ((sdn_ft_callback_action_ipaddr_t)(action_rule->data))(&UIP_IP_BUF->destipaddr);

  /* We haven't dropped it, so pass it on to upper uip_process layers */
  accept:
    LOG_DBG("ACTION_HANDLER Pass back to UIP, ACCEPT!\n");
    return UIP_ACCEPT;

  /* Drop the packet */
  drop:
    LOG_DBG("ACTION_HANDLER Finished processing, DROP!\n");
    LOG_DBG("CLEAR uip_buf (len=%d)\n", uip_len);
    uip_clear_buf();
    return UIP_DROP;

}

/*---------------------------------------------------------------------------*/
/* SDN Driver */
/*---------------------------------------------------------------------------*/
static void
init()
{
  /* Resgister the action handler */
  sdn_ft_register_action_handler(action_handler);
}

/*---------------------------------------------------------------------------*/
// FIXME: No longer used as we are checking in uip6 and uip-udp-packet.
//        However! We should check to see how much time this actually adds on
//        as it's easier to read and manage when we are in here.
// static sdn_driver_response_t
// check_whitelist()
// {
//   LOG_DBG("WHITELIST\n");
//
//   /* Destination = MY_ADDRESS */
//   if(uip_ds6_is_my_addr(&UIP_IP_BUF->destipaddr)) {
//     LOG_DBG("WHITELIST This is for me, deliver to upper layers (ACCEPT)\n");
//     return UIP_ACCEPT;
//   }
//
// #if (SDN_CONTROLLER_TYPE != SDN_CONTROLLER_NONE)
//   /* Only do this check if we are the controller. Need to copy uip_buf to
//      sdn_buf as the sdn_fallback() function assumes we are working from the
//      sdn_buf. */
//   if(uip_ipaddr_cmp(&UIP_IP_BUF->destipaddr, &DEFAULT_CONTROLLER->ipaddr)) {
//     LOG_DBG("We are the controller and the dest is for us. FALLBACK + DROP\n");
//     sdn_fallback();
//     return UIP_DROP;
//   }
// #else
//   /* Only do this check if we are NOT the controller */
//   if(uip_ipaddr_cmp(&UIP_IP_BUF->destipaddr, &DEFAULT_CONTROLLER->ipaddr) ||
//      uip_ipaddr_cmp(&UIP_IP_BUF->srcipaddr, &DEFAULT_CONTROLLER->ipaddr)) {
//     LOG_DBG("WHITELIST Src/Dest is controller, deliver to upper layers (ACCEPT)\n");
//     return UIP_ACCEPT;
//   }
// #endif
//
//   /* Destination = MULTICAST */
//   if(uip_ds6_is_my_maddr(&UIP_IP_BUF->destipaddr)) {
//     LOG_DBG("WHITELIST Dest is mc, deliver to upper layers (ACCEPT)\n");
//     return UIP_ACCEPT;
//   }
//   /* else continue */
//   return SDN_CONTINUE;
// }

/*---------------------------------------------------------------------------*/
static uint8_t
process(uint8_t flag) {
  uint8_t result;

  /* Check SDN controller connection */
  if(!sdn_connected()) {
    return UIP_ACCEPT;
  }

  /* Check why we are looking at the flowtables */
  switch(flag) {
    case RPL_SRH:
      goto srh;
    case SDN_UIP:
      goto uip;
    case SDN_UDP:
      goto send;
    default:
      LOG_ERR("Unknown flag, return UIP_DROP\n");
      return UIP_DROP;
  }

srh:
  LOG_DBG("Checking srh...\n");
  result = sdn_ft_check(WHITELIST, &uip_buf, uip_len, uip_ext_len);
  if(result == SDN_NO_MATCH) {
    LOG_DBG("No match! Return to upper layers (ACCEPT).\n");
    return UIP_ACCEPT;
  } else {
    LOG_DBG("Match! Do something...\n");
  }

uip:
  /* Check whitelist flowtable (i.e. for RPL DAO) */
  LOG_DBG("Checking whitelist...\n");
  result = sdn_ft_check(WHITELIST, &uip_buf, uip_len, uip_ext_len);
  if(result != SDN_CONTINUE && result != SDN_NO_MATCH) {
    return UIP_ACCEPT;
  }

send:
  /* Check default entry */
  LOG_DBG("Checking default rule...\n");
  result = sdn_ft_check_default(&uip_buf, uip_len, uip_ext_len);
  if(result != SDN_NO_MATCH) {
    return result;
  }
  /* Check if we have an entry in the flowtable for this packet */
  LOG_DBG("Checking flowtable rules...\n");
  result = sdn_ft_check(FLOWTABLE, &uip_buf, uip_len, uip_ext_len);
  switch(result) {
    case UIP_DROP:
      /* If ft_result returns DROP then we have already dealt with it */
      LOG_DBG("Match! No further processing of the datagram (DROP)\n");
      /* We set the uip_len = 0 to stop the core from resending it */
      return result;
    case UIP_ACCEPT:
      LOG_DBG("Match! However there may still be further processing. Return to upper layers (ACCEPT)\n");
      return result;
    case SDN_NO_MATCH:
      if(uip_ds6_is_addr_onlink(&UIP_IP_BUF->destipaddr)) {
          LOG_DBG("No match! But dest is onlink, so we should return to upper layers (ACCEPT).\n");
          return UIP_ACCEPT;
      } else {
        LOG_DBG("No match! Dest is NOT onlink. Query controller (BUFFER)\n");
        sdn_query();
        return UIP_DROP;
      }
    default:
      LOG_ERR("Unknown FT result. Drop datagram and clear buffers (DROP)\n");
      /* We set the uip_len = 0 to stop the core from resending it */
      // sdn_clear_buf();
      LOG_ERR("CLEAR uip_buf (len=%d)\n", uip_len);
      uip_clear_buf();
      return UIP_DROP;
  }
}


/*---------------------------------------------------------------------------*/
static uint8_t
retry(uint16_t flow)
{
  sdn_bufpkt_t *p;
  uint8_t state = UIP_DROP;

  LOG_DBG("RETRY Re-attempt packets with flow id [%d] \n", flow);
  for(p = list_head(sdn_pbuf_list); p != NULL; p = list_item_next(p)) {
    if(p->id == flow) {
      /* Copy the buffered packet back onto the uip_buf */
      copy_buf_packet_to_uip(p);

      // TODO: Surely any FT match would apply to all buffered packets with the same ID?
      /* Check if we have an entry in the flowtable for this packet */
      state = sdn_ft_check(FLOWTABLE, &uip_buf, uip_len, uip_ext_len);

      switch(state) {
        case UIP_ACCEPT:
          LOG_DBG("RETRY Pass on to UIP\n");
          break;
        case SDN_NO_MATCH:
          LOG_DBG("RETRY No match (FREE)\n");
          break;
        case UIP_DROP:
          LOG_DBG("RETRY No further processing (FREE)\n");
          break;
      }
      sdn_pbuf_free(p);
      LOG_ANNOTATE("#A p=%d/%d\n\n", list_length(sdn_pbuf_list), SDN_PACKET_BUF_LEN);
    }
  }

  LOG_DBG("CLEAR uip_buf (len=%d)\n", uip_len);
  uip_clear_buf();
  return state;
}


/*---------------------------------------------------------------------------*/
const struct uip_sdn_driver usdn_driver = {
  init,
  process,
  retry,
  usdn_add_fwd_on_dest,
  usdn_add_fwd_on_flow,
  usdn_add_fallback_on_dest,
  usdn_add_srh_on_dest,
  usdn_add_accept_on_src,
  usdn_add_accept_on_dest,
  usdn_add_accept_on_icmp6_type,
  usdn_add_do_callback_on_dest
};
