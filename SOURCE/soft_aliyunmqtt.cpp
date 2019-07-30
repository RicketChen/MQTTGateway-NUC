#include "soft_aliyunmqtt.h"

#include <iostream>
#include <string>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <queue>

#include "mqtt_api.h"
#include "dev_sign_api.h"
#include "MyData.h"
#include "soft_mymodbus.h"
#include "libserial/Serial.h"
#include "base64/base64.h"
#include "cJSON/cJSON.h"
#include "soft_myfunction.h"

using std::cout;
using std::endl;
using std::string;
using std::map;
using std::queue;

void MyAliyunMqtt::message_arrive(void* pcontext, void* pclient, iotx_mqtt_event_msg_pt msg)
{
	MyAliyunMqtt* pointer = (MyAliyunMqtt*)pcontext;
	iotx_mqtt_topic_info_t* topic_info = (iotx_mqtt_topic_info_pt)msg->msg;
	switch (msg->event_type)
	{
	case IOTX_MQTT_EVENT_PUBLISH_RECEIVED:
	{
		/* print topic name and topic message */
		printf("Message Arrived: \n");
		printf("Topic  : %.*s\n", topic_info->topic_len, topic_info->ptopic);
		printf("Payload: %.*s\n\n", topic_info->payload_len, topic_info->payload);
		pointer->getmessage = true;
		strcpy(pointer->payload, topic_info->payload);

		pointer->topic.clear();
		pointer->messageid = "0";
		pointer->topic.assign(topic_info->ptopic, 0, topic_info->topic_len);
		size_t pos = pointer->topic.find("request/", 0);
		if (pos != std::string::npos)
		{
			pointer->messageid = pointer->topic.substr(pos + strlen("request/"));
		}
		break;
	}
	default:
		break;
	}
}
int MyAliyunMqtt::subscribe(char* subscribetopic, int Qos)
{
	int res = 0;
	iotx_mqtt_qos_t iotx_qos;
	if (Qos == 0)
		iotx_qos = IOTX_MQTT_QOS0;
	else if (Qos == 1)
		iotx_qos = IOTX_MQTT_QOS1;
	else if (Qos == 2)
		iotx_qos = IOTX_MQTT_QOS2;
	else
	{
		printf("Unsupport Qos Setting!\n");
		return -1;
	}
	printf("subscribe topic is %s\n", subscribetopic);

	res = IOT_MQTT_Subscribe(pclient, subscribetopic, iotx_qos, message_arrive, this);
	if (res < 0) {
		cout << BOLDRED << "Subscribe Topic fail!" << RESET << endl;
	}
	else
	{
		cout << BOLDYELLOW << "Subscribe Topic SUCCESS!" << RESET << endl;
	}
	return 0;
}

int MyAliyunMqtt::publish(char* publishtopic, int Qos, cJSON* json_payload)
{
	int res = -1;
	iotx_mqtt_topic_info_t topic_msg;
	char* publish_payload = (char*)malloc(strlen(cJSON_Print(json_payload)) + 1);

	publish_payload = cJSON_PrintUnformatted(json_payload);
	topic_msg.qos = Qos;
	topic_msg.retain = 0;
	topic_msg.dup = 0;
	topic_msg.payload = publish_payload;
	topic_msg.payload_len = strlen(publish_payload);

	res = IOT_MQTT_Publish(pclient, publishtopic, &topic_msg);

	cout << BOLDGREEN << "Publish to topic : " << publishtopic << endl;
	publish_payload = cJSON_Print(json_payload);
	if (res < 0)
	{
		cout << BOLDRED << "FAILED!" << endl << endl;
		cout << BOLDYELLOW << "Paylod is :\n " << BOLDRED << publish_payload << RESET << endl;
		free(publish_payload);
		return -1;
	}
	cout << BOLDYELLOW << "SUCCESS!" << endl << endl;
	cout << BOLDGREEN << "Paylod is -> " << publish_payload << RESET << endl;
	free(publish_payload);
	return 0;
}
void MyAliyunMqtt::event_handle(void* pcontext, void* pclient, iotx_mqtt_event_msg_pt msg)
{
	//	printf("msg->event_type : %d\n", msg->event_type);
}

/*
 *  NOTE: About demo topic of /${productKey}/${deviceName}/get
 *
 *  The demo device has been configured in IoT console (https://iot.console.aliyun.com)
 *  so that its /${productKey}/${deviceName}/get can both be subscribed and published
 *
 *  We design this to completely demonstrate publish & subscribe process, in this way
 *  MQTT client can receive original packet sent by itself
 *
 *  For new devices created by yourself, pub/sub privilege also requires being granted
 *  to its /${productKey}/${deviceName}/get for successfully running whole example
 */
