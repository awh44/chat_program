#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <pthread.h>

const int MAX_STRING_SIZE = 128;

void *get_messages(void *args);

int main()
{
	struct sockaddr_in sad;
	int clientSocket;
	struct hostent *ptrh;

	char host[MAX_STRING_SIZE];
	gethostname(host, sizeof(host));
	strcpy(host, "tux64-13.cs.drexel.edu");
	int port = 10001;

	printf("Connecting to server...\n");

	clientSocket = socket(PF_INET, SOCK_STREAM, 0);
	memset((char *) &sad, 0, sizeof(sad));
	sad.sin_family = AF_INET;
	sad.sin_port = htons((u_short) port);
	ptrh = gethostbyname(host);
	memcpy(&sad.sin_addr, (*ptrh).h_addr, (*ptrh).h_length);
	
	if (connect(clientSocket, (struct sockaddr *) &sad, sizeof(sad)) < 0)
	{
		printf("Error: couldn't connect to host\n");
		close(clientSocket);
		return 5;
	}

	
	printf("What would you like your username to be? (< 128 characters)\n");
	char username[MAX_STRING_SIZE];
	scanf("%s", username);
	write(clientSocket, username, sizeof(username));
	
	char from_server[MAX_STRING_SIZE];
	read(clientSocket, from_server, sizeof(from_server));
	while (strcmp(from_server, "taken") == 0)
	{
		printf("Sorry, that username is already taken. Please try again.\n");
		scanf("%s", username);
		write(clientSocket, username, sizeof(username));
		read(clientSocket, from_server, sizeof(from_server));
	}

	printf("Welcome to the chatroom! You may now begin chatting. (Type \"quit\" to exit.)\n");

	pthread_t msg_thread;
	if (pthread_create(&msg_thread, NULL, get_messages, (void *) (intptr_t) clientSocket))
	{
		printf("Couldn't create pthread\n");
		close(clientSocket);
		return 10;
	}

	char user_input[MAX_STRING_SIZE];
	scanf("%s", user_input);
	while (strcmp(user_input, "quit") != 0)
	{
		write(clientSocket, user_input, sizeof(user_input));
		scanf("%s", user_input);
	}

	close(clientSocket);

	return 0;
}

void *get_messages(void *args)
{
	int clientSocket = (intptr_t) args;
	char from_server[MAX_STRING_SIZE];
	read(clientSocket, from_server, sizeof(from_server));
	printf("%s\n", from_server);
}
