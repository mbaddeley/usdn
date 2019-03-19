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
 *         uSDN Core header file.
 * \author
 *         Michael Baddeley <m.baddeley@bristol.ac.uk>
 */

#ifndef SDN_H_
#define SDN_H_

#include "net/ip/uip.h"
#include "net/ipv6/uip-ds6-nbr.h"

#include "net/rpl/rpl-private.h"

#include "sdn-ft.h"

typedef enum SDN_FLAG {
  RPL_SRH,
  SDN_UIP,
  SDN_UDP
} sdn_flag_t;

/*---------------------------------------------------------------------------*/
/* SDN UIP Constants */
/*---------------------------------------------------------------------------*/

/* Offset values for various IP header fields */
#define UIP_IPH_LEN_OFFSET        4
#define UIP_IPH_SRC_OFFSET        8
#define UIP_IPH_DST_OFFSET        24

/* Offset values for various ICMP header fields */
#define UIP_ICMPH_TYPE_OFFSET     0
#define UIP_ICMPH_CODE_OFFSET     1

/* Offset values for various UDP header fields */
#define UIP_UDPH_SRCPORT_OFFSET   0
#define UIP_UDPH_DESTPORT_OFFSET  2

/* Index values for datagram fields used within the engine */
#define uip_len_index (UIP_LLH_LEN + UIP_IPH_LEN_OFFSET)
#define uip_src_index (UIP_LLH_LEN + UIP_IPH_SRC_OFFSET)
#define uip_dst_index (UIP_LLH_LEN + UIP_IPH_DST_OFFSET)
#define uip_icmp_type_index (uip_l2_l3_hdr_len + UIP_ICMPH_TYPE_OFFSET)
#define uip_icmp_code_index (uip_l2_l3_hdr_len + UIP_ICMPH_CODE_OFFSET)
#define uip_udp_srcport_index (uip_l2_l3_hdr_len + UIP_UDPH_SRCPORT_OFFSET)
#define uip_udp_destport_index (uip_l2_l3_hdr_len + UIP_UDPH_DESTPORT_OFFSET)
#include "sdn-stack.h"

/*---------------------------------------------------------------------------*/
/* Control Layer Data Structures */
/*---------------------------------------------------------------------------*/
/* Prototype of controller struct */
typedef struct sdn_controller sdn_controller_t;
/* SDN Node ID */
typedef uint16_t sdn_node_id_t;

/*---------------------------------------------------------------------------*/
/* SDN Extension Headers */
/*---------------------------------------------------------------------------*/
/* SDN IPv6 extension header option. */
#define SDN_RH_LEN                4
#define SDN_SRH_LEN               4
#define SDN_RH_TYPE_SRH           3

/* Source Routing Header */
typedef struct uip_sdn_srh_hdr {
  uint8_t cmpr; /* CmprI and CmprE */
  uint8_t pad;
  uint8_t reserved[2];
} uip_sdn_srh_hdr;

/* Source Routing Entry for Flowtable */
#define SDN_CONF_MAX_ROUTE_LEN 10
typedef struct sdn_srh_route {
  uint8_t       cmpr;
  uint8_t       length;
  sdn_node_id_t nodes[SDN_CONF_MAX_ROUTE_LEN];
} sdn_srh_route_t;
#define srh_route_length(route) sizeof(route->cmpr) + \
                                sizeof(route->length) + \
                                (sizeof(sdn_node_id_t) * route->length)

/*---------------------------------------------------------------------------*/
/* Global SDN Buffers and Headers */
/*---------------------------------------------------------------------------*/
extern uint8_t         sdn_buf[UIP_BUFSIZE];
extern uint16_t        sdn_len;
extern uint8_t         sdn_ext_len;

/*
 * Clear sdn buffer
 *
 * This function clears the sdn buffer by reseting the sdn_len and
 * sdn_ext_len pointers.
 */
#define sdn_clear_buf() { \
  sdn_len = 0; \
  sdn_ext_len = 0; \
}

/*---------------------------------------------------------------------------*/
/* SDN Adapter */
/*---------------------------------------------------------------------------*/
#ifdef SDN_CONF_MAX_CONNECTIONS
#define SDN_MAX_CONNECTIONS SDN_CONF_MAX_CONNECTIONS
#else
#define SDN_MAX_CONNECTIONS 1
#endif

#ifdef SDN_CONF_ADAPTER
#define SDN_ADAPTER_TYPE SDN_CONF_ADAPTER
#else
#define SDN_ADAPTER_TYPE SDN_ADAPTER_NONE
#endif

#if SDN_ADAPTER_TYPE
#if SDN_ADAPTER_TYPE == SDN_ADAPTER_USDN
#define SDN_ADAPTER usdn_adapter
#else
#error "SDN enabled with an unknown ADAPTER type."
#error "Check the value of SDN_ADAPTER_TYPE in conf files."
#endif

