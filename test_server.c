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
	pthread_t thread_Num; //Users thread pointer
	int sockfd; //File Descripter for user
	char name[NAMELEN]; //Name for the user
};

struct UserNode{
	struct UserInfo * userInfo; //The user's information
	struct UserNode * next; //Pointer to the next user in the list
};

void Send(int sockfd, char * buffer);
void add_user(UserNode node, UserInfo info);
void remove_user(UserNode node, UserInfo delInfo);
int compare(UserInfo user1, UserInfo user2);
int userListSize;
struct UserInfo users[USERS];
struct UserNode userList;
pthread_mutex_t clientListMutex;
void * user_handler(void * user);
void execute_command(int fd, char * cmd);

int main()
{
	struct sockaddr_in sad;
	struct sockaddr_in cad;
	int welcomeSocket, connectionSocket, clilen;
	
	userList->next = NULL;
	userList->userInfo = NULL;

	int port = 9034;
	srand(time(NULL));
	if((welcomeSocket = socket(PF_INET, SOCK_STREAM, 0)) == -1){
		printf(stderr, "Failed to make Socket");
		return errno;
	}
	memset((char *) &sad, 0, sizeof(sad));
	sad.sin_family = AF_INET;
	sad.sin_addr.s_addr = INADDR_ANY;
	sad.sin_port = htons((u_short) port);
	if(bind(welcomeSocket, (struct sockaddr *) &sad, sizeof(sad))==-1){
		printf(stderr, "Failed to bind Socket");
		return errno;
	}

	if (listen(welcomeSocket, WAITING) < 0)
		printf("Too many clients attempting to connect");
	clilen = sizeof(cad);

	char buffer[128];
	strcpy(buffer, "Hello!");

	while (1)
	{	
		connectionSocket = accept(welcomeSocket, (struct sockaddr *) &cad, &clilen);
		if(clientListSize > USERS){
			fprintf(stderr, "Connection full, request rejected...\n");
			close(connectionSocket);
			continue;
		}else{
			fprintf(stdout, "CONNECTION ACCEPTED!\n");
			struct UserInfo user;
			info->sockfd = connectionSocket;
			pthread_mutex_lock(&clientListMutex);
			add_user(userList, user);
			pthread_mutex_unlock(&clientListMutex);
			pthread_create(&user.thread_num, NULL, client_handler, (void *) &user);
		}
	}

	close(welcomeSocket);

	return 0;
}

void * user_handler(void * user){
	struct UserInfo * info = (struct UserInfo *)user;
	struct UserNode * node;
	int recBytes, sentBytes;
	bool userNameChosen = false;

	char name[NAMELEN];
	char buffer[BUFFSIZE];
	//Get user's chosen name, see if it's already taken
	//If it is, then we keep checking until they give one that isn't taken
	while(!userNameChosen){
		//Get name from server
		recBytes = recv(info->sockfd, name, sizeof(name));

		//Go through the list of clients
		pthread_mutex_lock(&clientListMutex);
		for(node = UserList.head; node != NULL; node = node->next){
			//If the name is already taken, try again
			//(note:) Send taken to the user, so the client
				//Will know to prompt the user for a new name
			if(strcmp(curr->userInfo->name, name) == 0){
				strcpy(buffer, "taken");
				Send(info->sockfd, buffer);
				pthread_mutex_unlock(&clientListMutex);
				continue;
			}
		}
		//Add UserName to User Information, Notify client
		info->name = name;
		strcpy(buffer, "success");
		Send(info->sockfd, buffer);
		pthread_mutex_unlock(&clientListMutex);
		userNameChosen = true;
	}

	while(1){
		recBytes = recv(info->sockfd, buffer, sizeof(buffer));
		if(!strcmp(buffer, "/quit")){
			delete_user(&userList, info);
			break;
		}else if(buffer[0] == '/'){
			execute_command(info, &buffer[1]);
		}else{
			pthread_mutex_lock(&clientListMutex);
			for(node = UserList.head; node != NULL; node = node->next;){
				Send(node->sockfd, buffer);
			}
			pthread_mutex_unlock(&clientListMutex);
		}
	}

	close(info->sockfd);
	return NULL;

}

void execute_command(UserInfo info, char * buffer){
	char * command = strdup(buffer);
	char * token = strtok(command, " ");
	
	//EXPECTED INPUT - /roll 3d10
	if(!strcmp(token, "roll")){
		//ROLL BABY ROLL
		char * nextToken = strtok(NULL, " ");
		if(nextToken != NULL){
			-char buffer2[BUFFSIZE];
			buffer2[0] = "\0";
			char * c_numDice = strtok(NULL, "d");
			char * c_numOnDice = strtok(NULL, "d");

			int numDice, numOnDice;
			if(c_numDice != NULL && (numDice = atoi(c_numDice)) != 0){
				if(c_numOnDice != NULL && (numOnDice = atoi(c_numOnDice)) != 0){
					int i, total, curr = 0;
					//Show the dice rolls
					for(i = 1; i <= numDice; i++){
						curr = rand()%numOnDice;
						strcat(buffer2, "Roll ");
						strcat(buffer2, itoa(i));
						strcat(buffer2, ": ");
						strcat(buffer2, itoa(rand()%curr));
						strcat(buffer2, "\n");
						total += curr;
					}
					//Print out the total
					strcat(buffer2, "Total: ");
					strcat(buffer2, itoa(total));
					strcat(buffer2, "\n");

					//Send the result
					Send(info->sockfd, buffer2);
				}
			}
	
		}
	//EXPECTED INPUT /whisper bob Hello
	}else if(!strcmp(token, "whisper")){
		//...whispering...
		
		char * whispName = strtok(NULL, " ");
		if(whispName != NULL){	
			for(node = UserList.head; node != NULL; node = node->next){
				//If the name is actually there, send them the 
					//message, otherwise, send them 
					//that they failed miserably
				if(!strcmp(curr->userInfo->name, whispName)){
					char * message = strtok(NULL, " ");
					if(message != NULL){
						Send(curr->fd, message);
					}
					break;
				}
			}
		}
	}else if(!strcmp(token, "me")){
		//Just prepending client's name to message
		char message[BUFFSIZE];
		message[0] = "\0";
		strcat(message, info->name);
		strcat(message, " ");
		char * temp = strtok(NULL, " " ):
		if(temp != NULL){
			strcat(message, temp);
			if(message != NULL){
				Send(info->sockfd, message);
			}
	}
}

void Send(int sockfd, char * buffer){
	if(send(sockfd, buffer, sizeof(buffer), 0) < 0){
		printf(stderr, "Error sending information");
	}
}

void add_user(UserNode head, UserInfo user){
	UserNode curr = head;

	if(curr->userInfo == NULL){
		curr->userInfo = user;
	}else{
		while(curr->next != NULL){
			curr = curr->next;
		}
		curr->next = (UserNode *)malloc(sizeof(UserNode));
		curr = curr->next;
		curr->userInfo = user;
		userListSize--;
	}
}

void remove_user(UserNode * head, UserInfo user){
	UserNode * curr = head;

	if(curr->userInfo->sockfd = user->sockfd){
		UserNode temp = curr;
		head = curr->next;
		free(temp);
		userList = head;
	}
	
	UserNode * prev = head;
	curr = head->next;

	while(curr != NULL){
		if(curr->userInfo->sockfd == user->sockfd){
			prev->next = curr->next;
			userListSize++;
			free(curr);
			break;
		}
		prev = prev->next;
		curr = curr->next;
	}
}
