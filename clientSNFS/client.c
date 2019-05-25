#include "client.h"

char *port, *mount, *address;
//struct sigaction sa = { .sa_handler = sigint_handler};
char recv_ack[4];
char send_ack[4] = "^^^";

//global base path length = strdup(mount) before its freed
//this will leak until sighandler for CTRL-C is made
char* basepath = NULL;

//copies the part of the path the server will
//need since both directories will be mirrored excluding
//possibly the inital mount point, retval needs to be freed
char* strip_path(const char *path){
    char* cp_path = strdup(path);
    char* dir = NULL;
    int baselen = strlen(basepath);
    int curlen = strlen(cp_path);
    int i = 0;
    if(curlen > baselen && strncmp(basepath, cp_path, baselen) == 0){
        int size = curlen-baselen;
        dir = malloc(sizeof(char)*(size+2));
        for(i = 0; i <= size; i++)
            dir[i] = cp_path[baselen + i];
    }
    else if(curlen > baselen && strncmp(basepath, cp_path, baselen) != 0){
        dir = strdup(cp_path);
    }
    else if(curlen == baselen && strcmp(basepath, cp_path) == 0){
        dir = strdup("/");
    }
    else if(curlen == baselen && strcmp(basepath, cp_path) != 0){ // ocd
        dir = strdup(cp_path);
    } 
    else {
        dir = strdup(cp_path);
    }
    
    printf("path sent to strip path: %s\n", path);
    printf("stripped path: %s\n",dir);
    free(cp_path);
    return dir;
}

//FUSE Functions

/* *
 * Protocol for client interacting with Server.
 *
 *  1. Send command.
 *  2. Send args necessary for command.
 *  3. Recv 1 byte char* res   1 = SUCCESS,  0 = FAILURE
 *  4. If res is 1 accept results from server or return -1 from current function
 *
 * */

static int sf_utimens(const char* path, const struct timespec ts[2]) {
    printf("-----Entering sf_utimens\n");
	char cmd[11] = "sf_utimens";
	int sock = get_socket();
	int ret = -1;
	char* new_path;
	if (send(sock, cmd, strlen(cmd), 0) < 0) {
		perror("error occurred");
		return -errno;
	}
	if (recv(sock, recv_ack, 3, 0) < 0) {
		perror("error occurred-1");
		return -errno;
	}
	new_path = strip_path(path);
	if (send(sock, ts, sizeof(struct timespec) * 2, 0) < 0) {
		perror("couldn't send timespec structs");
		goto finito;
	}
	if (recv(sock, recv_ack, 3, 0) < 0) {
		perror("couldn't get ack for timespec");
		goto finito;
	}
	if (send(sock, new_path, strlen(new_path), 0) < 0) {
		perror("couldn't send path");
		goto finito;
	}
	if (recv(sock, &ret, sizeof(ret), 0) < 0) {
		perror("couldn't get back timespec results");
		goto finito;
	}

	if (ret < 0)
		errno = -ret;
	finito:
	free(new_path);
    printf("-------Leaving sf_utimens\n");
	return ret;
}

static int sf_truncate(const char* path, off_t size) {
	int sock = get_socket(), len;
    printf("--------In sf_truncate\n");
	char cmd[12] = "sf_truncate";
	char* new_path;
	if (send(sock, cmd, strlen(cmd), 0) < 0) {
		perror("truncate command failed to be sent over socket");
		return -errno;
	}
	if (recv(sock, recv_ack, 3, 0) < 0) {
		perror("truncate command failed to recieve ack for command");
		return -errno;
	}
	new_path = strip_path(path);
	len = strlen(new_path);
	if (send(sock, &len, sizeof(len), 0) < 0) {
		perror("failed to send the path length in the ttruncate function");
		free(new_path);
		return -errno;
	}
	if (send(sock, new_path, len, 0) < 0) {
		perror("failed to send the path in the ttruncate function");
		free(new_path);
		return -errno;
	}
	if (send(sock, &size, sizeof(size), 0) < 0) {
		perror("failed to send size");
		free(new_path);
		return -errno;
	}
	len = -1;
	if (recv(sock, &len, sizeof(len), 0) < 0) {
		perror("failed to get the result in the ttruncate function");
		free(new_path);
		return -errno;
	}
	printf("truncate got %d\n", len);
	if (len < 0)
		errno = -len;
    printf("------Leaving sf_trucate\n");
	return len;
}

