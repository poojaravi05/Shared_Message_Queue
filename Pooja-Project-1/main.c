#include<stdio.h>
#include<pthread.h>
#include<stdlib.h>
#include <fcntl.h> // for open
#include<sys/stat.h>
#include <unistd.h> // for close
#include<errno.h>
#include<linux/input.h>
#include<string.h>
#include <semaphore.h>
#include<math.h>

#include"shared_queue_lib.h"


#define MAX_MSGS 10
#define max_number 50
#define minimum_number 10
#define Base_period 1000
#define EVENT_FILE_NAME "/dev/input/event6"

//Global variables

int fd, nbytes;
struct input_event event;
unsigned long long prev = 0;

int error_status[2], error_status2[7];
int mutex_lock_error[2];

pthread_t p_thread[7];
pthread_attr_t tattr[7];
struct sched_param param[7];

struct mq* q1 ;
struct mq* q2 ;
struct message * object;
pthread_mutex_t lock[2]; //lock[0] -> Q1 ; lock[1] -> Q2
int q_Flag[2] = {0,0};
int q_delete[2] = {0,0};
int termination_Flag = 0;
int terminate = 0;

const int p_period_multiplier[] = {12,32,18,28};
const int r_period_multiplier[] = {40};

//semaphores
sem_t sLock, sLock2;
int s_status;

long long total_QTime[2] = {0,0}, mean1 = 0,mean2 = 0,sd1 =0, sd2=0;
int total_messages[2] = {0,0};
long long sum_sq1 = 0,sum_sq2 = 0;


//Function declaration
void thread_create();

void* onPeriodicActive(void * id);
void* onReceiverActive(void * id);
void* onAperiodicActive(void * id);

void * msg_function( void*id);
long double calcPI();

void termination_sequence();

// MAIN
int main(){
	//Local variables
	int j; 

	//opening mouse-event file
	fd = open(EVENT_FILE_NAME, O_RDONLY);
	if (fd < 0)
	{
	    printf("failed to open input device %s: %d\n", EVENT_FILE_NAME, errno);
	    
	}

	//Initiating Shared Queues
	printf("\nInitiating Shared Queues...\n");
	q1 = sq_create(MAX_MSGS);
	q2 = sq_create(MAX_MSGS);

	//Initializing Mutex Locks
	printf("\nInitializing Mutex Locks...\n");
	for(j=0;j<2;j++){
		error_status[j] = pthread_mutex_init(&lock[j], NULL);
		if(error_status[j] != 0){
			printf("mutex_init %d error status = %d\n",j,error_status[j]);
		}
	}
	
	//Initializing semaphore
	s_status = sem_init(&sLock, 0, 0);
	if(s_status == -1){
		printf("Semaphore 1 init error = %d\n",s_status);
	}

	s_status = sem_init(&sLock2, 0, 0);
	if(s_status == -1){
		printf("Semaphore 2 init error = %d\n",s_status);
	}

	//Creating Threads
	printf("\nCreating Threads...\n");
	thread_create();	
		
	//sleep(15000);

	while(1){
		nbytes = read(fd,&event,sizeof(event));

		if(event.code == 272 && event.value == 1 && termination_Flag != 1){
			sem_post(&sLock);
		}
		else if(event.code == 273 && event.value == 1  && termination_Flag != 1){
			sem_post(&sLock2);
		}
	}

	while(terminate == 0);
	termination_sequence();
	printf("Execution Terminated\n");
	//closing mouse-event file
	close(fd);
	return 0;
}

