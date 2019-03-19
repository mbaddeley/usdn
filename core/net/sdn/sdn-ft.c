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

#include <stdio.h>
#include <string.h>

#include "lib/memb.h"
#include "lib/list.h"
#include "sys/node-id.h"

#include "contiki.h"
#include "net/ip/uip.h"
#include "net/ipv6/uip-nd6.h"
#include "net/ipv6/uip-ds6-nbr.h"
#include "net/nbr-table.h"

#include "sdn.h"
#include "sdn-ft.h"
#include "sdn-conf.h"

/* Log configuration */
#include "sys/log-ng.h"
#define LOG_MODULE "SDN-FT"
#define LOG_LEVEL LOG_LEVEL_SDN

// #undef LOG_WITH_ANNOTATE
// #define LOG_WITH_ANNOTATE 0

#define ID_MAX   255
static int current_id = 0;
#define generate_id() (++current_id % ID_MAX)

/* Flowtables */
static sdn_ft_match_rule_t *default_match  = NULL;
static sdn_ft_action_rule_t *default_action = NULL;
LIST(whitelist);
LIST(flowtable);
MEMB(entries_memb, sdn_ft_entry_t, SDN_FT_MAX_ENTRIES);
MEMB(matches_memb, sdn_ft_match_rule_t, SDN_FT_MAX_MATCHES);
MEMB(actions_memb, sdn_ft_action_rule_t, SDN_FT_MAX_ACTIONS);
MEMB(data_memb, uint8_t, SDN_FT_DATA_MEMB_SIZE);

static uint8_t e_memb_len = 0;
static uint8_t m_memb_len = 0;
static uint8_t a_memb_len = 0;

/* Prototypes */
int sdn_ft_rm_entry(sdn_ft_entry_t *entry);
static int default_cmp(sdn_ft_entry_t *e);
/*---------------------------------------------------------------------------*/
/*                            Memory Management                              */
/*---------------------------------------------------------------------------*/
static void *
data_alloc(uint8_t size)
{
  int i;
  void *data = NULL;
  for(i = 0; i < size; i++) {
    data = memb_alloc(&data_memb);
    if(data == NULL) {
      LOG_ERR("FAILED to allocate data!\n");
    }
  }
  /* Need to return -1 on the size otherwise we get a pointer to the element
     BEFORE the one we actually need. (i.e. we would get 0xaaaa rather than
     0xaaab) */
  return (data-(size-1));
}

/*---------------------------------------------------------------------------*/
static int
data_free(void *data, uint8_t size) {
  int i, res = -1;
  for(i = 0; i < size; i++) {
    res = memb_free(&data_memb, data + i);
    if (res != 0) {
      LOG_ERR("FAILED to free data (%d bytes)! \n", size);
    }
  }
  return res;
}

/*---------------------------------------------------------------------------*/
static sdn_ft_match_rule_t *
match_allocate()
{
  sdn_ft_match_rule_t *m;
  m = memb_alloc(&matches_memb);
  if(m == NULL) {
    LOG_ERR("FAILED to allocate a match! (%d/%d)\n",
             m_memb_len, SDN_FT_MAX_MATCHES);
  }
  m_memb_len++;
  return m;
}

/*---------------------------------------------------------------------------*/
static void
match_free(sdn_ft_match_rule_t *m)
{
  int res;
  /* Free any match data */
  res = data_free(m->data, m->len);
  if (res != 0){
    LOG_ERR("FAILED to match data! Reference count: %d\n",res);
  }
  /* Free the match */
  res = memb_free(&matches_memb, m);
  if (res != 0){
    LOG_ERR("FAILED to free a match! Reference count: %d\n",res);
  } else {
    m_memb_len--;
  }
}

/*--------------------------------------------------------------------------*/
static sdn_ft_action_rule_t *
action_allocate(void)
{
  sdn_ft_action_rule_t *a;
  a = memb_alloc(&actions_memb);
  if(a == NULL) {
    LOG_ERR("FAILED to allocate an action! (%d/%d)\n",
              a_memb_len, SDN_FT_MAX_ACTIONS);
    return NULL;
  }
  memset(a, 0, sizeof(*a));
  a_memb_len++;
  return a;
}