static int sf_release(const char* path, struct fuse_file_info *fi) {
	int ret, sock = get_socket();
    printf("---------In sf_realease\n");
	char cmd[11] = "sf_release";
	if (send(sock, cmd, strlen(cmd), 0) < 0) {
		perror("Failed to send sf_release command");
		return -errno;
	}
	if (recv(sock, recv_ack, 3, 0) < 0) {
		perror("Didn't quite get the ack");
		return -errno;
	}
	if (send(sock, &(fi->fh), sizeof(fi->fh), 0) < 0) {
		perror("Didn't quite get the ack");
		return -errno;
	}
	ret = -1;
	if (recv(sock, &ret, sizeof(ret), 0) < 0) {
		perror("getting the return value failed for release");
		return -errno;
	}
	printf("release got %d\n", ret);
	if (ret < 0){
		errno = -ret;
		perror("sf_release returned ret < 0");
	}
    printf("---------Leaving sf_release\n");
	return ret;
}

static int sf_flush(const char* path, struct fuse_file_info* fi) {
	int ret, sock = get_socket();
    printf("--------Inside sf_flus\n");
	char cmd[9] = "sf_flush";
	if (send(sock, cmd, strlen(cmd), 0) < 0) {
		perror("Failed to send the flush command");
		return -errno;
	}
	if (recv(sock, recv_ack, 3, 0) < 0) {
		perror("Didn't quite get the ack");
		return -errno;
	}
	if (send(sock, &(fi->fh), sizeof(fi->fh), 0) < 0) {
		perror("Didn't quite get the ack");
		return -errno;
	}
	ret = -1;
	if (recv(sock, &ret, sizeof(ret), 0) < 0) {
		perror("getting the return value failed for flush");
		return -errno;
	}
	printf("flush got: %d\n", ret);
	if (ret < 0){
		errno = -ret;
		perror("sf_flush returned ret < 0");
	}
    printf("-------Leaving flush \n");
	return ret;
}

static int sf_releasedir (const char* path, struct fuse_file_info* fi) {
	int clientSocket = get_socket();
    printf("-------Inside sf_releasedir\n");
    char cmd[15] = "sf_releasedir";
    int len = 0;
    if (send(clientSocket, cmd, strlen(cmd), 0) < 0) {
        perror("sending the releasedir command failed");
        return -errno;
    }
    if (recv(clientSocket, recv_ack, 3, 0) < 0){
        perror("ack recieving failed");
        return -errno;
    }
    len = fi->fh;
    if (send(clientSocket, &len, sizeof(len), 0) < 0) {
    	perror("sending file handler failed");
    	return -errno;
    }
    len = -1;
    if (recv(clientSocket, &len, sizeof(len), 0) < 0) {
        perror("failed to get the result int releasedir");
        return -errno;
    }
    if (len < 0)
        errno = -len;

    printf("-------Leaving sf_releasedir\n");
    return len;
}

