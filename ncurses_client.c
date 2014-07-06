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
#define SPACE_VALUE 32
#define DEL_VALUE 127

#define PORT_NUM 9034

#define BASIC_ERROR 1
#define CONNECTION_ERROR 2
#define READ_ERROR 4
#define WRITE_ERROR 8
#define PTHREAD_ERROR 16
#define OUT_OF_MEMORY_ERROR 32

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

typedef struct
{
	int alloc_size;
	char *str;
} History;

typedef struct
{
	intptr_t fd;
	WINDOW *out_window;
} ThreadArguments;

void *get_messages(void *args);
int ncurses_getline(char **output, size_t *alloced, WINDOW *in_win);
void print_commands(char cmd_str[], WINDOW *out_win);

int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		printf("Please include the URL of the server you would like to connect to as a command line argument.\n");
		return BASIC_ERROR;
	}
	
	//initialize ncurses requirements---------------------------------------
	initscr();
	cbreak();
	noecho();

	WINDOW *out_window = newwin(LINES - 1, COLS, 0, 0);
	scrollok(out_window, TRUE);
	leaveok(out_window, TRUE);

	WINDOW *in_window = newwin(1, COLS, LINES - 1, 0);
	scrollok(in_window, TRUE);
	//----------------------------------------------------------------------

	//Set up a string which will be used by another function in the program
	char commands_str[] = "\nThe commands are:\n/roll [dice]d[number] - roll [dice] number of dice with [number] sides\n/me [action] - outputs your username as performing the action\n/whisper [username] [message] - send [message] only to user [username]\n/quit - quit the program\n\n";

	//Establish the connection to the server--------------------------------
	struct sockaddr_in sad;
	int clientSocket;
	int port = PORT_NUM;
	struct hostent *ptrh;

	char host[BUFF_SIZE];
	strcpy(host, argv[1]);

	wprintw(out_window, "Connecting to server...\n");
	wrefresh(out_window);

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
		delwin(out_window);
		delwin(in_window);
		endwin();
		printf("Error: couldn't connect to host\n");
		return CONNECTION_ERROR;
	}

	wprintw(out_window, "Successfully connected to server.\n\n");
	wrefresh(out_window);
	//----------------------------------------------------------------------

	//have the user choose a username---------------------------------------
	wprintw(out_window, "What would you like your username to be? (Less than %d characters)\n", BUFF_SIZE);
	wrefresh(out_window);
	char *user_input = NULL;
	size_t size = 0;
	int chars_read = ncurses_getline(&user_input, &size, out_window);

	if (user_input == NULL)
	{
		delwin(out_window);
		delwin(in_window);
		endwin();
		printf("Error: could not allocate enough memory.\n");
		close(clientSocket);
		return OUT_OF_MEMORY_ERROR;
	}

	//get rid of the newline, or make sure the string will
	//null terminate when placed into a buffer on the server
	user_input[MIN(chars_read, BUFF_SIZE) - 1] = '\0';
	//send the desired username to the server; either write
	//all of the characters read or keep it to the maximum
	//username size
	write(clientSocket, user_input, MIN(chars_read, BUFF_SIZE));
	
	//read from the server to determine its response
	char from_server;
	if (read(clientSocket, &from_server, 1) < 0)
	{
		delwin(out_window);
		delwin(in_window);
		endwin();
		printf("Error in reading from the server.\n");
		close(clientSocket);
		return READ_ERROR;
	}

	//if the username was taken (indicated by receiving 't' from the server),
	//have the user try another
	while (from_server == 't')
	{
		wprintw(out_window, "Sorry, that username is already taken. Please try another.\n");
		wrefresh(out_window);
		chars_read = ncurses_getline(&user_input, &size, out_window);
		if (user_input == NULL)
		{
			delwin(out_window);
			delwin(in_window);
			endwin();
			printf("Error: could not allocate enough memory.\n");
			close(clientSocket);
			return OUT_OF_MEMORY_ERROR;
		}
		user_input[MIN(chars_read, BUFF_SIZE) - 1] = '\0';

		write(clientSocket, user_input, MIN(chars_read, BUFF_SIZE));
		if (read(clientSocket, &from_server, 1) < 0)
		{
			delwin(out_window);
			delwin(in_window);
			endwin();
			printf("Error in reading from the server.\n");
			close(clientSocket);
			return READ_ERROR;
		}
	}
	char username[BUFF_SIZE];
	strcpy(username, user_input);
	//----------------------------------------------------------------------

	wprintw(out_window, "\nWelcome to the chatroom, %s! You may now begin chatting. (Type \"/cmd\" to list commands.)\n", username);
	wrefresh(out_window);

	//create the thread for receiving messages from the server--------------
	ThreadArguments args;
	args.fd = clientSocket;
	args.out_window = out_window;
	pthread_t msg_thread;
	if (pthread_create(&msg_thread, NULL, get_messages, &args))
	{
		delwin(out_window);
		delwin(in_window);
		endwin();
		printf("Error: could not create pthread.\n");
		free(user_input);
		int tmp = strlen("/quit\n");
		write(clientSocket, &tmp, sizeof(int));
		write(clientSocket, "/quit\n", tmp);
		close(clientSocket);
		return PTHREAD_ERROR;
	}
	//thread isn't joined because it *should* terminate when main finishes
	//----------------------------------------------------------------------

	//get the user's messages and write them to the server------------------
	do
	{
		//read the user's input, store the number of characters read
		chars_read = ncurses_getline(&user_input, &size, in_window);

		if (user_input == NULL)
		{
			pthread_kill(msg_thread);
			delwin(in_window);
			delwin(out_window);
			endwin();
			printf("Error: couldn't allocate enough memory.\n");
			pthread_kill(msg_thread);
			close(clientSocket);
			return OUT_OF_MEMORY_ERROR;	
		}

		//determine if they want the commands printed
		if (strcmp(user_input, "/cmd\n") == 0)
		{
			print_commands(commands_str, out_window);
		}
		else
		{
			//if not, write the user's input to the server; note that, because
			//getline is used, it includes the trailing '\n'
			//first inform the server how long the message will be...
			write(clientSocket, &chars_read, sizeof(int));
			//...and then send the message
			write(clientSocket, user_input, chars_read);
		}
	} while (strcmp(user_input, "/quit\n") != 0);
	//----------------------------------------------------------------------

	//clean up--------------------------------------------------------------
	free(user_input);
	//make sure to kill the thread before closing socket so that this thread
	//isn't switched out for the other and data read from the server
	pthread_kill(msg_thread);
	delwin(out_window);
	delwin(in_window);
	close(clientSocket);
	endwin(); //end ncurses
	//----------------------------------------------------------------------

	return 0;
}

