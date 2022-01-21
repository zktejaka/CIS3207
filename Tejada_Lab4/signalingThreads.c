#include <stdlib.h>
#include <stdio.h> 
#include <signal.h> 
#include <time.h> 
#include <sys/types.h>
#include <pthread.h>
#include <sys/wait.h> 
#include <sys/times.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <assert.h> 
#include <sys/types.h>
#include <string.h>
#define NUM_THREADS 5

void* generating_thread(void* arg);
void* signal_handler_sigusr1(void *arg);
void* signal_handler_sigusr2(void *arg);
void* reporting_thread(void *arg);
void sleep_random_interval(double low, double high); 
int random_signal(); 
void block_both();
void block_signal(int signal);
double calc_average(double diff[10]);

int sigusr1_received_counter; 
int sigusr2_received_counter; 
int sigusr1_sent_counter; 
int sigusr2_sent_counter;  

pthread_mutex_t lock_sigusr1_sent; 
pthread_mutex_t lock_sigusr2_sent; 
pthread_mutex_t lock_sigusr1_received; 
pthread_mutex_t lock_sigusr2_received; 

pthread_t threads[NUM_THREADS];
double avg_avg1, avg_avg2;  

int main (int argc, char *argv[]){
  sigusr1_sent_counter = 0;
  sigusr2_sent_counter = 0; 
  sigusr1_received_counter = 0;
  sigusr2_received_counter = 0; 

  int res = pthread_mutex_init(&lock_sigusr1_sent, NULL);
  int res2 = pthread_mutex_init(&lock_sigusr2_sent, NULL);
  int res3 = pthread_mutex_init(&lock_sigusr1_received, NULL);
  int res4 = pthread_mutex_init(&lock_sigusr2_received, NULL);

  if(res == -1 || res2 == -2 || res3 == -1 || res4 == -1){
    puts("pthread_mutex_init() error");
    exit(1);
  }

  // create threads 
  for(int i = 0; i<NUM_THREADS; ++i){
    if(i == 0 || i == 1){
      //SIGUSR1 handling thread 
      if(pthread_create(&threads[i], NULL, signal_handler_sigusr1, NULL) != 0){
        fprintf(stderr, "%s\n", "Could not create SIGUSR1 handling thread"); 
        exit(1); 
      }
    }

    if(i == 2 || i == 3){
      //SIGUSR2 handling thread 
      if(pthread_create(&threads[i], NULL, signal_handler_sigusr2, NULL) != 0){
        fprintf(stderr, "%s\n", "Could not to create SIGUSR2 handling thread"); 
        exit(1); 
      }
    }

    if(i == 4 || i == 5 || i == 6){
      //Generating threads (3)
      if(pthread_create(&threads[i], NULL, generating_thread, NULL) != 0){
        fprintf(stderr, "%s\n", "Could not to create generating thread"); 
        exit(1); 
      }
    }

    if(i == 7){
      //Reporting thread
      if(pthread_create(&threads[i], NULL, reporting_thread, NULL) != 0){
        fprintf(stderr, "%s\n", "Could not to create reporting thread"); 
        exit(1); 
      }
    }

  }
  block_both();

  puts("Currently running ...  Multiple Processes");
  puts("\nPress the Enter key to stop program execution\n");


  while(1){
    if(getchar())
      break; 
    //sleep(30);
    //break; 
  }

  
  puts("data2.log has report\n");

  //display avg of avgs
  FILE *fptr = fopen("data2.log", "a+");
  fprintf(fptr, "\n\n%s%lf%s\n", "SIGUSR1 avg time: ", avg_avg1, " seconds");
  fprintf(fptr, "%s%lf%s\n", "SIGUSR2 avg time: ", avg_avg2, " seconds");
  fclose(fptr);

  for(int i = 0; i < 8; ++i){
  //kill threads 
    pthread_kill(threads[i], SIGTERM); 
  }
}

