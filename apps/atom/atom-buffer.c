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
 *         Atom SDN Controller: Buffer for ingress and egress packets.
 * \author
 *         Michael Baddeley <m.baddeley@bristol.ac.uk>
 */
#include "contiki.h"

#include "lib/memb.h"
#include "lib/list.h"

#include "net/ip/uip.h"

#include "atom.h"

/* Log configuration */
#include "sys/log-ng.h"
#define LOG_MODULE "ATOM"
#define LOG_LEVEL LOG_LEVEL_ATOM

/* Memory for incomming packets */
MEMB(atom_msg_memb, atom_msg_t, ATOM_BUFFER_MAX);
LIST(atom_msg_list);

/* Data structure to hold the list and buffer */
static atom_queue_t queue;

/* Controller buffer pointers */
#define C_BUF            ((uint8_t *)&c_buf[UIP_LLH_LEN])
/* UIP buffer pointers */
#define UIP_BUF          ((uint8_t *)&uip_buf[UIP_LLH_LEN])
#define UIP_IP_BUF       ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])

/* Our own input buffer. Firstly so we don't mess up uip. Secondly to solve
   word alignment issues when we use the sdn_packetbuf (which doesn't store
   packets as alligned) */
uip_buf_t c_alligned_buf;
uint16_t  c_len;
uint8_t   c_ext_len;
uint16_t  c_slen;
uint8_t   c_hops;

/* Action buffer */
static atom_action_t action_buf;
/* Response buffer */
static atom_response_t response_buf;

/*---------------------------------------------------------------------------*/
#define ID_MAX   255
/* Buffer messsage ID */
static int msg_id = 0;
#define atom_msg_generate_id() (++msg_id % ID_MAX)
/* Action response ID */
static int ar_id = 0;
#define atom_ar_generate_id() (++ar_id % ID_MAX)

/*---------------------------------------------------------------------------*/
/* Atom southbound input buffer */
/*---------------------------------------------------------------------------*/
void
atom_buffer_init()
{
  /* Initialise memb and list */
  memb_init(&atom_msg_memb);
  list_init(atom_msg_list);
  /* Initialise queue data structure */
  queue.buf = &atom_msg_memb;
  queue.list = atom_msg_list;
  queue.size = ATOM_BUFFER_MAX;
  /* Initialise action and response buffers */
  memset(&action_buf, 0, sizeof(action_buf));
  memset(&response_buf, 0, sizeof(response_buf));

  LOG_INFO("Atom buffer initialised\n");
}

/*---------------------------------------------------------------------------*/
void
atom_buffer_add(struct atom_sb *sb_connector)
{
  atom_msg_t *m = memb_alloc(queue.buf);
  if (m != NULL) {
    /* Populate the message */
    m->next = NULL;
    m->id = atom_msg_generate_id();
    memcpy(&m->buf, UIP_BUF, uip_len);
    m->buf_len = uip_len;
    m->ext_len = uip_ext_len;
    m->sb = sb_connector;
    m->hops = uip_ds6_if.cur_hop_limit - UIP_IP_BUF->ttl;
    /* Add the message to the underlying list */
    LOG_DBG("Copy uip_buf (len=%d , ext=%d) to queue (id=%d)\n",
             uip_len, uip_ext_len, m->id);
    list_add(queue.list, m);
    LOG_ANNOTATE("#A cb=%d/%d\n", list_length(queue.list), queue.size);
  } else {
    LOG_ERR("List full (%d/%d)\n", list_length(queue.list), queue.size);
  }
}

/*---------------------------------------------------------------------------*/
atom_msg_t *
atom_buffer_head(void)
{
  /* Get the head of the underlying list */
  atom_msg_t *m = list_head(queue.list);
  if(m != NULL) {
    LOG_DBG("Copy queue (id=%d, len=%d, ext=%d) to c_buf (len=%d, ext=%d)\n",
             m->id, m->buf_len, m->ext_len, uip_len, uip_ext_len);
    /* Copy to cbuf */
    c_len = m->buf_len;
    c_ext_len = m->ext_len;
    memcpy(C_BUF, m->buf, m->buf_len);
    c_hops = m->hops;
  } else {
    LOG_ERR("List empty (%d/%d)\n", list_length(queue.list), queue.size);
  }

  return m;
}

/*---------------------------------------------------------------------------*/
void
atom_buffer_remove(atom_msg_t *m)
{
  /* Remove message from list */
  list_remove(queue.list, m);
  /* Free message from memory */
  memb_free(queue.buf, m);
  LOG_ANNOTATE("#A cb=%d/%d\n", list_length(queue.list), queue.size);
  /* Clear cbuf */
  c_clear_buf();
}

/*---------------------------------------------------------------------------*/
void
atom_buffer_copy_cbuf_to_uip(void)
{
  /* Copy the sdn_buf into the uip buffer */
  LOG_DBG("COPY c_buf (len=%d) (ext=%d) to uip_buf (len=%d) (ext=%d)\n",
    c_len, c_ext_len, uip_len, uip_ext_len);
  memcpy(uip_buf, &c_buf, c_len);
  uip_len = c_len;
  uip_ext_len = c_ext_len;
}


/*---------------------------------------------------------------------------*/
/* Action and response buffers */
/*---------------------------------------------------------------------------*/
atom_action_t *
atom_action_buf_copy_to(atom_action_type_t type, void *data)
{
  /* Get length from type */
  switch(type) {
    case ATOM_ACTION_NETUPDATE:
      action_buf.datalen = sizeof(atom_netupdate_action_t);
      break;
    case ATOM_ACTION_ROUTING:
      action_buf.datalen = sizeof(atom_routing_action_t);
      break;
    case ATOM_ACTION_JOIN:
      action_buf.datalen = sizeof(atom_routing_action_t);
      break;
    default:
      return NULL;
  }
  /* Copy id */
  action_buf.id = atom_ar_generate_id();
  /* Copy type */
  action_buf.type = type;
  /* Copy data */
  if(action_buf.datalen != 0) {
    memcpy(&action_buf.data, data, action_buf.datalen);
  }

  return &action_buf;
}

/*---------------------------------------------------------------------------*/
void
atom_action_buf_clear(void)
{
  /* Clear action data memory */
  memset(&action_buf.data, 0, ATOM_ACTION_BUFSIZE);
}

/*---------------------------------------------------------------------------*/
atom_response_t *
atom_response_buf_copy_to(atom_response_type_t type, void *data)
{
  /* Get length from type */
  switch(type) {
    case ATOM_RESPONSE_ROUTING:
      response_buf.datalen = sizeof(atom_routing_response_t);
      break;
    case ATOM_RESPONSE_ACK:
    case ATOM_RESPONSE_NACK:
      response_buf.datalen = 0;
      break;
    case ATOM_RESPONSE_CFG:
      response_buf.datalen = 0; //sizeof(atom_configure_response_t);
      break;
    default:
      return NULL;
  }
  /* Copy type */
  response_buf.type = type;
  /* Copy data */
  if(response_buf.datalen != 0) {
    memcpy(&response_buf.data, data, response_buf.datalen);
  }

  // TODO: ID

  return &response_buf;
}

/*---------------------------------------------------------------------------*/
void
atom_response_buf_clear(void)
{
  /* Set type to NONE */
  // response_buf.type = ATOM_RESPONSE_NONE;
  /* Clear memory */
  memset(&response_buf.data, 0, ATOM_RESPONSE_BUFSIZE);
}
