#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>

#include<pthread.h>


#include"rdtsc.h"

//Macro definition
#define MAX_MSGS 10
#define MAX_MSG_LENGTH 512
int message_id = -1;
//Structure definition
struct message{
	long double buff; //16
	int m_id;	//4
	unsigned long long enqueue_time, dequeue_time; //8 *2 = 16
	pthread_t sender_id;	//8
	
};
	
struct mq{ 
	//int *item[MAX_MSGS];			//pointer to keep track of message
	struct message *msg[MAX_MSGS];
	int front;				//front points to front end of the queue
	int rear;				//rear points to last element of the queue
	int maxSize;				//Maximum capacity of queue
	int currentSize;			//keep track of current number of messages in queue
	
};

//********* FUNCTION DECLARATION *************//

//function to create queue SQ_CREATE
struct mq* sq_create(int size);

//funtion to check queue empty or not
int isEmptyMQ(struct mq *q);

//function to check queue full or not
int isFullMQ(struct mq *q);

//function to enqueue SQ_WRITE
int sq_write(struct mq *q, long double buff);

//function to dequeue SQ_READ
struct message * sq_read(struct mq *q);

//function to delete queue SQ_DELETE
int sq_delete(struct mq *q);