void* generating_thread(void* arg){
  block_both();
  int j = 0; 
  while(1){
    sleep_random_interval(.01, .1); 
    int signal = random_signal(); 
     for(int i =0; i<NUM_THREADS; ++i){
        pthread_kill(threads[i], signal);

        if(signal == 0)
          puts("signal not sent");
       }

      if(signal == SIGUSR1){
       //increase counter for sigusr1
       pthread_mutex_lock(&lock_sigusr1_sent); 
       ++sigusr1_sent_counter;
       pthread_mutex_unlock(&lock_sigusr1_sent);
      }  
      else{
        //increase counter for sigusr2
        pthread_mutex_lock(&lock_sigusr2_sent); 
        ++sigusr2_sent_counter;
        pthread_mutex_unlock(&lock_sigusr2_sent);
      }
  }
}

void* signal_handler_sigusr1(void *arg){
  block_both();

  sigset_t sigset;
  sigemptyset(&sigset);
  sigaddset(&sigset, SIGUSR1); //wait for SIGUSR1
  int retval = 0;
  int signal;

  while (1){
	  retval = sigwait(&sigset, &signal);
	  if(signal == SIGUSR1){ //success
        pthread_mutex_lock(&lock_sigusr1_received); 
        ++sigusr1_received_counter; 
        pthread_mutex_unlock(&lock_sigusr1_received);
	  }
  }
}

void* signal_handler_sigusr2(void *arg){
  block_both();

  sigset_t sigset;
  sigemptyset(&sigset);
  sigaddset(&sigset, SIGUSR2); // wait SIGUSR2
  int retval = 0;
  int signal;

  while (1){
	  retval  = sigwait(&sigset, &signal); 
	  if(signal == SIGUSR2){
        pthread_mutex_lock(&lock_sigusr2_received); 
        ++sigusr2_received_counter; 
        pthread_mutex_unlock(&lock_sigusr2_received);
	  }
  }
}

void* reporting_thread(void *arg){
  block_both();

  sigset_t sigset;
  sigemptyset(&sigset);
  sigaddset(&sigset, SIGUSR1);
  sigaddset(&sigset, SIGUSR2); // wait SIGUSR1 && SIGUSR2
  int retval = 0;
  int signal;
  int report_count = 0; 
  FILE *fptr = fopen("data2.log", "w");
  fprintf(fptr, "%s\n", "signaling multiple threads");
  fclose(fptr);

  //Record the previous time of signal 
  struct timespec before_sigsur1;
  struct timespec before_sigsur2;

  int report_sig1= 0;
  int report_sig2 = 0; 

  //Every 10 signals store difference
  double sigusr1_differences[10]; 
  double sigusr2_differences[10]; 
  int sigusr1_index_differences = 0; 
  int sigusr2_index_differences = 0; 

  double prev_avg1 = 0; 
  double prev_avg2 = 0; 


    while(1){
	    retval  = sigwait(&sigset, &signal);
        ++report_count; 

        struct timespec current;

        if(clock_gettime( CLOCK_MONOTONIC, &current) == -1 ){
            fprintf(stderr, "%s", "clock_gettime error");
            exit(1);
        }

	  if(signal == SIGUSR1){
        ++report_sig1;
        if(report_sig1 == 1){
            //Min 2 signals 
            before_sigsur1.tv_nsec = current.tv_nsec;
            before_sigsur1.tv_sec = current.tv_sec;
            continue;
      }
      //printf("CURRENT: %ld\n", current.tv_nsec + current.tv_sec);

      double difference = (current.tv_sec - before_sigsur1.tv_sec) * 1e9; 
      difference =  (difference + (current.tv_nsec - before_sigsur1.tv_nsec)) * 1e-9; // in SECONDS 

      sigusr1_differences[sigusr1_index_differences] = difference; 

      before_sigsur1.tv_nsec = current.tv_nsec; 
      before_sigsur1.tv_sec = current.tv_sec;

      ++sigusr1_index_differences; 
	  }

    else if (signal == SIGUSR2){
      ++report_sig2;

      if(report_sig2 == 1){
        //Min 2 signals 
        before_sigsur2.tv_nsec = current.tv_nsec;
        before_sigsur2.tv_sec = current.tv_sec;
        continue;
      }

      double difference = (current.tv_sec - before_sigsur2.tv_sec) * 1e9; 
      difference = (difference + (current.tv_nsec - before_sigsur2.tv_nsec)) * 1e-9; 

      sigusr2_differences[sigusr2_index_differences] = difference; 

      before_sigsur2.tv_nsec = current.tv_nsec; 
      before_sigsur2.tv_sec = current.tv_sec;

      ++sigusr2_index_differences;
    }


    if(report_count % 10 == 0){
      struct timespec current;

      if(clock_gettime( CLOCK_MONOTONIC, &current) == -1 ){
        fprintf(stderr, "%s", "clock_gettime() error");
        exit(1);
      }  

      double avg = calc_average(sigusr1_differences); 

      if(prev_avg1 == 0){
        prev_avg1 = avg;
      }

      avg_avg1 = (prev_avg1 + avg) / 2.0;


      //printf("SIGUSR1 avg time: %lf SECONDS\n", avg);
      double avg2 = calc_average(sigusr2_differences); 
      //printf("SIGUSR2 avg time: %lf SECONDS\n", avg);

      if(prev_avg2 == 0){
        prev_avg2 = avg2; 
      }

      if(avg2 == 0)
        avg2 = prev_avg2;

      avg_avg2 = (prev_avg2 + avg2) / 2.0;

      

      fptr = fopen("data2.log", "a+");
      fprintf(fptr, "\n");
      fprintf(fptr, "%s%ld\n", "System Time (nsec+sec): ", (current.tv_nsec + current.tv_sec));
      fprintf(fptr, "%s%d\n", "SIGUSR1 S: ", sigusr1_sent_counter);
      fprintf(fptr, "%s%d\n", "SIGUSR2 S: ", sigusr2_sent_counter);
      fprintf(fptr, "%s%d\n", "SIGUSR1 R: ", sigusr1_received_counter);
      fprintf(fptr, "%s%d\n", "SIGUSR2 R: ", sigusr2_received_counter);
      fprintf(fptr, "%s%d\n", "Count Total: ", report_count);
      fprintf(fptr, "%s%lf%s\n", "SIGUSR1 avg time: ", avg, " seconds");
      fprintf(fptr, "%s%lf%s\n", "SIGUSR2 avg time: ", avg2, " seconds");
      fclose(fptr);

      //Reset, 10 signals reached 
      sigusr1_index_differences = 0; 
      sigusr2_index_differences = 0; 
      memset(sigusr1_differences, 0, sizeof(sigusr1_differences)); //https://www.tutorialspoint.com/c_standard_library/c_function_memset.htm
      memset(sigusr2_differences, 0, sizeof(sigusr2_differences)); 
    }
  }
}

