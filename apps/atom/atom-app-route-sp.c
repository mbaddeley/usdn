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
 *         Atom SDN Controller: Shortest path routing application.
 * \author
 *         Michael Baddeley <m.baddeley@bristol.ac.uk>
 */
#include "net/ip/uip.h"

#include "net/sdn/sdn.h"

#include "atom.h"

/* Log configuration */
#include "sys/log-ng.h"
#define LOG_MODULE "ATOM"
#define LOG_LEVEL LOG_LEVEL_ATOM

/* Search */
static atom_node_t *stack[ATOM_MAX_NODES];  /* Stack for searching */
static int top = -1;                  /* Keeps track of the stack index */

/* Path */
static atom_node_t *spath[ATOM_MAX_NODES];  /* Keeps track of shortest path */
static int spath_length = ATOM_MAX_NODES;  /* Assume worst case */

/*---------------------------------------------------------------------------*/
/* Printing */
/*---------------------------------------------------------------------------*/
// static void
// print_stack() {
//   int i;
//   PRINTF("ATOM-APP : STACK ================\n\n");
//   PRINTF("ATOM-APP : \n\n");
//   for(i = 0; i <= top; i++) {
//     PRINTF(" %d ", stack[i]->id);
//   }
//   PRINTF("\nATOM-APP : ======================\n\n");
// }
/*---------------------------------------------------------------------------*/
// static void
// print_shortest_path() {
//   int i;
//   for(i = 0; i < spath_length; i++) {
//     PRINTF(" %d ", spath[i]->id);
//   }
// }

/*---------------------------------------------------------------------------*/
#if LOG_LEVEL >= LOG_LEVEL_DBG
static void
print_route(sdn_srh_route_t *route) {
  int i;
  for(i = 0; i < route->length; i++) {
    LOG_DBG_(" %d ", route->nodes[i]);
  }
}
#else
static void print_route(sdn_srh_route_t *route) {}
#endif /* LOG_LEVEL >= LOG_LEVEL_DBG */

/*---------------------------------------------------------------------------*/
/* Stack and Shortest Path */
/*---------------------------------------------------------------------------*/
static void
push(atom_node_t *n) {
   stack[++top] = n;
}

static atom_node_t *
pop() {
   return stack[top--];
}

// static atom_node_t *
// peek() {
//    return stack[top];
// }

// static int
// is_empty() {
//    return top == -1;
// }

static int
size() {
  return top+1;
}

static int
contains(int id) {
  int i;
  for (i = 0; i <= top; i++) {
    if(stack[i]->id == id) {
      return 1;
    }
  }
  return 0;
}

void
clear_spath(void) {
  int i;
  for(i = 0; i < ATOM_MAX_NODES; i++) {
    spath[i] = NULL;
  }
  spath_length = ATOM_MAX_NODES;
}

static void
copy_to_spath() {
  int i;
  for(i = 0; i<= top; i++) {
    spath[i] = stack[i];
  }
  spath_length = top+1;
}

/*---------------------------------------------------------------------------*/
static void
depth_first_search(atom_node_t *src, atom_node_t *dest)
{
  int i;
  atom_node_t *new_src;
  if(src != NULL && dest != NULL) {
    /* push to the search stack */
    push(src);
    /* Check if we have reached the destination */
    if(src->id == dest->id) {
      /* If this stack is the shortest we want to copy the path */
      if(size() <= spath_length) {
        clear_spath();
        copy_to_spath();
      }
      /* Pop the stack */
      pop(src);
      return;
    }
    /* If we havent reached the destination, then search the neighbours that
       haven't been visited */
    for(i = 0; i < src->num_links; i++) {
      if(!contains(src->links[i].dest_id)) {
        new_src = atom_net_get_node_id(src->links[i].dest_id);
        if(new_src != NULL) {
          depth_first_search(new_src, dest);
        } else {
          LOG_ERR("ERROR DFS could not find node!\n");
        }
      }
    }
    /* Pop the stack */
    pop(src);
  } else {
    LOG_ERR("ERROR depth_first_search SRC or DEST was NULL!\n");
  }
}

/*---------------------------------------------------------------------------*/
/* Application API */
/*---------------------------------------------------------------------------*/
static void
init(void) {
  LOG_INFO("Atom shortest path routing app initialised\n");
}

/*---------------------------------------------------------------------------*/
static atom_response_t *
run(void *data)
{
  int i;
  sdn_srh_route_t route;
  atom_node_t *src, *dest;

  /* Dereference the action data */
  atom_routing_action_t *action = (atom_routing_action_t *)data;

  /* Get the nodes from the net layer */
  if(&action->src != NULL && &action->dest != NULL) {
    src = atom_net_get_node_ipaddr(&action->src);
    dest = atom_net_get_node_ipaddr(&action->dest);
  } else {
    LOG_ERR( "src or dest is NULL\n");
    return NULL;
  }

  /* Make sure the spath is cleared */
  clear_spath();
  /* Perform the DFS */
  depth_first_search(src, dest);
  /* Check DFS was succesful */
  if(spath[0] != NULL) {
    /* Copy to route */
    route.cmpr = 15;
    route.length = spath_length;
    for(i = 0; i < spath_length; i++) {
      route.nodes[i] = spath[i]->id;
    }
    LOG_DBG("Found route from ");
    LOG_DBG_6ADDR(&action->src);
    LOG_DBG_(" to ");
    LOG_DBG_6ADDR(&action->dest);
    print_route(&route);
    LOG_DBG_("\n");
  } else {
    LOG_ERR("ERROR No path between [%d] and [%d]! MAX_NODES=%d",
      src->id, dest->id, ATOM_MAX_NODES);
    return NULL;
  }

  /* Return the response */
  return atom_response_buf_copy_to(ATOM_RESPONSE_ROUTING, &route);
}

/*---------------------------------------------------------------------------*/
/* Application instance */
/*---------------------------------------------------------------------------*/
struct atom_app app_route_sp = {
  "SP Routing",
  ATOM_ACTION_ROUTING,
  init,
  run
};