/* SDN connection types */
typedef enum {
  SDN_CONN_TYPE_USDN,
  SDN_CONN_TYPE_6TOP
} sdn_conn_type_t;

/* SDN connection events */
typedef enum {
  SDN_CONN_EVENT_CONNECTED,
  SDN_CONN_EVENT_DISCONNECTED,
  /* Errors */
  SDN_CONN_EVENT_ERROR = 0x80,
} sdn_conn_event_t;

/* SDN connection state */
typedef enum {
  SDN_CONN_STATE_DISCONNECTED,
  SDN_CONN_STATE_REGISTER,
  SDN_CONN_STATE_REGISTERED,
  SDN_CONN_STATE_ERROR
} sdn_conn_state_t;

struct sdn_adapter {
  void (* init)(void);
  void (* register_controller)(sdn_controller_t *c);
  void (* send)(sdn_controller_t *c,  uint8_t length, void *data);
};

extern const struct sdn_adapter SDN_ADAPTER;
#endif /* SDN_ADAPTER_TYPE */

/*---------------------------------------------------------------------------*/
/* SDN Southbound Engine */
/*---------------------------------------------------------------------------*/
/* Choose a SDN engine based on user configuration, or select default */
#ifdef SDN_CONF_ENGINE
#define SDN_ENGINE_TYPE SDN_CONF_ENGINE
#else
#define SDN_ENGINE_TYPE SDN_CONF_ENGINE_NONE
#endif /* SDN_CONF_ENGINE */

#if SDN_ENGINE_TYPE
#if SDN_ENGINE_TYPE == SDN_ENGINE_USDN
#define SDN_ENGINE usdn_engine
#else
#error "SDN Enabled with an unknown Engine type."
#error "Check the value of SDN_CONF_ENGINE in conf files."
#endif

typedef enum {
  SDN_TMR_STATE_STOP,
  SDN_TMR_STATE_START,
  SDN_TMR_STATE_RESET,
  SDN_TMR_STATE_IMMEDIATE,
} sdn_tmr_state_t;

struct sdn_engine {
  void (* init)(void);
  void (* in)(void *data, uint8_t length, void *ptr);
  void (* controller_join)(sdn_tmr_state_t state);
  void (* controller_update)(sdn_tmr_state_t state);
  void (* controller_query)(void *data);
};

extern const struct sdn_engine SDN_ENGINE;
#endif /* SDN_ENGINE_TYPE */

/*---------------------------------------------------------------------------*/
/* SDN Driver */
/*---------------------------------------------------------------------------*/
/* SDN Process State */
typedef enum sdn_state {
  UIP_DROP,
  UIP_ACCEPT,
  SDN_CONTINUE,
  SDN_NO_MATCH,
} sdn_driver_response_t;

/* Driver data structure */
struct uip_sdn_driver {
  void (* init)(void);          /* Initialize the SDN driver */
  uint8_t (* process)(uint8_t flag);    /* Process a datagram */
  uint8_t (* retry)(uint16_t flow); /* Retry processing a datagram */

  /* Generic SDN functions that should be implemented */
  void (* add_fwd_on_dest)(uip_ipaddr_t *dest, uip_ipaddr_t *fwd);
  void (* add_fwd_on_flow)(flowtable_id_t id, uint8_t flow, uip_ipaddr_t *fwd);
  void (* add_fallback_on_dest)(flowtable_id_t id, uip_ipaddr_t *dest);
  void (* add_srh_on_dest)(flowtable_id_t id, uip_ipaddr_t *dest, sdn_node_id_t *route, uint8_t len);
  void (* add_accept_on_src)(flowtable_id_t id, uip_ipaddr_t *src);
  void (* add_accept_on_dest)(flowtable_id_t id, uip_ipaddr_t *dest);
  void (* add_accept_on_icmp6_type)(flowtable_id_t id, uint8_t type);
  void (* add_do_callback_on_dest)(flowtable_id_t id, uip_ipaddr_t *dest, sdn_ft_callback_action_ipaddr_t callback);
};

/* Choose a SDN driver or turn off based on user configuration */
#ifdef SDN_CONF_DRIVER
#define SDN_DRIVER_TYPE SDN_CONF_DRIVER
#else
#define SDN_DRIVER_TYPE SDN_DRIVER_NONE
#endif /* SDN_CONF_DRIVER */

/* Configure SDN and core/net with the SDN driver */
#if SDN_DRIVER_TYPE
#if SDN_DRIVER_TYPE == SDN_DRIVER_USDN
#define SDN_DRIVER usdn_driver
#else
#error "SDN Enabled with an unknown Driver type."
#error "Check the value of SDN_CONF_DRIVER in conf files."
#endif /* SDN_DRIVER_TYPE */
/* End of SDN hooks */

extern const struct uip_sdn_driver SDN_DRIVER;
#endif /* SDN_DRIVER_TYPE */


