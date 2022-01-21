#include <stdio.h>
#include <stdlib.h>
#include <math.h>

//defining the events as integers
#define ARRIVAL 0
#define ENTER_CPU 1
#define START_CPU 2
#define LEAVE_CPU 3
#define ENTER_DISK 4
#define START_DISK1 5
#define LEAVE_DISK1 6
#define START_DISK2 7
#define LEAVE_DISK2 8
#define ENTER_NETWORK 9
#define START_NETWORK 10
#define LEAVE_NETWORK 11
#define LEAVE_SYSTEM 12
#define SIM_END -1

//config variables in .txt file global for easier access
int SEED, FIN_TIME, ARRIVE_MIN, ARRIVE_MAX, CPU_MIN, CPU_MAX, DISK1_MIN, DISK1_MAX, DISK2_MIN, DISK2_MAX, NETWORK_MIN, NETWORK_MAX;

int INIT_TIME = 0;
float QUIT_PROB = .2;
float NETWORK_PROB = .3;

int priQueueSize;         //Current amount of events in the queue
int priQueueMax;
int compQueueMax;
int jobNum;             //Variable to keep track of how many jobs are running
int currTime;

//a struct for each job
struct event{
  int jobID;
  int event;
  int time;
};

/*
  Struct for the components. Includes the component's queue along with
  variables for statistic collection.
*/
struct component{
  int *queue;
  int memSize;              //integer for resizing the queue
  int maxSize;              //Largest amount of events in the queue at one time
  int jobCount;             //Count of jobs in the queue
  int avgNum;               //Numerator to find the average size; adds the current size every time the queue is accessed
  int avgDenom;             //Denomenator to find the average size; adds one every time avgsizenum is updated
  int busy;                 //If the component is busy (0 or 1)
  int lastFree;             //Holds the last time the busy variable was set to 0
  int timeBusy;             //Current time - lastfree, How long it was busy
  int throughput;           //Count of jobs finished with the component
} comp;

//function prototypes
void setvars(FILE *);
void popPriQueue(struct event *);
void pushPriQueue(struct event *, int, int, int);
void resizePriQueue(struct event *);
void resizeQueue(struct component *);
void newJob(struct event *);
int randomNum(int, int);
void pushQueue(struct component *, int);
int popQueue(struct component *);
void handleCPU(struct event *, struct component *);
void handleDISKS(struct event *, struct component *, struct component *);
void handleNETWORK(struct event *, struct component *);
void initComponents(struct component *);
void writeLog(int, int, int);
void stats(struct component, char *);


