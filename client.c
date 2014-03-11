#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <pthread.h>

#define BUFFER_SIZE 128
#define PORT_NUM 9034

void *get_messages(void *args);
void print_commands();

int main()
{
	//Establish the connection to the server--------------------------------
	struct sockaddr_in sad;
	int clientSocket;
	int port = PORT_NUM;
	struct hostent *ptrh;

	char host[BUFFER_SIZE];
	strcpy(host, "tux64-11.cs.drexel.edu");

	printf("Connecting to server...\n");

	//set up the socket and sockaddr
	clientSocket = socket(PF_INET, SOCK_STREAM, 0);
	memset((char *) &sad, 0, sizeof(sad));
	sad.sin_family = AF_INET;
	sad.sin_port = htons((u_short) port);
	ptrh = gethostbyname(host);
	memcpy(&sad.sin_addr, (*ptrh).h_addr, (*ptrh).h_length);
	//-------------------------------
	
	if (connect(clientSocket, (struct sockaddr *) &sad, sizeof(sad)) < 0)
	{
		printf("Error: couldn't connect to host\n");
		close(clientSocket);
		return 5;
	}

	printf("Successfully connected to server.\n\n");
	//----------------------------------------------------------------------


	//have the user choose a username---------------------------------------
	printf("What would you like your username to be? (No spaces, < 128 characters)\n");
	char username[BUFFER_SIZE];
	scanf("%s", username);
	write(clientSocket, username, sizeof(username));
	
	char from_server[BUFFER_SIZE];
	read(clientSocket, from_server, sizeof(from_server));
	while (strcmp(from_server, "taken") == 0)
	{
		printf("Sorry, that username is already taken. Please try another.\n");
		scanf("%s", username);
		write(clientSocket, username, sizeof(username));
		read(clientSocket, from_server, sizeof(from_server));
	}
	//----------------------------------------------------------------------

	printf("\nWelcome to the chatroom, %s! You may now begin chatting. (Type \"/cmd\" to list commands.)\n", username);

	//create the thread for receiving messages from the server--------------
	pthread_t msg_thread;
	if (pthread_create(&msg_thread, NULL, get_messages, (void *) (intptr_t) clientSocket))
	{
		printf("Couldn't create pthread\n");
		close(clientSocket);
		return 10;
	}
	//thread isn't joined because it *should* terminate when main finishes
	//----------------------------------------------------------------------

	//get the user's messages and write them to the server------------------
	char *user_input = (char *) malloc(BUFFER_SIZE * sizeof(char));
	int chars_read = BUFFER_SIZE;
	do
	{
		chars_read = getline(&user_input, &chars_read, stdin);
		if (strcmp(user_input, "/cmd\n") == 0)
		{
			print_commands();
		}
		else
		{
			write(clientSocket, user_input, chars_read);
		}
	} while (strcmp(user_input, "/quit\n") != 0);
	//----------------------------------------------------------------------

	close(clientSocket);

	return 0;
}

void *get_messages(void *args)
{
	//get the value of the shared file descriptor for the clientSocker
	int clientSocket = (intptr_t) args;
	//continually read from the server
	while (1)
	{
		char from_server[BUFFER_SIZE + 1];
		int bytes_read = read(clientSocket, from_server, BUFFER_SIZE);
		if (bytes_read >= 0)
		{
			from_server[bytes_read] = '\0';
		}
		printf("%s", from_server);
	}
}

void print_commands()
{
	printf("The commands are:\n");
	printf("/roll [number] - roll die with [number] sides\n");
	printf("/me [action] - outputs your username as performing the action\n");
	printf("/whispter [username] [message] - send [message] only to user [username]\n");
	printf("/quit - quit the program\n");
}
