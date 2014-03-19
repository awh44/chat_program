#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <time.h>

#define USERS 10
#define WAITING 10
#define BUFFSIZE 1024
#define NAMELEN 32

struct UserInfo{
	pthread_t thread_num; //Users thread pointer
	int sockfd; //File Descripter for user
	char *name; //Name for the user
};

struct UserNode{
	struct UserInfo * userInfo; //The user's information
	struct UserNode * next; //Pointer to the next user in the list
};

void Send(int sockfd, char * buffer);
void add_user(struct UserNode * node, struct UserInfo * info);
void remove_user(struct UserNode * node, struct UserInfo * delInfo);
int userListSize;
struct UserInfo users[USERS];
struct UserNode  * userList;
pthread_mutex_t clientListMutex;
void * user_handler(void * user);
void execute_command(struct UserInfo * user, char * cmd);

int main()
{
	struct sockaddr_in sad;
	struct sockaddr_in cad;
	int welcomeSocket, connectionSocket, clilen;
	
	userList = (struct UserNode *)malloc(sizeof(struct UserNode));
	userList->next = NULL;
	userList->userInfo = NULL;

	int port = 9034;
	srand(time(NULL));
	if((welcomeSocket = socket(PF_INET, SOCK_STREAM, 0)) == -1){
		fprintf(stderr, "Failed to make Socket\n");
		return errno;
	}
	memset((char *) &sad, 0, sizeof(sad));
	sad.sin_family = AF_INET;
	sad.sin_addr.s_addr = INADDR_ANY;
	sad.sin_port = htons((u_short) port);
	if(bind(welcomeSocket, (struct sockaddr *) &sad, sizeof(sad))==-1){
		fprintf(stderr, "Failed to bind Socket\n");
		return errno;
	}

	if (listen(welcomeSocket, WAITING) < 0)
		printf("Too many clients attempting to connect\n");
	clilen = sizeof(cad);

	char buffer[128];
	strcpy(buffer, "Hello!");

	while (1)
	{	
		connectionSocket = accept(welcomeSocket, (struct sockaddr *) &cad, &clilen);
		if(userListSize > USERS){
			fprintf(stderr, "Connection full, request rejected...\n");
			close(connectionSocket);
			continue;
		}else{
			fprintf(stdout, "CONNECTION ACCEPTED!\n");
			struct UserInfo * user = (struct UserInfo *)malloc(sizeof(struct UserInfo));
			user->sockfd = connectionSocket;
			pthread_mutex_lock(&clientListMutex);
			add_user(userList, user);
			pthread_mutex_unlock(&clientListMutex);
			pthread_create(&user->thread_num, NULL, user_handler, (void *)user);
		}
	}

	close(welcomeSocket);
	//Free our User List
	struct UserNode * curr;
	for(curr = userList; curr != NULL; ){
		struct UserNode * temp = curr;
		curr = curr->next;
		free(temp);
	}

	return 0;
}

void * user_handler(void * user){
	printf("Started User Handler Thread\n");
	struct UserInfo * info = (struct UserInfo *)user;
	struct UserNode * node;
	int recBytes, sentBytes;
	bool userNameChosen = false;
	bool userNameTaken = false;

	char name[NAMELEN];
	char buffer[BUFFSIZE];
	//Get user's chosen name, see if it's already taken
	//If it is, then we keep checking until they give one that isn't taken
	while(!userNameChosen){
		//Get name from server
		recBytes = recv(info->sockfd, name, sizeof(name), 0);

		//Go through the list of clients
		pthread_mutex_lock(&clientListMutex);
		for(node = userList; node != NULL; node = node->next){
			//If the name is already taken, try again
			//(note:) Send taken to the user, so the client
				//Will know to prompt the user for a new name
			printf("%s\n", node->userInfo->name);
			if(strcmp(node->userInfo->name, name) == 0){
				strcpy(buffer, "taken");
				Send(info->sockfd, buffer);
				pthread_mutex_unlock(&clientListMutex);
				userNameTaken = true;
				break;
			}
		}
		//Add UserName to User Information, Notify client
		if(!userNameTaken){
			info->name = name;
			strcpy(buffer, "success");
			Send(info->sockfd, buffer);
			pthread_mutex_unlock(&clientListMutex);
			userNameChosen = true;
		}else{
			userNameTaken = false;
		}
	}

	while(1){
		char * ptr = buffer;
		int remBytes = BUFFSIZE;
/*		while((recBytes = recv(info->sockfd, ptr, remBytes,0)) > 0){
			printf("Current Buffer: %s\n", ptr);
			remBytes-=recBytes;
			ptr+=recBytes;
		} */

		recBytes = recv(info->sockfd, buffer, sizeof(buffer),0);

		printf("Final Buffer: %s\n", buffer);

		//If it's quit command, exit
		if(!strcmp(buffer, "/quit")){
			pthread_mutex_lock(&clientListMutex);
			remove_user(userList, info);
			pthread_mutex_unlock(&clientListMutex);
			break;
			//If it's a command, execute it
		}else if(buffer[0] == '/'){
			execute_command(info, &buffer[1]);
		//Else, send to all users
		}else{
			pthread_mutex_lock(&clientListMutex);
			for(node = userList; node != NULL; node = node->next ){
				Send(node->userInfo->sockfd, buffer);
			}
			pthread_mutex_unlock(&clientListMutex);
		}
		//Reset the buffer
		memset(buffer, 0, sizeof(buffer));
	}

	close(info->sockfd);
	return NULL;

}