int main(int argc, char *argv[] ){

  //program requires the config file to run
  if (argc != 2){
    printf("Try again, use ./simulation config.txt");
    exit(1);
  }

  //Opening the config file
  FILE *file = fopen(argv[1], "r");
  if (file == NULL){
    printf(" Config File cannot be opened");
    exit(1);
  }
  setvars(file);
  fclose(file);

  //Setting the seed for the random function
  srand(SEED);

  //Defining the components and allocating space for their queues
  struct component cpu, disk1, disk2, network;
  initComponents(&cpu);
  initComponents(&disk1);
  initComponents(&disk2);
  initComponents(&network);

  //Current time
  currTime = INIT_TIME;

  priQueueMax = (FIN_TIME-INIT_TIME)*sizeof(struct event);
  //Priority queue to hold the events
  struct event *priorityQueue = malloc(priQueueMax);

  //adding the first job to the queue
  priorityQueue[0].jobID = 1;
  priorityQueue[0].event = ARRIVAL;
  priorityQueue[0].time = INIT_TIME;
  
  //adding the event that ends the simulation
  priorityQueue[1].jobID = -1;
  priorityQueue[1].event = SIM_END;
  priorityQueue[1].time = FIN_TIME;

  priQueueSize = 2;
  //Variable to control the loop; set to 0 when the
  int running = 1;
  jobNum = 1;

   printf("\n\n");

  while(priQueueSize != 0 && running){
    //Pop event from priority pqueue
    //send event to the write log function
    //Set the current time
    //if event == job arrival, set arrival time for next job
    //handle the event accordingly
    currTime = priorityQueue[0].time;
    if (currTime >= FIN_TIME){
      break;
    }
    writeLog(priorityQueue[0].jobID, priorityQueue[0].event, priorityQueue[0].time);

    if (priQueueSize >= (priQueueMax-3)){
        resizePriQueue(priorityQueue);
    }

    switch(priorityQueue[0].event){
      case ARRIVAL:
        pushPriQueue(priorityQueue, priorityQueue[0].jobID, ENTER_CPU, currTime);
        newJob(priorityQueue);
        break;
      case ENTER_CPU:
        handleCPU(priorityQueue, &cpu);
        break;
      case START_CPU:
        handleCPU(priorityQueue, &cpu);
        break;
      case LEAVE_CPU:
        handleCPU(priorityQueue, &cpu);
        break;
      case ENTER_DISK:
        handleDISKS(priorityQueue, &disk1, &disk2);
        break;
      case START_DISK1:
        handleDISKS(priorityQueue,  &disk1, &disk2);
        break;
      case LEAVE_DISK1:
        handleDISKS(priorityQueue, &disk1, &disk2);
        break;
      case START_DISK2:
        handleDISKS(priorityQueue, &disk1, &disk2);
        break;
      case LEAVE_DISK2:
        handleDISKS(priorityQueue, &disk1, &disk2);
        break;
      case ENTER_NETWORK:
        handleNETWORK(priorityQueue, &network);
        break;
      case  START_NETWORK:
        handleNETWORK(priorityQueue, &network);
        break;
      case LEAVE_NETWORK:
        handleNETWORK(priorityQueue, &network);
        break;
      case LEAVE_SYSTEM:
        //nullify job?
        break;
      case SIM_END:
        running = 0;
        break;
    }
    popPriQueue(priorityQueue);
  }

  printf("\nStatistics\n\n");
  stats(cpu, "CPU");
  stats(disk1, "DISK1");
  stats(disk2, "DISK2");
  stats(network, "NETWORK");

  free(priorityQueue);
  free(cpu.queue);
  free(disk1.queue);
  free(disk2.queue);
  free(network.queue);
  exit(0);
}

/*
  This function sets the configuration variables.
  Multiply each number by 10^i, where i is the
  number's place value, and adding it to the appropriate variable.
  For each line, the characters are placed into the num array
  until a non-numerical character is found. Then, the num array
  is read, multiplying each number by 10^i, where i is the
  number's place value, and adding it to the appropriate variable.
  Then the values of each variable are checked if they are appropriate.
*/
void setvars(FILE *config){
  //char arrays for the lines and the nummerical characters taken from each line
  char *line = (char *)malloc(sizeof(char)*50);
  char *num = (char *)malloc(sizeof(char)*50);
  //int array for the values of the variables
  int *vnums = (int *)malloc(sizeof(int)*50);

  int linenum = 0;      //index for while loops
  int i;                //index for nested loop;
  int j;
  int total;            //variable to keep track of the total value

  printf("Configuration Variables\n\n");

  //Loop is stopped when linenum = 15 since there are only 15 lines
  while (linenum < 15){
    fgets(line, 50, config);
    printf("%s", line);
    i = 0;
    //Adding the numerical characters to the num array
    while ((line[i] >= '0') && (line[i] <= '9')){
      num[i] = line[i];
      i++;
    }
    num[i--] = '\0';

    j = 0;
    total = 0;
    //Converting the characters to an integer by multiplying by 10^i where i is the
    //appropriate place value
    while (num[i] != '\0'){
      total += (num[i] - 48) * (int)pow(10, j);
      i--;
      j++;
    }
    vnums[linenum] = total;
    linenum++;
  }
  i = 0;
  int error = 0;                    //int to indicate an inappropriate value

  //Setting the variables and checking constraints
  SEED = vnums[i++];

  if(vnums[i]%10 != 0){
    printf("ERROR: FIN_TIME must be a positive number divisible by 10.\n");
    error = 1;
  }
  FIN_TIME = vnums[i++];            // restrict numbers to be divisible by 10 for simplicity

  if((vnums[i] <= 10)){
    printf("ERROR: ARRIVE_MIN should be at least 10.\n");
    error = 1;
  }
  ARRIVE_MIN = vnums[i++];          //at least 10

  if((vnums[i] < ARRIVE_MIN)){
    printf("ERROR: ARRIVE_MAX must be greater than ARRIVE_MIN.\n");
    error = 1;
  }
  else
    ARRIVE_MAX = vnums[i++];

  if((vnums[i] < 15)){
    printf("ERROR: CPU_MIN should be at least 15.\n");
    error = 1;
  }
  CPU_MIN  = vnums[i++];            //at least 15

  if((vnums[i] <= CPU_MIN) || (vnums[i] > 40)){
    printf("ERROR: ARRIVE_MIN should be at least 10\n");
    error = 1;
  }
    CPU_MAX  = vnums[i++];            //at most 40

  if((vnums[i] < 100)){
    printf("ERROR: DISK1_MIN SHOULD BE AT LEAST 100\n");
    error = 1;
  }
  DISK1_MIN  = vnums[i++];          //should be significantly greater than CPU time

  if((vnums[i] > 250)){
    printf("ERROR: DISK1_MAX should be at most 250\n");
    error = 1;
  }
  DISK1_MAX  = vnums[i++];

  if((vnums[i] < 100)){
    printf("ERROR: DISK2_MIN SHOULD BE AT LEAST 100\n");
    error = 1;
  }
  DISK2_MIN  = vnums[i++];

  if((vnums[i] > 250)){
    printf("ERROR: DISK2_MAX should be at most 250\n");
    error = 1;
  }
  DISK2_MAX  = vnums[i++];

  if((vnums[i] < 100)){
    printf("ERROR: NETWORK_MIN should be at least 100\n");
    error = 1;
  }
  NETWORK_MIN  = vnums[i++];

  if((vnums[i] > 400)){
    printf("ERROR: NETWORK_MAX should be at most 400\n");
    error = 1;
  }
  NETWORK_MAX  = vnums[i++];

  if (error == 1)
    exit(1);
  //printf("0 - INIT_TIME\n");
  //printf(".2 - QUIT_PROB\n");
  //printf(".3 - NETWORK_PROB\n");
  free(line);
  free(num);
  free(vnums);
}

