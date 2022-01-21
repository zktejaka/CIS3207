#include <stdlib.h>
#include <stdio.h> 
#include <string.h> 
#include <unistd.h> 
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h> 
#include <ctype.h> 
#include <signal.h>

#define DEFAULT_DICTIONARY "dictionary.txt"
#define DEFAULT_PORT 8889
#define SIZE 50
#define NUM_WORKERS 50 // up to 50 threads
#define FALSE 1
#define TRUE 0


//network 
int socket_desc;
int port_number; 

//dictionary 
char *dictionary; //Default is "dictionary.txt" 
char **dictionary_words; //Every string in dictionary
int dictionary_count; //Allocate enough memory

//producer/consumer connect
int buffer_pro_cons[SIZE]; 
int queue_num; 
int add_index, remove_index; 
int current_thread; 
pthread_mutex_t connection_lock; 
pthread_cond_t produce; //Wait until can produce 
pthread_cond_t consume; //Wait until can consume

//producer-consumer log
char *buffer_log_Q[SIZE]; 
int log_queue_num; 
int add_index2, remove_index2; 
pthread_mutex_t log_lock;
pthread_cond_t produce2;  
pthread_cond_t consume2; 

//prototypes
void read_dictionary(); 
void init_locks(); 
int check_dictionary(char *word); 
void produce_queue(int socket); 
void create_threads(); 

//worker threads 
void* worker_thread(void* id); 
int consume_queue(); 
void add_log(char *result); 
pthread_attr_t detach;

//log prototypes
FILE *fptr; 
void* log_thread(void *arg);
char* remove_log(); 

//Free heap when program killed  
void sigint_handler(int sig_num); 

FILE *fptr2; 

void print_queue(); // testing 

int main(int argc , char *argv[]) { //Usually dictionary.txt then port num
    if(argc == 1){
    //Dictionary and port not specified
    dictionary = DEFAULT_DICTIONARY;
    port_number = DEFAULT_PORT; 
  }

  else if (argc == 2){ //is arg passed a word or number?
    if((access(argv[1], F_OK) != -1)){ //Dictionary file 
      dictionary = argv[1]; 
      port_number = DEFAULT_PORT; 
    }
    else if(isdigit(*argv[1])){ //Port number 
      port_number = atoi(argv[1]);
      dictionary = DEFAULT_DICTIONARY; 
    }
    else{ //Not recongnized, use defaults 
      dictionary = DEFAULT_DICTIONARY;
      port_number = DEFAULT_PORT; 
    }
  }

  else if(argc == 3){ //Figure out the order of dictionary / port
    if((access(argv[1], F_OK) != -1) && (isdigit(*argv[2]))){ //Dictionary then port
      dictionary = argv[1]; 
      port_number = atoi(argv[2]); 
    }
    else if((access(argv[2], F_OK) != -1) && (isdigit(*argv[1]))){ //Port then dictionary
      dictionary = argv[2]; 
      port_number = atoi(argv[1]); 
    }
    else if((access(argv[1], F_OK) != -1) && (!(isdigit(*argv[1])))){ //Dictionary good, port bad
      dictionary = argv[1]; 
      port_number = DEFAULT_PORT; 
    }
    else if((access(argv[1], F_OK) == -1) && (isdigit(*argv[2]))){ //Port good, dictionary bad
      port_number = atoi(argv[2]);
      dictionary = DEFAULT_DICTIONARY; 
    }
    else{ //Otherwise set to default
      dictionary = DEFAULT_DICTIONARY;
      port_number = DEFAULT_PORT; 
    }
  }
  
  else{ //Too many args
    fprintf(stderr, "%s", "Too many args, try again\n"); 
    exit(1); 
  }

   printf("File: %s\n", dictionary);
   printf("Port number: %d\n", port_number); 

  //Initialize vars
  queue_num, add_index, remove_index = 0; 
  log_queue_num, add_index2, remove_index2 = 0; 
  current_thread = 0;  

  read_dictionary(); //char **dictionary_words, holds a string for each word placed on a line in dictionary file  
  init_locks();   //init condition variables and locks 
  signal(SIGINT, sigint_handler); 

  //Create worker and log threads, these will write to socket and log the concated word + OK/MISSPELLED
  create_threads(); 
  

  //Network setup, code from powerpoint slide
  int new_socket, c;

  struct sockaddr_in server, client;
  char *message;
  socket_desc = socket(AF_INET, SOCK_STREAM, 0);

  if (socket_desc == -1){ 
	  puts("Could not create socket");
	  exit(1);
  }

  server.sin_family = AF_INET;
  server.sin_addr.s_addr = inet_addr("127.0.0.1"); //default to 127.0.0.1 as listed in pdf
  server.sin_port = htons(port_number);

  printf("Local Host Should Be: %d\n",  server.sin_addr.s_addr); 

  //https://docs.oracle.com/cd/E19620-01/805-4041/sockets-47146/index.html
  //Convert the server's socket address to the socket descriptor to make connection
  int bind_result = bind(socket_desc, (struct sockaddr *)&server, sizeof(server));
  if (bind_result < 0){
	  fprintf(stderr, "%s","Failed to bind socket");
	  exit(1);
  }

  puts("Bind Successful");

  //Converts the socket to "Listening" to accept values
  listen(socket_desc, 3);
  puts("Listening for incoming connections...");
 
  while (1){
	  c = sizeof(struct sockaddr_in);
	  new_socket = accept(socket_desc, (struct sockaddr*)&client, (socklen_t*)&c);

	  if (new_socket < 0){
	    fprintf(stderr, "%s","Information cannot be accepted");
	    continue;
	  }
    //fprintf(stdout, "%s","Connection accepted");
    fprintf(stdout, "%s%d%s%d\n", "Thread: ", current_thread+1, " Socket Obtained: ", new_socket); 

    produce_queue(new_socket); //Adding a new socket to the queus which signals the worker threads
    ++current_thread;
  }
   
  return 0; 
}

