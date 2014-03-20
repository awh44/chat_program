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

int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		printf("Please include the URL of the server you would like to connect to as a command line argument.\n");
		return 1;
	}

	//Establish the connection to the server--------------------------------
	struct sockaddr_in sad;
	int clientSocket;
	int port = PORT_NUM;
	struct hostent *ptrh;

	char host[BUFFER_SIZE];
	strcpy(host, argv[1]);

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
		//close(clientSocket);
		return 5;
	}

	printf("Successfully connected to server.\n\n");
	//----------------------------------------------------------------------


	//have the user choose a username---------------------------------------
	printf("What would you like your username to be? (No spaces, < 128 characters)\n");
	char username[BUFFER_SIZE];
	scanf("%s", username);
	//send the desired username to the server
	write(clientSocket, username, sizeof(username));
	
	char from_server[BUFFER_SIZE];
	//read from the server to determine its response
	if (read(clientSocket, from_server, sizeof(from_server)) < 0)
	{
		printf("Error in reading from the server.\n");
		close(clientSocket);
		return 15;
	}
	//if the username was taken (indicated by receiving "t" from the server),
	//have the user try another
	while (strcmp(from_server, "t") == 0)
	{
		printf("Sorry, that username is already taken. Please try another.\n");
		scanf("%s", username);
		write(clientSocket, username, sizeof(username));
		if (read(clientSocket, from_server, sizeof(from_server)) < 0)
		{
			printf("Error in reading from the server.\n");
			close(clientSocket);
			return 15;
		}
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
		//read the user's input, store the number of characters read
		chars_read = getline(&user_input, &chars_read, stdin);
		//determine if they want the command printed
		if (strcmp(user_input, "/cmd\n") == 0)
		{
			print_commands();
		}
		else
		{
			//if not, write the user's input to the server; note that, because
			//getline is used, it includes the trailing '\n'
			write(clientSocket, user_input, chars_read);
		}
	} while (strcmp(user_input, "/quit\n") != 0);
	//----------------------------------------------------------------------

	//clean up--------------------------------------------------------------
	free(user_input);
	//make sure to kill the thread before closing socket so that this thread
	//isn't switched out for the other and data read from the server
	pthread_kill(msg_thread);
	close(clientSocket);
	//----------------------------------------------------------------------

	return 0;
}

void *get_messages(void *args)
{
	//get the value of the shared file descriptor for the clientSocker
	int clientSocket = (intptr_t) args;

	//continuously read from the server and print out what is received;
	//because TCP is being used, it is assured that that all data will be
	//received in the correct order; because no formatting is done, data is
	//only read and then printed it, it does not matter how many bytes are
	//read on each iteration, just that they are printed
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
	printf("/roll [dice]d[number] - roll [dice] number of [dice] with [number] sides\n");
	printf("/me [action] - outputs your username as performing the action\n");
	printf("/whisper [username] [message] - send [message] only to user [username]\n");
	printf("/quit - quit the program\n");
}
