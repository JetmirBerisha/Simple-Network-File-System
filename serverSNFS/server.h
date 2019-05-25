#ifndef SERVER_H
#define SERVER_H

#define BACKLOG 15

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <signal.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <dirent.h>


void *get_in_addr(struct sockaddr *sa);
void sigint_handler();
void *handle_client(void* t_args);

struct t_args{
    int sock_t;
};
#endif
