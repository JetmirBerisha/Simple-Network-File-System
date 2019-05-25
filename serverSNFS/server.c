#include "server.h"

// Global variables for SIGINT handler cleanup
char *port, *mountPoint; 
int sock = 0;
pthread_t tids[100];
struct t_args *last = NULL; 
int thread_count = 0;

pthread_mutex_t lock;

int main(int argc, char **argv){
    int c, flag = 0;
    port = NULL;
    mountPoint = NULL;
    mountPoint = malloc(sizeof(char)* 50);
    pthread_mutex_init(&lock, 0);
    while (1) {
        static struct option long_options[] =
        { 
            { "port", required_argument, 0, 'p'  },
            { "mount", required_argument, 0, 'm' },
            { 0,       0,                 0,  0  }
        };
        int option_index = 0;
        c = getopt_long_only(argc, argv, "", long_options, &option_index);

        if (c == -1)
            break;

        switch (c) {
            case 'p':
                port = strdup(optarg);
                if(port == NULL)
                    return 1;
                break;
            case 'm':
                mountPoint = strcpy(mountPoint, optarg);
                if(mountPoint == NULL)
                    return 1;
                break;
            case '?':
            default:
                if(mountPoint != NULL)
                    free(mountPoint);
                if(port != NULL)
                    free(port);
                return 1;
        }
    }
    /* Print any remaining command line arguments (not options). */
    if (optind < argc) {
        printf("non-option ARGV-elements: ");
        while (optind < argc)
            printf("%s", argv[optind++]);
        putchar('\n');
    }

    if (optind == 1) {
        fprintf(stderr,
                "\nERROR:  - Please type some variation of the following on one line:\n"\
                "Usage:  ./serverSNFS -mount \"mountpoint\" -port ####\n\n");
        if(mountPoint != NULL)
            free(mountPoint);
        if(port != NULL)
            free(port);
        return 1;
    }

   if(mountPoint[strlen(mountPoint)-1] != '/'){
       mountPoint = strcat(mountPoint, "/");
   }

    if( mkdir(mountPoint, 0777) < 0){
	    if( errno != EEXIST){
 	    	perror("mkdir failed");
		    free(mountPoint);
		    free(port);
		    return 1;
	    }
    }
    printf("\nMount: %s  Port: %s\n",mountPoint, port);

    //----------------------  Server starts here ----------------------------
    struct addrinfo hints, *serverInfo, *p;
    struct sockaddr_storage clientAddr;
    socklen_t sin_size;
    char s[INET6_ADDRSTRLEN];
    int rv;
    struct sigaction sa = {.sa_handler = sigint_handler};
    int opt = 1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;    //use my IP

    if ((rv = getaddrinfo(NULL, port, &hints, &serverInfo)) != 0) {
        fprintf(stderr, "getaddrinfo(): %s\n", gai_strerror(rv));
        goto clean_exit;
    }

    // loop through all the results and bind to the first thing we can
    for (p = serverInfo; p != NULL; p = p->ai_next) {

        if ((sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("Server: Could not open sock");
            continue;
        }

        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
            perror("Server: setsockopt () failed");
            goto clean_exit;
        }

        if (bind(sock, p->ai_addr, p->ai_addrlen) == -1) {
            perror("Server: bind() failed");
            close(sock);
            continue;
        }
        break;
    }

    freeaddrinfo(serverInfo);

    if(p == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        goto clean_exit;
    }

    if (listen(sock, BACKLOG) == -1) {
        perror("Server: listen() failed");
        goto clean_exit;
    }

    //Setup signal handler for SIGINT
    sigemptyset(&sa.sa_mask);
    if(sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction sigint");
        goto clean_exit;
    }

    printf("server: waiting for connections. . .\n");
    int error = 0;
    while(1){
        sin_size = sizeof(clientAddr);
        struct t_args *threadArgs = malloc(sizeof(struct t_args));
        flag += 1; //flag for checking whether threadArgs should be freed
        last = threadArgs;
        threadArgs->sock_t = accept(sock, (struct sockaddr*)&clientAddr, &sin_size);
        if(threadArgs->sock_t == -1){
            perror("Error accepting client");
            free(threadArgs);
            continue;
        }

        inet_ntop(clientAddr.ss_family,  get_in_addr((struct sockaddr*)&clientAddr), s, sizeof(s));
        printf("Server: got connection from %s\n", s);
        if((error = pthread_create(&tids[thread_count++], NULL, &handle_client, threadArgs)) != 0){
            close(threadArgs->sock_t);
            free(threadArgs);
            thread_count--;
            perror("pthread_create failed");
            continue;
        }
    }

//this will only be reached on error otherwise sigint_handler
clean_exit:
    free(port);
    free(mountPoint);
    if(sock != 0)
        close(sock);
    return 0;
}


