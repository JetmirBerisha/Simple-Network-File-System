#ifndef CLIENT_H
#define SERVER_H 

#define FUSE_USE_VERSION 31
#include <fuse.h>
#include <stdio.h>  
#include <stdlib.h>    
#include <string.h>
#include <getopt.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>

void print_usage();
//void sigint_handler();
//void init_sighandler();
char* strip_path(const char *path);
static int sf_getattr( const char *path, struct stat *buf);
static int sf_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
static int sf_open(const char *path, struct fuse_file_info *fi);
static int sf_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
static int sf_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
static int sf_mkdir(const char* path, mode_t mode);
static int sf_releasedir (const char* path, struct fuse_file_info* fi);
static int sf_flush(const char* path, struct fuse_file_info* fi);
static int sf_release(const char* path, struct fuse_file_info *fi);
static int sf_truncate(const char* path, off_t size);
static int sf_utimens(const char* path, const struct timespec ts[2]);

void *get_in_addr(struct sockaddr *sa);

int get_socket();

#endif
