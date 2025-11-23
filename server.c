#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#include <sys/wait.h>

#define MAX_CODE_SIZE 1024
#define MAX_OUTPUT_SIZE 4096


void error(const char *msg)
{
    perror(msg);
    exit(1);
}

void execute_python_code(const char *python_code, char *output_buffer){
    char command[MAX_CODE_SIZE + 64];

    snprintf(command, sizeof(command),
             "python3 -c '%s' 2>&1", python_code);
    
    FILE *fp = popen(command, "r");
    if (fp == NULL) {
        snprintf(output_buffer, MAX_OUTPUT_SIZE, "SERVER ERROR: Failed to run command\n");
        return;
    }

    bzero(output_buffer, MAX_OUTPUT_SIZE);

    char line_buffer[256];
    size_t current_len = 0;

    while (fgets(line_buffer, sizeof(line_buffer), fp) != NULL) {
        size_t line_len = strlen(line_buffer);
        if (current_len + line_len < MAX_OUTPUT_SIZE - 1) {
            strncat(output_buffer, line_buffer, line_len);
            current_len += line_len;
        } else {
            strncat(output_buffer, "\n[Output truncated]\n", MAX_OUTPUT_SIZE - current_len - 1);
            break;
        }
    }

    int status = pclose(fp);

    if (status != 0 && output_buffer[0] == '\0') {
        snprintf(output_buffer, MAX_OUTPUT_SIZE, "Python execution failed with status %d\n", WEXITSTATUS(status));
    } else if (status == 0 && output_buffer[0] == '\0') {
        snprintf(output_buffer, MAX_OUTPUT_SIZE, 
                 "(Program finished successfully with no output.)");
    }
}

void *thread_function(void *arg) {
    int newsockfd = *((int *)arg);

    free(arg); 
    
    char code_buffer[MAX_CODE_SIZE];
    char result_buffer[MAX_OUTPUT_SIZE];
    int n;
    
    bzero(code_buffer, MAX_CODE_SIZE);
    n = read(newsockfd, code_buffer, MAX_CODE_SIZE - 1); 
    if (n < 0) {
        perror("ERROR reading from socket");
        close(newsockfd);
        return NULL;
    }
    code_buffer[n] = '\0';

    execute_python_code(code_buffer, result_buffer);

    n = write(newsockfd, result_buffer, strlen(result_buffer));
    if (n < 0){
        perror("ERROR writing to socket");
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