static int sf_getattr( const char *path, struct stat *st){
    char ack[4] = "^^^";
    if(strncmp(path, "/.Trash", 7) == 0){ 
        return 0;
    } 
    int clientSocket = get_socket();]
    printf( "---------[getattr] Called\n" );
	printf( "---------Attributes of %s requested\n", path);

    char* new_path = strip_path(path);
    int ret = 0;
    char *cmd = malloc(sizeof(char)*12);
    memset(cmd, '\0', 12);
	cmd = strcpy(cmd, "sf_getattr");

	if( send(clientSocket, cmd, strlen(cmd), 0) < 0){ //send command
        printf("Send failed\n");
        ret = -errno; 
        goto attr_exit;
    }

	printf("Client sent Command sf_getattr\n");
	if(recv(clientSocket,recv_ack, 3, 0 ) < 0){ //recv_ack
        perror("receive of ack failed");
        goto attr_exit;
    }

	if(send(clientSocket, new_path, strlen(new_path), 0) < 0){ //send path
        printf("Send failed\n"); 
        ret = -errno;
        goto attr_exit;
    }
	printf("Client sent path %s\n", path);
	

	if(recv(clientSocket, &ret, sizeof(ret), 0) < 0){ //recv res
        printf("Receive failed\n"); 
        ret = -errno;
        goto attr_exit;
    }
	printf("res value: %d\n",ret);    
    if(ret < 0){
        errno = -ret;
        perror("ret < 0");
	}
    else {
        if (send(clientSocket, ack, 3, 0) < 0) {
        	perror("ACK FAil");
        	return -errno;
        }
        if (recv(clientSocket, &(st->st_dev), sizeof(st->st_dev), 0) < 0 ) {
        	perror("ACK FAil");
        	return -errno;
        }
        if (send(clientSocket, ack, 3, 0) < 0) {
        	perror("ACK FAil");
        	return -errno;
        }
        if (recv(clientSocket, &(st->st_ino), sizeof(st->st_ino), 0) < 0 ) {
        	perror("ACK FAil");
        	return -errno;
        }
        if (send(clientSocket, ack, 3, 0) < 0) {
        	perror("ACK FAil");
        	return -errno;
        }
        if (recv(clientSocket, &(st->st_mode), sizeof(st->st_mode), 0) < 0 ) {
        	perror("ACK FAil");
        	return -errno;
        }
        if (send(clientSocket, ack, 3, 0) < 0) {
        	perror("ACK FAil");
        	return -errno;
        }
        if (recv(clientSocket, &(st->st_nlink), sizeof(st->st_nlink), 0) < 0 ) {
        	perror("ACK FAil");
        	return -errno;
        }
        if (send(clientSocket, ack, 3, 0) < 0) {
        	perror("ACK FAil");
        	return -errno;
        }
        if (recv(clientSocket, &(st->st_uid), sizeof(st->st_uid), 0) < 0 ) {
        	perror("ACK FAil");
        	return -errno;
        }
        if (send(clientSocket, ack, 3, 0) < 0) {
        	perror("ACK FAil");
        	return -errno;
        }
        if (recv(clientSocket, &(st->st_gid), sizeof(st->st_gid), 0) < 0 ) {
        	perror("ACK FAil");
        	return -errno;
        }
        if (send(clientSocket, ack, 3, 0) < 0) {
        	perror("ACK FAil");
        	return -errno;
        }
        if (recv(clientSocket, &(st->st_size), sizeof(st->st_size), 0) < 0 ) {
        	perror("ACK FAil");
        	return -errno;
        }
        if (send(clientSocket, ack, 3, 0) < 0) {
        	perror("ACK FAil");
        	return -errno;
        }
		if (recv(clientSocket, &(st->st_atim), sizeof(st->st_atim), 0) < 0) {
			perror("failed to get the time struct");
			return -errno;
		}
		if (send(clientSocket, ack, 3, 0) < 0) {
			perror("get ack fial");
			return -errno;
		}
		if (recv(clientSocket, &(st->st_mtim), sizeof(st->st_mtim), 0) < 0) {
			perror("failed to get the time struct");
			return -errno;
		}
		if (send(clientSocket, ack, 3, 0) < 0) {
			perror("get ack fial");
			return -errno;
		}
		if (recv(clientSocket, &(st->st_ctim), sizeof(st->st_ctim), 0) < 0) {
			perror("failed to get the time struct");
			return -errno;
		}
		if (send(clientSocket, ack, 3, 0) < 0) {
			perror("get ack fial");
			return -errno;
		}
		//atim
		//mtim
		//ctim
	    printf("printing stat info from st\n");
	    printf(" dev id: %d\n", (int) st->st_dev);
	    printf(" inode: %d\n", (int) st->st_ino);
	    printf(" mode: %08x\n", (int) st->st_mode);
	    printf(" links: %d\n", (int) st->st_nlink);
	    printf(" uid: %d\n", (int) st->st_uid);
	    printf(" gid: %d\n", (int) st->st_gid);
	    printf("size: %d\n", (int) st->st_size);
		printf("atim: %ld\n", st->st_atim.tv_sec);
		printf("mtim: %ld\n", st->st_mtim.tv_sec);
		printf("ctim: %ld\n", st->st_ctim.tv_sec);
	}