void termination_sequence(){
	int j;
	
	//waiting for thread termination
	for(j=0;j<7;j++){
		//printf("pthread_join\n");
		error_status2[j] = pthread_join(p_thread[j], NULL);
		if(error_status2[j] != 0){
			printf("thread %d join error status = %d\n",j,error_status2[j]);
		}
		error_status2[j] = pthread_attr_destroy(&tattr[j]);
		if(error_status2[j] != 0){
			printf("thread %d attr_destroy error status = %d\n",j,error_status2[j]);
		}	
  	}

	//Destroying mutex locks
	printf("Destroying Mutex Locks\n");
	for(j=0;j<2;j++){
		error_status[j] = pthread_mutex_destroy(&lock[j]);	
		if(error_status[j] != 0){
			printf("mutex_destroy %d error status = %d\n",j,error_status[j]);
		}
	}
	
	//Destroying shared queues
	printf("Deleting queues\n");
	q_delete[0] = sq_delete(q1);
	if(q_delete[0] != 1){
		printf("Q1 delete error = %d\n",q_delete[0]);
	}
	q_delete[1] = sq_delete(q2);
	if(q_delete[1] != 1){
		printf("Q2 delete error = %d\n",q_delete[1]);
	}
	//terminate = 1;
	
}

//Thread Create Function
void thread_create(){	
	int j;
	int *id;
	id = (int*)malloc(sizeof(int));
	//prio = 1(low) and prio = 99(high)
	param[0].sched_priority = 70; // Periodic thread
	param[1].sched_priority = 70; // " "
	param[2].sched_priority = 70; // " "
	param[3].sched_priority = 70; // " "

	param[4].sched_priority = 65; // Receiver thread

	param[5].sched_priority = 90; // Aperiodic thread
	param[6].sched_priority = 90; // " "
	

	for(j=0;j<7;j++){
		//thread attribute creation
		error_status2[j] = pthread_attr_init(&tattr[j]);
		if(error_status2[j] != 0){
			printf("attr_init error %d= %d\n",j,error_status2[j]);
		}

		//Thread attribute initialization
		error_status2[j] = pthread_attr_setinheritsched(&tattr[j],PTHREAD_EXPLICIT_SCHED);
		if(error_status2[j] != 0){
			printf("thread %d setinherit_sched error = %d\n",j,error_status2[j]);
		}

		error_status2[j] = pthread_attr_setschedpolicy(&tattr[j],SCHED_FIFO);
		if(error_status2[j] != 0 ){
			printf("thread %d setschedpolicy error = %d\n",j,error_status2[j]);
		}
	
		error_status2[j] = pthread_attr_setschedparam(&tattr[j], &param[j]);
		if(error_status2[j] != 0){
			printf("thread %d setschedparam error = %d\n", j,error_status2[j]);
		}
	
		//Thread Creation
		switch(j){
			case 0:{
				*id = 0;	
				error_status2[j] = pthread_create( &p_thread[j], &tattr[j], &onPeriodicActive, (void*)id);
				if(error_status2[j] != 0){
					printf("thread %d create error status = %d\n",j,error_status2[j]);
				}
				printf("Created 1st periodic thread\n");
				break;
			}
			case 1:{
				*id = 1;	
				error_status2[j] = pthread_create( &p_thread[j], &tattr[j], &onPeriodicActive, (void*)id);
				if(error_status2[j] != 0){
					printf("thread %d create error status = %d\n",j,error_status2[j]);
				}
				printf("Created 2nd periodic thread\n");
				break;
			}
			case 2:{
				*id = 2;	
				error_status2[j] = pthread_create( &p_thread[j], &tattr[j], &onPeriodicActive, (void*)id);
				if(error_status2[j] != 0){
					printf("thread %d create error status = %d\n",j,error_status2[j]);
				}
				printf("Created 3rd periodic thread\n");
				break;
			}
			case 3:{
				*id = 3;	
				error_status2[j] = pthread_create( &p_thread[j], &tattr[j], &onPeriodicActive,(void*)id);
				if(error_status2[j] != 0){
					printf("thread %d create error status = %d\n",j,error_status2[j]);
				}
				printf("Created 4th periodic thread\n");
				break;
			}
			case 4:{
				*id = 4;
				error_status2[j] = pthread_create( &p_thread[j], &tattr[j], &onReceiverActive, (void*)id);
				if(error_status2[j] != 0){
					printf("thread %d create error status = %d\n",j,error_status2[j]);
				}
				printf("\nCreated Receiver thread\n");
				break;
			}
			case 5:{
				*id = 5;
				error_status2[j] = pthread_create( &p_thread[j], &tattr[j], &onAperiodicActive, (void*)id);
				if(error_status2[j] != 0){
					printf("thread %d create error status = %d\n",j,error_status2[j]);
				}
				printf("\nCreated  1st Aperiodic thread\n");
				break;
			}
			case 6:{
				*id = 6;
				error_status2[j] = pthread_create( &p_thread[j], &tattr[j], &onAperiodicActive, (void*)id);
				if(error_status2[j] != 0){
					printf("thread %d create error status = %d\n",j,error_status2[j]);
				}
				printf("Created  2nd Aperiodic thread\n");
				break;
			}			
		}
	}	
}