void sleep_random_interval(double low, double high){
  //generate random between low(.01) and high(.1) 
  
  int num = time(NULL) ^ pthread_self();
  srand(num); 

  double random_double = (double)rand() * (low - high) / (double)RAND_MAX + high; 
 // printf("%lf\n", random_double); 

  double microseconds = (random_double * 1000000); //get microseconds

  //printf("I will sleep for %lf seconds\n", (sleeep.tv_nsec/1e9)); 

  struct timespec start, stop, sleeep, rem;

  sleeep.tv_sec = 0; 
  sleeep.tv_nsec = microseconds * 1000; //nanoseconds  

  if(nanosleep(&sleeep, &rem) != 0){
    puts("nanosleep() error");
    exit(1);
  }

}

int random_signal(){
  //generate random number between 0 and 1, if 0 SIGUSR1, else SIGUSR2 

  int seed = pthread_self() ^ time(NULL); 
  srand(seed); 

  int random_int = (rand() % 2) + 1; //[1-2]

  if(random_int == 1)
    return SIGUSR1; 
  else
    return SIGUSR2; 
}

void block_both(){
  //Parent, will want to block both signal

  sigset_t sigset; 
  sigemptyset(&sigset); 
  sigaddset(&sigset, SIGUSR1);
  sigaddset(&sigset, SIGUSR2);

  pthread_sigmask(SIG_BLOCK, &sigset, NULL);
}

void block_signal(int signal){
  // block either SIGUSR1 or SIGUSR2

  sigset_t sigset; 
  sigemptyset(&sigset); 
  sigaddset(&sigset, signal);
  pthread_sigmask(SIG_BLOCK, &sigset, NULL);
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
    // possible recent 10 signals
    avg = 0; 
  }
  else{
    avg = (sum / j); 
  }
  return avg; 
}