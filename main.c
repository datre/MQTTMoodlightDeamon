#include "stdio.h"
#include "stdlib.h"
#include "string.h"

#include "durchreiche.h"
#include <MQTTAsync.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

//MQTT Setup
#define ADDRESS     "tcp://raspi2:1883"
#define CLIENTID    "MQTTMoodlightDeamon"
#define TOPIC       "kitchen/switch/#"
#define QOS         1

#define DAEMON_NAME "MQTTMoodlightDeamon"
#define PORT "/dev/ttyAMA0"
#define GAMMA 2.8f
//#define DEAMON

const char controlleradress  = 0x10; //0x10
const char clientadress = 0xFE;     //0xFE
const char payloadlength = 0x1E;    //30 da immer 3 byte pro Lampe a 10 Lampen

volatile MQTTAsync_token deliveredtoken;

bool disconnectFinished = 0;
bool subscribed = 0;
bool somethingwentwrong = 0;
bool file = false;
struct Packet* packet = NULL;
int correction[256];

void onConnectionLost(void *context, char *reason)
{
	MQTTAsync client = (MQTTAsync) context;
#ifdef DEAMON
	syslog(LOG_NOTICE,"\nConnection lost\n");
	syslog(LOG_NOTICE,"     reason: %s\n", reason);
#else
    printf("\nConnection lost\n");
	printf("     reason: %s\n", reason);
#endif
}

int onMessageArrived(void *context, char *topicName, int topicLen, MQTTAsync_message *message)
{
    int i, length = message->payloadlen;
    char* payloadptr = NULL;
    char* msg = malloc(length + 1);
#ifdef DEAMON
    syslog(LOG_NOTICE,"Message arrived\n");
    syslog(LOG_NOTICE,"     topic: %s\n", topicName);
#else
    printf("Message arrived\n");
    printf("     topic: %s\n", topicName);
#endif // DEAMON

    payloadptr = message->payload;
    for(i=0; i<length; i++)
    {
        msg[i] = payloadptr[i];
    }
    msg[length] = '\0';
#ifdef DEAMON
    syslog(LOG_NOTICE,"   message: %s\n", payloadptr);
#else
    printf("   message: %s\n", msg);
#endif
    packet->Source = clientadress;
	packet->Destination = controlleradress;
	packet->Length = payloadlength;
	//TODO do colors in payload.color[30]
    //Send(PORT, &file, packet);
    MQTTAsync_freeMessage(&message);
    MQTTAsync_free(topicName);
    return 1;
}

void onDisconnect(void* context, MQTTAsync_successData* response)
{
#ifdef DEAMON
	syslog(LOG_NOTICE,"Successful disconnection\n");
#else
	printf("Successful disconnection\n");
#endif // DEAMON
	disconnectFinished = 1;
}

void onSubscribe(void* context, MQTTAsync_successData* response)
{
#ifdef DEAMON
	syslog(LOG_NOTICE,"Subscribe succeeded\n");
#else
    printf("Subscribe succeeded\n");
#endif
	subscribed = 1;
}

void onSubscribeFailure(void* context, MQTTAsync_failureData* response)
{
#ifdef DEAMON
	syslog(LOG_NOTICE,"Subscribe failed, rc %d\n", response ? response->code : 0);
#else
    printf("Subscribe failed, rc %d\n", response ? response->code : 0);
#endif
	somethingwentwrong = 1;
}

void onConnectFailure(void* context, MQTTAsync_failureData* response)
{
#ifdef DEAMON
	syslog(LOG_NOTICE,"Connect failed, rc %d\n", response ? response->code : 0);
#else
	printf("Connect failed, rc %d\n", response ? response->code : 0);
#endif // DEAMON
	somethingwentwrong = 1;
}

