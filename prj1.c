#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/uio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>

// Global parameters
int HOSTNAME_MAX_LEN = 50; // Max number of characters for hostname strings
int MAX_HOSTS = 10; // Max number of hosts in the network, including this process
char* PORT_NO = "3000";
int MESSAGE_INTERVAL = 3; // Sender thread interval in seconds


typedef struct {
    char* hostname;
    char* my_hostname;
} thread_args;


/*
Function that runs in each "sender" thread. Each hostname specified in hostfile.txt gets a sender thread.
*/
void *send_messages(void *data) {
    char content[64];
    int len;

    thread_args *args = data;
    char* hostname = args->hostname;
    char* my_hostname = args->my_hostname;
    printf("Hostname in thread: %s\n", hostname);

    struct addrinfo hints;

    memset(&hints, 0, sizeof hints);
    hints.ai_family=AF_UNSPEC;
    hints.ai_socktype=SOCK_DGRAM;
    hints.ai_protocol=0;
    hints.ai_flags=AI_PASSIVE|AI_ADDRCONFIG;

    struct addrinfo* res;
    struct addrinfo* valid_addr;

    int err=-1;
    // Repeatedly try and get the addrinfo for the other host until success
    while (err != 0) {
        err=getaddrinfo(hostname,PORT_NO,&hints,&res);
        sleep(1);
    }
    printf("Got addrinfo for hostname: %s\n", hostname);

    int fd;
    int error;
    // Create a socket - iterate through the res list and try and make one until it succeeds
    for (valid_addr = res; valid_addr != NULL; valid_addr = valid_addr->ai_next) {
        fd=socket(valid_addr->ai_family,valid_addr->ai_socktype,valid_addr->ai_protocol);
        if (fd==-1) {
            printf("socket creation failed"); 
        } else {
            break;
        }
    }

    printf("Created socket for hostname: %s : ", hostname);
    printf("%i\n", fd);

    // Message content is always the sender hostname
    strcpy(content, my_hostname);
    len = strlen(content);

    int i;
    // Repeatedly send messages to the target host
    while (true) {
        printf("Sending message to %s\n", hostname);
        if (sendto(fd,content,len,0,valid_addr->ai_addr,valid_addr->ai_addrlen)==-1) {
            printf("Couldn't send datagram\n");
        }

        sleep(MESSAGE_INTERVAL);
    }
}


/*
Helper function to determine if a fixed-size array of strings is empty.
Emptiness in this case is defined as all members being equal to the empty string.
*/
int array_is_empty(char** arr, int len) {
    int i;
    for (i=0; i<len; i++) {
        if (strcmp(arr[i], "") != 0) {
            return 1;
        }
    }
    return 0;
}


/**
Helper function to determine if an array of strings contains a certain string.
*/
int array_contains(char** arr, char* dest, int len) {
    int i;
    for (i=0; i<len; i++) {
        if (strcmp(arr[i], dest) == 0) {
            return i;
        }
    }
    return -1;
}


int main(){
    printf("I am alive\n");

    // Get the hostname for this process, to be used in initializing the listener loop
    char my_hostname[256];
    gethostname(my_hostname, sizeof(my_hostname));
    printf("My hostname: %s\n", my_hostname);

    // Open the hostfile
    FILE* hostfile;
    hostfile = fopen("hostfile.txt", "r");
    if (hostfile == NULL) {
        perror("Couldn't open hostfile"); 
        exit(EXIT_FAILURE); 
    }

    int h = 0; // How many hosts are specified in hostfile.txt, not including this process
    pthread_t* sender_threads[MAX_HOSTS];
    char** hostnames = malloc(MAX_HOSTS * sizeof(char*));
    char hstnm[HOSTNAME_MAX_LEN];
    // Loop through the hostnames in hostfile.txt, copying them to memory
    while (fgets(hstnm, HOSTNAME_MAX_LEN, hostfile) != NULL) {
        // Trim the newline character from end of the hostname
        hstnm[strlen(hstnm) - 1] = '\0';

        // Create a copy of the hostname, so that different threads aren't accessing the same variable
        char *hstnm_copy = (char*) malloc(HOSTNAME_MAX_LEN*sizeof(char));
        strcpy(hstnm_copy, hstnm);

        // Adding the hostname to the hostnames array in memory
        hostnames[h] = malloc((HOSTNAME_MAX_LEN+1) * sizeof(char));
        strcpy(hostnames[h], hstnm);

        if (strcmp(hstnm_copy, my_hostname) == 0) {
            continue;
        }

        // Initialize the args for and begin the sender thread for this process
        thread_args *args = malloc(sizeof *args);
        args->hostname = hstnm_copy;
        args->my_hostname = my_hostname;

        pthread_t *sender_thread;
        pthread_create(&sender_thread, NULL, send_messages, args);
        sender_threads[h] = sender_thread;

        h++;
        if (h>=MAX_HOSTS) {
            printf("Maximum hosts reached. Remaining hostnames will be ignored.\n");
        }
        sleep(1);
    }

    int i;
    printf("HOSTS IN ARRAY\n");
    // Confirm that the hostnames array contains the names of the other hosts, and not this process's name
    for (i=0; i< h; i++) {
        printf("%s\n", hostnames[i]);
    }
    
    struct addrinfo hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_family=AF_UNSPEC;
    hints.ai_socktype=SOCK_DGRAM;
    hints.ai_protocol=0;
    hints.ai_flags=AI_PASSIVE|AI_ADDRCONFIG;

    struct addrinfo* res;
    struct addrinfo* valid_addr;

    // Get the address info for this process
    int err=getaddrinfo(my_hostname,PORT_NO,&hints,&res);
    if (err!=0) {
        printf("couldn't get this container's addrinfo\n"); 
        exit(EXIT_FAILURE); 
    }

    int fd;
    int error;
    
    // Create an unassociated socket - iterate through the res list and try and make one until it succeeds
    for (valid_addr = res; valid_addr != NULL; valid_addr = valid_addr->ai_next) {
        fd=socket(valid_addr->ai_family,valid_addr->ai_socktype,valid_addr->ai_protocol);
        if (fd==-1) {
            printf("socket creation failed"); 
        } else {
            // Associate the socket (fd) with the specific port
            if (bind(fd,valid_addr->ai_addr,valid_addr->ai_addrlen) < 0 ) { 
                printf("bind failed"); 
            } 
            break;
        }
    }

    struct sockaddr_in cliaddr; 
    int len, n; 
    char buffer[256];
    len = sizeof(cliaddr);
    
    // Listen for messages from the other hosts, until all hosts have been heard from
    while (array_is_empty(hostnames, h) != 0) {
        printf("Waiting for messages:\n");

        n = recvfrom(fd, (char *)buffer, 64,  
                    MSG_WAITALL, ( struct sockaddr *) &cliaddr,
                    &len); 
        buffer[n] = '\0'; 

        printf("Received message : %s\n", buffer);

        // If this process hasn't heard from the sender hostname yet, remove it from the hostnames array.
        // The sender identity is determined by the content of the message.
        int r = array_contains(hostnames, buffer, h);
        if (r != -1) {
            strcpy(hostnames[r], "");
        }
        
        memset(buffer, 0, sizeof buffer);
    }

    free(hostnames);
    fprintf(stderr, "READY");

    // Join sender threads so that they continue sending messages even after the program is ready
    int t;
    for (t=0; t<h; t++) {
        pthread_join(sender_threads[t], NULL);
    }
}