attr_exit:
    printf("------getattr ret value: %d\n",ret);
    free(cmd);
	free(new_path);
	return ret;
}

static int sf_opendir(const char* path, struct fuse_file_info *fi){
    
    int clientSocket = get_socket();
    char* new_path = strip_path(path);
    printf("--------in opendir\n");
    char cmd[11] = "sf_opendir";
    if(send(clientSocket, cmd, strlen(cmd), 0) < 0){
        perror("send opendir cmd failed");
        return -errno;
    }

    if(recv(clientSocket, recv_ack, 3, 0) < 0){
        perror("recv_ack failed in sf_opendir");
        return -errno;
    }

    if(send(clientSocket, new_path, strlen(new_path), 0) < 0){
        perror("send new_path failed");
        return -errno;
    }

    int dirfd;
    if(recv(clientSocket, &dirfd, sizeof(dirfd), 0) < 0){
        perror("recv dirfd failed");
        return -errno;
    }
    
    if(dirfd < 0){
        errno = -dirfd;
        printf("sf_openddir failed on the server\n");
        return -errno;
    }
    printf("-----------dirfd received %d\n",dirfd);
    fi->fh = dirfd;
    printf("-----------fi->fh: %d\n",(int)fi->fh);
    free(new_path);

    printf("--------End of opendir %d\n",dirfd);
    return 0;
}

static int sf_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi ){
    int clientSocket = get_socket();
    printf("-----in sf_readdir\n");
    int ret = 0;
    char* num_dirs = malloc(sizeof(char)*5);
    char* res = malloc(sizeof(char)*2);
    char* cmd = malloc(sizeof(char)*30);
    memset(cmd, '\0', 30);
    memset(num_dirs, '\0', 5);
    struct dirent *dirent_ptr  = malloc(sizeof(struct dirent));


    cmd = strcpy(cmd, "sf_readdir");
    if(send(clientSocket, cmd, strlen(cmd), 0) < 0){ //send readdir
        perror("send opendir cmd failed");
        ret = -errno;
        goto clean_exit;
    }

    if(recv(clientSocket, recv_ack, 3, 0) < 0){  //recv_ack
        perror("recv_ack failed");
        ret = -errno;
        goto clean_exit;
    }
    
    int fd = fi->fh;
    printf("--------fi->fh: %d\n",(int)fi->fh);
    printf("---------fd:%d \n",fd);
    if(send(clientSocket, &fd, sizeof(fd), 0) < 0){ //send path
        perror("send new_path failed");
        ret = -errno;
        goto clean_exit;
    }

    memset(res, '\0', 2);
    if(recv(clientSocket, res, 2, 0) < 0){  //recv res
        perror("recv res failed");
        ret = -errno;
        goto clean_exit;
    }
    
    printf("-----res %s\n",res);
    if(strcmp(res, "0") == 0){ 
        ret = -errno;
        goto clean_exit;
    }
    // sf_opendir done -------------------------------
   
    
    if(send(clientSocket, send_ack, 3, 0) < 0){     //send_ack
        perror("send_ack failed in readdir");
        ret = -errno;
        goto clean_exit;
    }
   
    if(recv(clientSocket, num_dirs, 5, 0) < 0){   //recv num dirs
        perror("recv num_dirs failed in readdir");
        ret = -errno;
        goto clean_exit;
    }

 
    if(send(clientSocket, send_ack, 3, 0) < 0){     //send_ack
        perror("send_ack failed in readdir");
        ret = -errno;
        goto clean_exit;
    }

    int m = atoi(num_dirs);
    int i = 0;
   
    printf( "--> Getting The List of Files of %s\n", path );
    for(i = 0; i < m; i++){
        //recv dirents
        if(recv(clientSocket, dirent_ptr, sizeof(struct dirent), 0) < 0){ //recv dirents
            perror("recv dirent failed");
	        ret = -errno;
	        goto clean_exit;
        }
         
        filler(buffer, dirent_ptr->d_name, NULL, 0 );
        
        printf("d_name returned from struct dirent -> %s\n", dirent_ptr->d_name);
       
        if(send(clientSocket, send_ack, 3, 0) < 0){     //send_ack
            perror("send_ack failed in readdir");
            ret = -errno;
            goto clean_exit;
        }
    }

    if(recv(clientSocket, recv_ack, 3, 0) < 0){   //recv ack
        perror("recv_ack failed");
    }
    free(dirent_ptr);
    
    printf("-------Leaving readdir\n");

clean_exit:
    free(res);
    free(cmd);
	return ret;
}