void read_dictionary(){ //Reads from dictionary file and stores strings in array of pointers to strings 

  FILE *fptr = fopen(dictionary, "r");

  if(fptr == NULL) {
    printf("%s not found", dictionary); 
    exit(0); 
  }

  dictionary_count = 0; 
  char buffer[100];

  while(fgets(buffer, 100, fptr) != NULL)
    ++dictionary_count; //Count the lines in dictionary


  //Allocate memory for n amount of lines found in dictionary.txt
  //dictionary_words = (char **)malloc(sizeof(char*) * dictionary_count); 
  dictionary_words = (char **)malloc((dictionary_count+1) * sizeof(*dictionary_words)); 
  rewind(fptr);
  int i = 0, j = 0; 

  char *line = NULL;
  size_t len = 0; 
  ssize_t read = -1; 
  char arr[] = " \n\t";  
  char *token = NULL; 

  while(read = getline(&line, &len, fptr) != -1){ //Fill the array with info
    token = strtok(line, arr); // WORK ON
    dictionary_words[i] = token;
    ++i; 
    line = NULL; //Reset 
    token = NULL; 
  }

  dictionary_words[i] = NULL;  
  free(line); 
  free(token);
  fclose(fptr); 
}

void init_locks(){//Initilizate condition vars and locks to ensure less errors

  int cond = pthread_mutex_init(&connection_lock, NULL);
  int cond2 = pthread_mutex_init(&log_lock, NULL);
  int cond3 = pthread_cond_init(&produce, NULL); 
  int cond4 = pthread_cond_init(&produce2, NULL);
  int cond5 = pthread_cond_init(&consume, NULL); 
  int cond6 = pthread_cond_init(&consume2, NULL); 
  int cond7 = pthread_attr_init(&detach);
  int cond8 = pthread_attr_setdetachstate(&detach, PTHREAD_CREATE_DETACHED);

  if(cond == -1 || cond2 == -1 || cond3 == -1 || cond4 == -1 || cond5 == -1 || cond6 == -1 || cond7 == -1 || cond8 == -1){
    fprintf(stdout, "%s", "Error when initalizing, check function-> init_locks()");
    exit(1);
  } 
}

int check_dictionary(char *word){//Binary search to check if word is in dictionary

  char arr[] = " \n\t\r"; //Possible whitespace
  word = strtok(word, arr); //Remove extra spaces

  //Init indexes 
  int left, middle, right, goal; 
  left = 0, //First seen
  right = (dictionary_count-1); //End of count

  while(left <= right){ //Should be true till end
    middle = (left + right) / 2; //Half the size
    goal = strcmp(word, dictionary_words[middle]); 

    if(goal == 0){ //Found word, return TRUE = found
      return TRUE; // 0
    }
    else if(goal < 0){ //word has lesser value, check earlier
      right = middle - 1; 
    }
    else{ //word has greater value, check later
      left = middle + 1; 
    }
  } 
  return FALSE; // FALSE = not found   
}

void produce_queue(int socket){
  if(pthread_mutex_lock(&connection_lock) != 0){ //If not available
     fprintf(stdout, "%s", "Could not get lock");
     exit(1); 
  }  

  while(queue_num == SIZE){ //If queue is full, release lock and wait for signal
    pthread_cond_wait(&produce, &connection_lock); 
  }

  //Lock was re-aquired 
  buffer_pro_cons[add_index] = socket; //Socket to add 

  add_index = (add_index + 1) % SIZE; 
  ++queue_num; 

  pthread_cond_signal(&consume); //Wake consumer and remove from queue

   //Release lock for consumer
  if(pthread_mutex_unlock(&connection_lock) != 0){
    fprintf(stdout, "%s", "Could not release lock");
    exit(1); 
  }
}

