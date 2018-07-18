#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

#if UIP_CONF_IPV6_SDN
#include "sdn-stack.h"
#endif

/* Include controller header */
#ifdef CONTROLLER_CONF_PATH
#include CONTROLLER_CONF_PATH
#endif /* CONTROLLER_CONF_PATH */

/*---------------------------------------------------------------------------*/
/* !!! THERE ARE ALSO SOME CONFIGUARTION OPTIONS IN atom-conf.h WHICH SHOULD BE
   INSPECTED BEFORE TRYING TO RUN THIS APPLICATION !!! */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* SDN Core Configuration */
/*---------------------------------------------------------------------------*/
#if UIP_CONF_IPV6_SDN
/* SDN Stats */
#define SDN_CONF_STATS                      1

/* SDN Stack */
#define SDN_CONF_DRIVER                     SDN_DRIVER_USDN
#define SDN_CONF_ENGINE                     SDN_ENGINE_USDN
#define SDN_CONF_ADAPTER                    SDN_ADAPTER_USDN

/* SDN behaviour coniguration options */

/* SDN Packetbuffer Configuration */
#ifdef CONF_NUM_APPS
#define SDN_CONF_PACKETBUF_LEN              CONF_NUM_APPS
#else
#define SDN_CONF_PACKETBUF_LEN              1
#endif
#define SDN_CONF_PACKETBUF_LIFETIME         20      /* seconds. */

/* Allows a callback after packet sent in sicslowpan */
#define SDN_SICSLOWPAN_CALLBACK             0

/* Sets the the first flowtable entry as a default (fast copy) entry */
#define SDN_CONF_DEFAULT_FT_ENTRY           1

/* Options already in makefile */
#ifndef SDN_CONF_FT_LIFETIME
#define SDN_CONF_FT_LIFETIME                300  /* seconds. 0xFFFF = inf */
#endif
#ifndef SDN_CONF_CONTROLLER_UPDATE_PERIOD
#define SDN_CONF_CONTROLLER_UPDATE_PERIOD   60      /* seconds */
#endif
#ifndef SDN_CONF_FORCE_UPDATE
#define SDN_CONF_FORCE_UPDATE               1
#endif
#ifndef SDN_CONF_RETRY_AFTER_QUERY
#define SDN_CONF_RETRY_AFTER_QUERY          0       /* N.B. This increases average lat */
#endif

#endif /* UIP_CONF_IPV6_SDN */

/*---------------------------------------------------------------------------*/
/* Memory Optimisations to improve memory usage */
/*---------------------------------------------------------------------------*/
// #define PROCESS_CONF_NO_PROCESS_NAMES 1
#undef UIP_CONF_DS6_DEFRT_NBU
#define UIP_CONF_DS6_DEFRT_NBU              1
/* These can be fiddled with depending on what you're trying to do. Note that
   if there are any packet sizes > 117B you'll need the frag on */
#undef NBR_TABLE_CONF_MAX_NEIGHBORS
#define NBR_TABLE_CONF_MAX_NEIGHBORS        8
#undef SICSLOWPAN_CONF_FRAG
#define SICSLOWPAN_CONF_FRAG                0
#undef UIP_CONF_TCP
#define UIP_CONF_TCP                        0

/*---------------------------------------------------------------------------*/
/* RPL Configuration */
/*---------------------------------------------------------------------------*/
// #undef NULLRDC_CONF_802154_AUTOACK
// #define NULLRDC_CONF_802154_AUTOACK              1

#define RPL_CONF_STATS                           0
#define RPL_CONF_DEFAULT_LIFETIME_UNIT           60 /* seconds */
#define RPL_CONF_DEFAULT_ROUTE_INFINITE_LIFETIME 1
/* Can be configured through usdn CFG message */
#define RPL_CONF_DEFAULT_LIFETIME                10 /* minutes */
#define RPL_CONF_DIO_INTERVAL_MIN                12
#define RPL_CONF_DAG_LIFETIME                    3

/* RPL Non-Storing */
#if RPL_MODE_NS
  #undef RPL_CONF_MOP
  #define RPL_CONF_MOP                           RPL_MOP_NON_STORING /* RPL Mode of operation*/
  #undef RPL_NS_CONF_LINK_NUM
  #define RPL_NS_CONF_LINK_NUM                   50 /* Number of links maintained at the root */
  #undef UIP_CONF_MAX_ROUTES
  #define UIP_CONF_MAX_ROUTES                    0  /* No need for node routes */
#endif /* WITH_NON_STORING */

/*---------------------------------------------------------------------------*/
/* Contiki Configuration */
/*---------------------------------------------------------------------------*/
#define UIP_CONF_ROUTER                          1

// #undef UIP_CONF_ND6_SEND_NS
// #undef UIP_CONF_ND6_SEND_NA
// #undef UIP_CONF_ND6_SEND_RA
// #define UIP_CONF_ND6_SEND_NS        0
// #define UIP_CONF_ND6_SEND_NA        0
// #define UIP_CONF_ND6_SEND_RA        0

#endif /* PROJECT_CONF_H_ */
