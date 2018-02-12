//gcc master_profile.c -o master_profile -lwiringPi -lpthread -lmosquitto


//#include "speedometer.h"
//#include "distance.h"
#include <wiringPi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <mosquitto.h>




//int piHiPri (const int pri);
const int PWMpin = 17;
const int HALL_PIN = 3;
const int trigger = 23;
const int echo = 24;
double distance = 0.0;

const int num_speeds = 10;
double times[10] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
double speed = 0.0;
double wheelCircumfrence = 0.1979;
double timeDeltas[9] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
//double timeDelta1 = 0.0;
//double timeDelta2 = 0.0;

struct sigaction old_action;

int i = 0;
double newTime;

void updateTimes()
{
	//printf("reached update times\n");
	newTime = millis();
	//printf("new time is: %f\n", newTime);
	//times[2] = times[1];
	//times[1] = times[0];
	//times[0] = newTime;
	//i = 0;
	for(i = num_speeds - 1; i > 0; i = i - 1)
	{
		times[i] = times[i - 1];

	}
	times[0] = newTime;
	
}

void calculateSpeed()
{
	//printf("reached calculate speed\n");
	/*if(times[1] == 0.0)
	{
		speed = 0.0;
	}
	else if(times[2] == 0.0)
	{
		timeDelta1 = times[0] - times[1];
		speed = wheelCircumfrence / timeDelta1;
	}
	else
	{
		timeDelta1 = times[0] - times[1];
		timeDelta2 = times[1] - times[2];
		speed = wheelCircumfrence / (0.65 * timeDelta1 + 0.35 * timeDelta2);
	}*/

	for(i = 0; i < num_speeds - 1; i = i + 1)
	{
		timeDeltas[i] = times[i] - times[i + 1];
	}
	speed = (wheelCircumfrence / ((1.0 / (num_speeds - 1)) * timeDeltas[0] + (1.0/(num_speeds - 1)) * timeDeltas[1] + (1.0/(num_speeds - 1)) * timeDeltas[2] + (1.0/(num_speeds - 1)) * timeDeltas[3] + (1.0/9.0) * timeDeltas[4] + (1.0/9.0) * timeDeltas[5] + (1.0/9.0) * timeDeltas[6] + (1.0/9.0) * timeDeltas[7] + (1.0/9.0) * timeDeltas[8])) / 2;

}

void hall_main()
{
	//printf("reached hall main\n");
	updateTimes();
	calculateSpeed();
	int snd = mqtt_send();
	if(snd != 0) printf("mqtt_send error=%i\n", snd);
	printf("current speed is: %f\n", speed);
}






struct mosquitto *mosq = NULL;
char *topic = "test";
void mqtt_setup()
{
	char *host = "192.168.0.1";
	int port = 1883;
	int keepalive = 60;
	bool clean_session = true;
	mosquitto_lib_init();

	mosq = mosquitto_new(NULL, clean_session, NULL);

	if(mosquitto_connect(mosq, host, port, keepalive))
	{
		fprintf(stderr, "Unable to connect.\n");
		exit(1);
	}

	int loop = mosquitto_loop_start(mosq);
  	if(loop != MOSQ_ERR_SUCCESS)
  	{
    	fprintf(stderr, "Unable to start loop: %i\n", loop);
    	exit(1);
	}

}

char output[50];
int mqtt_send()
{
  snprintf(output, 50, "%f", speed);
  return mosquitto_publish(mosq, NULL, topic, 50, output, 0, 0);
}

void sigIntHandler(int signal)
{
	//digitalWrite(trigger, 0);
	softPwmWrite(PWMpin, 0);
	digitalWrite(trigger, 0);
	mosquitto_destroy(mosq);
	mosquitto_lib_cleanup();
	printf("\nProgram ended with exit value -2.\n");
	sigaction(SIGINT, &old_action, NULL);
    kill(0, SIGINT);
	//exit(-2);
}

void measureDistance()
{
	//printf("getting distance\n");
	double start_time;
	double stop_time;
	double difference;
	//double distance;

	digitalWrite(trigger, 1);
	delayMicroseconds(20);
	digitalWrite(trigger, 0);

	while(digitalRead(echo) == 0)
	{
		//printf("first while\n");
		//start_time = time(NULL);	
		start_time = micros();
	}
	//start_time = micros();
	//printf("response is high\n");
	while(digitalRead(echo) == 1)
	{
		//printf("second while\n");
		//stop_time = time(NULL);
		stop_time = micros();
	}
	//printf("response is low\n");
	//stop_time = micros();

	difference = (double)stop_time - (double)start_time; //difftime(stop_time, start_time);
	distance = difference / 58;

	printf("Current distance: %f \n", distance);
}




int main(void)
{

	printf("starting leader\n");
	/*struct sigaction sigHandler;
	sigHandler.sa_handler = sigIntHandler;
	sigemptyset(&sigHandler.sa_mask);
    sigHandler.sa_flags = 0;
    sigaction(SIGINT, &sigHandler, NULL);*/
    //atexit(sigIntHandler);

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = &sigIntHandler;
    sigaction(SIGINT, &action, &old_action);

	if (piHiPri(99) == -1)
	{
		printf("Error setting priority! Exiting");
		return -1;
	}

	mqtt_setup();

	wiringPiSetupGpio();
	pinMode(PWMpin, OUTPUT);
	pinMode(HALL_PIN, INPUT);
	pinMode(trigger, OUTPUT);
	pinMode(echo, INPUT);
	digitalWrite(trigger, 0);

	if(softPwmCreate(PWMpin, 40, 100) != 0)
	{
		printf("Error setting soft PWM pin! Exiting");
		return -1;
	}

	wiringPiISR(HALL_PIN, INT_EDGE_RISING, hall_main);

	softPwmWrite(PWMpin, 65);
	bool first_90 = true;
	bool first_70 = true;
	bool first_60 = true;
	while(1)
    {
		if(micros() < 4000000)
		{
			softPwmWrite(PWMpin, 65);
		}
		else if(micros() < 8000000)
		{
			softPwmWrite(PWMpin, 90);
			if(first_90 == true)
			{
				printf("switched PWM to 90\n");
				first_90 = false;
			}
		}
		else if(micros() < 12000000)
		{
			softPwmWrite(PWMpin, 70);
			if(first_70 == true)
			{
				printf("switched PWM to 70\n");
				first_70 = false;
			}
		}
		else
		{
			softPwmWrite(PWMpin, 60);
			if(first_60 == true)
			{
				printf("switched PWM to 60\n");
				first_60 = false;
			}
		}
		//printf("starting reading\n");    	
		//measureDistance();
		//int snd = mqtt_send();
		//if(snd != 0) printf("mqtt_send error=%i\n", snd);
    	delay(100);
    }


	return(0);

}



/*int piHiPri (const int pri)
{
  struct sched_param sched ;

  memset (&sched, 0, sizeof(sched)) ;

  if (pri > sched_get_priority_max (SCHED_RR))
    sched.sched_priority = sched_get_priority_max (SCHED_RR) ;
  else
    sched.sched_priority = pri ;

  return sched_setscheduler (0, SCHED_RR, &sched) ;
}*/