static int sf_open(const char *path, struct fuse_file_info *fi){
    printf("--------In sf_open\n");
    int clientSocket = get_socket();
	int temp = fi->flags;
	int fd = -1;
	char ack[4];
	memset(ack, 0, 4);
	char command[8] = "sf_open";
	if (send(clientSocket, command, strlen(command), 0) < 0) {
		perror("Sending data failed");
		return -errno;
	}
	//Recv ack
	if (recv(clientSocket, ack, 3, 0) < 0) {
		perror("ack recv failed");
		return -errno;
	}
	// Send flags fi->flags
	if (send(clientSocket, &temp, sizeof(temp), 0) < 0) {
		perror("Sending data failed");
		return -errno;
	}
    printf("flags sent: %d\n", temp);
	if (recv(clientSocket, ack, 3, 0) < 0) {
		perror("ack recv failed");
		return -errno;
	}
	// send path, path
	char* new_path = strip_path(path);
	if (send(clientSocket, new_path, strlen(new_path), 0) < 0) {
		perror("sending data failed");
		return -errno;
	}
	if (recv(clientSocket, &fd, sizeof(fd), 0) < 0) {
		perror("recieving the fd failed");
		return -errno;
	}
	if (fd < 0) {
		errno = -fd;
		perror("fd < 0");
		return fd;
	}
	fi->fh = fd;
	free(new_path);
    printf("-------leave sf_open\n");
	return 0;
}

static int sf_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
    int clientSocket = get_socket();
    printf("-------Inside sf_read");
    int fd = fi->fh;
    char cmd[8] = "sf_read";
    if(send(clientSocket, cmd, strlen(cmd), 0) < 0){
        perror("send cmd failed in sf_read");
        return -errno;
    }

    if(recv(clientSocket, recv_ack, 3, 0) < 0){
        perror("recv_ack failed in sf_read");
        return -errno;
    }

    if(send(clientSocket, &fd, sizeof(fd), 0) < 0){
        perror("send fd failed in sf_read");
        return -errno;
    }

    if(recv(clientSocket, recv_ack, 3, 0) < 0){
        perror("recv_ack failed in sf_read");
        return -errno;
    }
    
    int ssize = (int) size;
    if(send(clientSocket, &ssize, sizeof(ssize), 0 ) < 0){
        perror("send size to read");
        return -errno;
    }
     
    if(recv(clientSocket, recv_ack, 3, 0) < 0){
        perror("recv_ack failed in sf_read");
        return -errno;
    }
                                            
    int ooffset = (int) offset;
    if(send(clientSocket, &ooffset, sizeof(offset), 0) < 0){
        perror("send offset failed in sf_read");
        return -errno;
    }

    int res;
    if(recv(clientSocket, &res, sizeof(res), 0 ) < 0){
        perror("recv res of read from server failed");
        return -errno;
    }
    
    if(res > 0){
        if(send(clientSocket, send_ack, 3, 0) < 0){
            perror("send_res failed in sf_read");
            return -errno;
        }
        
        if(recv(clientSocket, buf, size, 0) < 0){
            perror("recv buff in sf_read failed");
            return -errno;
        }
    }
    else{
        errno = -res;
    }
    printf("--------leaving sf_read\n");
    
    return res;
}

