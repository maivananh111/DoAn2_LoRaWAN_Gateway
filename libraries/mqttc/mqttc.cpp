/*
 * mqttc.cpp
 *
 *  Created on: Nov 8, 2023
 *      Author: anh
 */


#include "mqttc/mqttc.h"

#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"

#include "log/log.h"

#include "string.h"



static const char *TAG = "MQTT CLIENT";


#define MQTT_DBG(mes) LOG_MESS(LOG_INFO, TAG, mes)

static void mqttclient_connect_handler(mqtt_client_t *client, void *arg, mqtt_connection_status_t status);
static void mqttclient_topic_data_handler(void *arg, const u8_t *data, u16_t len, u8_t flags);
static void mqttclient_topic_publish_handler(void *arg, const char *topic, u32_t tot_len);
static void mqttclient_topic_sub_handler(void *arg, err_t err);
static void mqttclient_topic_unsub_handler(void *arg, err_t err);

mqttc_t::mqttc_t(void){}

err_t mqttc_t::initialize(mqttc_config_t *pconfig){
	_conf = pconfig;

	/**
	 * Create new MQTT client.
	 */
	if(_mqttc == NULL){
		_mqttc = mqtt_client_new();
		MQTT_DBG("Create new MQTT client.");
		if(_mqttc == NULL){
			MQTT_DBG("Memory exhausted, invalid MQTT client.");
			return ERR_MEM;
		}
	}

	if(_mqttc_info == NULL){
		_mqttc_info = (struct mqtt_connect_client_info_t *)malloc(sizeof(struct mqtt_connect_client_info_t));
		if(_mqttc_info == NULL){
			MQTT_DBG("Memory exhausted, invalid MQTT client info.");
			return ERR_MEM;
		}
		memset(_mqttc_info, 0, sizeof(struct mqtt_connect_client_info_t));

		_mqttc_info->client_id = (char *)malloc((strlen(_conf->client_id) + 1) * sizeof(char));
		if(_mqttc_info->client_id == NULL){
			MQTT_DBG("Memory exhausted, invalid MQTT client identifier.");
			return ERR_MEM;
		}
		memset((void *)_mqttc_info->client_id, '\0', (strlen(_conf->client_id) + 1));
		memcpy((void *)_mqttc_info->client_id, _conf->client_id, strlen(_conf->client_id));

		_mqttc_info->keep_alive = _conf->keep_alive_s;
	}

	/**
	 * Resolve host name to ip address.
	 */
	if(_conf->broker_hostname != NULL){
		struct hostent *host = lwip_gethostbyname(_conf->broker_hostname);
		if (host == NULL) {
			MQTT_DBG("Error resolve host name to address info.");
			return ERR_VAL;
		}
		else {
		    struct in_addr **addr_list = (struct in_addr **)host->h_addr_list;
		    if (addr_list[0] != NULL) {
		    	in_addr_t addr = inet_addr(inet_ntoa(*addr_list[0]));
		    	IP_ADDR4(&_conf->broker_ipaddr, (uint8_t)(addr & 0xff), (uint8_t)((addr >> 8) & 0xff), (uint8_t)((addr >> 16) & 0xff), (uint8_t)((addr >> 24) & 0xff));
		        LOG_EVENT(TAG, "Resolved MQTT broker ip address: %s", ip4addr_ntoa(&_conf->broker_ipaddr));
		    }
		}
	}
	else{
		MQTT_DBG("Invalid broker ip address or host name parameter.");
		return ERR_VAL;
	}

	return ERR_OK;
}

err_t mqttc_t::deinitialize(void){
	mqtt_client_free(_mqttc);
	_mqttc = NULL;

	if(_mqttc_info != NULL){
		if(_mqttc_info->client_id != NULL)
			free((void *)_mqttc_info->client_id);
		_mqttc_info->client_id = NULL;

		if(_mqttc_info->client_user != NULL)
			free((void *)_mqttc_info->client_user);
		_mqttc_info->client_user = NULL;

		if(_mqttc_info->client_pass != NULL)
			free((void *)_mqttc_info->client_pass);
		_mqttc_info->client_pass = NULL;

		if(_mqttc_info != NULL)
			free(_mqttc_info);
		_mqttc_info = NULL;
	}

	MQTT_DBG("Free MQTT client.");

	return ERR_OK;
}


