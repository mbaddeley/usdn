#ifndef CONTROLLER_CONF_H_
#define CONTROLLER_CONF_H_

/* Set ourselves up as a control node */
#define SDN_CONF_CONTROLLER             SDN_CONTROLLER_ATOM

/* Max nodes for the controller network state. Careful, the links_memb
   can get big! */
#define KBC_CONF_MAX_NODES              32

#endif /* CONTROLLER_CONF_H_ */
