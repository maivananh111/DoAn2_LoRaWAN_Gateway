/*
 * ethconn.h
 *
 *  Created on: Mar 31, 2023
 *      Author: anh
 */

#ifndef ETHCONN_ETHCONN_H_
#define ETHCONN_ETHCONN_H_


#ifdef __cplusplus
extern "C"{
#endif

#include "lwip.h"
#include "lwip/netif.h"
#include "lwip/dhcp.h"
#include "lwip/ip_addr.h"
#include "lwip/etharp.h"


#define ETHERNET_LINK_TASK_SIZE_KBYTE 1U
#define ETHERNET_LINK_TASK_PRIORITY   16

typedef enum{
	ETHERNET_EVENT_LINKDOWN = 0,
	ETHERNET_EVENT_LINKUP,
	ETHERNET_EVENT_GOTIP,
} ethernet_event_t;

/**
 * Get global net interface and ethernet handler API.
 */
struct netif *ethernet_get_netif(void);
ETH_HandleTypeDef *ethernet_get_ethernet(void);

/**
 * Initialize and link.
 */
void ethernet_init_link(uint32_t waitIP_timeout);

void ethernet_register_event_handler(void (*link_event_handler_function)(ethernet_event_t));

bool ethernet_linked_up(void);

void ethernet_wait_ip_address(void);
/**
 * Address API.
 */
uint8_t *ethernet_get_mac_address(void);
char *ethernet_get_mac_address_str(void);

void ethernet_set_ip_address(ip_addr_t ip, ip_addr_t netmask, ip_addr_t gateway);
void ethernet_set_ip_address_str(char * ip, char *netmask, char *gateway);
ip_addr_t *ethernet_get_ip_address(void);
char *ethernet_get_ip_address_str(void);

ip_addr_t *ethernet_get_netmask(void);
char *ethernet_get_netmask_str(void);

ip_addr_t *ethernet_get_gateway(void);
char *ethernet_get_gateway_str(void);

/**
 * DHCP API.
 */
void ethernet_dhcp_start(void);
void ethernet_dhcp_stop(void);



#ifdef __cplusplus
}
#endif


#endif /* ETHCONN_ETHCONN_H_ */
