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
 *         Atom SDN Controller header file.
 * \author
 *         Michael Baddeley <m.baddeley@bristol.ac.uk>
 */
#ifndef __ATOM_H__
#define __ATOM_H__

#include "contiki.h"
#include "net/sdn/sdn.h"

#include "atom-conf.h"

// #define UNUSED(x) (void)(x)
/* Used for debug */
#if (LOG_LEVEL >= DEBUG_LEVEL)
#include "net/sdn/usdn/usdn.h"
#endif

#define RPL_CODE_STRING(S) \
  ((S == RPL_CODE_DIS) ? ("DIS") : \
  (S == RPL_CODE_DIO) ? ("DIO") : \
  (S == RPL_CODE_DAO) ? ("DAO") : \
  (S == RPL_CODE_DAO_ACK) ? ("DAO-ACK") : "UKWN")

#define ACTION_STRING(S) \
  ((S == ATOM_ACTION_NETUPDATE) ? ("NETUPDATE") : \
  (S == ATOM_ACTION_ROUTING) ? ("ROUTING") : \
  (S == ATOM_ACTION_JOIN) ? ("JOIN") : "UKWN")

#define RESPONSE_STRING(S) \
  ((S == ATOM_RESPONSE_ROUTING) ? ("ROUTING") : \
  (S == ATOM_RESPONSE_ACK) ? ("ROUTING") : \
  (S == ATOM_RESPONSE_NACK) ? ("JOIN") : \
  (S == ATOM_RESPONSE_CFG) ? ("CFG") : "UKWN")

#define HANDSHAKE_STRING(S) \
  ((S == ATOM_RESPONSE_CFG) ? ("CFG") : "UKWN")


/* Our controller address */
extern uip_ipaddr_t controller_addr;

/*---------------------------------------------------------------------------*/
/* Timers */
/*---------------------------------------------------------------------------*/
#define RAND_BETWEEN(MAX, MIN)  (int)((MIN * CLOCK_SECOND) +                  \
                                ((((MAX - MIN) * CLOCK_SECOND) *              \
                                (uint32_t)random_rand()) / RANDOM_RAND_MAX))

#define RAND_UP_TO(n)           (int)(((n * CLOCK_SECOND) * (uint32_t)random_rand()) / RANDOM_RAND_MAX)

#ifndef  ATOM_CONF_CFG_HS_DELAY_MIN
#define  ATOM_CONF_CFG_HS_DELAY_MIN 10
#endif
#ifndef  ATOM_CONF_CFG_HS_DELAY_MAX
#define  ATOM_CONF_CFG_HS_DELAY_MAX 15
#endif

#define ATOM_RANDOM_CFG_HS_DELAY() RAND_BETWEEN(ATOM_CONF_CFG_HS_DELAY_MAX, ATOM_CONF_CFG_HS_DELAY_MIN)

/*---------------------------------------------------------------------------*/
/* Network layer */
/*---------------------------------------------------------------------------*/
/* Max number of links per monitored node */
#define ATOM_MAX_LINKS_PER_NODE  NBR_TABLE_CONF_MAX_NEIGHBORS

typedef struct atom_link {
  uint8_t dest_id;
  int16_t rssi;
  uint8_t status;
} atom_link_t;

typedef struct atom_handshake {
  struct ctimer         timer;    /* Callback timer */
  uint8_t               type;     /* Type of response for handshake */
  uint8_t               n_tries;  /* Number of attempts */
  struct atom_sb        *sb;      /* Southbound to keep sending response to */
} atom_hs_t;

typedef struct atom_node {
  struct atom_node *next;         /* for list */
  sdn_node_id_t    id;            /* id based on the ip */
  uip_ipaddr_t     ipaddr;        /* node's global ipaddr */
  uint8_t          cfg_id;        /* configuration id */
  atom_hs_t        handshake;     /* Handshake to ensure node response */
  uint8_t          rank;          /* rank of the node */
  /* Neighbors */
  uint8_t          num_links;
  atom_link_t      links[ATOM_MAX_LINKS_PER_NODE];
} atom_node_t;

/* Network Layer API */
void atom_net_init(void);
atom_node_t *atom_net_get_node_ipaddr(uip_ipaddr_t *ipaddr);
atom_node_t *atom_net_get_node_id(sdn_node_id_t id);
atom_node_t *atom_net_node_heartbeat(uip_ipaddr_t *ipaddr);
atom_node_t *atom_net_node_update(uip_ipaddr_t *ipaddr, uint8_t cfg_id, uint8_t rank);
atom_link_t *atom_net_link_update(atom_node_t *src, sdn_node_id_t dest_id, int16_t rssi);

/*---------------------------------------------------------------------------*/
/* Appliction Layer */
/*---------------------------------------------------------------------------*/
/* Atom ACTION types */
typedef enum ATOM_ACTION_TYPE {
  ATOM_ACTION_NETUPDATE,
  ATOM_ACTION_ROUTING,
  ATOM_ACTION_JOIN,
  NUM_ATOM_ACTIONS
} atom_action_type_t;

typedef struct atom_routing_action {
  uint8_t      tx_id; // FIXME: Should really be using id in action
  uip_ipaddr_t src;
  uip_ipaddr_t dest;
} atom_routing_action_t;