// Thread function for handling client code
// Only gets passed the socket enclosed in a struct for ease
void *handle_client(void* t_args){
    pthread_mutex_lock(&lock);
    char send_ack[4] = "^^^";
    char recv_ack[4];
    char ack[4];
	printf("In handle_client\n");
    struct t_args *args = t_args;
    int t_sock = args->sock_t;
    free(t_args);
    pthread_detach(pthread_self());
    char *cmd = malloc(sizeof(char)*30);
    char *cmd_args = malloc(sizeof(char)*100);
    int res = 0;
    memset(cmd_args, '\0', 100);
    memset(cmd, '\0', 30);
    char* ret;


    
    if(recv(t_sock, cmd, 30, 0) < 0){
        perror("recv cmd failed");
        goto t_exit;
    }

    /* *
     *  Protocol for server client threads handling commands from 
     *  fuse.
     *
     *  1. Receive cmd
     *  2. Send send_ack
     *  3. Receive args necessary for cmd aka cmd_args
     *  4. Preform desired operation
     *  5. Send 1 for success or 0 for error
     *  6. Receive recv_ack
     *  7. Send result from operation.
     *
     * */

    printf("cmd recieved from client: %s\n",cmd);
    if(strcmp(cmd, "sf_getattr") == 0){
        if(send(t_sock, send_ack, 3, 0) < 0){ 
            perror("send ack failed");
            goto t_exit;
        }
        printf("Inside sf_getattr\n");

        char* path;
        struct stat buf;
        if(recv(t_sock, cmd_args, 100, 0) < 0){ //receive path
            perror("recv cmd_args failed");
            goto t_exit;
        }
        
        //get location for lstat from cmd_args and path 
        if(strcmp(cmd_args, "/") == 0){ 
            path = strdup(mountPoint);
        }
        else{
            path = malloc(sizeof(char)*(strlen(cmd_args)+strlen(mountPoint)+1));
            path = strcpy(path, mountPoint);
            path = strcat(path, cmd_args);
        }

        printf("Path recieved from client: %s\n", path);
        res = lstat(path, &buf);

        if(res < 0){ //lstat failure                   //send res if fail
            res = -errno;
            if(send(t_sock, &res, sizeof(res), 0) < 0){
                perror("send ret failed");
                goto t_exit;                                
            }
            printf("Server sent return value for error\n");
        }else{ //lstat success                          //send res if true
            if(send(t_sock, &res, sizeof(res), 0) < 0){
                perror("send ret failed");
                goto t_exit;
            }
            if(recv(t_sock, recv_ack, 3, 0) < 0 ){   //recv_ack
                perror("recv_ack failed");
                goto t_exit;
            }                       

            printf("Server sent return value for success\n");
            if (send(t_sock, &(buf.st_dev), sizeof(buf.st_dev), 0) < 0) {
                perror("stat data sending failed");
                goto t_exit;
            }
            if (recv(t_sock, recv_ack, 3, 0) < 0) {
                perror("stat data ack fail");
                goto t_exit;
            }
            if (send(t_sock, &(buf.st_ino), sizeof(buf.st_ino), 0) < 0) {
                perror("stat data sending failed");
                goto t_exit;
            }
            if (recv(t_sock, recv_ack, 3, 0) < 0) {
                perror("stat data ack fail");
                goto t_exit;
            }
            if (send(t_sock, &(buf.st_mode), sizeof(buf.st_mode), 0) < 0) {
                perror("stat data sending failed");
                goto t_exit;
            }
            if (recv(t_sock, recv_ack, 3, 0) < 0) {
                perror("stat data ack fail");
                goto t_exit;
            }
            if (send(t_sock, &(buf.st_nlink), sizeof(buf.st_nlink), 0) < 0) {
                perror("stat data sending failed");
                goto t_exit;
            }
            if (recv(t_sock, recv_ack, 3, 0) < 0) {
                perror("stat data ack fail");
                goto t_exit;
            }
            if (send(t_sock, &(buf.st_uid), sizeof(buf.st_uid), 0) < 0) {
                perror("stat data sending failed");
                goto t_exit;
            }
            if (recv(t_sock, recv_ack, 3, 0) < 0) {
                perror("stat data ack fail");
                goto t_exit;
            }
            if (send(t_sock, &(buf.st_gid), sizeof(buf.st_gid), 0) < 0) {
                perror("stat data sending failed");
                goto t_exit;
            }
            if (recv(t_sock, recv_ack, 3, 0) < 0) {
                perror("stat data ack fail");
                goto t_exit;
            }
            if (send(t_sock, &(buf.st_size), sizeof(buf.st_size), 0) < 0) {
                perror("stat data sending failed");
                goto t_exit;
            }
            if (recv(t_sock, recv_ack, 3, 0) < 0) {
                perror("stat data ack fail");
                goto t_exit;
            }
    		if (send(t_sock, &(buf.st_atim), sizeof(buf.st_atim), 0) < 0) {
    			perror("send stat time failed");
    			goto t_exit;
    		}
    		if (recv(t_sock, recv_ack, 3, 0) < 0){
    			perror("ack fail tog et");
    			goto t_exit;
    		}
    		if (send(t_sock, &(buf.st_mtim), sizeof(buf.st_mtim), 0) < 0) {
    			perror("send stat time failed");
    			goto t_exit;
    		}
    		if (recv(t_sock, recv_ack, 3, 0) < 0){
    			perror("ack fail tog et");
    			goto t_exit;
    		}
    		if (send(t_sock, &(buf.st_ctim), sizeof(buf.st_ctim), 0) < 0) {
    			perror("send stat time failed");
    			goto t_exit;
    		}
    		if (recv(t_sock, recv_ack, 3, 0) < 0){
    			perror("ack fail tog et");
    			goto t_exit;
    		}
            printf("Server sent stat\n");
        }
        free(path);        
    }
	else if (strcmp(cmd, "sf_mkdir") == 0) { //cmd is mkdir
        if(send(t_sock, send_ack, 3, 0) < 0){ 
            perror("send ack failed");
            goto t_exit;
        }
		mode_t modeC;
		char* path;
        printf("Inside sf_mkdir\n");

		if (recv(t_sock, cmd_args, 100, 0) < 0) { 			//recv path
			perror("recv cmd_args failed");
			goto t_exit;
		}
        if (send(t_sock, send_ack, 3, 0) < 0) {
            perror("recv ack fejll");
            goto t_exit;
        }
		if (recv(t_sock, &modeC, sizeof(mode_t), 0) < 0) { 		//recv mode
			perror("recv cmd_args failed w/ modeClient\n");
			goto t_exit;
		}
		if (strcmp(cmd_args, "/") == 0) { //if in root directory
			path = strdup(mountPoint);
		}
        else {						 //else copy the path from client.	
    		path = malloc(sizeof(char)*(strlen(cmd_args)+strlen(mountPoint)+1));
  			path = strcpy(path, mountPoint);
    		path = strcat(path, cmd_args);
		}
		res = mkdir(path, modeC);
		if (res < 0) { //mkdir failure
			res = -errno;
			if (send(t_sock, &res, sizeof(res), 0) < 0) {
				perror("send ret failed-");
				goto t_exit;
			}
		}
        else { //mkdir success
			if (send(t_sock, &res, sizeof(res), 0) < 0) {
				perror("send ret failed--.");
				goto t_exit;
			}
		}
		free(path);
	}
    else if(strcmp(cmd, "sf_opendir") == 0){
        char* path;
        if(send(t_sock, send_ack, 3, 0) < 0){
            perror("send ack failed");
            goto t_exit;
        }
        if(recv(t_sock, cmd_args, 100, 0) < 0){
            perror("recv cmd_args failed in sf_opendir");
            goto t_exit;
        }
        if(strcmp(cmd_args, "/") == 0){ 
            path = strdup(mountPoint);
        }
        else{
            path = malloc(sizeof(char)*(strlen(cmd_args)+strlen(mountPoint)+1));
            path = strcpy(path, mountPoint);
            path = strcat(path, cmd_args);
        }
        
        DIR* dirp = NULL;
        dirp = opendir(path);
        if(dirp == NULL){
            perror("opendir failed in sf_opendir");
            int error = -errno;
            if(send(t_sock, &error, sizeof(error), 0) < 0){
                perror("send error failed in sf_opendir");
                goto t_exit;
            }
            free(path);
            goto t_exit;
        }
        
        int dfd = dirfd(dirp);
        if(!(dfd > 0)){
            perror("opendir failed in sf_opendir");
            int error = -errno;
            if(send(t_sock, &error, sizeof(error), 0) < 0){
                perror("send error failed in sf_opendir");
                goto t_exit;
            }
            free(path);
            goto t_exit;
        }
        if(send(t_sock, &dfd, sizeof(dfd), 0) < 0){
            perror("send dfd failed in sf_opendir");
            goto t_exit;
        }
        free(path);
    }
    else if(strcmp(cmd,"sf_readdir") == 0 ){
        if(send(t_sock, send_ack, 3, 0) < 0){ 
            perror("send ack failed");
            goto t_exit;
        }
        printf("Inside sf_readdir\n");
        DIR* dirptr;
        struct dirent *dir_ent;
        struct dirent *dirarr[200];
        int m = 0;
        char* numdirs = malloc(sizeof(char)*5);
        int fd;
        if(recv(t_sock, &fd, sizeof(fd), 0) < 0){
            perror("recv fd failed");
            goto t_exit;
        }

        dirptr = fdopendir(fd);
        if(dirptr == NULL){ //lstat failure
            perror("opendir failed in readdir server fucntion");
            ret = strdup("0"); //0 for failure
            if(send(t_sock, ret, 1, 0) < 0){        //send ret if failed opendir
                free(ret);
                perror("send ret failed");
                goto t_exit;
            }
        }
        else{
            ret = strdup("1");
            if(send(t_sock, ret, 1, 0) < 0 ){       //send ret if opendir succeded
                perror("send ret failed");
                free(ret);
                goto t_exit;
            }
        }

        if(recv(t_sock, recv_ack, 3, 0) < 0){   //recv_ack
            perror("recv ack failed"); 
            goto t_exit;
        }
        printf("opendir portion of readdir done\n");
        while((dir_ent = readdir(dirptr)) != NULL){
                dirarr[m++] = dir_ent;
        }
        if(m == 0){
            perror("readdir failed in server function");
            numdirs = strcpy(numdirs, "0");
            if(send(t_sock, numdirs, 1, 0) < 0){         //send 0 if failed readdir
                perror("send ret failed");
                goto t_exit;
            }
        }else{
            sprintf(numdirs, "%d", m);
            if(send(t_sock, numdirs, 5, 0) < 0){        //send num dirs if succeed
                perror("send numdirs failed");
                goto t_exit;
            }

            if(recv(t_sock, recv_ack, 3, 0) < 0){  //recv_ack
                perror("recv_ack failed in reddir");
                goto t_exit;
            }
            
            struct dirent dsend;
            int i = 0; 
            for(i = 0; i < m; i++){
                dsend = *dirarr[i];
                if(send(t_sock, &dsend, sizeof(struct dirent), 0) < 0){ //send dirent
                    perror("send dirent struct failed in readdir");
                    goto t_exit;
                }
                printf("Sending dirent\n");
                if(recv(t_sock, recv_ack, 3, 0) < 0){        //recv_ack
                    perror("recv_ack failed when sending dirents");
                    goto t_exit;
                }
            }
            if(send(t_sock, send_ack, 3, 0) < 0){ 
                perror("send send_ack failed");
                goto t_exit;
            }
        }
        free(ret);
        free(numdirs);
    }
    else if (!strcmp(cmd, "sf_open")) {
        // First thing send ack for the command
        strcpy(ack, "^^^");
        if(send(t_sock, send_ack, 3, 0) < 0){ 
            perror("send ack failed");
            goto t_exit;
        }
        int fd, flags = 0;
        if (recv(t_sock, &flags, sizeof(flags), 0) < 0 || flags == 0) {
            perror("failed to get flags in open");
            goto t_exit;
        }
        printf("flags in open: %d\n", flags);
        if(send(t_sock, ack, 3, 0) < 0) {
            perror("failed to send ack");
            goto t_exit;
        }
        char buff[4096];
        memset(buff, 0, 4096);
        if (recv(t_sock, buff, 4096, 0) < 0) {
            perror("failed to get file path");
            goto t_exit;
        }
        char* fullPath = malloc(sizeof(char) * (strlen(buff) + strlen(mountPoint) + 1));
        strcpy(fullPath, mountPoint);
        strcat(fullPath, buff);
        fd = open(fullPath, flags);
        if (fd < 0)
            fd = -errno;
        if (send(t_sock, &fd, sizeof(fd), 0) < 0)
            perror("can't send back the file descriptor. Probably the client will hang");
        free(fullPath);
    }
    else if (!strcmp(cmd, "sf_create")) {
        mode_t mode = 0;
        strcpy(ack, "^^^");
        if(send(t_sock, send_ack, 3, 0) < 0){ 
            perror("send ack failed");
            goto t_exit;
        }
        int fd, flags = 0;
        if (recv(t_sock, &flags, sizeof(flags), 0) < 0 || flags == 0) {
            perror("failed to get flags in open");
            goto t_exit;
        }
        if(send(t_sock, ack, 3, 0) < 0) {
            perror("failed to send ack");
            goto t_exit;
        }
        char buff[4096];
        memset(buff, 0, 4096);
        if (recv(t_sock, buff, 4096, 0) < 0) {
            perror("failed to get file path");
            goto t_exit;
        }
        strcpy(ack, "^^^");
        if (send(t_sock, ack, 3, 0) < 0) {
            perror("ack send failed");
            goto t_exit;
        }
        if (recv(t_sock, &mode, sizeof(mode), 0) < 0) {
            perror("getting mode failed in create");
            goto t_exit;
        }
        char* fullPath = malloc(sizeof(char) * (strlen(buff) + strlen(mountPoint) + 1));
        strcpy(fullPath, mountPoint);
        strcat(fullPath, buff);
        fd = open(fullPath, flags | O_CREAT, mode);
        if (fd < 0)
            fd = -errno;
        if (send(t_sock, &fd, sizeof(fd), 0) < 0)
            perror("can't send back the file descriptor. Probably the client will hang");
        free(fullPath);
    }
    else if (!strcmp(cmd, "sf_releasedir")) {
        int len = 0, fd = -1;
        strcpy(ack, "^^^");
        if (send(t_sock, ack, 3, 0) < 0) {
            perror("ack send fail");
            goto t_exit;
        }
        //get the file handler
        if (recv(t_sock, &fd, sizeof(fd), 0) < 0) {
            perror("ack send fail");
            goto t_exit;
        }
        len = close(fd);
        if (len < 0)
            len = -errno;
        if (send(t_sock, &len, sizeof(len), 0) < 0) {
            perror("ack send fail");
            goto t_exit;
        }
    }
    else if(!strcmp(cmd, "sf_flush")) {
        strcpy(ack, "^^^");
        if (send(t_sock, ack, 3, 0) < 0) {
            perror("ack send fail");
            goto t_exit;
        }
        int fd = -1;
        if (recv(t_sock, &fd, sizeof(fd), 0) < 0) {
            perror("getting the fd from sf_flush failed");
            goto t_exit;
        }
        res = fsync(fd);
        if (res < 0)
            res = -errno;
        send(t_sock, &res, sizeof(res), 0);
    }
    else if (!strcmp(cmd, "sf_release")) {
        strcpy(ack, "^^^");
        if (send(t_sock, ack, 3, 0) < 0) {
            perror("ack send fail");
            goto t_exit;
        }
        int fd = -1;
        if (recv(t_sock, &fd, sizeof(fd), 0) < 0) {
            perror("getting the fd from sf_flush failed");
            goto t_exit;
        }
        res = fsync(fd);
        if (res < 0){
            res = -errno;
            if(send(t_sock, &res, sizeof(res), 0) < 0)
                perror("failed to send the error message back.xD");
            goto t_exit;
        }
        res = close(fd);
        if (res < 0)
            res = -errno;
        if(send(t_sock, &res, sizeof(res), 0) < 0)
            perror("failed to send the error message back.xD");
    }
    else if (!strcmp(cmd, "sf_truncate")) {
        off_t size = 0;
        strcpy(ack, "^^^");
        if (send(t_sock, ack, 3, 0) < 0) {
            perror("ack send fail");
            goto t_exit;
        }
        res = -1;
        if (recv(t_sock, &res, sizeof(res), 0) < 0) {
            perror("Failed to get the path length");
            goto t_exit;
        }
        if (res < 0){
            perror("the path length is negative in the truncate server side");
            goto t_exit;
        }
        res++;
        char path[res];
        memset(path, 0, res);
        if (recv(t_sock, path, res, 0) < 0) {
            perror("couldn't retrieve path in truncate");
            goto t_exit;
        }
        res = strlen(path) + strlen(mountPoint) + 1;
        char abso[res];
        strcpy(abso, mountPoint);
        strcat(abso, path);
        if (recv(t_sock, &size, sizeof(size), 0) < 0) {
            perror("couldn't retrieve size in truncate");
            goto t_exit;
        }
        res = truncate(abso, size);
        if (res < 0)
            res = -errno;
        if (send(t_sock, &res, sizeof(res), 0) < 0)
            perror("failed to sent the result for truncate");
    }
    else if(strcmp(cmd, "sf_read") == 0){
        if(send(t_sock, send_ack, 3, 0) < 0){
            perror("send_ack failed in sf_read");
            goto t_exit;
        }
        int fd;
        if(recv(t_sock, &fd, sizeof(fd), 0) < 0){
            perror("recv fd failed in sf_read");
            goto t_exit;
        }
        if(send(t_sock, send_ack, 3, 0) < 0){
            perror("send_ack failed in sf_read");
            goto t_exit;
        }
        int size;
        if(recv(t_sock, &size, sizeof(size), 0) < 0){
            perror("recv size failed in sf_read");
            goto t_exit;
        }
        if(send(t_sock, send_ack, 3, 0) < 0){
            perror("send_ack failed in sf_read");
            goto t_exit;
        }
        int offset;
        if(recv(t_sock, &offset, sizeof(offset), 0) < 0){
            perror("recv offset failed in sf_read");
            goto t_exit;
        }
        char* buf = malloc(sizeof(char)*(size+1));
        memset(buf, 0, size+1);
        int res = read(fd, buf, size);
        if (res < 0)
            res = -errno;
        if(send(t_sock, &res, sizeof(res), 0 ) < 0){
            perror("send res failed in sf_read");
            goto t_exit;
        }
        if (res >= 0) {
            if(recv(t_sock, recv_ack, 3, 0) < 0){
                perror("recv_ack failed in sf_read");
                goto t_exit;
            }
            printf("\nbuff: %s\n", buf);
            if(send(t_sock, buf, size, 0) < 0){
                perror("send buffer from read failed");
                goto t_exit;
            }
        }
        free(buf);          
    }
    else if(strcmp(cmd, "sf_write") == 0){
        if(send(t_sock, send_ack, 3, 0) < 0){
            perror("send_ack failed in sf_write");
            goto t_exit;
        }
        int fd;
        if(recv(t_sock, &fd, sizeof(fd), 0) < 0){
            perror("recv fd failed in sf_write");
            goto t_exit;
        }
        if(send(t_sock, send_ack, 3, 0) < 0){
            perror("send_ack failed in sf_write");
            goto t_exit;
        }
        int size;
        if(recv(t_sock, &size, sizeof(size), 0) < 0){
            perror("receive size failed in server");
            goto t_exit;
        }
        if(send(t_sock, send_ack, 3, 0) < 0){
            perror("send_ack failed in sf_write");
            goto t_exit;
        }
        int offset;
        if(recv(t_sock, &offset, sizeof(offset), 0) < 0){
            perror("recv offset failed in sf_write");
            goto t_exit;
        }
        if(send(t_sock, send_ack, 3, 0) < 0){
            perror("send_ack failed in sf_write");
            goto t_exit;
        }
        char* buf = malloc(sizeof(char)*size+1);            
        if(recv(t_sock, buf, size+1, 0) < 0){
            perror("recv buf failed in sf_write");
        }
        int off = lseek(fd, offset, SEEK_SET);
        if(off != offset){
            perror("lseek failed in sf_write");
            goto t_exit;
        }
        int res = write(fd, buf, size);
        if(res < 0){
            perror("write");
            res = -errno;
        }
        if(send(t_sock, &res, sizeof(res), 0) < 0){
            perror("send res failed in sf_write");
            goto t_exit;
        }
        free(buf);
    }
    else if(!strcmp(cmd, "sf_utimens")) {
        struct timespec ts[2];
        strcpy(send_ack, "^^^");
        char buf[4096];
        memset(buf, 0, 4096);
        if(send(t_sock, send_ack, 3, 0) < 0){
            perror("send_ack failed in sf_write");
            goto t_exit;
        }
        if (recv(t_sock, ts, sizeof(struct timespec) * 2, 0) < 0) {
            perror("recieving structs failed");
            goto t_exit;
        }
        if(send(t_sock, send_ack, 3, 0) < 0){
            perror("send_ack failed in sf_write");
            goto t_exit;
        }
        if (recv(t_sock, buf, 4096, 0) < 0) {
            perror("failed to get the path");
            goto t_exit;
        }
        res = strlen(buf) + strlen(mountPoint) + 1;
        char path[res];
        strcpy(path, mountPoint);
        strcat(path, buf);
        res = utimensat(0, path, ts, 0);
        if (res < 0)
            res = -errno;
        if (send(t_sock, &res, sizeof(res), 0) < 0)
            perror("failed to send back result in sf_utimens");
    }

t_exit:
    pthread_mutex_unlock(&lock);
    free(cmd);
    free(cmd_args);
    close(t_sock);
    return NULL;
}



/*When the server loop is running and someone kills it with CRTL-C
  This function catches it frees memory and closes the open server socket */
void sigint_handler(){ 
    pthread_mutex_destroy(&lock);
    if(last != NULL)
        free(last);
    close(sock);
    free(mountPoint);
    free(port);
    _exit(0);

}

// Beej's function for handling IPv4 and IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