/*---------------------------------------------------------------------------*/
static void
action_free(sdn_ft_action_rule_t *a)
{
  int res;
  /* Free any action data */
  res = data_free(a->data, a->len);
  if (res != 0){
    LOG_ERR("FAILED to action data! Reference count: %d\n",res);
  }
  /* Free the action */
  res = memb_free(&actions_memb, a);
  if (res != 0){
    LOG_ERR("FAILED to free an action! Reference count: %d\n",res);
  } else {
    a_memb_len--;
  }
}

/*---------------------------------------------------------------------------*/
static sdn_ft_entry_t *
entry_allocate(void)
{
  sdn_ft_entry_t *e;

  /* try to allocate */
  e = memb_alloc(&entries_memb);
  if(e == NULL) {
    /* table is full!!! */
    // TODO: What if the table is full?
    LOG_ERR("FAILED to allocate an entry! (%d/%d)\n",
              e_memb_len, SDN_FT_MAX_ENTRIES);
    return NULL;
  }
  e_memb_len++;
  /* if we have successfully allocated then initialise it, then return */
  memset(e, 0, sizeof(*e));
  e->next = NULL;
  e->match_rule = NULL;
  e->action_rule = NULL;
  // LIST_STRUCT_INIT(e, match_list);
  // LIST_STRUCT_INIT(e, action_list);
  // e->stats.ttl = 0;
  // e->stats.count = 0;
  return e;
}

/*---------------------------------------------------------------------------*/
static void
entry_free(sdn_ft_entry_t *e)
{
  LOG_DBG("Freeing entry (%p)\n", e);
  match_free(e->match_rule);
  action_free(e->action_rule);
  list_remove(flowtable, e);
  int res = memb_free(&entries_memb, e);
  if (res !=0){
    LOG_ERR("FAILED to free an entry! Reference count: %d\n",res);
  } else {
    a_memb_len--;
  }
}

/*---------------------------------------------------------------------------*/
static void
entry_timedout(void *ptr) {
  sdn_ft_entry_t *e = ptr;
  LOG_DBG("TIMEOUT Flowtable entry timed out! (%p)\n", e);
  /* Check to see if this was the default entry */
  if(default_cmp(e)){
    default_match = NULL;
    default_action = NULL;
  }
  /* Remove from list */
  sdn_ft_rm_entry(e);
  /* Free the entry */
  entry_free(e);
  e = NULL;
}

/*---------------------------------------------------------------------------*/
// static void
// purge_flowtable(void)
// {
//   sdn_ft_entry_t *e;
//   sdn_ft_entry_t *next;
//
//   for(e = list_head(flowtable); e != NULL;) {
//     next = e->next;
//     entry_free(e);
//     e = next;
//   }
// }


/*---------------------------------------------------------------------------*/
/*                             Helper Functions                              */
/*---------------------------------------------------------------------------*/
static int
match_cmp(sdn_ft_match_rule_t *a, sdn_ft_match_rule_t *b)
{
  // memcmp returns 0 with equality
  return a->operator == b->operator &&
         a->index == b->index &&
         a->len == b->len &&
         (memcmp(a->data, b->data, a->len) == 0);
}

/*---------------------------------------------------------------------------*/
static int
action_cmp(sdn_ft_action_rule_t *a, sdn_ft_action_rule_t *b)
{
  // memcmp returns 0 with equality
  return a->action == b->action &&
         a->index == b->index &&
         a->len == b->len &&
         (memcmp(a->data, b->data, a->len) == 0);
}

/*---------------------------------------------------------------------------*/
static int
default_cmp(sdn_ft_entry_t *e)
{
  return match_cmp(e->match_rule, default_match) && action_cmp(e->action_rule, default_action);
}

/*---------------------------------------------------------------------------*/
static int
entry_cmp(sdn_ft_entry_t* a, sdn_ft_entry_t* b)
{
  //TODO need to do this for lists of matches and actions
  return match_cmp(a->match_rule, b->match_rule) && action_cmp(a->action_rule, b->action_rule);
}

/*---------------------------------------------------------------------------*/
static int
entry_exists(sdn_ft_entry_t *e) {
  /* Check to see if this entry is already in the table */
  sdn_ft_entry_t *tmp = list_head(flowtable);
  while(tmp != NULL) {
    if (entry_cmp(tmp, e)){
      return 1;
    }
    tmp = tmp->next;
  }
  return 0;
}

/*---------------------------------------------------------------------------*/
/*                           Flowtable Functions                             */
/*---------------------------------------------------------------------------*/
/* Perform match on the data with the entry in the ft. Return
 * 1 if successful, 0 if not.
 */