void* onReceiverActive(void * id){
	//int n = *(int*)id;
	int count = 0;
	struct timespec next, period;
	object = (struct message *)malloc(sizeof(sizeof(char)*64));

	clock_gettime(CLOCK_MONOTONIC, &next);
	period.tv_nsec = Base_period*r_period_multiplier[0];

	while( isEmptyMQ(q1) != 1 && isEmptyMQ(q2) !=1 && termination_Flag != 1){
		count++;

		if( count%2 == 0){
			//read from q1
			pthread_mutex_lock(&lock[0]);
			object = sq_read(q1);
			sum_sq1 = sum_sq1 + pow((object->dequeue_time - object->enqueue_time),2);
			total_QTime[0] = total_QTime[0] + (object->dequeue_time - object->enqueue_time);
			total_messages[0]++;
			pthread_mutex_unlock(&lock[0]);
		}
		else{
			//read from q2
			pthread_mutex_lock(&lock[1]);
			object = sq_read(q2);
			sum_sq2 = sum_sq2 + pow((object->dequeue_time - object->enqueue_time),2);
			total_QTime[1] = total_QTime[1] + (object->dequeue_time - object->enqueue_time);
			total_messages[1]++;
			pthread_mutex_unlock(&lock[1]);
		}
		if((next.tv_nsec+period.tv_nsec)>=1000000000){
			next.tv_nsec = (next.tv_nsec+period.tv_nsec)%1000000000;
			next.tv_sec++;

		}
		else{		
			next.tv_nsec = next.tv_nsec+period.tv_nsec;
		}
		
		//printf("Next wakingup time. \ntv_nsec = %ld\n",next.tv_nsec);
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, 0);
		
	}
	//termination started;
	
	mean1 = total_QTime[0]/total_messages[0];

	sd1 = sqrt((1/total_messages[0])*((sum_sq1) - ((pow(total_QTime[0],2))/total_messages[0])));

	mean2 = total_QTime[1]/total_messages[1];

	sd2 = sqrt((1/total_messages[1])*((sum_sq2) - ((pow(total_QTime[1],2))/total_messages[1])));
	
	printf("Average Queueing time of queue 1 = %lld\n",mean1);
	printf("Total number of messages of queue1 = %d\n",total_messages[0]);
	printf("SD of queue1 = %lld\n", sd1);

	printf("Average Queueing time of queue 2 = %lld\n",mean2);
	printf("Total number of messages of queue2 = %d\n",total_messages[1]);
	printf("SD of queue2 = %lld\n", sd2);

	terminate = 1;
	termination_sequence();
	return 0;

}

