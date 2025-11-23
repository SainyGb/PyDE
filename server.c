#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>


#define MAX_CODE_SIZE 1024
#define MAX_OUTPUT_SIZE 4096
#define PATH_MAX_LEN 512

#define PATH_OVERHEAD 4 // for ".py" and null terminator
#define COMMAND_OVERHEAD 14 // for "python3 " and " 2>&1" and null terminator

#define HEADER_SIZE 256


void error(const char *msg)
{
    perror(msg);
    exit(1);
}

void execute_python_code(const char *python_code, char *output_buffer){

    FILE *code_file = NULL;
    FILE *fp = NULL;

    char temp_file[PATH_MAX_LEN];
    char command[PATH_MAX_LEN];

    snprintf(temp_file, sizeof(temp_file), "%s/temp_code_XXXXXX", P_tmpdir);

    int fd = mkstemp(temp_file);
    if (fd == -1) {
        snprintf(output_buffer, MAX_OUTPUT_SIZE, 
                 "SERVER ERROR: mkstemp failed (%s) on path: %s\n", strerror(errno), temp_file);
        return;
    }

    close(fd);

    char final_temp_file[PATH_MAX_LEN];
    snprintf(final_temp_file, sizeof(final_temp_file), "%.*s.py", 
         (int)sizeof(final_temp_file) - PATH_OVERHEAD, temp_file);

    if (rename(temp_file, final_temp_file) != 0) {
        snprintf(output_buffer, MAX_OUTPUT_SIZE, 
                 "SERVER ERROR: rename failed (%s) on path: %s to %s\n", strerror(errno), temp_file, final_temp_file);
        remove(temp_file);
        return;
    }

    code_file = fopen(final_temp_file, "w");
    if (code_file == NULL) {
        snprintf(output_buffer, MAX_OUTPUT_SIZE, "SERVER ERROR: failed to open file (%s) on path: %s to %s\n", strerror(errno), temp_file, final_temp_file);
        remove(final_temp_file);
        return;
    }

    fprintf(code_file, "%s", python_code);
    fclose(code_file);

    snprintf(command, sizeof(command),
         "python3 %.*s 2>&1", (int)sizeof(command) - COMMAND_OVERHEAD, final_temp_file);
    
    fp = popen(command, "r");
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

    remove(final_temp_file);
}

void *thread_function(void *arg) {
    int newsockfd = *((int *)arg);

    free(arg); 
    
    char request_buffer[MAX_CODE_SIZE];
    char code_buffer[MAX_CODE_SIZE];
    char result_buffer[MAX_OUTPUT_SIZE];
    
    bzero(request_buffer, MAX_CODE_SIZE);
    int n = read(newsockfd, request_buffer, MAX_CODE_SIZE - 1);

    if (n <= 0) {
        perror("ERROR reading from socket");
        close(newsockfd);
        return NULL;
    }

    request_buffer[n] = '\0';

    char *body_start = strstr(request_buffer, "\r\n\r\n");

    if (body_start == NULL) {
        snprintf(result_buffer, MAX_OUTPUT_SIZE, "Error: Invalid HTTP request format.");
    } else {
        body_start += 4;
        strncpy(code_buffer, body_start, MAX_CODE_SIZE - 1);
        code_buffer[MAX_CODE_SIZE - 1] = '\0';

        execute_python_code(code_buffer, result_buffer);
    }

    char response[MAX_OUTPUT_SIZE + HEADER_SIZE ];

    int content_length = strlen(result_buffer);

    snprintf(response, sizeof(response),
         "HTTP/1.1 200 OK\r\n"
         "Content-Type: text/plain\r\n"
         "Content-Length: %d\r\n"
         "Connection: close\r\n"
         "\r\n"
         "%s",
         content_length, result_buffer);

    write(newsockfd, response, strlen(response));

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

