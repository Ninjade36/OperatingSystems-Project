
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <netdb.h>

//global pipe
int mypipe[2];


#define DO_SYS(syscall) do {		\
    if( (syscall) == -1 ) {		\
        perror( #syscall );		\
        exit(EXIT_FAILURE);		\
    }						\
} while( 0 )

struct addrinfo* alloc_tcp_addr(const char *host, uint16_t port, int flags)
{
    int err;   struct addrinfo hint, *a;   char ps[16];

    snprintf(ps, sizeof(ps), "%hu", port); 
    memset(&hint, 0, sizeof(hint));
    hint.ai_flags    = flags;
    hint.ai_family   = AF_UNSPEC;
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_protocol = IPPROTO_TCP;

    if( (err = getaddrinfo(host, ps, &hint, &a)) != 0 ) {
        fprintf(stderr,"%s\n", gai_strerror(err));
        exit(EXIT_FAILURE);
    }

    return a; // should later be freed with freeaddrinfo()
}

int tcp_connect(const char *host, uint16_t port)
{
    int clifd;
    struct addrinfo *a = alloc_tcp_addr(host, port, 0);

    DO_SYS( clifd = socket( a->ai_family,a->ai_socktype,a->ai_protocol ) );
    DO_SYS( connect( clifd,a->ai_addr,a->ai_addrlen  )   );
    freeaddrinfo( a );
    return clifd;
}
void command_line_interpreter(){
	char buffer[256];
	char command[256];
	printf(">");
	while(1){
		bzero(command,256);
		if(fgets(buffer,sizeof(buffer),stdin)){
			if (sscanf(buffer, "%[^\n]%*c", command)==1) {//"%[^\n]%*c" means that all the characters entered untill we get enter buttton
			write(mypipe[1],command,strlen(command)+1);
			}
		}
	}
}
void communication_process(const char *host,int port){
	char buff[256] ,command_out[256];
	int k, fd = tcp_connect(host, port);
    
	while(1){
		bzero(command_out, 256);
        read(mypipe[0], command_out, 256);
        DO_SYS(     write(fd, command_out, strlen(command_out)+1)  );
        DO_SYS( k = read (fd, buff, sizeof(buff))  );
	if((strcmp(buff,"exit")==0)){
		DO_SYS(close(fd));
		pid_t p_pid=getppid();
		kill(p_pid,SIGKILL);
		exit(1);}
	DO_SYS(     write(STDOUT_FILENO, buff, k) );
}
DO_SYS(close(fd));
}
int main(int argc,char *argv[]){
	int sockfd,portno, n;
	if (argc < 3) {
       fprintf(stderr,"usage %s hostname port\n", argv[0]);
       exit(0);}
    char *servername=argv[1];
	portno=atoi(argv[2]);
	pipe(mypipe);
	if(fork()==0){
	close(mypipe[1]);
	communication_process(servername,portno);
		}
	else{
		close(mypipe[0]);
		command_line_interpreter();
		}
	return 0;
	}
	
	
	

