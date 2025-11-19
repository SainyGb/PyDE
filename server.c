#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

void *thread_function(void *arg) {
    int newsockfd = *((int *)arg);

    free(arg); 
    
    char buffer[256];
    int n;
    
    bzero(buffer, 256);
    n = read(newsockfd, buffer, 255); 
    if (n < 0) {
        perror("ERROR reading from socket");
    } else {
        printf("Message from client %d: %s\n", newsockfd, buffer);
        
        n = write(newsockfd, "I got your message", 18);
        if (n < 0) {
            perror("ERROR writing to socket");
        }
    }

    close(newsockfd);
    printf("Client %d connection closed.\n", newsockfd);
    
    return NULL;
}


int main(int argc, char *argv[])
{
    int sockfd, newsockfd, portno;
    socklen_t clilen;
    char buffer[256];
    struct sockaddr_in serv_addr, cli_addr;
    int n;
    
    if (argc < 2)
    {
        fprintf(stderr, "ERROR, no port provided\n");
        exit(1);
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    bzero((char *)&serv_addr, sizeof(serv_addr));
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *)&serv_addr,
            sizeof(serv_addr)) < 0)
    error("ERROR on binding");

    listen(sockfd, 5);

    printf("Server started on port %d\n", portno);
    
    while (1){
        pthread_t thread_id;
        clilen = sizeof(cli_addr);

        int *newsockfd_ptr = malloc(sizeof(int));
        *newsockfd_ptr = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        
        if (*newsockfd_ptr < 0) {
            perror("ERROR on accept");
            free(newsockfd_ptr);
            continue;
        }
        
        if (pthread_create(&thread_id, NULL, thread_function, newsockfd_ptr) != 0) {
            perror("ERROR creating thread");
            free(newsockfd_ptr);
            close(*newsockfd_ptr);
        }
        
        pthread_detach(thread_id);
    }
    
    close(sockfd);
    return 0;
}

