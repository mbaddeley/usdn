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
 *         Atom SDN Controller: Configuration of applications and connectors.
 * \author
 *         Michael Baddeley <m.baddeley@bristol.ac.uk>
 */
#ifndef __ATOM_CONF_H__
#define __ATOM_CONF_H__

/*---------------------------------------------------------------------------*/
/* Atom main configuration */
/*---------------------------------------------------------------------------*/
/*  Southbound input queue length */
#ifdef ATOM_CONF_BUFFER_MAX
#define ATOM_BUFFER_MAX         ATOM_CONF_BUFFER_MAX
#else
#define ATOM_BUFFER_MAX         3
#endif

/* All atom apps */
#define ATOM_APPS { &app_route_sp, &app_route_rpl } // , &app_agg
/* All atom sb connectors */
#define ATOM_SB_CONNECTORS { &sb_usdn, &sb_rpl }

/*---------------------------------------------------------------------------*/
/* Atom netstate configuration */
/*---------------------------------------------------------------------------*/
/* Max number of nodes Atom can monitor */
#ifdef ATOM_CONF_MAX_NODES
#define ATOM_MAX_NODES           ATOM_CONF_MAX_NODES
#else
#define ATOM_MAX_NODES           42
#endif

/*---------------------------------------------------------------------------*/
/* usdn southbound connection configuration */
/*---------------------------------------------------------------------------*/
/* Connection ports for usdn-udp */
#ifdef ATOM_CONF_USDN_LPORT
#define ATOM_USDN_LPORT         ATOM_CONF_USDN_LPORT
#else
#define ATOM_USDN_LPORT         1234
#endif

#ifdef ATOM_CONF_USDN_RPORT
#define ATOM_USDN_RPORT         ATOM_CONF_USDN_RPORT
#else
#define ATOM_USDN_RPORT         4321
#endif

/* Atom apps in use by usdn southbound connector */
/* Network updates */
#ifdef ATOM_CONF_NETUPDATE_APPS_USDN
#define ATOM_NETUPDATE_APPS_USDN ATOM_CONF_NETUPDATE_APPS_USDN
#else
#define ATOM_NETUPDATE_APPS_USDN { NULL } //&app_agg }
#endif

/* Routing */
#ifdef ATOM_CONF_ROUTING_APPS_USDN
#define ATOM_ROUTING_APPS_USDN ATOM_CONF_ROUTING_APPS_USDN
#else
#define ATOM_ROUTING_APPS_USDN { &app_route_sp } //, &app_route_rpl }
#endif

/* Joining apps */
#ifdef ATOM_CONF_JOIN_APPS_USDN
#define ATOM_JOIN_APPS_USDN ATOM_CONF_JOIN_APPS_USDN
#else
#define ATOM_JOIN_APPS_USDN { NULL }
#endif


/* Atom usdn app matrix. This needs to be the same size and same order as the
   action enum */
#define ATOM_APP_MATRIX_USDN { ATOM_NETUPDATE_APPS_USDN, \
                               ATOM_ROUTING_APPS_USDN,   \
                               ATOM_JOIN_APPS_USDN}

/*---------------------------------------------------------------------------*/
/* RPL southbound connection configuration */
/*---------------------------------------------------------------------------*/
#ifdef ATOM_CONF_NETUPDATE_APPS_RPL
#define ATOM_NETUPDATE_APPS_RPL ATOM_CONF_NETUPDATE_APPS_RPL
#else
#define ATOM_NETUPDATE_APPS_RPL { NULL }
#endif

#ifdef ATOM_CONF_ROUTING_APPS_RPL
#define ATOM_ROUTING_APPS_RPL ATOM_CONF_ROUTING_APPS_RPL
#else
#define ATOM_ROUTING_APPS_RPL { NULL }
#endif

#ifdef ATOM_CONF_JOIN_APPS_RPL
#define ATOM_JOIN_APPS_RPL ATOM_CONF_JOIN_APPS_RPL
#else
#define ATOM_JOIN_APPS_RPL { &app_join_cfg }
#endif

#define ATOM_APP_MATRIX_RPL { ATOM_NETUPDATE_APPS_RPL, \
                              ATOM_ROUTING_APPS_RPL,   \
                              ATOM_JOIN_APPS_RPL}

#endif /* __ATOM_CONF_H__ */
