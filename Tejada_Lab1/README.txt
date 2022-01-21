# project-1-zktejaka
Kat Tejada CIS3207 Section 3: Project 1 - Giorgio's Discrete Event Simulator


*Basic Explanation*
This program is a simulation of an operating system handling events.

Several variables (time intervals, probability values, etc) are set through 
a configuration file. Jobs enter the system at a random time based on a predetermined interval. 
Events are added to a priority queue based on the time of execution.
Each component has it's own queue for events waiting to be processed. 
Length of use for each component is determined randomly when an event accesses the component. 
Each event is logged. Statistics for each component is calculated and printed to a file.



*Comprehensive Explanation*
In this simulation, there are events and device usage. For example, a process, or task, arrives to use
the CPU and there is an associated time of arrival. This tasks then uses the CPU for a period of time
and then the process finishes at a certain time. Each deviice is going to behvae in the same way. 


Queues exist for when an event is already active on a device and the second event waits in a queue before
running. Before entering a queue, an event will see which device is the least busy and pop into that queue.
All of the device queues will be FIFO (First in first out) even though this is not the case in every real
life system. When an event in the queue is ready to run, we will reccord the amount of time it takes 
to complete said event. 

There will also be one priority queue. A priority queue is an abstract data type similar to a regular 
queue or stack data structure in which each element additionally has a priority associated with it. 
This queue will be used to store events and drive the system. An event in the priority queue may look 
like printf("is ready for the CPU "); printf("stops using the CPU "); or printf("is ready to use a disk ");. 
Events will be removed from the priority queue and processed based on the time that the event is supposed 
to occur. The priority queue will be a switch statement. The first job to be added to the priority queue
will be initialized as such...

//adding the first job to the queue
  priorityQueue[0].jobID = 1;
  priorityQueue[0].event = ARRIVAL;
  priorityQueue[0].time = INIT_TIME;

    After the first job is initialized, the rest of the jobs will follow this logic....
    - while the priority queue size is not 0, pop event from the queue
    - when first arriving, push priority queue and put values into according variables
    - Manage disks and check activity, push accordingly

Ex: switch(priorityQueue[0].event){
      case ARRIVAL:
        pushPriQueue(priorityQueue, priorityQueue[0].jobID, ENTER_CPU, currTime);
        newJob(priorityQueue);
        break;

      case ENTER_CPU:
        handleCPU(priorityQueue, &cpu);
        break;

        etc...


    Components will be initialized such as...
    struct component{
        int *queue;
        int memSize;
        int jobCount;
        etc...

        Events 

When a new job arrives in the system, the job
This simulation will be driven by removing events from the priority queue and acting on the event.
Each event that is removed has a task to preform and an event time. The event time associated with an
event removed from the priority queue establishes a new 'current time'. To determine the next time of arrival
I generate a random int between ARRIVE_MIN & ARRIVE_MAX and add it to the current time. This process
will advance time until the program reaches FIN_TIME.

The actual start time for a new process entering the system...

Ex:
    void newJob(struct event *queue){
    //Choose arrival time randomyly
    int time = randomNum(ARRIVE_MAX, ARRIVE_MIN);
    pushPriQueue(queue, jobNum+1, ARRIVAL, currTime+time);
    jobNum++;
}