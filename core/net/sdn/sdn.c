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
 *         uSDN: A low-overhead SDN stack and embedded SDN controller for Contiki.
 *         See "Evolving SDN for Low-Power IoT Networks", IEEE NetSoft'18
 * \author
 *         Michael Baddeley <m.baddeley@bristol.ac.uk>
 */
#include "lib/memb.h"
#include "lib/list.h"

#include "net/ip/uip.h"
#include "net/ip/tcpip.h"
#include "net/ipv6/uip-icmp6.h"
#include "net/link-stats.h"

#include "sdn.h"
#include "sdn-conf.h"
#include "sdn-cd.h"
#include "sdn-ft.h"

#include "net/rpl/rpl-private.h"
#include "net/rpl/rpl-dag-root.h"

/* Log configuration */
#include "sys/log-ng.h"
#define LOG_MODULE "SDN"
#define LOG_LEVEL LOG_LEVEL_SDN

#if SDN_CONF_STATS
sdn_stats_t sdn_stats;
#endif /* UIP_SDN_STATS */

// TODO: Move the controller tables here and attach a SDN process to them. If
//       we turn this into a process we can also move the init() function for
//       the sdn engine to here, rather than in uip6.c.  Controller Discovery
//       then purely provides functions for the discovery and Maintainance of
//       controllers, while sdn.c hangles the controller structures plus their
//       connection applications.

/*---------------------------------------------------------------------------*/
/* Intialisation */
/*---------------------------------------------------------------------------*/
void
sdn_init(void)
{
  LOG_INFO("Initialising SDN Core Processes...\n");
  /* Initialise SDN Configuration */
  sdn_conf_init();
  LOG_INFO("SDN-CONF initialised!\n");
  /* Initialise Flowtable */
  sdn_ft_init();
  LOG_INFO("SDN-FT initialised!\n");
  /* Initialise Controller Discovery/Maintainance */
  sdn_cd_init();
  LOG_INFO("SDN-CD initialised!\n");
  /* Add sdn multicast address */
  uip_ipaddr_t sdnmaddr;
  uip_create_linklocal_sdnnodes_mcast(&sdnmaddr);
  uip_ds6_maddr_add(&sdnmaddr);
  /* Initialise DRIVER */
  SDN_DRIVER.init();
  LOG_INFO("USDN-DRIVER initialised!\n");
  /* Initialise ENGINE */
  SDN_ENGINE.init();
  LOG_INFO("USDN-ENGINE initialised!\n");
  /* Initialise ADAPTER */
  SDN_ADAPTER.init();
  LOG_INFO("USDN-ADAPTER initialised!\n");

  /* Initialise DEFAULT_CONTROLLER */
  sdn_cd_init_default_controller();
  LOG_INFO("DEFAULT_CONTROLLER initialised!\n");

  /* Initialise statistics */
#if SDN_CONF_STATS
  memset(&sdn_stats, 0, sizeof(sdn_stats));
#endif
}

/*---------------------------------------------------------------------------*/
/* Flowtable Data Conversion */
/*---------------------------------------------------------------------------*/
void
sdn_get_action_data_srh(sdn_ft_action_rule_t *rule, sdn_srh_route_t *route)
{
  int pos = 0;
  if(rule->action == SDN_FT_ACTION_SRH) {
    /* Get the first few fields */
    route->cmpr = *(uint8_t *)rule->data;           pos += sizeof(uint8_t);
    route->length = *(uint8_t *)(rule->data + pos); pos += sizeof(uint8_t);
    /* Set the node id data */
    memcpy(&route->nodes, (rule->data + pos), (route->length * sizeof(sdn_node_id_t)));
  } else {
    LOG_ERR("Action rule type not correct, cannot convert data!\n");
  }
}

/*---------------------------------------------------------------------------*/
/* SDN Controller State */
/*---------------------------------------------------------------------------*/
uint8_t
sdn_connected(void)
{
  ctrl_state_t s = DEFAULT_CONTROLLER->state;
  /* We only like fresh connections */
  if(s == CTRL_CONNECTED) {
    return 1;
  }
  return 0;
}

/*---------------------------------------------------------------------------*/
uint8_t
sdn_is_ctrl_addr(uip_ipaddr_t *addr)
{
  if(DEFAULT_CONTROLLER != NULL) {
    return uip_ipaddr_cmp(&DEFAULT_CONTROLLER->ipaddr, addr);
  } else {
    return 0;
  }
}

/*---------------------------------------------------------------------------*/
/* Local Network State */
/*---------------------------------------------------------------------------*/
sdn_node_id_t
sdn_node_id_from_ipaddr(uip_ipaddr_t *ipaddr)
{
  sdn_node_id_t id;
  int offset = sizeof(ipaddr->u8) - sizeof(sdn_node_id_t);
  memcpy(&id, &ipaddr->u8[offset], sizeof(sdn_node_id_t));
  // FIXME: memcpy reverses the byte order of the id!!!!!
  if(sizeof(sdn_node_id_t) == 2)
    id = id >> 8;
  return id;
}

/*---------------------------------------------------------------------------*/
uint8_t
sdn_rpl_get_current_instance_rank(void)
{
  if(default_instance != NULL){
    if(rpl_dag_root_is_root()){
      return ROOT_RANK(default_instance); /* ROOT_RANK is 128 */
    }else if(default_instance->current_dag->preferred_parent != NULL){
      return default_instance->current_dag->preferred_parent->rank >> 8;
    } else {
      LOG_ERR("get_rank preferred_parent was NULL!\n");
    }
  }
  LOG_ERR("get_rank default_instance was NULL!\n");
  return 0;
}

/*---------------------------------------------------------------------------*/
const struct link_stats *
sdn_get_nbr_link_stats(uip_ds6_nbr_t *nbr)
{
  const linkaddr_t *lladdr = nbr_table_get_lladdr(ds6_neighbors, nbr);
  return link_stats_from_lladdr(lladdr);
}

/*---------------------------------------------------------------------------*/
int
sdn_get_nbr_link_is_fresh(uip_ds6_nbr_t *nbr)
{
  const struct link_stats *stats = sdn_get_nbr_link_stats(nbr);
  return link_stats_is_fresh(stats);
}

/*---------------------------------------------------------------------------*/
void
print_sdn_srh_route(sdn_srh_route_t *route)
{
  int i;
  LOG_DBG("SDN SRH Route - cmpr:%u len:%d hops: [", route->cmpr, route->length);
  for(i = 0; i < SDN_CONF_MAX_ROUTE_LEN; i++) {
    LOG_DBG_("%02x:", route->nodes[i]);
  }
  LOG_DBG_("]\n");
}