typedef struct atom_netupdate_action {
  atom_node_t node;
} atom_netupdate_action_t;

typedef struct atom_join_action {
  uip_ipaddr_t node;
} atom_join_action_t;

// TODO: Alow user to configure ATOM_ACTION_BUFSIZE
#define ATOM_ACTION_BUFSIZE   255
// FIXME: Temp pack this for a memory alignement issue. Might need to do
//        something like uip_buf_aligned though.
typedef struct __attribute__((__packed__)) atom_action  {
  uint8_t               data[ATOM_ACTION_BUFSIZE];
  uint8_t               id;
  atom_action_type_t    type;
  uip_ipaddr_t          src;
  uint8_t               datalen;
} atom_action_t;

/* Atom RESPONSES types */
typedef enum ATOM_RESPONSE_TYPE {
  ATOM_RESPONSE_ROUTING,
  ATOM_RESPONSE_ACK,
  ATOM_RESPONSE_NACK,
  ATOM_RESPONSE_CFG,
  NUM_ATOM_RESPONSES
} atom_response_type_t;

typedef struct atom_routing_response {
  sdn_srh_route_t route;
} atom_routing_response_t;

// TODO: atom_configure_response

#define ATOM_RESPONSE_BUFSIZE 100
typedef struct atom_response {
  uint8_t               id;
  atom_response_type_t  type;
  uip_ipaddr_t          dest;
  uint8_t               datalen;
  uint8_t               data[ATOM_RESPONSE_BUFSIZE];
} atom_response_t;

/* Atom application data structure */
struct atom_app {
  char *             name;
  atom_action_type_t action_type;         /* Action type the app handles */
  void               (* init)(void);
  atom_response_t *  (* run)(void *data);
};
#define atom_app_ptr_t struct atom_app *

/* Concrete applications */
struct atom_app app_route_sp;
struct atom_app app_route_rpl;
struct atom_app app_join_cfg;

/*---------------------------------------------------------------------------*/
/* Southbound layer */
/*---------------------------------------------------------------------------*/
/* Atom SB types */
typedef enum ATOM_SB_TYPE {
  ATOM_SB_TYPE_USDN,
  ATOM_SB_TYPE_RPL,
} atom_sb_type_t;

/* Atom southbound connector data structure */
struct atom_sb {
  atom_sb_type_t   type;
  void             (* init)(void);
  atom_action_t *  (* in)(void);
  void             (* out)(atom_action_t *action, atom_response_t *response);
  /* App lists for each action. This needs to mirror the action enum! */
  atom_app_ptr_t   app_matrix[][NUM_ATOM_ACTIONS];
};

/* Concrete southbound connectors */
struct atom_sb sb_usdn;
struct atom_sb sb_rpl;

/* Prototypes of SB output functions to make them visible to other connectors */
uint8_t cack_output(uint8_t net_id, uint8_t flow, void *buf);

/*---------------------------------------------------------------------------*/
/* Atom buffer */
/*---------------------------------------------------------------------------*/
/* Southbound input queue length */
#ifndef ATOM_BUFFER_MAX
#define ATOM_BUFFER_MAX 1
#endif /* ATOM_BUF_LEN */

CCIF extern uip_buf_t   c_alligned_buf;
#define c_buf           (c_alligned_buf.u8)
CCIF extern uint16_t    c_len;
CCIF extern uint8_t     c_ext_len;
CCIF extern uint16_t    c_slen;
CCIF extern uint8_t    c_hops;

/* This function clears the controller buffer by reseting the c_len pointer. */
#define c_clear_buf() { \
  c_len = 0; \
  c_ext_len = 0; \
}

/* Southbound input queue */
typedef struct atom_message {
  struct atom_message *next;
  uint8_t         id;
  uint8_t         buf[UIP_BUFSIZE - UIP_LLH_LEN];
  uint16_t        buf_len;
  uint8_t         ext_len;
  uint8_t         hops;
  struct atom_sb  *sb;
} atom_msg_t;

typedef struct atom_queue {
  struct memb *buf;
  list_t      list;
  uint8_t     size;
} atom_queue_t;

/* Atom southbound input queue API */
void atom_buffer_init(void);
void atom_buffer_add(struct atom_sb *sb_connector);
atom_msg_t *atom_buffer_head(void);
void atom_buffer_remove(atom_msg_t *m);
void atom_buffer_copy_cbuf_to_uip(void);
/* Atom action buffer API */
atom_action_t *atom_action_buf_copy_to(atom_action_type_t type, void *data);
void atom_action_buf_clear(void);
/* Atom response buffer API */
atom_response_t * atom_response_buf_copy_to(atom_response_type_t type, void *data);
void atom_response_buf_clear(void);

/*---------------------------------------------------------------------------*/
/* Atom API */
/*---------------------------------------------------------------------------*/
void atom_init(uip_ipaddr_t *addr);
void atom_post(struct atom_sb *sb);
void atom_run(struct atom_sb *sb);
void atom_set_handshake_timer(sdn_tmr_state_t state,
                              uint8_t type,
                              struct ctimer *timer,
                              void (*callback)(void *),
                              void *data);

/*---------------------------------------------------------------------------*/
/* Printing API */
/*---------------------------------------------------------------------------*/
void print_network_graph();

#endif /*__ ATOM_H__ */