err_t mqttc_t::config_client_auth(mqttc_auth_t *pauth){
	_auth = pauth;

	if(_mqttc == NULL) return ERR_ARG;

	_mqttc_info->client_user = (char *)malloc((strlen(_auth->username) + 1) * sizeof(char));
	if(_mqttc_info->client_user == NULL){
		MQTT_DBG("Memory exhausted, invalid MQTT client user name.");
		return ERR_MEM;
	}
	memset((void *)_mqttc_info->client_user, '\0', (strlen(_auth->username) + 1));
	memcpy((void *)_mqttc_info->client_user, _auth->username, strlen(_auth->username));

	_mqttc_info->client_pass = (char *)malloc((strlen(_auth->password) + 1) * sizeof(char));
	if(_mqttc_info->client_pass == NULL){
		MQTT_DBG("Memory exhausted, invalid MQTT client password.");
		return ERR_MEM;
	}
	memset((void *)_mqttc_info->client_pass, '\0', (strlen(_auth->password) + 1));
	memcpy((void *)_mqttc_info->client_pass, _auth->password, strlen(_auth->password));

	return ERR_OK;
}

err_t mqttc_t::register_event_handler(mqttc_event_handler_f event_handler_function, void *event_arg){
	_event_handler = event_handler_function;
	_evparam = event_arg;

	return ERR_OK;
}

err_t mqttc_t::unregister_event_handler(void){
	_event_handler = NULL;
	_evparam = NULL;

	return ERR_OK;
}

err_t mqttc_t::connect_to_broker(bool auto_reconnect){
	_reconnect = auto_reconnect;

	if(_mqttc == NULL || _mqttc_info == NULL) {
		MQTT_DBG("Invalid MQTT client.");
		return ERR_ARG;
	}

	err_t ret = mqtt_client_connect(_mqttc,         			// Client
									&_conf->broker_ipaddr,		// Internet protocol address
									_conf->port_number,			// Port
									mqttclient_connect_handler, // Connect callback function
									this,						// Argument for callback
									_mqttc_info					// Client info
									);
	mqtt_set_inpub_callback(_mqttc,
			mqttclient_topic_publish_handler,
			mqttclient_topic_data_handler,
			this);

	return ret;
}

err_t mqttc_t::disconnect_to_broker(void){
	_reconnect = false;

	if(_mqttc == NULL || _mqttc_info == NULL) {
		MQTT_DBG("Invalid MQTT client.");
		return ERR_ARG;
	}

	mqtt_disconnect(_mqttc);

	return ERR_OK;
}

bool mqttc_t::isconnected(void){
	if(_mqttc == NULL || _mqttc_info == NULL) {
		MQTT_DBG("Invalid MQTT client.");
		return false;
	}

	return (mqtt_client_is_connected(_mqttc))? true : false;
}


err_t mqttc_t::set_option(uint8_t qos, uint8_t retain){
	_qos = qos;
	_retain = retain;

	if(_mqttc == NULL || _mqttc_info == NULL) {
		MQTT_DBG("Invalid MQTT client.");
		return ERR_ARG;
	}

	return ERR_OK;
}

err_t mqttc_t::publish(const char *topic, const char *topic_data, uint16_t topic_data_len){
	if(_mqttc == NULL || _mqttc_info == NULL) {
		MQTT_DBG("Invalid MQTT client.");
		return ERR_ARG;
	}

	return mqtt_publish(_mqttc, topic, topic_data, topic_data_len, _qos, _retain, NULL, NULL);
}

err_t mqttc_t::publish(const char *topic, const char *topic_data){
	if(_mqttc == NULL || _mqttc_info == NULL) {
		MQTT_DBG("Invalid MQTT client.");
		return ERR_ARG;
	}

	return mqtt_publish(_mqttc, topic, topic_data, strlen(topic_data), _qos, _retain, NULL, NULL);
}

err_t mqttc_t::subcribe(const char *topic){
	if(_mqttc == NULL || _mqttc_info == NULL) {
		MQTT_DBG("Invalid MQTT client.");
		return ERR_ARG;
	}

	return mqtt_sub_unsub(_mqttc, topic, _qos, mqttclient_topic_sub_handler, this, 1);
}

