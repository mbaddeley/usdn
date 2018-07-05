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
 *         Atom SDN Controller: Keeps tracks of and abstracts the network state
 *                              for the contrller.
 * \author
 *         Michael Baddeley <m.baddeley@bristol.ac.uk>
 */
#include <stdio.h>
#include <string.h>

#include "sys/clock.h"
#include "lib/memb.h"
#include "lib/list.h"

#include "contiki.h"
#include "net/ip/uip.h"

#include "net/sdn/sdn.h"

#include "atom.h"

/* Log configuration */
#include "sys/log-ng.h"
#define LOG_MODULE "ATOM"
#define LOG_LEVEL LOG_LEVEL_ATOM

#undef LOG_WITH_ANNOTATE
#define LOG_WITH_ANNOTATE 1

/*---------------------------------------------------------------------------*/
LIST(nodes);
MEMB(nodes_memb, atom_node_t, ATOM_MAX_NODES);
/*---------------------------------------------------------------------------*/
void
atom_net_init(void)
{
  list_init(nodes);
  memb_init(&nodes_memb);

  LOG_INFO("Atom net initialised\n");
}

/*---------------------------------------------------------------------------*/
/* Memory Management */
/*---------------------------------------------------------------------------*/
static atom_node_t *
node_allocate(void)
{
  atom_node_t *n;
  n = memb_alloc(&nodes_memb);
  if(n == NULL) {
    LOG_ERR("FAILED to allocate a node!\n");
    return NULL;
  }
  LOG_ANNOTATE("#A n=%d/%d\n", list_length(nodes), ATOM_MAX_NODES);
  memset(n, 0, sizeof(atom_node_t));
  return n;
}

/*----------------------------------------------------------------------------*/
// static void
// node_free(atom_node_t *n)
// {
//   list_remove(nodes, n);
//   int res = memb_free(&nodes_memb, n);
//   if(res !=0) {
//     LOG_ERR("FAILED to free a node!\n");
//   }
// LOG_ANNOTATE("#A n=%d/%d\n", list_length(nodes), ATOM_MAX_NODES);
// }

/*---------------------------------------------------------------------------*/
/* Static functions */
/*---------------------------------------------------------------------------*/
static atom_node_t *
node_add(uip_ipaddr_t *ipaddr)
{
  atom_node_t *n;
  n = node_allocate();
  if(n != NULL) {
    /* Set node ipaddr and id */
    uip_ipaddr_copy(&n->ipaddr, ipaddr);
    n->id = sdn_node_id_from_ipaddr(ipaddr);
    /* Set NOT_CONFIGURED */ // TODO: Should have a list of "MODES" which we can configure (low pwr etc)
    n->cfg_id = 0;
    /* Add to node list */
    list_add(nodes, n);
    LOG_DBG("Added node [%d] from IP [", n->id);
    LOG_DBG_6ADDR(&n->ipaddr);
    LOG_DBG_("]\n");
    return n;
  }
  LOG_ERR("FAILED to add a node [");
  LOG_ERR_6ADDR(&n->ipaddr);
  LOG_ERR_("]\n");
  return n;
}

/*---------------------------------------------------------------------------*/
static atom_node_t *
node_add_id(sdn_node_id_t id)
{
  atom_node_t *n;
  n = node_allocate();
  if(n != NULL) {
    /* Set node id */
    n->id = id;
    /* Add to node list */
    list_add(nodes, n);
    LOG_DBG("Added node id [%d]\n", n->id);
    return n;
  }
  LOG_ERR("FAILED to add a node!\n");
  return n;
}

static atom_link_t *
link_exists(atom_node_t *src, atom_node_t *dest)
{
  int i = 0;
  if(src != NULL && dest != NULL) {
    LOG_DBG("Check link (%d->%d)\n", src->id, dest->id);
    if(src->num_links <= ATOM_MAX_LINKS_PER_NODE) {
      for(i = 0; i < src->num_links; i++) {
        if(src->links[i].dest_id == dest->id) {
          return &src->links[i];
        }
      }
      LOG_DBG("Link (%d->%d) does not exist!\n", src->id, dest->id);
    } else {
      LOG_ERR("We have more links than we should!\n");
    }
  }
  /* Link does not exist */
  return NULL;
}

/*---------------------------------------------------------------------------*/
static atom_link_t *
link_add(atom_node_t *src, atom_node_t *dest)
{
  int eol; /*End of List*/
  if((src != NULL) && (dest != NULL)) {
    eol = src->num_links;
    /* Set link destination id */
    src->links[eol].dest_id = dest->id;
    src->num_links++;
    return &src->links[eol];
  }
  LOG_ERR("FAILED to add a link!\n");
  return NULL;
}