static int sf_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
    int clientSocket = get_socket();
    int fd = fi->fh;
    printf("----------In sf_write\n");
    char cmd[9] = "sf_write";
    perror("write check");

    if(send(clientSocket, cmd, strlen(cmd), 0) < 0){
        perror("send cmd failed in sf_write");
        return -1;
    }
    
    if(recv(clientSocket, recv_ack, 3, 0) < 0){
        perror("recv_ack failed in sf_write");
        return -1;
    }
   
    if(send(clientSocket, &fd, sizeof(fd), 0) < 0){
        perror("send fd failed in sf_write");
        return -1;
    }
    
    if(recv(clientSocket, recv_ack, 3, 0) < 0){
        perror("recv_ack failed in sf_write");
        return -1;
    }
        
    int ssize = (int) size; 
    if(send(clientSocket, &ssize, sizeof(ssize), 0) < 0 ){
        perror("send size failed in sf_write");
        return -1;
    }
    
    if(recv(clientSocket, recv_ack, 3, 0) < 0){
        perror("recv_ack failed in sf_write");
        return -1;
    }
    
    int ooffset = (int) offset;
    if(send(clientSocket, &ooffset, sizeof(ooffset), 0) < 0){
        perror("sending offset failed in sf_write");
        return -1;
    }
    
    if(recv(clientSocket, recv_ack, 3, 0) < 0){
       perror("recv_res failed in sf_read");
       return -1;
    }
        
    if(send(clientSocket, buf, size, 0) < 0){
        perror("recv buff in sf_read failed");
        return -1;
    }
    printf("buffer in write contains: %s\n", buf);
    int res;
    if(recv(clientSocket, &res, sizeof(res), 0 ) < 0){
        perror("recv res of read from server failed");
        return -1;
    }
    
    printf("-----------Leaving sf_write %d\n", res);

    if(res < 0){
    	perror("write returned res < 0");
        errno = -res;
    }

    return res;
}

static int sf_mkdir(const char* path, mode_t mode) {
    int clientSocket = get_socket();
	printf("-----------mkdir Making a new Dir...\n");
	char* new_path = strip_path(path);
	char message[10] = "sf_mkdir";
    char ack[4] = "^^^";
	int ret = 0; //return value

	if (send(clientSocket, message, strlen(message), 0) < 0) { //sending command
		printf("Send failed w/ command.\n");
		return -errno;
	}
	printf("Client sent Command\n");
    if (recv(clientSocket, ack, 3, 0) < 0) {
        perror("sending ack failed in mkdir");
        return -errno;
    }
	if (send(clientSocket, new_path, strlen(new_path), 0) < 0) { //sending path
		printf("Send failed w/ path.\n");
		return -errno;
	}
	printf("Client sent path\n");
    if (recv(clientSocket, ack, 3, 0) < 0) {
        perror("recieve ack failed in mkdiiir");
        return -errno;
    }

	if (send(clientSocket, &mode, sizeof(mode), 0) < 0) { //sending command
		printf("Send failed w/ mode.\n");
		return -errno;
	}
	printf("Client sent mode\n");

	//wait to receive the return from server so it can be passed on to FUSE!
	if (recv(clientSocket, &ret, sizeof(ret), 0) < 0) {
		printf("Receive failed w/ return value of mkdir.\n");
	}
	printf("ret value: %d <---mkdir\n", ret);
    if (ret < 0){
        errno = -ret;
        perror("mkdir returned < 0");
    }
    printf("--------Leaving mkdir\n");
	return ret;
}

static int sf_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    int clientSocket = get_socket();
    printf("--------In sf_create\n");
	int temp = fi->flags;
	int fd = -1;
	char ack[4];
	memset(ack, 0, 4);
	char command[10] = "sf_create";
	if (send(clientSocket, command, strlen(command), 0) < 0) {
		perror("Sending data failed");
		return -errno;
	}
	//Recv ack
	if (recv(clientSocket, ack, 3, 0) < 0) {
		perror("ack recv failed");
		return -errno;
	}
	// Send flags fi->flags
	if (send(clientSocket, &temp, sizeof(temp), 0) < 0) {
		perror("Sending data failed");
		return -errno;
	}
	if (recv(clientSocket, ack, 3, 0) < 0) {
		perror("ack recv failed");
		return -errno;
	}
	// send path, path
	char* new_path = strip_path(path);
	if (send(clientSocket, new_path, strlen(new_path), 0) < 0) {
		perror("sending data failed");
		return -errno;
	}
	if (recv(clientSocket, ack, 3, 0) < 0) {
		perror("ack recv failed");
		return -errno;
	}
	//send mode
	if (send(clientSocket, &mode, sizeof(mode), 0) < 0) {
		perror("sending mode from create() failed");
		return -errno;
	}
	//recieve the file handler
	if (recv(clientSocket, &fd, sizeof(fd), 0) < 0) {
		perror("recieving the fd failed");
		return -errno;
	}
	if (fd < 0) {
		errno = -fd;
		perror("fd < 0");
		return fd;
	}
	fi->fh = fd;
	free(new_path);
    printf("-------------Leaving sf_create");
	return 0;
}