void* onPeriodicActive(void * id){
	int q_status = 0;
	int n = *(int*)id;
	long double pi;
	struct timespec next, period;


	clock_gettime(CLOCK_MONOTONIC, &next);

	switch(n){ //switches to the respective periodic thread timer
		case 0:{
			
			period.tv_nsec = Base_period*p_period_multiplier[0];
			break;
		} 
		case 1:{
			period.tv_nsec = Base_period*p_period_multiplier[1];
			break;
		}  
		case 2:{
			period.tv_nsec = Base_period*p_period_multiplier[2];
			break;
		}  
		case 3:{
			period.tv_nsec = Base_period*p_period_multiplier[3];
			break;
		} 
	}

	while(termination_Flag != 1){
		pi = calcPI();
		//printf("\nAproximated value of PI = %1.16lf\n", pi);  

		if( n == 0 || n == 1){
			//Queue1
			pthread_mutex_lock(&lock[0]);
			printf("\nNow running thread with thread_id = %lu\n",pthread_self());										
			q_status = sq_write(q1,pi);
			if( q_status != 1){
				printf("Enqueue unsuccessful in Queue1\n");
			}
			pthread_mutex_unlock(&lock[0]);
		}
		else if(n==2 || n== 3){
			//Queue2
			pthread_mutex_lock(&lock[1]);
			printf("\nNow running thread with thread_id = %lu\n",pthread_self());
			q_status = sq_write(q2,pi);
			if( q_status != 1){
				printf("Enqueue unsuccessful in Queue2\n");
			}			
			pthread_mutex_unlock(&lock[1]);
		}

		if((next.tv_nsec+period.tv_nsec)>=1000000000){
			next.tv_nsec = (next.tv_nsec+period.tv_nsec)%1000000000;
			next.tv_sec++;

		}
		else{		
			next.tv_nsec = next.tv_nsec+period.tv_nsec;
		}
		
		//printf("Next wakingup time. \ntv_nsec = %ld\n",next.tv_nsec);
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, 0);
				
	}
	
	return 0;
}

void* onAperiodicActive(void * id){
	int n = *(int*)id;
	long double pi;
	int q_status = 0;
	
	while(1 ){
		if(event.code == 272 && event.value == 1 && n == 5 && termination_Flag != 1){			
			sem_wait(&sLock);
		
			printf("left click\n");
			//LEFT CLICK	
			if(prev != 0){
				if((event.time).tv_sec - prev < 1){
					printf("Entered termination sequence\n");
					termination_Flag = 1;
					//sem_post(&sLock2);
					break;
				}
				else{
					prev = (event.time).tv_sec;

					pi = calcPI();
					//printf("\nAproximated value of PI = %1.16lf\n", pi);

					pthread_mutex_lock(&lock[0]);
					printf("\nNow running thread with thread_id = %lu\n",pthread_self());										
					q_status = sq_write(q1,pi);
					if( q_status != 1){
						printf("Enqueue unsuccessful in Queue1\n");
					}
					pthread_mutex_unlock(&lock[0]);
				}
			}
			else{
				prev = (event.time).tv_sec;

				pi = calcPI();
				//printf("\nAproximated value of PI = %1.16lf\n", pi);

				pthread_mutex_lock(&lock[0]);
				printf("\nNow running thread with thread_id = %lu\n",pthread_self());				
				q_status = sq_write(q1,pi);
				if( q_status != 1){
					printf("Enqueue unsuccessful in Queue1\n");
				}
				pthread_mutex_unlock(&lock[0]);
			}
			
		}

		if(event.code == 273 && event.value == 1  && n == 6 && termination_Flag != 1){

			sem_wait(&sLock2);

				printf("right click\n");
				//RIGHT CLICK		
				pi = calcPI();
				//printf("\nAproximated value of PI = %1.16lf\n", pi);

				pthread_mutex_lock(&lock[1]);
				printf("\nNow running thread with thread_id = %lu\n",pthread_self());
				q_status = sq_write(q2,pi);
				if( q_status != 1){
					printf("Enqueue unsuccessful in Queue2\n");
				}
				pthread_mutex_unlock(&lock[1]);			
		}
		if(termination_Flag == 1){				
				break;
		}
	}
	//printf("terminating\n");
	return 0;
}

long double calcPI(){
	//Taken from https://www.codeproject.com/Articles/813185/Calculating-the-Number-PI-Through-Infinite-Sequenc
	// Approximation of the number pi through the Leibniz's series
	
	double _n,_i;          
	double s = 1;
	long double pi = 0;

	// Number of iterations
	_n = (rand()%(max_number - minimum_number))+minimum_number;
	//printf("n = %f\n",_n);
	
	for(_i = 1; _i <= (_n * 2); _i += 2){
     		pi = pi + s * (4 / _i);
     		s = -s;
   	}
	return pi;
	
}


