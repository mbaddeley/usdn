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
 *         uSDN Stack: The adapter provides a connection interface to the
 *                     controller for a specific protocol. Currently this is
 *                     for the μSDN protocol.
 * \author
 *         Michael Baddeley <m.baddeley@bristol.ac.uk>
 */
#include <stdio.h>
#include <string.h>

#include "contiki.h"
#include "lib/memb.h"
#include "lib/list.h"

#include "net/ip/uip.h"
#include "net/ip/udp-socket.h"
#include "net/ip/uip-udp-packet.h"

#include "sdn.h"
#include "sdn-cd.h"

#include "usdn.h"

/* Log configuration */
#include "sys/log-ng.h"
#define LOG_MODULE "SDN-A"
#define LOG_LEVEL LOG_LEVEL_SDN

/* Data buffer to avoid the uIP data being messed with by the ENGINE */
static uint8_t databuffer[UIP_BUFSIZE];
#define UIP_IP_BUF   ((struct uip_udpip_hdr *)&uip_buf[UIP_LLH_LEN])

/* Connection list */
#define USDN_MAX_CONNS 1
LIST(connlist);
MEMB(conn_memb, usdn_conn_t, 2);
static uint8_t conn_memb_len = 0;

PROCESS(usdn_adapter_process, "uSDN ADAPTER Process\n");
/*---------------------------------------------------------------------------*/
/* Memory management */
/*---------------------------------------------------------------------------*/
static usdn_conn_t *
conn_allocate()
{
  usdn_conn_t *conn;
  conn = memb_alloc(&conn_memb);
  if(conn == NULL) {
    LOG_ERR("FAILED to allocate a connection! (%d/%d)\n",
             conn_memb_len, USDN_MAX_CONNS);
  }
  conn_memb_len++;
  return conn;
}

/*---------------------------------------------------------------------------*/
// static int
// conn_free(usdn_conn_t *conn)
// {
//   int res;
//   res = memb_free(&conn_memb, conn);
//   if (res != 0){
//     LOG_ERR("SDN-FTBL\n", "FAILED to free a match! Reference count: %d\n",res);
//   } else {
//     conn_memb_len--;
//   }
//   return res;
// }

/*---------------------------------------------------------------------------*/
usdn_conn_t *
get_conn(sdn_controller_t *c)
{
  usdn_conn_t *conn = list_head(connlist);
  while(conn != NULL) {
    if(conn->controller == c) {
      return conn;
    }
  }
  LOG_ERR("No connection for controller\n");
  return NULL;
}

/*---------------------------------------------------------------------------*/
/* ADAPTER API */
/*---------------------------------------------------------------------------*/
void
init(void)
{
  list_init(connlist);
  memb_init(&conn_memb);
  process_start(&usdn_adapter_process, NULL);
}

/*---------------------------------------------------------------------------*/
static void
register_controller(sdn_controller_t *c)
{
  usdn_conn_t *conn;
  udp_data_t udp_info;

  LOG_INFO("Connect to controller... \n");

  /* Allocate a connection */
  if ((conn = conn_allocate()) != NULL) {
    list_add(connlist, conn);
  } else {
    return;
  }

  /* Use the conn info to create a conn connection */
  if(c != NULL) {
    /* Register the connection using the connection information */
    switch(c->conn_type) {
      case SDN_CONN_TYPE_USDN:
        /* Get the udp data */
        memcpy(&udp_info, &c->conn_data, sizeof(udp_data_t));
        // udp_info = (udp_data_t *)&c->conn_data;
        /* Assign the connection info */
        conn->lport = udp_info.rport;
        conn->rport = udp_info.lport;
        conn->state = SDN_CONN_STATE_REGISTER;
        conn->controller = c;
        LOG_DBG("usdn conn on ports %d/%d\n", conn->lport, conn->rport);
        /* Get a udp connection */
        PROCESS_CONTEXT_BEGIN(&usdn_adapter_process);
        conn->udp = udp_new(NULL, UIP_HTONS(conn->rport), conn); /* set remote port */
        if(conn->udp == NULL){
          LOG_ERR("No udp conn available!\n");
          conn->state = SDN_CONN_STATE_ERROR;
          LOG_INFO("Connection state ...ERRROR\n");
          return;
        }
        PROCESS_CONTEXT_END();
        LOG_DBG("usdn conn binding to port %d\n", conn->lport);
        udp_bind(conn->udp, UIP_HTONS(conn->lport)); /* set local port */
        break;

      default:
        LOG_ERR("Unknown connection type!\n");
        return;
    }
    /* We have successfully connected */
    conn->state = SDN_CONN_STATE_REGISTERED;
    LOG_INFO("Connection state ...REGISTERED\n");
  } else {
    conn->state = SDN_CONN_STATE_DISCONNECTED;
    LOG_ERR("Couldn't register NULL controller\n");
  }
}

/*---------------------------------------------------------------------------*/
static void
send(sdn_controller_t *c, uint8_t datalen, void *data)
{
  // LOG_TRC("Sending to [");
  // LOG_TRC_6ADDR(&c->ipaddr);
  // LOG_TRC_("]\n");
  if(c != NULL) {
    usdn_conn_t *conn = get_conn(c);
    if(conn != NULL) {
      uip_udp_packet_sendto(conn->udp,
                            data,
                            datalen,
                            &c->ipaddr,
                            UIP_HTONS(conn->rport));
    } else {
      LOG_ERR("Conn is NULL!\n");
    }
  } else {
    LOG_ERR("Controller is NULL!\n");
  }
}

/*---------------------------------------------------------------------------*/
/* uSDN ADAPTER Process */
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(usdn_adapter_process, ev, data)
{
  PROCESS_BEGIN();
  LOG_INFO("Adapter process started!\n");

  while(1) {
    /* Wait for event */
    PROCESS_YIELD();
    if (ev == tcpip_event) {
      // PRINTTRC("Got tcpip_event!\n");
      usdn_conn_t *conn = (usdn_conn_t *)data;
      /* Defensive coding: although the conn *should* be non-null
         here, we make sure to avoid the program crashing on us. */
      if (conn != NULL) {
        /* Copy the data from the uIP data buffer into our own
           buffer to avoid the uIP buffer being messed with by the
           callee. */
        if(uip_newdata()) {
          memcpy(databuffer, uip_appdata, uip_datalen());
          SDN_ENGINE.in(databuffer, uip_datalen(), conn->controller);
        } else {
          LOG_ERR("No new data at uip...\n");
        }
      } else {
        LOG_ERR("conn is null!\n");
      }
    /*********************************************/
    } else {
      LOG_ERR("Unknown event (0x%x) :(\n", ev);
    }
  }
  PROCESS_END();
}

/*---------------------------------------------------------------------------*/
/* SDN Adapter API */
/*---------------------------------------------------------------------------*/
const struct sdn_adapter usdn_adapter = {
  init,
  register_controller,
  send,
};
