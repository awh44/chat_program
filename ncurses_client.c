#include <ncurses.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

#define BUFF_SIZE 128
#define BACKSPACE 7
#define PORT_NUM 9034

void *get_messages(void *args);
int ncurses_getline(char **output, size_t *alloced);
void print_commands(char cmd_str[]);

int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		printf("Please include the URL of the server you would like to connect to as a command line argument.\n");
		return 1;
	}

	initscr();
	cbreak();
	keypad(stdscr, TRUE);
	scrollok(stdscr, TRUE);
	
	noecho();

	//Set up a string which will be used by another function in the program
	char commands_str[] = "\nThe commands are:\n/roll [dice]d[number] - roll [dice] number of dice with [number] sides\n/me [action] - outputs your username as performing the action\n/whisper [username] [message] - send [message] only to user [username]\n/quit - quit the program\n\n";

	//Establish the connection to the server--------------------------------
	struct sockaddr_in sad;
	int clientSocket;
	int port = PORT_NUM;
	struct hostent *ptrh;

	char host[BUFF_SIZE];
	strcpy(host, argv[1]);

	printw("Connecting to server...\n");
	refresh();

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
		endwin();
		printf("Error: couldn't connect to host\n");
		return 5;
	}

	printw("Successfully connected to server.\n\n");
	refresh();
	//----------------------------------------------------------------------

	//have the user choose a username---------------------------------------
	printw("What would you like your username to be? (Less than 128 characters)\n");
	refresh();
	char *user_input = NULL;
	size_t size = 0;
	int chars_read = ncurses_getline(&user_input, &size);
	user_input[chars_read - 1] = '\0';
	//send the desired username to the server
	write(clientSocket, user_input, chars_read);
	
	//read from the server to determine its response
	char from_server;
	if (read(clientSocket, &from_server, 1) < 0)
	{
		endwin();
		printf("Error in reading from the server.\n");
		close(clientSocket);
		return 15;
	}

	//if the username was taken (indicated by receiving 't' from the server),
	//have the user try another
	while (from_server == 't')
	{
		printw("Sorry, that username is already taken. Please try another.\n");
		refresh();
		chars_read = ncurses_getline(&user_input, &size);

		write(clientSocket, user_input, chars_read);
		if (read(clientSocket, &from_server, 1) < 0)
		{
			endwin();
			printf("Error in reading from the server.\n");
			close(clientSocket);
			return 15;
		}
	}
	char username[128];
	strcpy(username, user_input);
	//----------------------------------------------------------------------

	printw("\nWelcome to the chatroom, %s! You may now begin chatting. (Type \"/cmd\" to list commands.)\n", username);
	refresh();
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
	do
	{
		//read the user's input, store the number of characters read
		chars_read = ncurses_getline(&user_input, &size);
		//determine if they want the commands printed
		if (strcmp(user_input, "/cmd\n") == 0)
		{
			print_commands(commands_str);
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
	endwin(); //end ncurses
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
		char from_server[BUFF_SIZE + 1];
		int bytes_read = read(clientSocket, from_server, BUFF_SIZE);
		if (bytes_read >= 0)
		{
			from_server[bytes_read] = '\0';
		}
		printw("%s", from_server);
		refresh();
	}
}

int ncurses_getline(char **output, size_t *alloced)
{
	int alloc_size = *alloced;
	//char *retVal = malloc(alloc_size * sizeof(char));

	if (NULL == *output)
	{
		alloc_size = BUFF_SIZE;
		*output = malloc(alloc_size * sizeof(char));
		if (NULL == *output)
		{
			alloc_size = 0;
			return 0;
		}
	}
	
	int needed_size = 2; //one for '\n' and one for '\0'

	char input;
	while ((input = getch()) != '\n')
	{
		if (input == BACKSPACE)
		{
			if (needed_size > 2)
			{
				needed_size--;
				int x, y;
				getyx(stdscr, y, x);
				mvdelch(y, x - 1);
			}
		}
		else
		{
			needed_size++;
			//realloc by factor of two if more size needed to prevent
			//fragmentation, if possible; because needed_size is incremented
			//at most by 1 every iteration, it is safe to only realloc once,
			//i.e., needed_size will never be more than twice as big as
			//alloc_size
			if (needed_size > alloc_size)
			{
				alloc_size <<= 1;
				*output = realloc(*output, alloc_size * sizeof(char));
			}
			(*output)[needed_size - 3] = input;
			printw("%c", input);
		}
		refresh();
	}

	(*output)[needed_size - 2] = '\n';
	(*output)[needed_size - 1] = '\0';

	printw("\n");
	refresh();

	*alloced = alloc_size;
	//subtract one for the null terminator
	return needed_size - 1;
}

void print_commands(char cmd_str[])
{
	printw("%s", cmd_str);
	refresh();
}