void initComponents(struct component *comp){
  comp->memSize = (FIN_TIME-INIT_TIME)*sizeof(int);
  comp->queue = malloc(comp->memSize);
  comp->jobCount = 0;
  comp->maxSize = 0;
  comp->avgNum = 0;
  comp->avgDenom = 0;
  comp->busy = 0;
  comp->lastFree = 0;
  comp->timeBusy = 0;
  comp->throughput = 0;
}

void resizePriQueue(struct event *queue){
  priQueueMax += priQueueMax*sizeof(struct event);
  queue = realloc(queue, priQueueMax);
}

void resizeQueue(struct component *comp){
  comp->memSize += comp->memSize*sizeof(int);
  comp->queue = realloc(comp->queue, comp->memSize);
}


/*
  Function to push an event onto a queue.
  int *event is the array of {event, job, time}
  int queue is the target queue
*/
void pushPriQueue(struct event *queue, int job, int event, int time){
  queue[priQueueSize].jobID = job;
  queue[priQueueSize].event = event;
  queue[priQueueSize].time = time;

  int i = priQueueSize;
  int j = i - 1;
  int tempJob;
  int tempEvent;
  int tempTime;

  while (i > 0 && queue[j].time > queue[i].time){

    tempJob = queue[j].jobID;
    tempEvent = queue[j].event;
    tempTime = queue[j].time;

    queue[j].jobID = queue[i].jobID;
    queue[j].event = queue[i].event;
    queue[j].time = queue[i].time;

    queue[i].jobID = tempJob;
    queue[i].event = tempEvent;
    queue[i].time = tempTime;


    i--;
    j--;
  }
  priQueueSize++;
}


/*
  Function to pop an event off of a queue.
  int queue is the target queue
*/
void popPriQueue(struct event *queue){
  int i = 1;

  while (i < priQueueSize){
    queue[i-1].jobID = queue[i].jobID;
    queue[i-1].event = queue[i].event;
    queue[i-1].time = queue[i].time;
    i++;
  }
  priQueueSize--;
}