void onConnected(void* context, MQTTAsync_successData* response)
{
	MQTTAsync client = (MQTTAsync)context;
	MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
	MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
	int return_code;
#ifdef DEAEMON
	syslog(LOG_NOTICE,"Successful connection\n");
	syslog(LOG_NOTICE,"Subscribing to topic %s\nfor client %s using QoS%d\n", TOPIC, CLIENTID, QOS);
#else
    printf("Successful connection\n");
	printf("Subscribing to topic %s\nfor client %s using QoS%d\n", TOPIC, CLIENTID, QOS);
#endif

	opts.onSuccess = onSubscribe;
	opts.onFailure = onSubscribeFailure;
	opts.context = client;

	deliveredtoken = 0;

	if ((return_code = MQTTAsync_subscribe(client, TOPIC, QOS, &opts)) != MQTTASYNC_SUCCESS)
	{
	    #ifdef DEAMON
		syslog(LOG_NOTICE,"Failed to start subscribe, return code %d\n", return_code);
		#else
		printf("Failed to start subscribe, return code %d\n", return_code);
		#endif
		exit(-1);
	}
}

int MQTT()
{
    if(packet == NULL){
        packet = (struct Packet*) malloc(sizeof( struct Packet));
    }
    for(int i=0; i<256; i++)
    {
        correction[i] = (int)(pow(i/255.0, GAMMA)*255 + 0.5);
    }
    MQTTAsync client;
	MQTTAsync_connectOptions connectionOptions = MQTTAsync_connectOptions_initializer;
	MQTTAsync_disconnectOptions disconnectOptions = MQTTAsync_disconnectOptions_initializer;
	int returnedCode;
	MQTTAsync_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
	MQTTAsync_setCallbacks(client, NULL, onConnectionLost, onMessageArrived, NULL);

	connectionOptions.keepAliveInterval = 20;
	connectionOptions.cleansession = 1;
	connectionOptions.onSuccess = onConnected;
	connectionOptions.onFailure = onConnectFailure;
	connectionOptions.context = client;
	if ((returnedCode = MQTTAsync_connect(client, &connectionOptions)) != MQTTASYNC_SUCCESS)
	{
	    #ifdef DEAMON
        syslog(LOG_NOTICE, "Failed connecting, this code was returned%d\n", returnedCode);
	    #else
	    printf("Failed connecting, this code was returned%d\n", returnedCode);
	    #endif // DEAMON
		exit(-1);
	}
	while(!somethingwentwrong)
    {
        usleep(10000);
    }
	disconnectOptions.onSuccess = onDisconnect;
	if ((returnedCode = MQTTAsync_disconnect(client, &disconnectOptions)) != MQTTASYNC_SUCCESS)
	{
	    #ifdef DEAMON
        syslog(LOG_NOTICE,"Failed disconnecting, this code was returned %d\n", returnedCode);
	    #else
        printf("Failed disconnecting, this code was returned %d\n", returnedCode);
	    #endif // DEAMON
		exit(-1);
	}
 	while	(!disconnectFinished)
			usleep(10000L);
end:
	MQTTAsync_destroy(&client);
 	return returnedCode;
}

int main(int argc, char* argv[])
{
#ifdef DEAMON
    setlogmask(LOG_UPTO(LOG_NOTICE));
    openlog(DAEMON_NAME, LOG_CONS | LOG_NDELAY | LOG_PERROR | LOG_PID, LOG_USER);
    syslog(LOG_INFO, "Entering Daemon");
    pid_t pid, sid;
    pid = fork();
    if (pid < 0) { exit(EXIT_FAILURE); }
    if (pid > 0) { exit(EXIT_SUCCESS); }
    umask(0);
    sid = setsid();
    if (sid < 0) { exit(EXIT_FAILURE); }
    if ((chdir("/")) < 0) { exit(EXIT_FAILURE); }
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
#endif // DEAMON
    while(true)
    {
        MQTT();
        usleep(1000000);//Restart after 1 Second if somethingwentwrong
    }
    closelog();
}