static struct fuse_operations operations = {
	.getattr	= sf_getattr,	
	.readdir	= sf_readdir,	
	.opendir	= sf_opendir,
	.open		= sf_open,		
	.read		= sf_read,		
	.write		= sf_write,		
	.flush 		= sf_flush,	
	.release	= sf_release,
	.truncate 	= sf_truncate,
	.create 	= sf_create,
	.releasedir = sf_releasedir,
	.mkdir		= sf_mkdir,
	.utimens	= sf_utimens,
};

void print_usage() {
	fprintf(stderr, "Usage : ./clientSNFS -port 12345 -address 156.156.156.155 -mount /tmp/OS416_NFS\n");
}

int main(int argc, char* argv[]) {
	if(argc<2){
		print_usage();
		exit(0);
	}
	// Initialize an empty argument list
	struct fuse_args args = FUSE_ARGS_INIT(0, NULL);
    //init_sighandler();
	int option_index, c;
    port = malloc(sizeof(char)*10);
    memset(port, '\0', 10);
	mount = (char*) malloc(sizeof(char) * 200);
	while (1) {
		static struct option long_options[] = {
				{ "port", required_argument, 0, 'p' },
				{ "address", required_argument, 0, 'a' },
				{ "mount", required_argument, 0, 'm' },
				{ "f", 0, 0,'f'},
				{ 0, 0, 0, 0 }};
		c = getopt_long_only(argc, argv, "", long_options, &option_index);

		if (c == -1) {
			break;
		}

		switch (c) {
			case 'a':
				address = strdup(optarg);
				if (address == NULL) {
				        fprintf(stderr,"ERROR, no such host\n");
				        exit(0);
				    }
				break;
			case 'p':
				port = strcpy(port, optarg);
				break;
			case 'f':
				printf("-f\n");
				fuse_opt_add_arg(&args, "");
				break;
			case 'm':
				if (*optarg == '/' || *optarg == '.') {
					strcpy(mount,optarg);
					}
					else {
						print_usage();
						free(mount);
						exit(0);
					}
				break;
			default:
				print_usage();
				free(mount);
	            free(port); 
				exit(0);
		} //end switch
	}

	int i;
	printf("argc = %d\n", argc);
	for (i = 0; i < argc; i++) {
		if (i == 6 && argc == 7) {
			fuse_opt_add_arg(&args, "");
			fuse_opt_add_arg(&args, argv[i]);
		}
		if (i == 6 && argc == 8) {
			fuse_opt_add_arg(&args, argv[i]);
		}
		if (i == 7) {
			fuse_opt_add_arg(&args, "-f");
		}
	}

    if(mkdir(mount, 0777) < 0){
        if(errno != EEXIST){
            perror("mkdir failed");
            free(mount);
        }
    }

	// Create the socket.
	printf("port = %s\n", port);
	printf("address = %s\n", address);
	printf("mount= %s\n", mount);
    basepath = strdup(mount);
	free(mount);
	int ret;
	ret = fuse_main(args.argc, args.argv, &operations, NULL);
    if(ret != 0){
        printf("Fuse failed\n");
        fuse_opt_free_args(&args);
        exit(1);
    }
	 fuse_opt_free_args(&args);

	return ret;
}


//returns a socket
int get_socket(){
                
	int sockfd;  
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(address, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {

        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("client: socket()");
            continue;
        }

        if ((connect(sockfd, p->ai_addr, p->ai_addrlen)) == -1) {
            close(sockfd);
            // perror("client: connect()");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return -1;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof(s));
    printf("client: connecting to %s\n", s);

    freeaddrinfo(servinfo);
	return sockfd;
}

void *get_in_addr(struct sockaddr *sa){
    if(sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
