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
 *         uSDN Core: Packet buffer to store manage packets during the
 *                    controller query/response process.
 * \author
 *         Michael Baddeley <m.baddeley@bristol.ac.uk>
 */
#ifndef SDN_PACKETBUF_H_
#define SDN_PACKETBUF_H_

/* Packet buffer stuff so we can save packets while we query controller */
#ifdef SDN_CONF_PACKETBUF_LIFETIME
#define SDN_PACKETBUF_LIFETIME CLOCK_SECOND * SDN_CONF_PACKETBUF_LIFETIME
#else
#define SDN_PACKETBUF_LIFETIME CLOCK_SECOND * 4
#endif

#ifdef SDN_CONF_PACKETBUF_LEN
#define SDN_PACKET_BUF_LEN SDN_CONF_PACKETBUF_LEN
#else
#define SDN_PACKET_BUF_LEN 1
#endif

typedef enum {
  SDN_PB_PRINT_DFLT,
  SDN_PB_PRINT_UDP
} sdn_pb_print_level_t;

/* SDN buffer packetd */
typedef struct sdn_bufpkt {
  struct sdn_packet *next;
  uint8_t id;
  uint8_t packet_buf[UIP_BUFSIZE - UIP_LLH_LEN];
  uint16_t buf_len;
  uint8_t ext_len;
  struct memb *memb;
  list_t list;
  struct ctimer lifetimer;
} sdn_bufpkt_t;

/*---------------------------------------------------------------------------*/
/* uSDN Packet Buffer API */
sdn_bufpkt_t *sdn_pbuf_allocate(struct memb *memb, list_t list, clock_time_t lifetime, uint8_t *id);
void sdn_pbuf_free(sdn_bufpkt_t *p);
void sdn_pbuf_set(sdn_bufpkt_t *p, uint8_t *buf, uint16_t buf_len, uint8_t ext_len);
sdn_bufpkt_t *sdn_pbuf_find(list_t list, uint8_t id);
sdn_bufpkt_t *sdn_pbuf_contains(list_t list, uint8_t *buf, uint16_t buf_len, uint8_t *index, uint8_t *len);
uint8_t *sdn_pbuf_buf(sdn_bufpkt_t *p);
uint16_t sdn_pbuf_buflen(sdn_bufpkt_t *p);
uint8_t sdn_pbuf_extlen(sdn_bufpkt_t *p);
int sdn_pbuf_length(list_t list);
void print_sdn_pbuf_packet(sdn_bufpkt_t *p, sdn_pb_print_level_t level);
void print_sdn_pbuf(list_t list);

#endif /* SDN_PACKETBUF_H_ */
