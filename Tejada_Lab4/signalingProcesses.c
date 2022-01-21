#include <stdlib.h>
#include <stdio.h>
#include <string.h> 
#include <signal.h> 
#include <time.h> 
#include <sys/types.h>
#include <pthread.h>
#include <sys/wait.h> 
#include <sys/times.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <assert.h> 

#define BILLION 1000000000

void signal_create(); 
void signal_handler_sigusr1();
void signal_handler_sigusr2();
void signal_check_sigusr1(int signal);
void signal_check_sigusr2(int signal); 
void block_both();
void report(); 
void sleep_random_interval(double low, double high); 
int random_signal(); 

void block_sigusr1();
void block_sigusr2();

double calc_average(double diff[10]); 
void clean_up_processes(int signal); 

int reporting_counter; 
int report_sigusr1;
int report_sigusr2; 

double sigusr1_differences[10]; // up to 10
double sigusr2_differences[10];

struct timespec before_sigsur1;
struct timespec before_sigsur2; 

int sigusr1_index_differences; 
int sigusr2_index_differences; 

double avg_avg1, avg_avg2;  

struct shared_val{
  int sigusr1_recieved_counter; 
  int sigusr2_recieved_counter; 
  int sigusr1_sent_counter; 
  int sigusr2_sent_counter; 

  pthread_mutex_t lock_sigusr1_sent; 
  pthread_mutex_t lock_sigusr2_sent; 
  pthread_mutex_t lock_sigusr1_recieved; 
  pthread_mutex_t lock_sigusr2_recieved; 
  
} *shared_ptr; //Child can access

int main (int argc, char *argv[]){
  reporting_counter = 0; 
  report_sigusr1 = 0;
  report_sigusr2 = 0; 
  int shm_id; 
  pthread_mutexattr_t attr; 
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED); 

  shm_id = shmget(IPC_PRIVATE, sizeof(struct shared_val), IPC_CREAT | 0666);

  if(shm_id < 0){
    puts("shmget() error");
    exit(1); 
  }

  shared_ptr = (struct shared_val*) shmat(shm_id, NULL, 0); 

  if(shared_ptr == (struct shared_val *) -1){
    puts("shmat() error");
    exit(1); 
  }

  //init counters to 0 
  shared_ptr->sigusr1_sent_counter = 0; 
  shared_ptr->sigusr2_sent_counter = 0; 
  shared_ptr->sigusr1_recieved_counter = 0;
  shared_ptr->sigusr2_recieved_counter = 0; 

  //init locks with set attribute 
  pthread_mutex_init(&(shared_ptr->lock_sigusr1_sent), &attr); 
  pthread_mutex_init(&(shared_ptr->lock_sigusr2_sent), &attr); 
  pthread_mutex_init(&(shared_ptr->lock_sigusr1_recieved), &attr); 
  pthread_mutex_init(&(shared_ptr->lock_sigusr2_recieved), &attr); 

  //child processes ... 
  pid_t parent;
  pid_t pids[8]; 
  int status;

   pid_t pids2[8];

  for(int i = 0; i < 8; ++i){

    pids[i] = fork(); 

    if(pids[i] == -1){
      fprintf(stderr, "%s\n", "fork() failed");
      exit(1); 
    }

    else if(pids[i] == 0){
      // [0-1] sigusr1 handling 
      // [2-3] sigusr2 handling
      // [4-6] signal generating 
      // [7] reporting 

      if(i == 0 || i == 1){
        signal_handler_sigusr1(); 
        pids2[i] = getpid();
      }
      else if(i == 2 || i == 3) {
        signal_handler_sigusr2(); 
        pids2[i] = getpid();
      }
      else if(i == 4 || i == 5 || i == 6){
        block_both();
        signal_create(); 
        pids2[i] = getpid();
      }
      else if(i == 7) {
          report(); 
          pids2[i] = getpid();
      }
    }
    else {
    // PARENT 
      if((i) != 7)
        continue; 
        //puts("parent, other processes forked"); 
        block_both();
    }

  //printf("parent pid %d\n", getpid());
  puts("Currently running ...  Multiple Processes");
  puts("\nPress the Enter key to stop program execution\n");

   while(1){
    if(getchar())
      break; 
  }

  puts("data.log has report\n");
  shmdt(shared_ptr); //detach parent
  //puts("clean-up");

  for(int i = 0; i<8; ++i){
    //signal to all, handler will detach from shared memory region and exit(0)
    kill(pids2[i], SIGTERM); 
  }

}

  //puts("PARENT");
  //printf("SENT sig1  %d\n", shared_ptr->sigusr1_sent_counter);
  //printf("SENT sig2 %d\n", shared_ptr->sigusr2_sent_counter);
  
  //printf("RECIEVE sig1 %d\n", shared_ptr->sigusr1_recieved_counter);
  //printf("RECIEVE sig2 %d\n", shared_ptr->sigusr2_recieved_counter);

  //printf("TOTAL RECIEVED %d\n", shared_ptr->sigusr1_recieved_counter + shared_ptr->sigusr2_recieved_counter);
  //puts("\n");
  //fflush(stdout);
  //signal(SIGINT, clean_up); //testing, ctrl-c 
  

}