int
sdn_ft_do_match(sdn_ft_match_rule_t *match_rule, uint8_t *data, uint8_t ext_len)
{
  int ext = 0;
  /* If we are matching on anything above the IP header we need the ext_len */
  if(match_rule->req_ext) {
    ext = ext_len;
  }
  /* Compare the packet bytes with the match bytes at the index specified
     by the match_rule (+ possible extension length) */
  int cmp_result = memcmp(match_rule->data,
                          data + (match_rule->index) + ext,
                          match_rule->len);

  /* memcmp only returns <0, 0 or >0, so we need to do checks based on the
     match operator */
  switch(match_rule->operator) {
    case EQ:
      return (cmp_result == 0);
    case LT_EQ:
      return (cmp_result <= 0);
    case GT_EQ:
      return (cmp_result >= 0);
    case NOT_EQ:
      return (cmp_result != 0);
    case LT:
      return (cmp_result < 0);
    case GT:
      return (cmp_result > 0);
    default:
      return -1; //should never get here
  }
}

/*---------------------------------------------------------------------------*/
uint8_t
sdn_ft_contains(void *data, uint8_t len)
{
  sdn_ft_match_rule_t *m;
  sdn_ft_entry_t *e = list_head(flowtable);
  print_sdn_ft(FLOWTABLE);
  while(e != NULL) {
    m = e->match_rule;
    if(len == m->len && (memcmp(m->data, data, len) == 0)) {
      return 1;
    }
    e = e->next;
  }
  LOG_WARN("No FT match\n");
  return 0;
}

/*---------------------------------------------------------------------------*/
/* Perform a check against the flowtable lists
 */
int
sdn_ft_check_list(list_t list, void *data, uint16_t len, uint8_t ext_len)
{
  /* Search the FT entries for a match with the packet */
  sdn_ft_entry_t *e = list_head(list);
  if(e == NULL) {
    LOG_WARN("FLOWTABLE empty\n");
    return SDN_NO_MATCH;
  }
  while(e != NULL) {
    /* See if we can actually do the check, given the datagram */
    if(len >= (e->match_rule->index + e->match_rule->len)) {
       /* See if we have a successful match */
      if (sdn_ft_do_match(e->match_rule, data, ext_len)) {
#if SDN_CONF_REFRESH_LIFETIME_ON_HIT
        /* If REFRESH_HITS is on, then we need to reset the lifetimer */
        LOG_DBG("RESET entry timer!\n");
        ctimer_restart(&e->lifetimer);
#endif
        /* If the entry matches the datagram, we perform the associated
           action. We then return the results of that action */
        // TODO: What if we have multiple actions?
        LOG_DBG("Match found!\n");
        print_sdn_ft_entry(e);
        LOG_DBG("Returning action!\n");
        return ft_action_handler(e->action_rule, data);
      }
    }
    e = e->next;
  }

  LOG_DBG("No matches in flowtable! Return NO_MATCH\n");
  /* Table was completely emtpy, or we don't know what to do with it */
  return SDN_NO_MATCH;
}

/*---------------------------------------------------------------------------*/
/*                              Default Match                                */
/*---------------------------------------------------------------------------*/
static void
set_default(sdn_ft_match_rule_t *m, sdn_ft_action_rule_t *a)
{
  default_match = m;
  default_action = a;
}

/*---------------------------------------------------------------------------*/
/* Checks to see if a packet matches a default action, and then does a fast
   copy to edit the packet according to that default action. */
uint8_t
sdn_ft_check_default(void *data, uint8_t length, uint8_t ext_len)
{
  if(default_match != NULL && default_action != NULL) {
    /* See if we can actually do the check, given the datagram */
    if(length >= (default_match->index + default_match->len)) {
       /* See if we have a successful match */
      if (sdn_ft_do_match(default_match, data, ext_len)) {
        /* If the match is a hit, we do a fast copy into the datagram */
        LOG_DBG("Default match! Do fast copy...\n");
        return ft_action_handler(default_action, data);
      }
    }
  }
  LOG_WARN("Default not set\n");
  return SDN_NO_MATCH;
}

/*---------------------------------------------------------------------------*/
/*                              Flowtable API                                */
/*---------------------------------------------------------------------------*/
void
sdn_ft_init()
{
  /* Allocate flowtable memory*/
  list_init(whitelist);
  list_init(flowtable);
  memb_init(&entries_memb);
  memb_init(&matches_memb);
  memb_init(&actions_memb);
  memb_init(&data_memb);
  LOG_INFO("FT initialised");
}

