//gcc follower_new.c -o follower -lwiringPi -lpthread -lmosquitto

#include <wiringPi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <mosquitto.h>
#include <math.h>


const int PWMpin = 17;
const int HALL_PIN = 3;
const int trigger = 23;
const int echo = 24;
double distance = 0.0;
double old_distance = 0.0;
int current_pwm = 0;
double leader_speed = 0.0; 


const int num_speeds = 5;

double times[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
double speed = 0.0;
double wheelCircumfrence = 0.1979;
double timeDeltas[4] = {0.0, 0.0, 0.0, 0.0};
//double timeDelta1 = 0.0;
//double timeDelta2 = 0.0;
const int alot_up = 10;
const int alot_down = 22;
const int kinda = 2;
const int base_distance = 30;//25 centimeters for base distance
double desired_distance = 30;

FILE *fp;

struct sigaction old_action;

int i = 0;
double newTime;

void updateTimes()
{
	//printf("reached update times\n");
	newTime = millis();
	//printf("new time is: %f\n", newTime);
	//i = 0;
	for(i = num_speeds - 1; i > 0; i = i - 1)
	{
		times[i] = times[i - 1];

	}
	//times[2] = times[1];
	//times[1] = times[0];
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
	speed = (wheelCircumfrence / ((1.0 / (num_speeds - 1)) * timeDeltas[0] + (1.0/(num_speeds - 1)) * timeDeltas[1] + (1.0/(num_speeds - 1)) * timeDeltas[2] + (1.0/(num_speeds - 1)) * timeDeltas[3])) / 2;// + (1.0/9.0) * timeDeltas[4] + (1.0/9.0) * timeDeltas[5] + (1.0/9.0) * timeDeltas[6] + (1.0/9.0) * timeDeltas[7] + (1.0/9.0) * timeDeltas[8]);

}

void hall_main()
{
	//printf("reached hall main\n");
	updateTimes();
	calculateSpeed();
	//printf("current speed is: %f\n", speed);
}



void my_message_callback(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message)
{
	//fprintf(stderr, "reached callback func\n");
	//int i;
	if(message->payloadlen){
		//fprintf(stderr, "%s %s\n", message->topic, message->payload);
		//i = atoi(message->payload);
		sscanf(message->payload, "%lf", &leader_speed);

		//fprintf(stderr, "leader speed is %lf. \n", leader_speed);
		//softPwmWrite(PWMpin, i);
	}else{
		//fprintf(stderr,"%s (null)\n", message->topic);
	}
	fflush(stdout);
}

void my_connect_callback(struct mosquitto *mosq, void *userdata, int result)
{
	//int i;
	if(!result){
		/* Subscribe to broker information topics on successful connect. */
		mosquitto_subscribe(mosq, NULL, "test", 0);
	}else{
		fprintf(stderr, "Connect failed\n");
	}
}

void my_subscribe_callback(struct mosquitto *mosq, void *userdata, int mid, int qos_count, const int *granted_qos)
{
	int i;

	printf("Subscribed (mid: %d): %d", mid, granted_qos[0]);
	for(i=1; i<qos_count; i++){
		printf(", %d", granted_qos[i]);
	}
	printf("\n");
}




struct mosquitto *mosq = NULL;
char *topic = "test";
char *host = "192.168.0.1";
int port = 1883;
int keepalive = 60;
void mqtt_setup()
{
	
	
	
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
	
	if(difference / 58 > 400 || difference / 58 < 2)
	{
		distance = distance;
	}
	else
	{
		distance = difference / 58;
	}

	//printf("Current distance: %f \n", distance);
}


int new_pwm;
double stable_count = 1.0;
double muliplier = 30000.0;
int stable_offset = 8;
int slightly_far_offset = 40;
bool in_slight_case = false;
void checkSpeed()
{
	desired_distance = base_distance;// + speed;
	//printf("desired distance = %f\n", desired_distance);

	if(fabs(distance - desired_distance) < stable_offset)
	{
		in_slight_case = false;
		if(speed > 0.0005)
		{
			muliplier = 30000.0 / stable_count;
			new_pwm = current_pwm + (int)((leader_speed - speed) * muliplier);
		}
		else
		{
			new_pwm = current_pwm + (int)((distance - old_distance) * 0.5);
		}
		
		
		
		if(new_pwm > 100)
		{
			softPwmWrite(PWMpin, 100);
			current_pwm = 100;
		}
		else if(new_pwm < 20)
		{
			softPwmWrite(PWMpin, 20);
			current_pwm = 20;
		}
		else
		{
			softPwmWrite(PWMpin, new_pwm);
			current_pwm = new_pwm;
		}
		
		printf("used first case and new PWM change is: %f, new PWM value is %d\n", ((leader_speed - speed) * muliplier), current_pwm);
		stable_count = stable_count + 1;
	}
	else if(distance > desired_distance + stable_offset && distance < desired_distance + slightly_far_offset)
	{
		if(old_distance > distance && in_slight_case == false)//approaching sweet spot, slow down
		{
			new_pwm = current_pwm + (int)((distance - old_distance) * 15);
			if(new_pwm > 100)
			{
				softPwmWrite(PWMpin, 100);
				current_pwm = 100;
			}
			else if(new_pwm < 20)
			{
				softPwmWrite(PWMpin, 20);
				current_pwm = 20;
			}
			else
			{
				softPwmWrite(PWMpin, new_pwm);
				current_pwm = new_pwm;
			}
			printf("used slightly too far case 1, and new pwm is %d\n", current_pwm);
		}
		else//getting farther away from sweet spot, speed up
		{
			//new_pwm = current_pwm + (int)((leader_speed - speed) * 20000);
			new_pwm = current_pwm + (int)((distance - old_distance) * 15);
			if(new_pwm > 100)
			{
				softPwmWrite(PWMpin, 100);
				current_pwm = 100;
			}
			else if(new_pwm < 20)
			{
				softPwmWrite(PWMpin, 20);
				current_pwm = 20;
			}
			else
			{
				softPwmWrite(PWMpin, new_pwm);
				current_pwm = new_pwm;
			}
			printf("used slightly too far case 2, and new pwm is %d\n", current_pwm);

		}
		in_slight_case = true;
	}
	else if(distance > desired_distance + slightly_far_offset)
	{
		in_slight_case = false;
		stable_count = 1;
		if(speed > leader_speed)
		{
			new_pwm = current_pwm + (int)(/*(leader_speed - speed) * */(distance - desired_distance) * 0.03);
			if(new_pwm > 100)
			{
				softPwmWrite(PWMpin, 100);
				current_pwm = 100;
			}
			else if(new_pwm < 20)
			{
				softPwmWrite(PWMpin, 20);
				current_pwm = 20;
			}
			else
			{
				softPwmWrite(PWMpin, new_pwm);
				current_pwm = new_pwm;
			}

		}
		else
		{
			new_pwm = current_pwm + (int)(/*(leader_speed - speed) * */(distance - desired_distance) * 0.07);
			if(new_pwm > 100)
			{
				softPwmWrite(PWMpin, 100);
				current_pwm = 100;
			}
			else if(new_pwm < 20)
			{
				softPwmWrite(PWMpin, 20);
				current_pwm = 20;
			}
			else
			{
				softPwmWrite(PWMpin, new_pwm);
				current_pwm = new_pwm;
			}
			printf("used second case and new PWM change is: %d, new PWM is: %d\n", (int)(/*(leader_speed - speed) * */(distance - desired_distance) * 0.15), current_pwm);
		}
		
		
		
	}
	else
	{
		in_slight_case = false;
		stable_count = 1;
		if(distance < old_distance)
		{
			new_pwm = current_pwm + (int)((distance - old_distance) * 25);
			if(new_pwm < 20)
			{
				softPwmWrite(PWMpin, 20);
				current_pwm = 20;
			}
			else
			{
				softPwmWrite(PWMpin, new_pwm);
				current_pwm = new_pwm;
			}
			printf("used 3a case and new PWM is %d\n", current_pwm);
		}
		else
		{
			new_pwm = current_pwm + (int)((distance - old_distance) * 5);
			softPwmWrite(PWMpin, new_pwm);
			current_pwm = new_pwm;
			printf("used 3b case and new PWM is %d\n", current_pwm);

		}


		/*else if(leader_speed < speed)
		{
			new_pwm = current_pwm + (int)((distance - desired_distance) * (speed - leader_speed) * 8000);



			if(new_pwm > 100)
			{
				softPwmWrite(PWMpin, 100);
				current_pwm = 100;
			}
			else if(new_pwm < 20)
			{
				softPwmWrite(PWMpin, 20);
				current_pwm = 20;
			}
			else
			{
				softPwmWrite(PWMpin, new_pwm);
				current_pwm = new_pwm;
			}



			//softPwmWrite(PWMpin, new_pwm);
			//current_pwm = new_pwm;
			printf("used third case and new PWM is: %f\n", ((distance - desired_distance) * (speed - leader_speed) * 7000));
		}*/
	}



	
	//if(distance > desired_distance + 5 && current_pwm < 95)//adding 5 cm buffer to reduce oscillation at desired distance
	//{
	//	softPwmWrite(PWMpin, current_pwm + alot_up);
	//	current_pwm = current_pwm + alot_up;
		/*if(leader_speed > speed + 0.0005)
		{
			softPwmWrite(PWMpin, current_pwm + alot);
			current_pwm = current_pwm + alot;

		}
		else if(fabs(leader_speed - speed) < 0.0005)
		{
			softPwmWrite(PWMpin, current_pwm + kinda);
			current_pwm = current_pwm + kinda;
		}*/

	//}
	//else if(distance < desired_distance - 5 && current_pwm > 20)
	//{
	//	softPwmWrite(PWMpin, current_pwm - alot_down);
	//	current_pwm = current_pwm - alot_down;
		/*if(fabs(leader_speed - speed) < 0.0005)
		{
			softPwmWrite(PWMpin, current_pwm - kinda);
			current_pwm = current_pwm - kinda;
		}
		else if(leader_speed < speed - 0.0005)
		{
			softPwmWrite(PWMpin, current_pwm - alot);
			current_pwm = current_pwm - alot;
		}*/

	//}


}




void sigIntHandler(int signal)
{
	//digitalWrite(trigger, 0);
	softPwmWrite(PWMpin, 0);
	digitalWrite(trigger, 0);
	//mosquitto_loop_stop(mosq, 1);
	mosquitto_destroy(mosq);
	mosquitto_lib_cleanup();
	fclose(fp);
	printf("\nProgram ended with exit value -2.\n");
	sigaction(SIGINT, &old_action, NULL);
    kill(0, SIGINT);
	//exit(-2);
}



int main(void)
{
	printf("starting follower\n");
	/*struct sigaction sigHandler;
	sigHandler.sa_handler = sigIntHandler;
	sigemptyset(&sigHandler.sa_mask);
    sigHandler.sa_flags = 0;
    sigaction(SIGINT, &sigHandler, NULL);*/

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
	mosquitto_message_callback_set(mosq, my_message_callback);
	//mosquitto_log_callback_set(mosq, my_log_callback);
	mosquitto_connect_callback_set(mosq, my_connect_callback);
	mosquitto_subscribe_callback_set(mosq, my_subscribe_callback);

	if(mosquitto_connect(mosq, host, port, keepalive)){
		fprintf(stderr, "unable to connect to broker\n");
		return 1;
	}

	wiringPiSetupGpio();
	pinMode(PWMpin, OUTPUT);
	pinMode(HALL_PIN, INPUT);
	pinMode(trigger, OUTPUT);
	pinMode(echo, INPUT);
	digitalWrite(trigger, 0);

	wiringPiISR(HALL_PIN, INT_EDGE_RISING, hall_main);

	if(softPwmCreate(PWMpin, 0, 100) != 0)
	{
		printf("Error setting soft PWM pin! Exiting");
		return -1;
	}
	
	measureDistance();
	//if(distance < base_distance)
	//{
		softPwmWrite(PWMpin, 0);
		current_pwm = 0;
	/*}
	else
	{
		softPwmWrite(PWMpin, 50);
		current_pwm = 50;
	}*/

	//while(distance < 20)
	//{
	//	measureDistance();
	//}
	//softPwmWrite(PWMpin, 60);
	//current_pwm = 60;


	

	//fp = fopen("distances.csv", "w");
	//fprintf(fp, "time(us),distance(cm)\n");
	//bool distance_check = true;

	while(1)
	{
		//if(distance_check)
		//{
			measureDistance();
		//}
		//speed = leader_speed + ((distance - old_distance) / 1000);
		printf("current speed is: %f, leader speed is: %f, distance is: %f\n", speed, leader_speed, distance);
		checkSpeed();	
		
		old_distance = distance;
		//double dub = micros();
		//fprintf(fp, "%f,%f\n", dub, distance);
		printf("current PWM: %d\n", current_pwm);
		

		delay(25);	
	}


}