void signal_create(){
  //loop, select SIGUSR1 or SIGUSR2 to send to processes 
  printf("generating %d\n", getpid()); 
  signal(SIGTERM, clean_up_processes);
  //block_both();

  while(1){
    sleep_random_interval(.01, .1); // sleep [.01-.1] 
    int signal = random_signal();       
    //printf("%d will send SIGUSR: %d\n", getpid(), signal); 

    //send the signal to "its peers"/ all child processes
    if(kill(0, signal) == -1){
      puts("failed to send signal");
      exit(0); 
    }

    if(signal == SIGUSR1){
      //increase counter for sigusr1 sent 
      pthread_mutex_lock(&(shared_ptr->lock_sigusr1_sent)); 
      shared_ptr->sigusr1_sent_counter++; 
      pthread_mutex_unlock(&(shared_ptr->lock_sigusr1_sent));
    }

    else{
      //increase counter for sigusr2 sent 
      pthread_mutex_lock(&(shared_ptr->lock_sigusr2_sent)); 
      shared_ptr->sigusr2_sent_counter++; 
      pthread_mutex_unlock(&(shared_ptr->lock_sigusr2_sent)); 
    }   
  } 
}

void sleep_random_interval(double low, double high){
  //generate random between low(.01) and high(.1) and sleep
  
  int num = time(NULL) ^ getpid();
  srand(num); 

  double random_double = (double)rand() * (low - high) / (double)RAND_MAX + high;
  //printf("%lf\n", random_double); 

  double microseconds = (random_double * 1000000); //get microseconds

  //printf("(%d) will sleep for %d microseconds\n", getpid(), microseconds); 

  struct timespec start, stop, sleeep, rem;

  sleeep.tv_sec = 0;
  sleeep.tv_nsec = microseconds * 1000; //nanoseconds 
  if(nanosleep(&sleeep, &rem) != 0){
    puts("nanosleep error");
    exit(1);
  }
}

int random_signal(){
  //generate random number between 0 and 1, if 0 SIGUSR1, else SIGUSR2 

  int seed = getpid() ^ time(NULL); 
  srand(seed); 

  int random_int = (rand() % 2) + 1; //[1-2]

  if(random_int == 1)
    return SIGUSR1; 
  else
    return SIGUSR2; 
}


void signal_handler_sigusr1(){
  printf("sigusr1 handling %d\n", getpid()); 
  signal(SIGUSR1, signal_check_sigusr1); //set signal 
  block_sigusr2(); //block other signal types
  signal(SIGTERM, clean_up_processes);

  while(1){
    sleep(1); 
  }

}

void signal_handler_sigusr2(){
  printf("sigusr2 handling %d\n", getpid()); 
  signal(SIGUSR2, signal_check_sigusr1); //set signal 
  block_sigusr1(); //block other signal types 
  signal(SIGTERM, clean_up_processes);
  signal(SIGUSR1, signal_check_sigusr2);

  while(1){
    sleep(1); 
  }

}