int consume_queue(){ //Consumer 

  pthread_mutex_lock(&connection_lock); //Aquire lock

  while(queue_num == 0){ //If queue is empty, release lock and waitfor signal
    pthread_cond_wait(&consume, &connection_lock); 
  }

  //Start consuming
  int temp = buffer_pro_cons[remove_index]; 
  remove_index = (remove_index + 1) % SIZE; 
  --queue_num; 

  pthread_cond_signal(&produce); //Wake up producer
  pthread_mutex_unlock(&connection_lock); 

  return temp; 
}

void create_threads(){ //create thread pool of worker threads, and single log thread
  pthread_t w_threads[NUM_WORKERS]; //50
  long int id = 0; // w_thread id  

  for(int i = 0; i < NUM_WORKERS; ++i){
    ++id; 
    if(pthread_create(&w_threads[i], &detach, &worker_thread, (void*)id) != 0){ //Each thread created will exec  worker_thread() 
      fprintf(stderr, "%s\n", "Could not create worker thread"); 
      exit(1); 
    }
  }

  //Log thread 
  pthread_t loging; 
  int cond = pthread_create(&loging, NULL, &log_thread, NULL); // Writes results to data.log

  if(cond == -1){
    fprintf(stderr, "%s\n", "Could not create log thread"); 
    exit(1); 
  }

}

void* worker_thread(void* id){
  char word[30]; //word can attmost be 30 chars
  int cond; 
  char *welcome = "Kat Tejada's Clinet/Server Spell Checker ...\n"; 

  while(1){
    int sd = consume_queue(); //Remove from queue   
    write(sd, welcome, strlen(welcome));

    while(read(sd, word, 30) > 0){ //Loop until client disconnects 
      cond = check_dictionary(word); //Binary search for word
       
      char word_plus_status[100] = ""; // Concatenate word and the appropriate status
      strcat(word_plus_status, word); 
      strcat(word_plus_status, " "); 

      if(cond == 0){//Found 
        strcat(word_plus_status, "OK"); 
        strcat(word_plus_status, "\n"); 
      }

      else{//Not found
        strcat(word_plus_status, "MISSPELLED");  
        strcat(word_plus_status, "\n"); 
      }

      printf("Value: %s\n", word_plus_status); 
      ssize_t bytes_written = write(sd, word_plus_status, strlen(word_plus_status)); // Give response to client

      if(bytes_written == -1){
        fprintf(stderr, "%s", "Could not write word"); 
        break; 
      }

      add_log(word_plus_status);
     
    } //Client ended connection
    
    long w_thread_id = (long int) id; // arg passed from pthread create func. 
    fprintf(stdout, "%s%ld%s%d\n", "Thread: ", w_thread_id, " End Socket: ", sd);

    close(sd);  
  }
}

void add_log(char *result){
  if(pthread_mutex_lock(&log_lock) != 0){ //Get lock
    fprintf(stdout, "%s", "Could not get lock");
    exit(1); 
  }  

  while(log_queue_num == SIZE){//If queue is full, relase lock and wait for signal
    pthread_cond_wait(&produce2, &log_lock); 
  }

  ++log_queue_num; 

  //lock was re-aquired
  buffer_log_Q[add_index2] = result; // Word + OK/MISSPELLED 

  //printf("add to logQ: %s at index: %d\n", buffer_log_Q[add_index2], add_index2);

  add_index2 = (add_index2 + 1) % SIZE;
 
  pthread_cond_signal(&consume2); //Wake the consumer

  //Release lock for consumer to re-aquire 
  if(pthread_mutex_unlock(&log_lock) != 0){
    fprintf(stdout, "%s", "failed to release lock");
    exit(1); 
  }

}

char* remove_log(){// log thread (consumer)
   if(pthread_mutex_lock(&log_lock) != 0){ // get lock
    fprintf(stdout, "%s", "Could not get lock");
    exit(1); 
  } 

  while(log_queue_num == 0){//If queue is empty, release lock and wait for signal
    pthread_cond_wait(&consume2, &log_lock); 
  }

  --log_queue_num;  

  //Safe to consume
  char *temp = buffer_log_Q[remove_index2]; 

  //printf("removed from logQ: %s at index %d\n", temp, remove_index2);

  remove_index2 = (remove_index2 + 1) % SIZE; 
  
  pthread_cond_signal(&produce2); //Wake up producer

  if(pthread_mutex_unlock(&log_lock) != 0){ // release lock 
    fprintf(stdout, "%s", "Could not release lock");
    exit(1); 
  }
  
  return temp; 
}

void* log_thread(void *arg){ //Write to log file data stored in data_from_client.log

  fptr = fopen("data_from_client.log", "w"); 

  char *add = NULL; 

  while(1){
    add = remove_log();
    fprintf(fptr, "%s", add);
    fflush(fptr); 
 }
  free(add);  
  fclose(fptr); 
}

void sigint_handler(int sig_num) {   
  for(int i = 0; i < dictionary_count; ++i){
    if(dictionary_words[i] == NULL)
      continue; 
    free(dictionary_words[i]); 
  } 
  free(dictionary_words);
  fclose(fptr); 
  exit(1); 
}