/*
  Function to set the arrival time for a new job
*/
void newJob(struct event *queue){
  //Choose arrival time randomyly
  int time = randomNum(ARRIVE_MAX, ARRIVE_MIN);
  pushPriQueue(queue, jobNum+1, ARRIVAL, currTime+time);
  jobNum++;
}

int randomNum(int max, int min){
  int ret;
  ret = rand() % ((max+1-min)+min);
  if (ret >= min && ret <= max)
    return ret;
  else
    return randomNum(max, min);
}

/*
  Function to push an event onto a queue.
  int *event is the array of {job, event, time}
  int queue is the target queue
*/
void pushQueue(struct component *comp, int job){
  if (comp->memSize <= comp->jobCount-3){
    resizeQueue(comp);
  }
  else if (comp->jobCount == 0){
    comp->queue[0] = job;
  }
  else{
    comp->queue[comp->jobCount] = job;
  }
  comp->jobCount++;
}

int popQueue(struct component *comp){

  if (comp->jobCount == 0)
    return -1;

  int next = comp->queue[0];

  int i = 1;
  while (i < comp->jobCount){
    comp->queue[i-1] = comp->queue[i];
    i++;
  }
  comp->jobCount--;

  if (comp->jobCount == 0){
    comp->timeBusy += currTime-comp->lastFree;
    comp->busy = 0;
  }
  return next;
}

/*
  Function to handle any CPU events
*/
void handleCPU(struct event *queue, struct component *comp){
  int end, next;
  float probNum;

  switch (queue[0].event){
    case ENTER_CPU:
      if (comp->busy == 1){
        pushQueue(comp, queue[0].jobID);
      }
      else {
        pushPriQueue(queue, queue[0].jobID, START_CPU, currTime);
        comp->busy = 1;
      }
      break;

    case START_CPU:
      comp->busy = 1;
      end = randomNum(CPU_MAX, CPU_MIN);
      pushPriQueue(queue, queue[0].jobID, LEAVE_CPU, currTime + end);
      break;

    case LEAVE_CPU:
      if (comp->jobCount == 0){
        comp->busy = 0;
      }
      else {
        next = popQueue(comp);
        pushPriQueue(queue, next, START_CPU, currTime);
      }
      probNum = randomNum(100, 0);
      if (probNum <= 100*QUIT_PROB){
        pushPriQueue(queue, queue[0].jobID, LEAVE_SYSTEM, currTime);
      }
      else if (probNum <= 100*NETWORK_PROB){
        pushPriQueue(queue, queue[0].jobID, ENTER_NETWORK, currTime);
      }
      else {
        pushPriQueue(queue, queue[0].jobID, ENTER_DISK, currTime);
      }
      comp->throughput++;
      break;

    default: printf("\nERROR: handleCPU cannot handle this event.");
  }

  //modifying statistic variables
  if (comp->jobCount > comp->maxSize)
    comp->maxSize = comp->jobCount;

  comp->avgNum += comp->jobCount;
  comp->avgDenom++;
}

/*
  Function to handle any Disk events
*/
void handleDISKS(struct event *queue, struct component *comp, struct component *comp2){
   int rand, next;
  switch(queue[0].event){

    case ENTER_DISK:
      if (comp->busy && comp2->busy){
        if (comp->jobCount <= comp2->jobCount)
          pushQueue(comp, queue[0].jobID);
        else
          pushQueue(comp2, queue[0].jobID);
      }
      else if (comp->busy == 0){
        pushPriQueue(queue, queue[0].jobID, START_DISK1, currTime);
        comp->busy = 1;
      }
      else if (comp2->busy == 0){
        pushPriQueue(queue, queue[0].jobID, START_DISK2, currTime);
        comp2->busy = 1;
      }
      break;

    case START_DISK1:
      comp->busy = 1;
      rand = randomNum(DISK1_MAX, DISK1_MIN);
      pushPriQueue(queue, queue[0].jobID, LEAVE_DISK1, currTime + rand);
      break;

    case LEAVE_DISK1:
      if (comp->jobCount == 0){
        comp->busy = 0;
      }
      else {
        next = popQueue(comp);
        pushPriQueue(queue, next, START_DISK1, currTime);
      }
      pushPriQueue(queue, queue[0].jobID, ENTER_CPU, currTime);
      comp->throughput++;
      break;

    case START_DISK2:
      comp2->busy = 1;
      rand = randomNum(DISK2_MAX, DISK2_MIN);
      pushPriQueue(queue, queue[0].jobID, LEAVE_DISK2, currTime + rand);
      break;

    case LEAVE_DISK2:
      if (comp2->jobCount == 0){
        comp2->busy = 0;
      }
      else {
        next = popQueue(comp);
        pushPriQueue(queue, next, START_DISK2, currTime);
      }
      pushPriQueue(queue, queue[0].jobID, ENTER_CPU, currTime);
      comp2->throughput++;
      break;
  }
  //modifying statistic variables
  if (comp->jobCount > comp->maxSize)
    comp->maxSize = comp->jobCount;

  comp->avgNum += comp->jobCount;
  comp->avgDenom++;

  if (comp2->jobCount > comp2->maxSize)
    comp2->maxSize = comp2->jobCount;

  comp2->avgNum += comp2->jobCount;
  comp2->avgDenom++;
}