void block_sigusr1(){
  sigset_t sigset; 
  sigemptyset(&sigset); 
  sigaddset(&sigset, SIGUSR1); //add SIGUSR1 to set 

  int retval = sigprocmask(SIG_BLOCK, &sigset, NULL);

  if(retval == -1){
    puts("sigprocmask() error");
    exit(1); 
  }
}

void block_sigusr2(){
  sigset_t sigset; 
  sigemptyset(&sigset); 
  sigaddset(&sigset, SIGUSR2); //add SIGUSR2 to set 
  sigprocmask(SIG_BLOCK, &sigset, NULL); 
}


void signal_check_sigusr1(int signal){
  if(signal == SIGUSR1){
    //signal 1 arrived
    pthread_mutex_lock(&(shared_ptr->lock_sigusr1_recieved));
    shared_ptr->sigusr1_recieved_counter++;
    pthread_mutex_unlock(&(shared_ptr->lock_sigusr1_recieved));
  }
}

void signal_check_sigusr2(int signal){
  if(signal == SIGUSR2){
    //signal 2 arrived
    pthread_mutex_lock(&(shared_ptr->lock_sigusr2_recieved));
    shared_ptr->sigusr2_recieved_counter++;
    pthread_mutex_unlock(&(shared_ptr->lock_sigusr2_recieved));
  }
}