err_t mqttc_t::unsubcribe(const char *topic){
	if(_mqttc == NULL || _mqttc_info == NULL) {
		MQTT_DBG("Invalid MQTT client.");
		return ERR_ARG;
	}

	return mqtt_sub_unsub(_mqttc, topic, _qos, mqttclient_topic_unsub_handler, this, 0);
}


void mqttc_t::event_handler(mqttc_event_t event){
	if(_mqttc == NULL || _mqttc_info == NULL) {
		MQTT_DBG("Invalid MQTT client.");
		return;
	}

	if(_event_handler != NULL) _event_handler(event, _evparam);

	if(event.eventid == MQTTC_EVENT_DISCONNECTED && _reconnect == true)
		mqtt_client_connect(_mqttc,         			// Client
							&_conf->broker_ipaddr,		// Internet protocol address
							_conf->port_number,			// Port
							mqttclient_connect_handler, // Connect callback function
							this,						// Argument for callback
							_mqttc_info					// Client info
							);
}


static void mqttclient_connect_handler(mqtt_client_t *client, void *arg, mqtt_connection_status_t status){
	mqttc_event_t event = {
		.eventid = MQTTC_EVENT_DISCONNECTED,
		.data = NULL,
	};

	mqttc_t *mqtt = (mqttc_t *)arg;

	switch(status){
		case MQTT_CONNECT_ACCEPTED:
			event.eventid = MQTTC_EVENT_CONNECTED;
		break;
		case MQTT_CONNECT_DISCONNECTED:
			event.eventid = MQTTC_EVENT_DISCONNECTED;
		break;
		case MQTT_CONNECT_TIMEOUT:
			event.eventid = MQTTC_EVENT_ERROR_TIMEOUT;
		break;
		case MQTT_CONNECT_REFUSED_PROTOCOL_VERSION:
			event.eventid = MQTTC_EVENT_ERROR_REFUSED_PROTOCOL_VERSION;
		break;
		case MQTT_CONNECT_REFUSED_IDENTIFIER:
			event.eventid = MQTTC_EVENT_ERROR_IDENTIFIER;
		break;
		case MQTT_CONNECT_REFUSED_SERVER:
			event.eventid = MQTTC_EVENT_ERROR_REFUSED_SERVER;
		break;
		case MQTT_CONNECT_REFUSED_USERNAME_PASS:
			event.eventid = MQTTC_EVENT_ERROR_AUTHORIZE;
		break;
		case MQTT_CONNECT_REFUSED_NOT_AUTHORIZED_:
			event.eventid = MQTTC_EVENT_ERROR_ACCESS_DENIED;
		break;
	}

	mqtt->event_handler(event);
}

static void mqttclient_topic_data_handler(void *arg, const u8_t *data, u16_t len, u8_t flags){
	mqttc_t *mqtt = (mqttc_t *)arg;
	mqttc_event_t event = {
		.eventid = MQTTC_EVENT_TOPIC_DATA,
		.data = NULL,
	};

	if(flags & MQTT_DATA_FLAG_LAST){
		event.data = malloc((len+1) * sizeof(char));
		memset(event.data, '\0', len+1);
		memcpy(event.data, data, len);
	}

	mqtt->event_handler(event);

	if(event.data != NULL) free(event.data);
}

static void mqttclient_topic_publish_handler(void *arg, const char *topic, u32_t tot_len){
	mqttc_t *mqtt = (mqttc_t *)arg;
	mqttc_event_t event = {
		.eventid = MQTTC_EVENT_TOPIC_PUBLISHED,
		.data = (void *)topic,
	};

	mqtt->event_handler(event);
}

static void mqttclient_topic_sub_handler(void *arg, err_t err){
	mqttc_t *mqtt = (mqttc_t *)arg;
	mqttc_event_t event = {
		.eventid = MQTTC_EVENT_TOPIC_SUBSCRIBED,
		.data = (void *)&err,
	};

	mqtt->event_handler(event);
}

static void mqttclient_topic_unsub_handler(void *arg, err_t err){
	mqttc_t *mqtt = (mqttc_t *)arg;
	mqttc_event_t event = {
		.eventid = MQTTC_EVENT_TOPIC_UNSUBSCRIBED,
		.data = (void *)&err,
	};

	mqtt->event_handler(event);
}


