#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <memory.h>


#define DO_SYS(syscall) do {		\
    if( (syscall) == -1 ) {		\
        perror( #syscall );		\
        exit(EXIT_FAILURE);		\
    }						\
} while( 0 )

struct addrinfo* alloc_tcp_addr(const char *host, uint16_t port, int flags)
{
    int err;   struct addrinfo hint, *a;   char ps[16];

    snprintf(ps, sizeof(ps), "%hu", port); // why string?
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

int tcp_establish(int port) {
    int srvfd;
    struct addrinfo *a =
	    alloc_tcp_addr(NULL/*host*/, port, AI_PASSIVE);
    DO_SYS( srvfd = socket( a->ai_family,a->ai_socktype,a->ai_protocol ) 	);
    DO_SYS( bind( srvfd,a->ai_addr,a->ai_addrlen  ) 	);
    DO_SYS( listen( srvfd,5/*backlog*/   ) 	);
    freeaddrinfo( a );
    return srvfd;
}
typedef struct tpool{//threadpool struct
	pthread_mutex_t worker_mutex;
	pthread_cond_t worker_cond;
	int tasks[250];
	int taskcount;//tasks till now
	int currtask;//current task running
	
}tpool;


typedef struct user{ //a struct for each user
    char id[10];
    char password[50];
    int TA; // 1: TA , 0: student
    int signed_in; // 1:signed in , 0: signed out
    char grade[3]; 
}user;

//global variables
#define THREAD_NUM 5
#define USERS_NUM 512
user* users[USERS_NUM];//global array for all users
int users_iter=0; //iterator for the global array
int studentnumbers=0;
//all the commands to run a function 

void Reorder(){
	user *minuser=malloc(sizeof(user));
	int minindex;
	
	for(int i=0;i<users_iter;i++){
		minuser=users[i];
		minindex=i;
		for(int j=minindex+1;j<users_iter;j++){
			if(strcmp(users[j]->id,minuser->id)<0){
				minuser=users[j];
				minindex=j;
				}
			}
		users[minindex]=users[i];
		users[i]=minuser;
		}
	}

int login(char *userid,char *password,int socketnum);


int findfunc(tpool *temp,int socket_num);


//thread starting function

void *startThread(void *args){//put a struct here to take from all mutexqueues condqueues
	tpool *temp_tp=args;
	
	while(1){
		pthread_mutex_lock(&(temp_tp->worker_mutex));
		while((temp_tp->taskcount-temp_tp->currtask)<=0){//check that condition
			pthread_cond_wait(&(temp_tp->worker_cond), &(temp_tp->worker_mutex));
			}
		
	int mysocket=temp_tp->tasks[temp_tp->currtask];
	if (mysocket != -1){
            temp_tp->tasks[temp_tp->currtask] = -1;
            temp_tp->currtask++;
            pthread_mutex_unlock(&(temp_tp->worker_mutex));
            findfunc(temp_tp,mysocket); //the functions handler
            
        }
    else{
		pthread_mutex_unlock(&(temp_tp->worker_mutex));
		}
	}
}

int main(int argc, char *argv[]) {
	
	
	//initialize the data structures of the server.
	FILE *fs,*fT;
	fs=fopen("students.txt","r");
	fT=fopen("assistants.txt","r");  
	
	if(fs==NULL || fT==NULL){
		printf("unable to open file ");
		exit(1);}
		

	while(!feof(fs)){
        user *tmpuser = malloc(sizeof(user));
        
        char c = fgetc(fs);
        int id_iter = 0;
        while (c != ':'){ //reading the ID
            tmpuser->id[id_iter] = c;
            c = fgetc(fs);
            id_iter++;
        }
        tmpuser->id[id_iter] = '\0';
        c = fgetc(fs); //taking the :
        int password_iter = 0;
        while (c != '\n' && !feof(fs)){ //reading the password
            tmpuser->password[password_iter] = c;
            c = fgetc(fs);
            int d = feof(fs);
            password_iter++;
        }
        strcat(tmpuser->grade, "0");
        tmpuser->signed_in = 0;
        tmpuser->TA = 0;
        users[users_iter] = tmpuser;
        users_iter++;
        studentnumbers++;
    }
    fclose(fs);
    
	while(!feof(fT)){
        user* tmpuser = malloc(sizeof(user));
        char c = fgetc(fT);
        int id_iter = 0;
        while (c != ':'){ //reading the ID
            tmpuser->id[id_iter] = c;
            c = fgetc(fT);
            id_iter++;
        }
        tmpuser->id[id_iter] = '\0';
        c = fgetc(fT); //taking the :
        int password_iter = 0;
        while (c != '\n' && !feof(fT)){ //reading the password
			if(c==127){continue;}
            tmpuser->password[password_iter] = c;
            c = fgetc(fT);
            int d = feof(fT);
            password_iter++;
        }
        strcpy(tmpuser->grade, "-1");
        tmpuser->signed_in = 0;
        tmpuser->TA = 1;
        users[users_iter] = tmpuser;
		int passwordlength=strlen(users[users_iter]->password);
		if(users[users_iter]->password[passwordlength-1]==127){
			users[users_iter]->password[passwordlength-1]='\0';
			}
        
        users_iter++;
		
        
    }
    fclose(fT);
  
	
		
	//init the tpoolworker
	struct tpool *th_pool=malloc(sizeof(tpool));
	pthread_mutex_init(&(th_pool->worker_mutex), NULL);
    pthread_cond_init(&(th_pool->worker_cond), NULL);
    th_pool->taskcount=0;
	
	pthread_t threads[THREAD_NUM];
	//create the 5 workerthreads
	for (int i = 0; i < THREAD_NUM; i++) {
        pthread_create(&threads[i], NULL, &startThread, th_pool);
    }
	
	
	int portno = atoi(argv[1]);        //the port specified in the commandline
	
	int clifd,srvfd=tcp_establish(portno);
	while(1){               //infinite loop in which it will accept users connections 
		DO_SYS( clifd=accept(srvfd,NULL,NULL));
		pthread_mutex_lock(&(th_pool->worker_mutex));
		th_pool->tasks[th_pool->taskcount]=clifd;
		th_pool->taskcount++;
		pthread_cond_signal(&(th_pool->worker_cond));
		pthread_mutex_unlock(&(th_pool->worker_mutex));
}
	
	
	close(srvfd);
	
	return 0; 
}

	
int findfunc(tpool *th,int socket_num){//find the correct function and run it 
	
	char buffer[256];
	int logged_in=0;
	char not_logged[50] = "Not logged in\n";
	int foundf;
	char curfunction[200];
	char cmd[]="\n>\0";
	while(1){
		foundf=0;
		bzero(buffer,256);
		read(socket_num,buffer,256);
		int bufflen=strlen(buffer);
		int buff_iter=0;
		memset(curfunction,0,strlen(curfunction));
		char ch=buffer[buff_iter];
		char userid[9];
		char password[220];
		int it=0;
		
		while(buffer[buff_iter] >= 65 && buffer[buff_iter] <= 122){ //readinng the function
            curfunction[it] = buffer[buff_iter];
            buff_iter++;
            it++;
        }
		curfunction[it]='\0';
		buff_iter++;
		
		//--------------login function-----------------------------------
		
		if(strcmp(curfunction,"Login")==0){
			foundf=1;
			if(logged_in==1){write(socket_num,"a user is already logged in\n>\0",strlen("a user is already logged in\n>\0")+1);continue;}
			for(int j=0;j<9;j++){
				userid[j]=buffer[buff_iter];
				buff_iter++;
				}
			userid[9]='\0';
			
			buff_iter++;//for the ' '
			int passi=0;
			for(buff_iter;buff_iter<strlen(buffer);buff_iter++){
				password[passi++]=buffer[buff_iter];
				}
			
			write(socket_num,"\n",strlen("\n"+1));
			char wellcome[200]="Welcome ";
			char ta[200]="TA ";
			char student[200]="Student ";
			int found=0;
			
			int L1=0;
			for(L1;L1<users_iter;L1++){
			if(strcmp(users[L1]->id,userid)==0){
				password[passi]=0; 
				if(strcmp(users[L1]->password,password)==0){
					users[L1]->signed_in=1;
					logged_in=1;
					found=1;
				if(users[L1]->TA==1){
					strcat(wellcome,ta);
				}
				else{
					strcat(wellcome,student);
					}
				strcat(wellcome,users[L1]->id);
				strcat(wellcome,cmd);
				write(socket_num,wellcome,sizeof(wellcome)+1);
				break;
			}
			else{
				found=1;
				write(socket_num,"Wrong user information\n>\0",strlen("Wrong user information\n>\0")+1); break;
				}
		}
	}
	if(found==0){write(socket_num,"Wrong user information\n>\0",strlen("Wrong user information\n>\0")+1);}			
}				
				

				
		//--------------ReadGrade function-----------------------------------

		else if((strcmp(curfunction,"ReadGrade")==0) && bufflen==9){
			
			foundf=1;
			if(logged_in==0){
				write(socket_num,"not logged in\n>\0",strlen("not logged in\n>\0")+1);
				}
			else{
				
			for(int r1=0;r1<users_iter;r1++){
				
				userid[9]=0;
				
				if(strcmp(users[r1]->id,userid)==0){
					
					if(users[r1]->TA==0){
						char grd[3];
						int giter=0;
						for(int iter=0;iter<strlen(users[r1]->grade);iter++){
							if(users[r1]->grade[iter]>=48 && users[r1]->grade[iter]<=57){
								grd[giter]=users[r1]->grade[iter];
								giter++;
								}
							}
						grd[giter]=0;
						
						strcat(grd,cmd);
						
						write(socket_num,grd,strlen(grd));
					break;}
					
				else{write(socket_num,"Missing argument\n>\0",strlen("Missing argument\n>\0")+1); break;}
					}
				}	
			}
		}
		
		
		
		
		
		//--------------ReadGrade id function-----------------------------------
		
		else if((strcmp(curfunction,"ReadGrade")==0) ){
			
			int found2=0;
			if(logged_in==0){write(socket_num,"Not logged in\n>\0",strlen("Not logged in\n>\0")+1);}
			else{ 
			char requestedid[9];
			for(int r_i=0;r_i<9;r_i++){
				requestedid[r_i]=buffer[buff_iter];
				buff_iter++;
				}
			for(int R2=0;R2<users_iter;R2++){
				userid[9]=0;
				if(strcmp(users[R2]->id,userid)==0){
					if(users[R2]->TA==1){
						for(int lk=0;lk<users_iter;lk++){
							if(strcmp(users[lk]->id,requestedid)==0){
								found2=1;
								char grd[3];
								int giter=0;
								for(int iter=0;iter<strlen(users[lk]->grade);iter++){
									if(users[lk]->grade[iter]>=48 && users[lk]->grade[iter]<=57 && giter<3){
										grd[giter]=users[lk]->grade[iter];
										giter++;
										}
									}
									grd[giter]=0;
								strcat(grd,cmd);
								write(socket_num,grd,strlen(grd));
								
								break;
								}
							}
						if(found2==0){
							write(socket_num,requestedid,strlen(requestedid)+1);
							
							break;}	
				}else{write(socket_num,"Action not allowed\n>\0",strlen("Action not allowed\n>\0")+1);}
			}
		}
	}
}
			
			
			
		//--------------GradeList function-----------------------------------

		else if((strcmp(curfunction,"GradeList")==0)){
			userid[9]=0;
			foundf=1;
			Reorder();
			if(logged_in==0){write(socket_num,"Not logged in\n>\0",strlen("Not logged in\n>\0")+1);}
			for(int GL=0;GL<users_iter;GL++){
				
				if(strcmp(users[GL]->id,userid)==0){
					if (users[GL]->TA==0){
						write(socket_num,"Action not allowed\n>\0",strlen("Action not allowed\n>\0")+1);
						break;
						}
					else{
						int cnt=0;
						char grades[5000];
						
						int gradesiter=0;
						for(int zi=0;zi<users_iter;zi++){
							if(users[zi]->TA==0){
								for(int rena=0;rena<9;rena++){
									grades[gradesiter]=users[zi]->id[rena];
									gradesiter++;
									}
								
								cnt++;
								
								grades[gradesiter++]=':';
								grades[gradesiter++]=' ';
								
								for(int tg=0;tg<strlen(users[zi]->grade);tg++){
									
									grades[gradesiter++]=users[zi]->grade[tg];
									}
									grades[gradesiter++]='\n';
								}
							}
						char cmd2[]=">\0";
						strcat(grades,cmd2);
						write(socket_num,grades,strlen(grades)+1);
						bzero(grades,5000);
						break;
						}
					}
			}
		}
		
		
		
		
		//--------------UpdateGrade function-----------------------------------

		else if((strcmp(curfunction,"UpdateGrade")==0)){
			int found3=0;
			foundf=1;
			userid[9]=0;
			if(logged_in==0){write(socket_num,"Not logged in\n>\0",strlen("Not logged in\n>\0")+1); continue;}
			else{ 
			char requestedid[9];
			for(int UG=0;UG<9;UG++){
				requestedid[UG]=buffer[buff_iter];
				buff_iter++;
				}
			buff_iter++;// for the " "
			char grade[3];
			char ch2=buffer[buff_iter];
			int gi=0;
			int buffcnt=0;
			while(ch2>=48 && ch2<=57){
				ch2=buffer[buff_iter];
				grade[gi]=buffer[buff_iter];
				buff_iter++;
				buffcnt++;
				ch2=buffer[buff_iter];
				gi++;
				
				}
			grade[buffcnt]=0;

			for(int RG=0;RG<users_iter;RG++){
				if(strcmp(users[RG]->id,userid)==0){
					if(users[RG]->TA==1){
						for(int lk2=0;lk2<users_iter;lk2++){
							if(strcmp(users[lk2]->id,requestedid)==0){
								found3=1;
								strcpy((users[lk2]->grade),grade);
								
								write(socket_num,">\0",strlen(">\0")+1);
								break;
								}
							}
						if(found3==0){
							
							user *newuser=malloc(sizeof(user));
							newuser->TA=0;
							strcpy(newuser->password,"");
							strcpy((newuser->id),requestedid);
							strcpy((newuser->grade),grade);
							users[users_iter++]=newuser;
							write(socket_num,">\0",strlen(">\0")+1);
							break;
							
							}	
				}else{write(socket_num,"Action not allowed\n>\0",strlen("Action not allowed\n>\0")+1);}
			}
		}
	}
}
			
			
			
			
			
			
			
		//--------------Logout function-----------------------------------
	
		else if((strcmp(curfunction,"Logout"))==0){
			foundf=1;
			
			if(logged_in==0){write(socket_num,"Not logged in\n>\0",strlen("Not logged in\n>\0")+1);}
			char goodbye[20]="Good Bye ";
			userid[9]=0;
			for(int Lo=0;Lo<users_iter;Lo++){
				if(strcmp(users[Lo]->id,userid)==0){
					logged_in=0;
					users[Lo]->signed_in=0;
					strcat(goodbye,userid);
					memset(userid,0,strlen(userid));
					memset(password,0,strlen(password));
					strcat(goodbye,cmd);
					write(socket_num,goodbye,strlen(goodbye)+1);
					bzero(goodbye,60);
					break;
					}
				}
			}
			
			
			
		
		//--------------Exit function-----------------------------------
	
		else if((strcmp(curfunction,"Exit"))==0){
			foundf=1;
			if(logged_in==1){
				char goodbye[20]="Good Bye ";
				for(int f=0;f<users_iter;f++){
				if(strcmp(users[f]->id,userid)==0){
					logged_in=0;
					users[f]->signed_in=0;
					strcat(goodbye,userid);
					memset(userid,0,strlen(userid));
					memset(password,0,strlen(password));
					strcat(goodbye,cmd);
					
					bzero(goodbye,60);
					break;
				}
			}
		}
			write(socket_num,"exit",strlen("exit")+1);
			close(socket_num);
			return -5;
	}
		else{//case of wrong input
			write(socket_num,"Wrong Input\n>\0",strlen("Wrong Input\n>\0")+1);
			}
		}
	}	