void report(){
  //Recieves both SIGUSR1 and SIGUSR2 (dont block)
  //Report after every 10 signals

  //Able to recieve both usr SIGNAL types 
  signal(SIGTERM, clean_up_processes);
  FILE *fptr = fopen("data.log", "w");
  fclose(fptr);

  //printf("reporting pid %d\n", getpid()); 

  block_both();

  sigset_t sigset;
  sigemptyset(&sigset);
  sigaddset(&sigset, SIGUSR1);
  sigaddset(&sigset, SIGUSR2); // wait SIGUSR1 && SIGUSR2
  int retval = 0;
  int signal;

  struct timespec before_sigsur1;
  struct timespec before_sigsur2;

  before_sigsur1.tv_sec = 0; 
  before_sigsur1.tv_nsec = 0; 
  before_sigsur2.tv_sec = 0; 
  before_sigsur2.tv_nsec = 0; 

  sigusr1_index_differences = 0; 
  sigusr2_index_differences = 0; 

  double prev_avg1 = 0; 
  double prev_avg2 = 0; 

  while(1){ 

    retval  = sigwait(&sigset, &signal);
    //SIGNAL retrieved
    ++reporting_counter; 

    struct timespec current;

    if(clock_gettime( CLOCK_MONOTONIC, &current) == -1 ){
      fprintf(stderr, "%s", "clock_gettime() error");
      exit(1);
    }


    if(signal == SIGUSR1){

      struct timespec current;

      if(clock_gettime( CLOCK_MONOTONIC, &current) == -1 ){
        fprintf(stderr, "%s", "clock_gettime() error");
        exit(1);
      }

      ++report_sigusr1;   

      if(report_sigusr1 == 1){ //Need min 2 signals 
        before_sigsur1.tv_nsec = current.tv_nsec;
        before_sigsur1.tv_sec = current.tv_sec;
      }


    double difference = (current.tv_sec - before_sigsur1.tv_sec) * 1e9; 
    difference = (difference + (current.tv_nsec - before_sigsur1.tv_nsec)) * 1e-9; 

    sigusr1_differences[sigusr1_index_differences] = difference; 

    before_sigsur1.tv_nsec = current.tv_nsec; 
    before_sigsur1.tv_sec = current.tv_sec;

    ++sigusr1_index_differences;
  }
    
  else if(signal == SIGUSR2){
    struct timespec current;

    if(clock_gettime( CLOCK_MONOTONIC, &current) == -1 ){
      fprintf(stderr, "%s", "clock_gettime() error");
      exit(1);
   }

    ++report_sigusr2;    

    if(report_sigusr2 == 1){ //Need min 2 signals 
      before_sigsur2.tv_nsec = current.tv_nsec;
      before_sigsur2.tv_sec = current.tv_sec;
    }

    double difference = (current.tv_sec - before_sigsur2.tv_sec) * 1e9; 
    difference = (difference + (current.tv_nsec - before_sigsur2.tv_nsec)) * 1e-9; 

    sigusr2_differences[sigusr2_index_differences] = difference; 

    before_sigsur2.tv_nsec = current.tv_nsec; 
    before_sigsur2.tv_sec = current.tv_sec;

    ++sigusr2_index_differences;
  }

  if(reporting_counter % 10 == 0 && reporting_counter != 0){
    struct timespec current;

    if(clock_gettime( CLOCK_MONOTONIC, &current) == -1 ){
    fprintf(stderr, "%s", "clock_gettime() error");
    exit(1);
    }

    double avg = calc_average(sigusr1_differences); 
    //printf("SIGUSR1 avg time between receptions: %lf seconds\n", avg);

      if(prev_avg1 == 0){
        prev_avg1 = avg;
      }

      if(avg == 0)
        avg = prev_avg1;

      avg_avg1 = (prev_avg1 + avg) / 2.0;


    double avg2 = calc_average(sigusr2_differences); 
    //printf("SIGUSR2 avg time between receptions: %lf seconds\n", avg2);

      // if(prev_avg2 == 0){
      //   prev_avg2 = avg2; 
      // }
      // if(avg2 == 0)
      //   avg2 = prev_avg2;
      // avg_avg2 = (prev_avg2 + avg2) / 2.0;


    fptr = fopen("data.log", "a+");
    fprintf(fptr, "\n");
    fprintf(fptr, "%s%ld\n", "System Time (nsec+sec): ", (current.tv_nsec + current.tv_sec));
    fprintf(fptr, "%s%d\n", "SIGUSR1 S: ", shared_ptr->sigusr1_sent_counter);
    fprintf(fptr, "%s%d\n", "SIGUSR2 S: ", shared_ptr->sigusr2_sent_counter);
    fprintf(fptr, "%s%d\n", "SIGUSR1 R: ", shared_ptr->sigusr1_recieved_counter);
    fprintf(fptr, "%s%d\n", "SIGUSR2 R: ", shared_ptr->sigusr2_recieved_counter);
    fprintf(fptr, "%s%d\n", "Total: ", reporting_counter);
    fprintf(fptr, "%s%lf%s\n", "SIGUSR1 avg time: ", avg, " seconds");
    fprintf(fptr, "%s%lf%s\n", "SIGUSR2 avg time: ", avg2, " seconds");
    fprintf(fptr, "%s%lf%s\n", "Total Averages: ", avg_avg1, " seconds");
    fclose(fptr);

    //Reset, 10 signals reached 
    sigusr1_index_differences = 0; 
    sigusr2_index_differences = 0; 
    memset(sigusr1_differences, 0, sizeof(sigusr1_differences)); //https://www.tutorialspoint.com/c_standard_library/c_function_memset.htm
    memset(sigusr2_differences, 0, sizeof(sigusr2_differences));  
    } 
  }
}

void clean_up_processes(int signal){
  if(signal == SIGTERM){
    printf("Process: %d terminated\n", getpid());
    shmdt(shared_ptr);
  }
  exit(0);
} 


void block_both(){
  //Parent, will want to block both signal

  sigset_t sigset; 
  sigemptyset(&sigset); 
  sigaddset(&sigset, SIGUSR1); 
  sigaddset(&sigset, SIGUSR2); 
  sigprocmask(SIG_BLOCK, &sigset, NULL);
}

double calc_average(double diff[10]){
  //Get avg based on diff in sigusr1 sigusr2

  int j = 0;
  double sum = 0; 
  double avg; 

  while(diff[j]){
    sum += diff[j];
    ++j; 
  }

  if(sum == 0){
    //possible recent 10 signals
    avg = 0; 
  }
  else{
    avg = (sum / j); 
  }
  return avg; 
}