/*---------------------------------------------------------------------------*/
/* Controller API */
/*---------------------------------------------------------------------------*/
/* Choose a SDN contrller or turn off based on user configuration */
#ifdef SDN_CONF_CONTROLLER
#define SDN_CONTROLLER_TYPE SDN_CONF_CONTROLLER
#else
#define SDN_CONTROLLER_TYPE SDN_CONTROLLER_NONE
#endif

/*---------------------------------------------------------------------------*/
/* SDN Addresses */
/*---------------------------------------------------------------------------*/
//TODO: These are just the same as the all-nodes address for now. But we need
//      to change this.
#define uip_is_addr_linklocal_sdnnodes_mcast(a)     \
  ((((a)->u8[0]) == 0xff) &&                        \
   (((a)->u8[1]) == 0x02) &&                        \
   (((a)->u16[1]) == 0) &&                          \
   (((a)->u16[2]) == 0) &&                          \
   (((a)->u16[3]) == 0) &&                          \
   (((a)->u16[4]) == 0) &&                          \
   (((a)->u16[5]) == 0) &&                          \
   (((a)->u16[6]) == 0) &&                          \
   (((a)->u8[14]) == 0) &&                          \
   (((a)->u8[15]) == 0x01))

/** \brief Set IP address addr to the link-local, all-sdn-nodes
    multicast address. */
#define uip_create_linklocal_sdnnodes_mcast(addr) uip_ip6addr((addr), 0xff02, 0, 0, 0, 0, 0, 0, 0x0001)

/*---------------------------------------------------------------------------*/
/* SDN Statistics */
/*---------------------------------------------------------------------------*/
/* Set to 1 to enable RPL statistics */
#ifndef SDN_CONF_STATS
#define SDN_CONF_STATS 0
#endif /* SDN_CONF_STATS */

#if SDN_CONF_STATS
/* The platform can override the stats datatype */
#ifdef SDN_CONF_STATS_DATATYPE
#define SDN_STAT_TYP UIP_SDN_CONF_STATS_DATATYPE
#else
#define SDN_STAT_TYP uint16_t
#endif /* UIP_SDN_CONF_STATS_DATATYPE */

/* The actual SDN stats structure. SDN engine implementations can be added to
   this in order to collect engine-specific stats */
typedef struct sdn_stats {
  struct {
    SDN_STAT_TYP in;            /**< datagrams passed into sdn */
    SDN_STAT_TYP out;           /**< datagrams sent by sdn */
    SDN_STAT_TYP fwd;           /**< datagrams forwarded by sdn */
    SDN_STAT_TYP query;         /**< number of controller queries */
    SDN_STAT_TYP ft_fwd;        /**< datagrams forwarded by ft */
    SDN_STAT_TYP ft_mod;        /**< datagrams modified by ft */
    SDN_STAT_TYP ft_srh;        /**< srh headers inserted by ft */
    SDN_STAT_TYP icmp_in;       /**< icmp received */
    SDN_STAT_TYP icmp_out;      /**< icmp sent */
    SDN_STAT_TYP noctrl;        /**< datagrams dropped due to no controller response */
  } sdn;
#if SDN_DRIVER_TYPE == SDN_DRIVER_USDN
  struct {
    SDN_STAT_TYP nsu;           /**< nsu packets sent */
    SDN_STAT_TYP ftr;           /**< ftr packets sent */
    SDN_STAT_TYP fsr;           /**< fts packets received */
    SDN_STAT_TYP dropped;       /**< packets sent to ctrl */
    SDN_STAT_TYP received;      /**< packets received from ctrl */
  } usdn;
#endif /* SDN_DRIVER_USDN */
} sdn_stats_t;
#endif /* SDN_CONF_STATS */

/* This is the variable in which the SDN satistics are gathered. */
#if SDN_CONF_STATS
extern sdn_stats_t sdn_stat;
#define SDN_STAT(s) s
#else
#define SDN_STAT(s)
#endif /* SDN_CONF_STATS */

/*---------------------------------------------------------------------------*/
/* Public SDN functions. */
/*---------------------------------------------------------------------------*/
void sdn_init();
uint8_t sdn_connected(void);
uint8_t sdn_is_ctrl_addr(uip_ipaddr_t *addr);
const struct link_stats *sdn_get_nbr_link_stats(uip_ds6_nbr_t *nbr);
sdn_node_id_t sdn_node_id_from_ipaddr(uip_ipaddr_t *ipaddr);
uint8_t sdn_rpl_get_current_instance_rank(void);

/* Flowtable data conversion */
void sdn_get_action_data_srh(sdn_ft_action_rule_t *rule, sdn_srh_route_t *route);

/* Extension header functions */
int sdn_ext_insert_srh(sdn_srh_route_t *route);

/* Print functions */
void print_sdn_srh_route(sdn_srh_route_t *route);

#endif /* SDN_H_ */