int
sdn_ft_add_entry(flowtable_id_t id, sdn_ft_entry_t *entry)
{
  list_t list;
  switch(id) {
    case WHITELIST:
      list = whitelist;
      break;
    case FLOWTABLE:
      list = flowtable;
      break;
    default: return 0;
  }

  if(!entry_exists(entry)){
    /* It wasn't found, so add it */
    list_add(list, entry);
    print_sdn_ft_entry(entry);
    LOG_ANNOTATE("#A %s=%d/%d\n", ((id == FLOWTABLE) ? "ft" : "wl"),
                              list_length(list), SDN_FT_MAX_ENTRIES);
    return 1;
  } else {
    LOG_ERR("Entry %p already exists! Cannot ADD!\n", entry);
  }
  return 0;
}

/*---------------------------------------------------------------------------*/
int
sdn_ft_rm_entry(sdn_ft_entry_t *entry)
{
  sdn_ft_entry_t* tmp;
  /* Whitelist */
  for(tmp = list_head(whitelist); tmp != NULL; tmp = tmp->next) {
    if (entry_cmp(entry, tmp)){
      /* Remove entry from the whitelist */
      list_remove(whitelist, entry);
      LOG_DBG("WHITELIST Removed entry (%p) from list\n", entry);
      LOG_ANNOTATE("#A wl=%d/%d\n", list_length(whitelist), SDN_FT_MAX_ENTRIES);
    }
  }
  /* Flowtable */
  for(tmp = list_head(flowtable); tmp != NULL; tmp = tmp->next) {
    if (entry_cmp(entry, tmp)){
      /* Remove entry from the flowtable */
      list_remove(flowtable, entry);
      LOG_DBG("FLOWTBLE Removed entry (%p) from list\n", entry);
      LOG_ANNOTATE("#A ft=%d/%d\n", list_length(flowtable), SDN_FT_MAX_ENTRIES);
    }
  }

  return 0;
}

/*---------------------------------------------------------------------------*/
sdn_ft_entry_t *
sdn_ft_create_entry(flowtable_id_t id,
                    sdn_ft_match_rule_t *match,
                    sdn_ft_action_rule_t *action,
                    clock_time_t lifetime,
                    uint8_t is_default)
{
  LOG_DBG("Creating entry\n");
  sdn_ft_entry_t *e = entry_allocate();
  if(e != NULL) {
    e->id = generate_id();
    e->match_rule = match;
    e->action_rule = action;
    if(sdn_ft_add_entry(id, e)) {
      if (lifetime == SDN_FT_INFINITE_LIFETIME) {
        ctimer_stop(&e->lifetimer);
      } else {
        ctimer_set(&e->lifetimer, lifetime, entry_timedout, e);
      }
      /* Check if we are to set this entry as default */
      if(is_default) {
        set_default(e->match_rule, e->action_rule);
      }
      /* Return a pointer to this entry */
      return e;
    }
  }
  /* Either we couldn't allocate e, or we couldn't add it to the FT */
  return NULL;
}
/*---------------------------------------------------------------------------*/
sdn_ft_match_rule_t *
sdn_ft_create_match(sdn_ft_match_op_t operator,
                              uint8_t index,
                              uint8_t len,
                              uint8_t req_ext,
                              void    *data)
{
  sdn_ft_match_rule_t *m = match_allocate();
  if(m != NULL) {
    m->operator = operator;
    m->index = index;
    m->len = len;
    m->req_ext = req_ext;
    /* Allocate ourselves a bunch of bytes for our data */
    m->data = data_alloc(len);
    /* Copy our data over to our flowtable match */
    memcpy(m->data, data, len);
    return m;
  }
  /* We couldn't allocate m */
  return NULL;
}

/*---------------------------------------------------------------------------*/
sdn_ft_action_rule_t *
sdn_ft_create_action(sdn_ft_action_type_t action,
                                  uint8_t index,
                                  uint8_t len,
                                  void    *data)
{
  sdn_ft_action_rule_t *a = action_allocate();
  if(a != NULL) {
    a->action = action;
    a->index = index;
    a->len = len;
    /* Allocate ourselves a bunch of bytes for our data */
    a->data = data_alloc(len);
    /* Copy our data over to our flowtable action */
    memcpy(a->data, data, len);
    return a;
  }
  /* We couldn't allocate a */
  return NULL;
}

