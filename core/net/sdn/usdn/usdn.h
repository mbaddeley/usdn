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
 *         uSDN Stack header file.
 * \author
 *         Michael Baddeley <m.baddeley@bristol.ac.uk>
 */
#ifndef USDN_H_
#define USDN_H_

#include "net/ip/uip.h"
#include "sdn.h"
#include "sdn-ft.h"

/*---------------------------------------------------------------------------*/
/* uSDN Protocol Definitions */
/*---------------------------------------------------------------------------*/
/* uSDN Packet Types */
typedef enum USDN_CODE {
  USDN_MSG_CODE_CFG,
  USDN_MSG_CODE_CJOIN,
  USDN_MSG_CODE_NSU,
  USDN_MSG_CODE_CACK,
  USDN_MSG_CODE_CNACK,
  USDN_MSG_CODE_FTQ,
  USDN_MSG_CODE_FTS,
  USDN_MSG_CODE_TRACKRQ,
  USDN_MSG_CODE_DATA,
} usdn_msg_code_t;

#define USDN_CODE_STRING(code) \
  ((code == USDN_MSG_CODE_CFG) ? ("CFG") : \
  (code == USDN_MSG_CODE_CJOIN) ? ("CJOIN") : \
  (code == USDN_MSG_CODE_NSU) ? ("NSU") : \
  (code == USDN_MSG_CODE_CACK) ? ("CACK") : \
  (code == USDN_MSG_CODE_CNACK) ? ("CNACK") : \
  (code == USDN_MSG_CODE_FTQ) ? ("FTQ") : \
  (code == USDN_MSG_CODE_FTS) ? ("FTS") : \
  (code == USDN_MSG_CODE_TRACKRQ) ? ("TR") : \
  (code == USDN_MSG_CODE_DATA) ? ("DATA") : "UNKNOWN")

/* Offset values for various uSDN header fields */
#define USDN_HDR_NET_OFFSET  0
#define USDN_HDR_TYP_OFFSET  1
#define USDN_HDR_FLOW_OFFSET 2

/*---------------------------------------------------------------------------*/
/* Logical Representation of a uSDN Header */
typedef struct usdn_hdr {
  uint8_t            net;  /*<< virtual network packet is associated with */
  uint8_t            typ;  /*<< type of uSDN packet */
  uint16_t          flow;  /*<< flow id */
} usdn_hdr_t;
#define USDN_H_LEN  sizeof(usdn_hdr_t) /* uSDN packet header length */

/*---------------------------------------------------------------------------*/
/* Logical Representation of uSDN Node State Update */
typedef struct usdn_nsu_link {
  sdn_node_id_t nbr_id;
  // uint16_t      etx;
  int16_t       rssi;
  // uint8_t       is_fresh;
} usdn_nsu_link_t;

typedef struct usdn_nsu {
  /* Node Info */
  uint8_t         cfg_id;
  uint8_t         rank;
  /* Link Info */
  uint8_t         num_links;
  usdn_nsu_link_t links[];
} usdn_nsu_t;
#define nsu_length(nsu) sizeof(usdn_nsu_t) + \
                        (sizeof(usdn_nsu_link_t) * nsu->num_links)

/*---------------------------------------------------------------------------*/
/* Logical Representation of uSDN Controller Join */

/* Empty. CJOIN has no payload. */

/*---------------------------------------------------------------------------*/
/* Logical Representation of uSDN Flowtable Set */
// TODO: Make this user configurable
#define USDN_CONF_MAX_FTS_DATA 20

typedef struct usdn_match {
  sdn_ft_match_op_t     operator;      /**< ==, !=, <= etc... */
  uint8_t               index;         /**< field index within uip_buf */
  uint8_t               len;           /**< length of the field in uip_buf */
  uint8_t               req_ext;       /**< requires ext_header_len */
  uint8_t               data[USDN_CONF_MAX_FTS_DATA];
} usdn_match_t;

