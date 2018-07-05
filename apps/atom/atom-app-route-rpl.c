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
 *         Atom SDN Controller: RPL routing application.
 * \author
 *         Michael Baddeley <m.baddeley@bristol.ac.uk>
 */
#include "net/ip/uip.h"
#include "net/ipv6/uip-ds6.h"

#include "net/rpl/rpl.h"
#include "net/rpl/rpl-ns.h"
#include "net/rpl/rpl-private.h"

#include "atom.h"

/* Log configuration */
#include "sys/log-ng.h"
#define LOG_MODULE "ATOM"
#define LOG_LEVEL LOG_LEVEL_ATOM

/*---------------------------------------------------------------------------*/
static int
count_matching_bytes(const void *p1, const void *p2, size_t n)
{
  int i = 0;
  for(i = 0; i < n; i++) {
    if(((uint8_t *)p1)[i] != ((uint8_t *)p2)[i]) {
      return i;
    }
  }
  return n;
}

/*---------------------------------------------------------------------------*/
/* Application API */
/*---------------------------------------------------------------------------*/
static void
init(void) {
  LOG_INFO("Atom RPL routing app initialised");
}

/*---------------------------------------------------------------------------*/
static atom_response_t *
run(void *data)
{
  uint8_t path_len, up_path_len, down_path_len;
  // uint8_t ext_len;
  uint8_t cmpri;
  // uint8_t cmpre; /* ComprI and ComprE fields of the RPL Source Routing Header */
  uint8_t *hop_ptr;
  rpl_dag_t *dag;
  rpl_ns_node_t *node, *src_node, *dest_node, *root_node;
  uip_ipaddr_t node_addr;

  /* Route for response */
  sdn_srh_route_t route = {0};

  /* Dereference the action data */
  atom_routing_action_t *action = (atom_routing_action_t *)data;

  /* Get the nodes from the net layer */
  uip_ipaddr_t *src = &action->src;
  uip_ipaddr_t *dest = &action->dest;

  /* Get link of the destination and src */
  dag = rpl_get_dag(dest);

  if(dag == NULL) {
    return NULL;
  }

  dest_node = rpl_ns_get_node(dag, dest);
  if(dest_node == NULL) {
    rpl_ns_print_nodelist(dag, dest);
    /* The destination is not found, skip SRH insertion */
    // PRINTF("SDN-EXT: SRH destination not found, skip SRH\n");
    return NULL;
  }

  root_node = rpl_ns_get_node(dag, &dag->dag_id);
  if(root_node == NULL) {
    rpl_ns_print_nodelist(dag, &dag->dag_id);
    // PRINTF("SDN-EXT: SRH root node not found\n");
    return NULL;
  }

  src_node = rpl_ns_get_node(dag, src);
  if(src_node == NULL) {
    rpl_ns_print_nodelist(dag,src);
    // PRINTF("SDN-EXT: SRH source node not found\n");
    return NULL;
  }

  if(!rpl_ns_is_node_reachable(dag, src)) {
    // PRINTF("SDN-EXT: SRH no path found to source\n");
    return NULL;
  }

  if(!rpl_ns_is_node_reachable(dag, dest)) {
    // PRINTF("SDN-EXT: SRH no path found to destination\n");
    return NULL;
  }

  /* Compute path length and compression factors (we use cmpri == cmpre) */
  path_len = up_path_len = down_path_len = 0;
  node = src_node->parent;
  /* For simplicity, we use cmpri = cmpre */
  cmpri = 15;
  // cmpre = 15;

  /* source to root */
  // PRINTF("SDN-EXT: Get Src to Root\n");
  while(node != NULL && node != root_node) {
    rpl_ns_get_node_global_addr(&node_addr, node);

    /* How many bytes in common between all nodes in the path? */
    cmpri = MIN(cmpri, count_matching_bytes(&node_addr, src, 16));
    // cmpre = cmpri;

    // PRINTF("SDN-EXT: SRH Hop ");
    // PRINT6ADDR(&node_addr);
    // PRINTF("\n");
    node = node->parent;
    path_len++;
    up_path_len++;
  }

  /* root node */
  // PRINTF("SDN-EXT: Root\n");
  node = root_node;
  rpl_ns_get_node_global_addr(&node_addr, node);
  cmpri = MIN(cmpri, count_matching_bytes(&node_addr, dest, 16));
  // cmpre = cmpri;
  // PRINTF("SDN-EXT: SRH Hop ");
  // PRINT6ADDR(&node_addr);
  // PRINTF("\n");
  path_len++;

  /* root to dest: need to get dest to root and then reverse it */
  node = dest_node->parent;
  // PRINTF("SDN-EXT: Get Dest to Root\n");
  while(node != NULL && node != root_node) {
    rpl_ns_get_node_global_addr(&node_addr, node);

    /* How many bytes in common between all nodes in the path? */
    cmpri = MIN(cmpri, count_matching_bytes(&node_addr, dest, 16));
    // cmpre = cmpri;

    // PRINTF("SDN-EXT: SRH Hop ");
    // PRINT6ADDR(&node_addr);
    // PRINTF("\n");
    node = node->parent;
    path_len++;
    down_path_len++;
  }

  // PRINTF("SDN-EXT: SRH path_len: %u up_len: %u down_len: %u\n",
          // path_len, up_path_len, down_path_len);

  /* Extension header length: fixed headers + (n-1) * (16-ComprI) + (16-ComprE)*/
  // ext_len = RPL_RH_LEN + RPL_SRH_LEN
  //     + (path_len - 1) * (16 - cmpre)
  //     + (16 - cmpri);

  /* Start at element 0 of the route */
  hop_ptr = (uint8_t *)route.nodes;
  /* Do route from src to root */
  node = src_node;
  while(node != NULL) {
    rpl_ns_get_node_global_addr(&node_addr, node);
    memcpy(hop_ptr, ((uint8_t*)&node_addr) + cmpri, 16 - cmpri);
    hop_ptr += sizeof(sdn_node_id_t);
    node = node->parent;
  }
  /* Do route from dest to root (reverse, and miss out root) */
  node = dest_node;
  /* start from last element and work backwards */
  hop_ptr += (down_path_len * sizeof(sdn_node_id_t));
  while(node != NULL && node != root_node) {
    rpl_ns_get_node_global_addr(&node_addr, node);
    memcpy(hop_ptr, ((uint8_t*)&node_addr) + cmpri, 16 - cmpri);
    hop_ptr -= sizeof(sdn_node_id_t);
    node = node->parent;
  }

  /* Set the route */
  route.cmpr = cmpri;
  route.length = path_len + 1;

  LOG_INFO("Found RPL NS route\n");
  // PRINTFNC(TRACE_LEVEL, print_sdn_srh_route(&route));

  /* Return the response */
  return atom_response_buf_copy_to(ATOM_RESPONSE_ROUTING, &route);
}

/*---------------------------------------------------------------------------*/
/* Application instance */
/*---------------------------------------------------------------------------*/
struct atom_app app_route_rpl = {
  "RPL Routing",
  ATOM_ACTION_ROUTING,
  init,
  run
};