/*---------------------------------------------------------------------------*/
void
sdn_ft_register_action_handler(sdn_ft_action_handler_callback_t callback)
{
  /* Set action handler from engine */
  if(callback != NULL) {
    ft_action_handler = callback;
  } else {
    LOG_ERR("Action handler is NULL!\n");
  }
}

/*---------------------------------------------------------------------------*/
int
sdn_ft_check(flowtable_id_t id, void *data, uint16_t len, uint8_t ext_len)
{
  list_t list;
  switch(id) {
    case WHITELIST:
      list = whitelist;
      break;
    case FLOWTABLE:
      list = flowtable;
      break;
    default:
      return -1;
  }
  LOG_DBG("Checking %s\n", (id == WHITELIST) ? "wl" : "ft");
  return sdn_ft_check_list(list, data, len, ext_len);
}

/*---------------------------------------------------------------------------*/
/*                             Print Functions                               */
/*---------------------------------------------------------------------------*/
void
print_sdn_ft_match(sdn_ft_match_rule_t *m)
{
  int i;
  printf("M = OP:");
  if (m != NULL) {
    switch(m->operator){
      case EQ: printf("EQ "); break;
      case NOT_EQ: printf("NOT_EQ "); break;
      case GT: printf("GT "); break;
      case LT: printf("LT "); break;
      case GT_EQ: printf("GT_OR_EQ "); break;
      case LT_EQ: printf("LT_OR_EQ "); break;
      default: printf("UNKNOWN "); break;
    }
    printf("INDEX:%d LEN:%d VAL:[", m->index, m->len);
    if(m->data != NULL) {
      for(i = 0; i < m->len; i++) {
        printf("%x ", ((uint8_t *)m->data)[i]);
      }
    } else {
      printf("NULL\n");
    }
  }
  printf("]\n");
}

/*---------------------------------------------------------------------------*/
void
print_sdn_ft_action(sdn_ft_action_rule_t *a)
{
  int i;
  printf("A = ");
  if(a != NULL) {
    switch(a->action){
      case SDN_FT_ACTION_ACCEPT: printf("ACCEPT "); break;
      case SDN_FT_ACTION_QUERY: printf("QUERY "); break;
      case SDN_FT_ACTION_FORWARD: printf("FORWARD "); break;
      case SDN_FT_ACTION_DROP: printf("DROP "); break;
      case SDN_FT_ACTION_MODIFY: printf("MODIFY "); break;
      case SDN_FT_ACTION_FALLBACK: printf("FALLBACK "); break;
      case SDN_FT_ACTION_SRH: printf("SRH "); break;
      case SDN_FT_ACTION_CALLBACK: printf("CALLBACK "); break;
    }
    printf("INDEX:%d LEN:%d VAL:[", a->index, a->len);
    if(a->data != NULL) {
      for(i = 0; i < a->len; i++) {
        printf("%x ", ((uint8_t *)a->data)[i]);
      }
    } else {
      printf("NULL");
    }
  }
  printf("]\n");
}

/*---------------------------------------------------------------------------*/
void
print_sdn_ft_entry(sdn_ft_entry_t *e)
{
// #if LOG_LEVEL == LOG_LEVEL_DBG
  LOG_DBG("ENTRY: (%p) id:%d ttl: %ld...\n", e, e->id, timer_remaining(&e->lifetimer.etimer.timer));
  sdn_ft_match_rule_t *m = e->match_rule;
  sdn_ft_action_rule_t *a = e->action_rule;
  print_sdn_ft_match(m);
  print_sdn_ft_action(a);
// #endif
}

/*---------------------------------------------------------------------------*/
void
print_sdn_ft(flowtable_id_t id)
{
  list_t list;
  switch (id) {
    case WHITELIST: list = whitelist; break;
    case FLOWTABLE: list = flowtable; break;
    default: return;
  }
  sdn_ft_entry_t *e;
  uint8_t num_entries = list_length(list);
  LOG_DBG("FLOWTBLE =================== Flowtable\n");
  for(e = list_head(list); e != NULL; e = e->next) {
    print_sdn_ft_entry(e);
  }
  LOG_DBG("FLOWTBLE =================== (%d) Entries\n", num_entries);
}