void *get_messages(void *args)
{
	//get the value of the shared file descriptor for the clientSocker
	ThreadArguments *args_p = (ThreadArguments *) args;
	int clientSocket = args_p->fd;
	WINDOW *out_win = args_p->out_window;

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
		wprintw(out_win, "%s", from_server);
		wrefresh(out_win);
	}
}

//Returns the number of characters read, including the newline
//but ignoring the null terminator. Could potentially changed
//the passed *output pointer by reallocing. Sets *output to
//NULL on out of memory errors, after freeing. Note that this
//means there will be no way to recover information from
//user input upon out of memory errors. Note that the number
//of characters read before running out of memory will always
//be returned though
int ncurses_getline(char **output, size_t *alloced, WINDOW *in_win)
{
	int alloc_size = *alloced;

	//if output == NULL, ignore the claimed *alloced size
	if (NULL == *output)
	{
		alloc_size = BUFF_SIZE;
		*output = malloc(alloc_size * sizeof(char));
		if (NULL == *output)
		{
			*alloced = 0;
			return 0;
		}
	}
	else if (alloc_size == 1)
	{
		//if the alloc_size is only 1, there's not enough room for the
		//required '\0' and '\n', so realloc to at least a size of two
		alloc_size++;
		*output = realloc(*output, alloc_size * sizeof(char));
	}
	
	int needed_size = 2; //one for '\n' and one for '\0'

	char input;
	while ((input = wgetch(in_win)) != '\n')
	{
		if ((input == BACKSPACE) || (input == DEL_VALUE))
		{
			if (needed_size > 2)
			{
				needed_size--;
				int x, y;
				getyx(in_win, y, x);
				mvwdelch(in_win, y, x - 1);
			}
		}
		else if ((input >= SPACE_VALUE) && (input < DEL_VALUE))
		{
			//after determing the input is a regular character, continue
			needed_size++;
			//realloc by factor of two if more size needed to prevent
			//fragmentation, if possible; because needed_size is incremented
			//at most by 1 every iteration, it is safe to only realloc once,
			//i.e., needed_size will never be more than twice as big as
			//alloc_size
			if (needed_size > alloc_size)
			{
				alloc_size *= 2;
				char *tmp = realloc(*output, alloc_size * sizeof(char));
				if (NULL == tmp)
				{
					free(*output);
					*alloced = 0;
					return needed_size - 1;
				}
				*output = tmp;
			}
			(*output)[needed_size - 3] = input;
			wprintw(in_win, "%c", input);
		}
		wrefresh(in_win);
	}

	(*output)[needed_size - 2] = '\n';
	(*output)[needed_size - 1] = '\0';

	wprintw(in_win, "\n");
	wrefresh(in_win);

	*alloced = alloc_size;
	//subtract one for the null terminator
	return needed_size - 1;
}

void print_commands(char cmd_str[], WINDOW *out_win)
{
	wprintw(out_win, "%s", cmd_str);
	wrefresh(out_win);
}