/*
  Function to handle any network events
*/
void handleNETWORK(struct event *queue, struct component *comp){
  int rand, next;
  switch (queue[0].event){

    case ENTER_NETWORK:
      if (comp->busy){
        pushQueue(comp, queue[0].jobID);
      }
      else{
        pushPriQueue(queue, queue[0].jobID, START_NETWORK, currTime);
        comp->busy = 1;
      }
      break;

    case START_NETWORK:
      comp->busy = 1;
      rand = randomNum(NETWORK_MAX, NETWORK_MIN);
      pushPriQueue(queue, queue[0].jobID, LEAVE_NETWORK, currTime + rand);
      break;

    case LEAVE_NETWORK:
      if (comp->jobCount == 0){
        comp->busy = 0;
      }
      else {
        next = popQueue(comp);
        pushPriQueue(queue, next, START_NETWORK, currTime);
      }
      pushPriQueue(queue, queue[0].jobID, ENTER_CPU, currTime);
      comp->throughput++;
      break;
  }

  //modifying statistic variables
  if (comp->jobCount > comp->maxSize)
    comp->maxSize = comp->jobCount;

  comp->avgNum += comp->jobCount;
  comp->avgDenom++;
}
/*
  Function to write event information to the log file
*/
void writeLog(int job, int event, int time){
  if (event == ENTER_CPU || event == ENTER_DISK || event == ENTER_NETWORK){
      return;
  }
  if (event < 0){
      printf("Simulation ends at time %d\n", time);
      return;
    }

    printf("Job %d ", job);

    switch(event){
      case ARRIVAL:
        printf("enters the system ");
        break;
      case ENTER_CPU:
        printf("is ready for the CPU ");
        break;
      case START_CPU:
        printf("starts using the CPU ");
        break;
      case LEAVE_CPU:
        printf("stops using the CPU ");
        break;
      case ENTER_DISK:
        printf("is ready to use a disk ");
        break;
      case START_DISK1:
        printf("starts using DISK1 ");
        break;
      case LEAVE_DISK1:
        printf("stops using the DISK1 ");
        break;
      case START_DISK2:
        printf("starts using the DISK2 ");
        break;
      case LEAVE_DISK2:
        printf("stops using the DISK2 ");
        break;
      case ENTER_NETWORK:
        printf("is ready to use the NETWORK ");
        break;
      case START_NETWORK:
        printf("starts using the NETWORK ");
        break;
      case LEAVE_NETWORK:
        printf("stops using the NETWORK ");
        break;
      case LEAVE_SYSTEM:
        printf("leaves the system ");
    }

    printf("at time %d\n", time);
}
/*
  Function to calculate and print the statistics
*/
void stats(struct component comp, char *compName){
  printf("\n%s\n", compName);
  printf("Maximum Queue Size - %d\n", comp.maxSize);
  float avg = comp.avgNum/comp.avgDenom;
  printf("Average Queue Size - %.2f\n", avg);
  float busy = comp.timeBusy/FIN_TIME;
  printf("Utilization - %.2f\n", busy);
  printf("Throughput - %d\n", comp.throughput);
}