MyAliyunMqtt::~MyAliyunMqtt()
{

}
int MyAliyunMqtt::MqttInit(char* host, int port, char* clientid, char* username, char* password)
{
	iotx_mqtt_param_t       mqtt_params;

	memset(&mqtt_params, 0x0, sizeof(mqtt_params));

	mqtt_params.write_buf_size = 1024;
	mqtt_params.read_buf_size = 1024;
	mqtt_params.host = host;
	mqtt_params.client_id = clientid;
	mqtt_params.password = password;
	mqtt_params.username = username;
	mqtt_params.keepalive_interval_ms = 60000;
	mqtt_params.request_timeout_ms = 20000;
	mqtt_params.port = port;
	mqtt_params.clean_session = 1;
	mqtt_params.handle_event.h_fp = event_handle;
	mqtt_params.handle_event.pcontext = NULL;

	pclient = IOT_MQTT_Construct(&mqtt_params);
	if (NULL == pclient) {
		printf("MQTT construct failed");
		return -1;
	}
	return 0;
}
MyAliyunMqtt::MyAliyunMqtt()
{
}
int MyAliyunMqtt::MqttInterval(void* Params)
{
	MyAliyunMqtt* point = (MyAliyunMqtt*)Params;
	while (1)
	{
		IOT_MQTT_Yield((point->pclient), 200);
	}
	return 0;
}
int MyAliyunMqtt::MqttRecParse(void* Params)
{
	MyAliyunMqtt* point = (MyAliyunMqtt*)Params;
	Serial s;
	while (1)
	{
		if (point->getmessage == true)
		{
			cJSON* json, * json_params;
			json = cJSON_Parse(point->payload);

			json_params = cJSON_GetObjectItem(json, "params");
			if (json_params)
			{
				cJSON* jsontemp = json_params->child;
				while (jsontemp != NULL)
				{
					varinfo.varname = jsontemp->string;
					varinfo.value = jsontemp->valuedouble;
					queue_var_write.push(varinfo);
					jsontemp = jsontemp->next;
				}
			}
			else if ((json_params = cJSON_GetObjectItem(json, "COM")))
			{
				int com = json_params->valueint;
				char dev[100];
				char dest[1024];
				char readbuff[1024];
#ifndef gcc
				int gpio = (com == 1) ? 46 : (com == 2) ? 103 : (com == 3) ? 102 : (com == 4) ? 132 : (com == 5) ? 32 : -1;
				com = (com == 1) ? 10 : (com == 2) ? 3 : (com == 3) ? 9 : (com == 4) ? 1 : (com == 5) ? 6 : com;
				if (gpio != -1)
				{
					gpioexport(gpio);
					gpiooutput(gpio);
					gpiovalue(gpio, 1);
				}
#endif
				cJSON* json_payload;

				json_payload = cJSON_GetObjectItem(json, "payload");
				int ComIsResp = cJSON_GetObjectItem(json, "ComIsResp")->valueint;
				int serialtimeout = 0;
				if (ComIsResp)
					serialtimeout = cJSON_GetObjectItem(json, "ComTimeoutMs")->valueint;
				sprintf(dev, "%s%d", "/dev/ttyS", com);

				int len = Base64decode(dest, json_payload->valuestring);

				s.open(dev, 115200, 8, 'N', 1);

				s.write(dest, len);
#ifndef gcc
				if (gpio != -1)
				{
					gpiovalue(gpio, 0);
				}
#endif
				int seriallen = s.read_wait(readbuff, 1024, serialtimeout);
				if (seriallen == 0)
				{
					cout << "read serial timeout!" << endl;
				}
				memset(dest, 0, sizeof(dest));
				Base64encode(dest, readbuff, seriallen);
				s.close();

				cJSON* response;
				time_t nowtime;
				char tmp[64];
				char payload[1024];
				nowtime = time(NULL); //get now time
				strftime(tmp, sizeof(tmp), "%Y%m%d%H%M%S", localtime(&nowtime));

				response = cJSON_CreateObject();
				cJSON_AddNumberToObject(response, "COM", com);

				if (!ComIsResp)
				{
					sprintf(payload, "Com%d Complete", com);
				}
				else
					strcpy(payload, dest);

				if (seriallen == 0)
				{
					sprintf(payload, "Com%d Timeout", com);
				}

				cJSON_AddStringToObject(response, "Time", tmp);
				cJSON_AddStringToObject(response, "payload", payload);
				if (point->messageid != "0")
				{
					string topic = "/sys/"+point->productkey+"/"+ point->devicename+"/rrpc/response/" + point->messageid;
					point->publish((char*)topic.c_str(), 0, response);
				}
				else
					point->publish(ThemeUpload[0].CtrlPub, 1, (response));
				cJSON_Delete(response);
			}
			else
			{
				exit(-1);
			}
			cJSON_Delete(json);
			point->getmessage = false;
		}
		usleep(10000);
	}
	return 0;
}
int MyAliyunMqtt::openmainthread()
{
	threadid = std::thread(MqttMain, this);
	return 0;
}
int MyAliyunMqtt::openintervalthread()
{
	threadid_interval = std::thread(MqttInterval, this);
	return 0;
}
int MyAliyunMqtt::openrecparsethread()
{
	threadid_recparse = std::thread(MqttRecParse, this);
	return 0;
}
int MyAliyunMqtt::MqttMain(void* Params)
{
#ifndef gcc
	for (unsigned int i = 0; i < sizeof(Allinfo) / sizeof(Allinfo_t); i++)
	{
		if (Allinfo[i].portinfo.PortNum == 0 || Allinfo[i].portinfo.gpio == -1)
			continue;
		int gpio = Allinfo[i].portinfo.gpio;
		if (gpio != -1)
		{
			char buff[100];
			sprintf(buff, "%s%d%s", "echo ", gpio, " > /sys/class/gpio/export");
			system(buff);
			sprintf(buff, "%s%d%s", "echo out > /sys/class/gpio/gpio", gpio, "/direction");
			system(buff);
			sprintf(buff, "%s%d%s", "echo 1 > /sys/class/gpio/gpio", gpio, "/value");
			system(buff);
		}
	}
#endif
	MyAliyunMqtt* point = (MyAliyunMqtt*)Params;

	string username = MqttInfo[0].UserName;
	size_t pos = username.find('&', 0);
	point->devicename = username.substr(0, pos);
	point->productkey = username.substr(pos+1);

	point->MqttInit(MqttInfo[0].ServerLink, MqttInfo[0].ServerPort, MqttInfo[0].ClientId, MqttInfo[0].UserName, MqttInfo[0].Password);
	int res = point->subscribe(ThemeCtrl[0].CtrlSub, 1);
	if (res < 0)
	{
		IOT_MQTT_Destroy(&(point->pclient));
		return -1;
	}
	string rrpc_topic = "/sys/" + point->productkey + "/" + point->devicename + "/rrpc/request/+";
	res = point->subscribe((char*)rrpc_topic.c_str(), 0);
	if (res < 0) {
		IOT_MQTT_Destroy(&(point->pclient));
		return -1;
	}
	point->openintervalthread();		//create MQTT interval thread
	point->openrecparsethread();		//create thread----parse the receive data
	time_t nowtime;
	cJSON* publish_json;
	cJSON* params_json;
	char tmp[64];
	while (1)
	{
		nowtime = time(NULL); //get now time
		strftime(tmp, sizeof(tmp), "%Y%m%d%H%M%S", localtime(&nowtime));

		publish_json = cJSON_CreateObject();

		cJSON_AddStringToObject(publish_json, "id", tmp);
		cJSON_AddStringToObject(publish_json, "method", "method.event.property.post");
		cJSON_AddItemToObject(publish_json, "params", params_json = cJSON_CreateObject());

		int alluploadvarcount = 0;	//记录所有的变量数量
		for (unsigned int i = 0; i < sizeof(Allinfo) / sizeof(Allinfo_t); i++)
		{
			if (Allinfo[i].portinfo.id == 0)
				continue;
			for (int j = 0; j < Allinfo[i].devcount; j++)
			{
				alluploadvarcount += Allinfo[i].deviceinfo->uploadvarcount;
			}
		}

		int alluploadvarcount_temp = 0;	//记录已经上传的变量的数量
		for (unsigned int i = 0; i < sizeof(Allinfo) / sizeof(Allinfo_t); i++)
		{
			if (Allinfo[i].portinfo.id == 0)
			{
				continue;
			}
			for (int j = 0; j < Allinfo[i].devcount; j++)
			{
				if (Allinfo[i].deviceinfo[j].id == 0)
				{
					continue;
				}
				for (int k = 0; k < Allinfo[i].deviceinfo[j].uploadvarcount; k++)
				{
					VarParam_t* uploadvartemp = &Allinfo[i].deviceinfo[j].uploadvarparam[k];
					if (uploadvartemp->id == 0)
					{
						continue;
					}
					std::string varname = uploadvartemp->VarName;
					double value = var[varname];
					cJSON_AddNumberToObject(params_json, varname.c_str(), value);
					//检查变量数量是否到达设定的数量或者该设备下已无可读变量，是则上发数据到指定主题
					alluploadvarcount_temp++;
					if ((alluploadvarcount_temp % uploadperiod) \
						&& alluploadvarcount_temp != alluploadvarcount\
						&& (k != Allinfo[i].deviceinfo[j].uploadvarcount - 1))
					{
						continue;
					}
					alluploadvarcount_temp = 0;
					point->publish(ThemeUpload[0].CtrlPub, ThemeUpload[0].QosPub, publish_json);
					cJSON_DeleteItemFromObject(publish_json, "params");
					cJSON_AddItemToObject(publish_json, "params", params_json = cJSON_CreateObject());
				}
			}
		}
		sleep(ThemeUpload[0].PubPeriod);
	}
}