void execute_command(struct UserInfo * info, char * buffer){
	char * command = strdup(buffer);
	char * token = strtok(command, " ");
	
	//EXPECTED INPUT - /roll 3d10
	if(!strcmp(token, "roll")){
		//ROLL BABY ROLL
		char * nextToken = strtok(NULL, " ");
		if(nextToken != NULL){
			char buffer2[BUFFSIZE];
			buffer2[0] = '\0';
			char * c_numDice = strtok(NULL, "d");
			char * c_numOnDice = strtok(NULL, "d");

			int numDice, numOnDice;
			if(c_numDice != NULL && (numDice = atoi(c_numDice)) != 0){
				if(c_numOnDice != NULL && (numOnDice = atoi(c_numOnDice)) != 0){
					int i, total, curr = 0;
					char * istr = malloc(sizeof(char)*3);
					char * cRand = malloc(sizeof(char)*3);
					//Show the dice rolls
					for(i = 1; i <= numDice; i++){
						curr = rand()%numOnDice;
						strcat(buffer2, "Roll ");
						//DO STUFF HERE ITOA
						sprintf(istr, "%d", i);
						strcat(buffer2, istr);
						strcat(buffer2, ": ");
						//DO STUFF HERE ITOA
						sprintf(cRand, "%d", curr);
						strcat(buffer2, cRand);
						strcat(buffer2, "\n");
						total += curr;
					}
					//Print out the total
					strcat(buffer2, "Total: ");
					char * cTotal = malloc(sizeof(char)*5);
					sprintf(cTotal, "%d", total);
					strcat(buffer2, cTotal);
					strcat(buffer2, "\n");

					//Send the result
					struct UserNode * node;
					for(node = userList; node != NULL; node = node->next ){
						Send(node->userInfo->sockfd, buffer);
					}
				}
			}
	
		}
	//EXPECTED INPUT /whisper bob Hello
	}else if(!strcmp(token, "whisper")){
		//...whispering...
		
		char * whispName = strtok(NULL, " ");
		if(whispName != NULL){	
			struct UserNode * node;
			for(node = userList; node != NULL; node = node->next){
				//If the name is actually there, send them the 
					//message, otherwise, send them 
					//that they failed miserably
				if(!strcmp(node->userInfo->name, whispName)){
					char * message = strtok(NULL, " ");
					if(message != NULL){
						Send(node->userInfo->sockfd, message);
					}
					break;
				}
			}
		}
	}else if(!strcmp(token, "me")){
		//Just prepending client's name to message
		char message[BUFFSIZE];
		message[0] = '\0';
		strcat(message, info->name);
		strcat(message, " ");
		char * temp = strtok(NULL, " " );
		if(temp != NULL){
			strcat(message, temp);
			if(message != NULL){
				Send(info->sockfd, message);
			}
	
		}
	}
}

void Send(int sockfd, char * buffer){
	if(send(sockfd, buffer, sizeof(buffer), 0) < 0){
		fprintf(stderr, "Error sending information\n");
	}
}

void add_user(struct UserNode * head, struct UserInfo * user){
	printf("ADDING USER\n");
	struct UserNode * curr = head;
	user->name = "Anonymous";

	if(curr->userInfo == NULL){
		curr->userInfo = user;
	}else{
		while(curr->next != NULL){
			curr = curr->next;
		}
		curr->next = (struct UserNode *)malloc(sizeof(struct UserNode));
		curr = curr->next;
		curr->userInfo = user;
		userListSize++;
	}
}

void remove_user(struct UserNode * head, struct UserInfo * user){
	struct UserNode * curr = head;

	if(curr->userInfo->sockfd == user->sockfd){
		printf("Removing user: %s", curr->userInfo->name);
		struct UserNode * temp = curr;
		head = curr->next;
		free(temp);
		userList = head;
	}
	
	struct UserNode * prev = head;
	curr = head->next;

	while(curr != NULL){
		if(curr->userInfo->sockfd == user->sockfd){
			printf("Removing user: %s", curr->userInfo->name);
			prev->next = curr->next;
			userListSize--;
			free(curr);
			break;
		}
		prev = prev->next;
		curr = curr->next;
	}
}
