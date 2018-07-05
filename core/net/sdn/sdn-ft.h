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
 *         uSDN Core: Flowtable for matching and performing actions on ingress
 *                    and egress data flows.
 * \author
 *         Michael Baddeley <m.baddeley@bristol.ac.uk>
 */

#ifndef SDN_FT_H_
#define SDN_FT_H_

//TODO: Need to introduce the concept of priority into the flow table

#include "lib/list.h"
#include "net/ip/uip.h"

#ifdef SDN_CONF_FT_MAX_WHITELIST
#define SDN_CONF_FT_MAX_WHITELIST    SDN_CONF_FT_MAX_WHITELIST
#else
#define SDN_CONF_FT_MAX_WHITELIST    10
#endif
#ifdef SDN_CONF_FT_MAX_MATCHES
#define SDN_FT_MAX_MATCHES    SDN_CONF_FT_MAX_MATCHES
#else
#define SDN_FT_MAX_MATCHES    10
#endif
#ifdef SDN_CONF_FT_MAX_ACTIONS
#define SDN_FT_MAX_ACTIONS    SDN_CONF_FT_MAX_ACTIONS
#else
#define SDN_FT_MAX_ACTIONS    10
#endif
#ifdef SDN_CONF_FT_MAX_ENTRIES
#define SDN_FT_MAX_ENTRIES    SDN_CONF_FT_MAX_ENTRIES
#else
#define SDN_FT_MAX_ENTRIES    10
#endif
#ifdef SDN_CONF_FT_MAX_DATA_MEMB
#define SDN_FT_DATA_MEMB_SIZE SDN_CONF_FT_MAX_DATA_MEMB
#else
#define SDN_FT_DATA_MEMB_SIZE 1024
#endif

#define SDN_FT_INFINITE_LIFETIME 0xFFFF

typedef enum flowtable_id {
  WHITELIST,
  FLOWTABLE
} flowtable_id_t;

typedef uip_ds6_addr_t (* sdn_ft_callback_action_ipaddr_t)(uip_ipaddr_t *ipaddr);

/*---------------------------------------------------------------------------*/
/* Table ENUMS */
/*---------------------------------------------------------------------------*/
typedef enum __attribute__((__packed__)) ft_match_op {
  LT_EQ  =    -2,
  LT     =    -1,
  EQ     =     0,
  GT     =     1,
  GT_EQ  =     2,
  NOT_EQ =     3
} sdn_ft_match_op_t;

typedef enum __attribute__((__packed__)) ft_action_type {
  SDN_FT_ACTION_ACCEPT,                      /**< accept and pass to upper layers */
  SDN_FT_ACTION_DROP,                        /**< drop the packet */
  SDN_FT_ACTION_QUERY,                       /**< query the controller */
  SDN_FT_ACTION_FORWARD,                     /**< forward to neighbor */
  SDN_FT_ACTION_MODIFY,                      /**< modify the packet */
  SDN_FT_ACTION_FALLBACK,                    /**< send to the fallback interface */
  SDN_FT_ACTION_SRH,
  SDN_FT_ACTION_CALLBACK
} sdn_ft_action_type_t;

/*---------------------------------------------------------------------------*/
/* Table Structures */
/*---------------------------------------------------------------------------*/
typedef struct __attribute__((__packed__)) ft_match_rule{
  sdn_ft_match_op_t   operator;       /**< ==, !=, <= etc... */
  uint8_t             index;          /**< field index within uip_buf */
  uint8_t             len;            /**< length of the field in uip_buf */
  uint8_t             req_ext;        /**< requires ext_header_len */
  void                *data;          /**< data to evaluate against */
} sdn_ft_match_rule_t;
#define sdn_ft_match_length(m) sizeof(sdn_ft_match_rule_t) - \
                               sizeof(void*) + \
                               m.len
#define SDN_FT_MATCH_HDR_LEN sizeof(sdn_ft_match_op_t) + 3

typedef struct __attribute__((__packed__)) ft_action_rule{
  sdn_ft_action_type_t  action;       /**< action to perform */
  uint8_t               index;        /**< (optional) index in uip_buf */
  uint8_t               len;          /**< (optional) length in uip_buf */
  void                  *data;        /**< (optional) data to use */
} sdn_ft_action_rule_t;
#define sdn_ft_action_length(a) sizeof(sdn_ft_action_rule_t) - \
                                sizeof(void*) + \
                                a.len

typedef struct stats_rule{
  uint16_t ttl;                       /**< time to live of entry */
  uint16_t count;                     /**< number of times it's matched */
} sdn_ft_stats_t;

typedef struct ft_entry {
  struct ft_entry *next;
  uint8_t id;
  // TODO: Introduce *<---->* relationship
  // LIST_STRUCT(matches_list);
  // LIST_STRUCT(actions_list);
  sdn_ft_match_rule_t *match_rule;
  sdn_ft_action_rule_t *action_rule;
  sdn_ft_stats_t stats;
  struct ctimer lifetimer;
} sdn_ft_entry_t;

/*---------------------------------------------------------------------------*/
/* Callback Functions */
/*---------------------------------------------------------------------------*/
/* Action handler provided by sdn engine. This allows engines to provide their
   own implementation of how actions should be handled. The handler is called
   after a successful match on an entry in the flowtable. */
typedef int (* sdn_ft_action_handler_callback_t)(sdn_ft_action_rule_t *action,
                                                 uint8_t *data);
sdn_ft_action_handler_callback_t ft_action_handler;

/*---------------------------------------------------------------------------*/
/* SDN Flowtable API */
/*---------------------------------------------------------------------------*/
void sdn_ft_init();
void sdn_ft_register_action_handler(sdn_ft_action_handler_callback_t callback);
uint8_t sdn_ft_check_default(void *data, uint8_t length, uint8_t ext_len);
int sdn_ft_check(flowtable_id_t id, void *data, uint16_t len, uint8_t ext_len);
uint8_t sdn_ft_contains(void *data, uint8_t len);

sdn_ft_entry_t *sdn_ft_create_entry(flowtable_id_t id,
                                    sdn_ft_match_rule_t *match,
                                    sdn_ft_action_rule_t *action,
                                    clock_time_t lifetime,
                                    uint8_t is_default);
sdn_ft_match_rule_t *sdn_ft_create_match(sdn_ft_match_op_t operator,
                                         uint8_t index,
                                         uint8_t len,
                                         uint8_t req_ext,
                                         void *data);
sdn_ft_action_rule_t *sdn_ft_create_action(sdn_ft_action_type_t action,
                                           uint8_t index,
                                           uint8_t len,
                                           void *data);
void print_sdn_ft(flowtable_id_t id);
void print_sdn_ft_entry(sdn_ft_entry_t *e);
void print_sdn_ft_match(sdn_ft_match_rule_t *m);
void print_sdn_ft_action(sdn_ft_action_rule_t *a);

#endif /* SDN_FT_H_ */
