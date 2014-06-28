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
#define BUFFSIZE 128
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

void Send(int sockfd, char * buffer); //Wrapper for send method
//Add user to list
void add_user(struct UserNode * node, struct UserInfo * info); 
//Remove user from user list
void remove_user(struct UserNode * node, struct UserInfo * delInfo);
//current size of user list
int userListSize;
//head of list of users
struct UserNode  * userList;
//Mutex for locking list when it is being manipulated
pthread_mutex_t clientListMutex;
//Function pointer (Function that thread is using to handle user input)
void * user_handler(void * user);
//Helper function to execute server functions
void execute_command(struct UserInfo * user, char * cmd);

int main()
{
	//Information for initial socket
	struct sockaddr_in sad;
	struct sockaddr_in cad;
	int welcomeSocket, connectionSocket, clilen;
	
	//Initialize user list
	userList = (struct UserNode *)malloc(sizeof(struct UserNode));
	userList->next = NULL;
	userList->userInfo = NULL;
	
	//Randomize the random number generator
	srand(time(NULL));

	char * host = (char *)malloc(sizeof(char)*BUFFSIZE);
	host[0] = '\0';
	gethostname(host, BUFFSIZE);
	printf("%s\n", host);
	//Port for chat program
	int port = 9034;

	//Initialize socket that is listening for users
	if((welcomeSocket = socket(PF_INET, SOCK_STREAM, 0)) == -1){
		fprintf(stderr, "Failed to make Socket\n");
		return errno;
	}

	//Set address to 0
	memset((char *) &sad, 0, sizeof(sad));

	//Settings for the socket
	sad.sin_family = AF_INET;
	sad.sin_addr.s_addr = INADDR_ANY;
	sad.sin_port = htons((u_short) port);

	//Bind the socket (make it use the port given)
	if(bind(welcomeSocket, (struct sockaddr *) &sad, sizeof(sad))==-1){
		fprintf(stderr, "Failed to bind Socket\n");
		return errno;
	}

	//Listen for clients
	if (listen(welcomeSocket, WAITING) < 0)
		printf("Too many clients attempting to connect\n");
	
	//length of client address
	clilen = sizeof(cad);

	while (1)
	{	
		//Continue accepting users that connect
		connectionSocket = accept(welcomeSocket, (struct sockaddr *) &cad, &clilen);
		//Disconnect user if there are too many Connected
		if(userListSize > USERS){
			fprintf(stderr, "Connection full, request rejected...\n");
			close(connectionSocket);
			continue;
		}else{
			fprintf(stdout, "CONNECTION ACCEPTED!\n");
			//Create user for the spawning thread
			struct UserInfo * user = (struct UserInfo *)malloc(sizeof(struct UserInfo));
			user->sockfd = connectionSocket;
			//Lock the mutex, because we are adding a client to the list
			pthread_mutex_lock(&clientListMutex);
			add_user(userList, user);
			pthread_mutex_unlock(&clientListMutex);

			//Spawn a thread to handle user output
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

	char name[BUFFSIZE] = {0};
	char buffer[BUFFSIZE] = {0};
	//Get user's chosen name, see if it's already taken
	//If it is, then we keep checking until they give one that isn't taken
	while(!userNameChosen){
		//Get name from server
		recBytes = recv(info->sockfd, name, BUFFSIZE, 0);
		name[recBytes - 1] = '\0';

		//Go through the list of clients
		pthread_mutex_lock(&clientListMutex);
		for(node = userList; node != NULL; node = node->next){
			//If the name is already taken, try again
			//(note:) Send taken to the user, so the client
				//Will know to prompt the user for a new name
			if(strcmp(node->userInfo->name, name) == 0){
				Send(info->sockfd, "t");
				pthread_mutex_unlock(&clientListMutex);
				userNameTaken = true;
				break;
			}
		}
		//Add UserName to User Information, Notify client
		if(!userNameTaken){
			info->name = name;
			Send(info->sockfd, "s");
			pthread_mutex_unlock(&clientListMutex);
			userNameChosen = true;
		}else{
			userNameTaken = false;
		}
	}
	
	char * intro = (char *)malloc(sizeof(char) * (BUFFSIZE + strlen(" joined the chatroom!")));
	strcpy(intro, info->name);
	strcat(intro, " joined the chatroom!\n");

	//Send welcome to all users
	pthread_mutex_lock(&clientListMutex);
	for(node = userList; node != NULL; node = node->next){
		if (strcmp(name, node->userInfo->name) != 0)
			Send(node->userInfo->sockfd, intro);
	}
	pthread_mutex_unlock(&clientListMutex);

	free(intro);
	
	//Recieve and parse input from the client until they disconnect
	while(1){
		
		//Recieive input from the client 
		recBytes = recv(info->sockfd, buffer, sizeof(buffer),0);
	

		//Check to see if the client disconnected	
		if(recBytes < 1){
			printf("Connection lost from %s\n", info->name);
			pthread_mutex_lock(&clientListMutex);
			//could need up to BUFF_SIZE for the name (including '\0'), plus the length of the
			//extra information
			char * remMsg = (char *)malloc(sizeof(char)* (BUFFSIZE + strlen(" disconnected from the chat room\n")));
			strcpy(remMsg, info->name);
			strcat(remMsg, " disconnected from the chat room\n");
			remove_user(userList, info);
			struct UserNode * node;
			for(node = userList; node != NULL; node = node->next){
				Send(node->userInfo->sockfd, remMsg);
			}
			pthread_mutex_unlock(&clientListMutex);
			break;
		}
		//If it's quit command, exit
		if(!strcmp(buffer, "/quit")){
			//Lock mutex, because we're removing a user
			pthread_mutex_lock(&clientListMutex);
			remove_user(userList, info);
			pthread_mutex_unlock(&clientListMutex);
			break;
			//If it's a command, execute it
		}else if(buffer[0] == '/'){
			execute_command(info, &buffer[1]);
		//Else, send to all users
		}else{
			//Lock the mutex, because we're traversing the user List
			pthread_mutex_lock(&clientListMutex);
			for(node = userList; node != NULL; node = node->next ){
				char * userMessage = (char *) malloc(sizeof(char)*BUFFSIZE);
				userMessage[0] = '\0';
				//Send the user's message -> put their name in the front so
				//other users will know who sent it
				strcat(userMessage, info->name);
				strcat(userMessage, ": ");
				strcat(userMessage, buffer);
				Send(node->userInfo->sockfd, userMessage);
				free(userMessage);
			}
			pthread_mutex_unlock(&clientListMutex);
		}
		//Reset the buffer
		memset(buffer, 0, sizeof(buffer));
	}

	//User disconnected, so we close the socket
	printf("Say goodbye to %d\n", info->sockfd);
	close(info->sockfd);
	return NULL;

}

void execute_command(struct UserInfo * info, char * buffer){
	char * command = strdup(buffer);
	char * token = strtok(command, " ");
	
	//EXPECTED INPUT - /roll 3d10
	if(!strcmp(token, "roll")){
		printf("ROLL BABY ROLL\n");
		char buffer2[BUFFSIZE];
		buffer2[0] = '\0';
		//because the numbers are separated by d, get the two tokens
		char * c_numDice = strtok(NULL, "d");
		char * c_numOnDice = strtok(NULL, "d");
		int numDice, numOnDice;

		//parse the two numbers separated by d -> if they aren't numbers,
		//don't send anything
		if(c_numDice != NULL && (numDice = atoi(c_numDice)) != 0){
			if(c_numOnDice != NULL && (numOnDice = atoi(c_numOnDice)) != 0){
				int i = 0, total = 0, curr = 0;
				//Strings holding the value of the die and random number
				char * istr = malloc(sizeof(char)*3);
				char * cRand = malloc(sizeof(char)*3);
				//Show the dice rolls
				for(i = 1; i <= numDice; i++){
					//Number between 1 and the sides of the dice the user input
					curr = (rand()%numOnDice)+1;
					//Prettify the output
					strcat(buffer2, "Roll ");
					sprintf(istr, "%d", i);
					strcat(buffer2, istr);
					strcat(buffer2, ": ");
					sprintf(cRand, "%d", curr);
					strcat(buffer2, cRand);
					strcat(buffer2, "\n");
					total += curr;
				}
				//Print out the total
				strcat(buffer2, "Total: ");
				//String holder for total number
				char * cTotal = malloc(sizeof(char)*5);
				sprintf(cTotal, "%d", total);
				strcat(buffer2, cTotal);
				strcat(buffer2, "\n");
				//Print the Dice rolls to the server -> for debugging purposes
				printf("Dice Rolls\n%s\n", buffer2);
				//Send the result to all users	
				struct UserNode * node;
				pthread_mutex_lock(&clientListMutex);
				for(node = userList; node != NULL; node = node->next ){
					printf("Sending rolls to: %s\n", node->userInfo->name);
					Send(node->userInfo->sockfd, buffer2);
				}
				pthread_mutex_unlock(&clientListMutex);
				//Free the allocated memory
				free(cTotal);
				free(istr);
				free(cRand);
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
					char * message = (char *) malloc(sizeof(char)*BUFFSIZE);
					message[0] = '\0';
					char * temp;
				  	while((temp	= strtok(NULL, " ")) != NULL){
						strcat(message, temp);
						strcat(message, " ");
					}
					free(temp);
			
					if(message != NULL){
						//Expected Output -> "(User) Whispers: (message)"
						char * userMessage = (char *)malloc(sizeof(char)*BUFFSIZE);
						userMessage[0] = '\0';
						strcat(userMessage, info->name);
						strcat(userMessage, " Whispers: ");
						strcat(userMessage, message);
						strcat(userMessage, "\n");
						Send(node->userInfo->sockfd, userMessage);
						free(userMessage);
					}
					break;
					free(message);
				}
			}
		}
	}else if(!strcmp(token, "me")){
		//Just prepending client's name to message
		char message[BUFFSIZE];
		message[0] = '\0';
		strcat(message, info->name);
		strcat(message, " ");
		char * temp;
		//Get the rest of the action
 		while( (temp = strtok(NULL, " " )) != NULL){
			strcat(message, temp);
			strcat(message, " ");
		}
		strcat(message, "\n");
		if(message != NULL){
				//Send the action to all users currently on the server
				pthread_mutex_lock(&clientListMutex);
				struct UserNode * node;
				for(node = userList; node != NULL; node = node->next ){
					Send(node->userInfo->sockfd, message);
				}
				pthread_mutex_unlock(&clientListMutex);
		}
	}
}

void Send(int sockfd, char * buffer){
	if(send(sockfd, buffer, strlen(buffer), 0) < 0){
		fprintf(stderr, "Error sending information\n");
	}
}

void add_user(struct UserNode * head, struct UserInfo * user){
	printf("ADDING USER\n");
	struct UserNode * curr = head;
	user->name = "Anonymous";
	
	if(curr == NULL){
		curr = (struct UserNode *)malloc(sizeof(struct UserNode));
		curr->userInfo = user;
		userList = curr;
	}else if(curr->userInfo == NULL){
		curr->userInfo = user;
	}else{
		while(curr->next != NULL){
			curr = curr->next;
		}
		curr->next = (struct UserNode *)malloc(sizeof(struct UserNode));
		curr = curr->next;
		curr->userInfo = user;
	}
	userListSize++;
}

void remove_user(struct UserNode * head, struct UserInfo * user){
	struct UserNode * curr = head;

	if(curr->userInfo->sockfd == user->sockfd){
		printf("Removing user: %s\n", curr->userInfo->name);
		struct UserNode * temp = curr;
		head = curr->next;
		free(temp);
		userList = head;
		return;
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