/*---------------------------------------------------------------------------*/
/* Atom net API */
/*---------------------------------------------------------------------------*/
atom_node_t *
atom_net_get_node_ipaddr(uip_ipaddr_t *ipaddr)
{
  atom_node_t *n;

  if(ipaddr != NULL) {
    for(n = list_head(nodes); n != NULL; n = list_item_next(n)) {
      if(&n->ipaddr != NULL && uip_ipaddr_cmp(&n->ipaddr, ipaddr)) {
        return n;
      }
    }
    LOG_WARN("Node [%d] does not exist for ipaddr [", ipaddr->u8[15]);
    LOG_WARN_6ADDR(ipaddr);
    LOG_WARN_("]\n");

    print_network_graph();
  } else {
    LOG_ERR( "ipaddr is NULL\n");
  }
  return NULL;
}

/*---------------------------------------------------------------------------*/
atom_node_t *
atom_net_get_node_id(sdn_node_id_t id)
{
  atom_node_t *n;

  for(n = list_head(nodes); n != NULL; n = list_item_next(n)) {
    if (n->id == id) {
      return n;
    }
  }
  LOG_DBG("Node id [%d] does not exist\n", id);
  return NULL;
}

/*---------------------------------------------------------------------------*/
atom_node_t *
atom_net_node_heartbeat(uip_ipaddr_t *ipaddr)
{
  atom_node_t *node;
  sdn_node_id_t id = sdn_node_id_from_ipaddr(ipaddr);
  /* See if we have a node with this id */
  if((node = atom_net_get_node_id(id)) == NULL) {
    /* Add node using ip address */
    node = node_add(ipaddr);
  } else {
    /* Check if this node is associated with this ip address */
    if(!uip_ipaddr_cmp(&node->ipaddr, &ipaddr)) {
      /* Node does not have this ip address, copy over */
      uip_ipaddr_copy(&node->ipaddr, ipaddr);
    }
  }
  LOG_DBG( "Updated node [%d] lifetimer \n", id);
  return  node;
}

/*---------------------------------------------------------------------------*/
atom_node_t *
atom_net_node_update(uip_ipaddr_t *ipaddr, uint8_t cfg_id, uint8_t rank)
{
  atom_node_t *node;
  sdn_node_id_t id = sdn_node_id_from_ipaddr(ipaddr);
  /* See if we have a node entry with id */
  if((node = atom_net_get_node_id(id)) == NULL) {
    /* Add node using ip address */
    node = node_add(ipaddr);
  } else {
    /* Check if this node is associated with this ip address */
    if(!uip_ipaddr_cmp(&node->ipaddr, &ipaddr)) {
      /* Node does not have this ip address, copy over */
      uip_ipaddr_copy(&node->ipaddr, ipaddr);
    }
  }
  /* Node should now exist. Update the node information */
  node->cfg_id = cfg_id;
  node->rank = rank;
  LOG_DBG("Updated node [%d] : [", id);
  LOG_DBG_6ADDR(ipaddr);
  LOG_DBG_("]\n");
  return node;
}

/*---------------------------------------------------------------------------*/
atom_link_t *
atom_net_link_update(atom_node_t *src, sdn_node_id_t dest_id, int16_t rssi)
{
  atom_node_t *dest;
  atom_link_t *l;
  if(src != NULL) {
    if((dest = atom_net_get_node_id(dest_id)) == NULL) {
      /* Dest Node does not exist, so add the node based on id alone */
      dest = node_add_id(dest_id);
    }
    /* Check to see if the link exists between the src and dest */
    if((l = link_exists(src, dest)) == NULL) {
      /* If it doesn't already exist then we add it */
      l = link_add(src, dest);
    }

    /* Update the link information */
    // l->rssi = rssi;
    // l->last_update = clock_time();
    LOG_DBG( "LINK: Updated link\n");
    return l;
  } else {
    LOG_ERR( "LINK: SRC was NULL!\n");
    return NULL;
  }
}

/*---------------------------------------------------------------------------*/
/* Print Functions */
/*---------------------------------------------------------------------------*/
void
print_node(atom_node_t *n)
{
  int i;
  LOG_DBG("Node[%d] - Links[", n->id);
  for(i = 0; i < n->num_links; i++) {
    LOG_DBG_("%d,", n->links[i].dest_id);
  }
  LOG_DBG_("] [");
  LOG_DBG_6ADDR(&n->ipaddr);
  LOG_DBG_("]\n");
}

/*---------------------------------------------------------------------------*/
void
print_network_graph(void)
{
  atom_node_t *n;
  uint16_t num_links = 0;
  LOG_DBG("%s: =================== Network State\n", "ATOM");
  for(n = list_head(nodes); n != NULL; n = list_item_next(n)) {
    print_node(n);
    num_links += n->num_links;
  }
  LOG_DBG("%s: ========= (%d) Nodes | (%d) Links\n", "ATOM",
          list_length(nodes), num_links);
}
