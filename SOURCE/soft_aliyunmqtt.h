#pragma once
#ifndef _SOFT_ALIYUNMQTT_H
#define _SOFT_ALIYUNMQTT_H_

#include "mqtt_api.h"
#include "thread"
#include "cJSON/cJSON.h"
class MyAliyunMqtt{
public:
	MyAliyunMqtt();
	int MqttInit(char* host, int port, char* clientid, char* username, char* password);

	~MyAliyunMqtt();

	/*
	���ĺ���
	handle�����,pClient
	*/
	int subscribe(char* subscribetopic, int Qos);
	int publish(char* publishtopic, int Qos,cJSON* payload);
	int openmainthread();
	int openintervalthread();
	int openrecparsethread();
	std::string topic;
	std::string messageid;
	std::string devicename;
	std::string productkey;
private:
	void* pclient;
	char payload[1024];
	bool getmessage = false;
	bool getrrpcmessage = false;
	static void event_handle(void* pcontext, void* pclient, iotx_mqtt_event_msg_pt msg);
	static void message_arrive(void* pcontext, void* pclient, iotx_mqtt_event_msg_pt msg);

	std::thread threadid;
	std::thread threadid_interval;
	std::thread threadid_recparse;

	static int MqttMain(void* Param);
	static int MqttInterval(void* Param);
	static int MqttRecParse(void* Param);
};

#endif