typedef struct usdn_action {
  sdn_ft_action_type_t  action;       /**< action to perform */
  uint8_t               index;        /**< (optional) index in uip_buf */
  uint8_t               len;          /**< (optional) length in uip_buf */
  uint8_t               data[USDN_CONF_MAX_FTS_DATA];
} usdn_action_t;

typedef struct usdn_ftset {
  uint8_t               tx_id;
  uint8_t               is_default;
  usdn_match_t          m;
  usdn_action_t         a;
} usdn_fts_t;
#define fts_length(fts) sizeof(fts->tx_id) + \
                        sizeof(fts->is_default) + \
                        sizeof(fts->m) + \
                        sizeof(fts->a)

/*---------------------------------------------------------------------------*/
/* Logical Representation of uSDN Configure */
typedef struct usdn_cfg {
  // uip_ipaddr_t       ipaddr;           /* The Controller address */
  uint8_t            sdn_net;          /* Virtual network id */
  uint8_t            cfg_id;           /* Configuration ID */
  clock_time_t       ft_lifetime;           /* Flowtable entry time to live */
  uint8_t            query_full;
  uint8_t            query_idx;        /* Index for FTQ messages */
  uint8_t            query_len;        /* Length for FTQ messages */
  uint16_t           update_period;    /* Period for node status updates */
  uint8_t            rpl_dio_interval;
  uint8_t            rpl_dfrt_lifetime;
  // uint8_t            conn_type;        /* Type of controller connection */
  // uint8_t            conn_length;      /* Controller connection length */
  // uint8_t            conn_data[];      /* Controller connection data */
} usdn_cfg_t;
#define cfg_length(cfg) sizeof(usdn_cfg_t) //+ cfg->conn_length

/*---------------------------------------------------------------------------*/
/* Logical Representation of uSDN Flowtable Query */
typedef struct usdn_ftquery {
  uint8_t  tx_id;
  uint8_t  index;
  uint16_t length;
  uint8_t  data[];
} usdn_ftq_t;
#define ftq_length(ftq) sizeof(usdn_ftq_t) + ftq->length

/*---------------------------------------------------------------------------*/
/* uSDN Connection Adapter */
/*---------------------------------------------------------------------------*/
typedef struct udp_data {
  uint16_t  lport;
  uint16_t  rport;
} udp_data_t;

typedef struct usdn_conn {
  /* UDP connection */
  uint16_t lport, rport;
  struct uip_udp_conn *udp;
  /* State */
  sdn_conn_state_t state;
  /* Pointer to controller */
  void *controller;
} usdn_conn_t;

/*---------------------------------------------------------------------------*/
/* uSDN Engine */
/*---------------------------------------------------------------------------*/
void usdn_engine_mac_callback(void *data, int status);

/*---------------------------------------------------------------------------*/
/* uSDN Driver */
/*---------------------------------------------------------------------------*/
/* Used to denote the start of the uSDN header and Payload */
#define uip_l2_l3_udp_hdr_len (uip_l2_l3_hdr_len + UIP_UDPH_LEN)
#define uip_l2_l3_udp_sdn_hdr_len (uip_l2_l3_udp_hdr_len + USDN_H_LEN)

/* Index values for uSDN specific datagram fields */
#define usdn_net_index (uip_l2_l3_udp_hdr_len + USDN_HDR_NET_OFFSET)
#define usdn_typ_index (uip_l2_l3_udp_hdr_len + USDN_HDR_TYP_OFFSET)
#define usdn_flow_index (uip_l2_l3_udp_hdr_len + USDN_HDR_FLOW_OFFSET)

/*---------------------------------------------------------------------------*/
/* uSDN API */
/*---------------------------------------------------------------------------*/
void print_usdn_header(usdn_hdr_t *hdr);
void print_usdn_ftq(usdn_ftq_t *ftq);
void print_usdn_fts(usdn_fts_t *fts);
void print_usdn_nsu(usdn_nsu_t *nsu);

void controller_query_data(void *data, uint8_t len);

#endif /* USDN_H_ */
