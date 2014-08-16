#include <ncurses.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>

#include "TuxChatConstants.h"

#define MAX_COMMAND_LENGTH 8

#define BACKSPACE 7
#define SPACE_VALUE 32
#define DEL_VALUE 127


#define NO_ERROR 0
#define INPUT_ERROR 1
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
	int fd;
	WINDOW *out_window;
	uint8_t *cont;
} ThreadArguments;

typedef struct
{
	WINDOW *input;
	WINDOW *output;
} UserWindow;

void *get_messages(void *args);
int ncurses_getline(char **output, size_t *alloced, WINDOW *in_win);
void print_commands(WINDOW *out_win);
void execute_command(const char const *username, const char const *input, const int clientSocket, WINDOW *notification_win);
int get_num_digits(int i);

UserWindow initializeNcurses();
int connect_to_server(char *server, UserWindow window, int *clientSocket);
int get_username(int clientSocket, UserWindow window, int *length, char **username);
int create_receiving_thread(int clientSocket, UserWindow window, uint8_t *cont_var, pthread_t *msg_thread);
int send_messages(const int clientSocket, const char const *username, const int username_length, UserWindow window);

//clean up functions
void print_error_message(int error)
{
	switch (error)
	{
		case NO_ERROR:
			break;
		case INPUT_ERROR:
			printf("Please include the URL of the server you would to connect to as a command line argument.\n");
			break;
		case CONNECTION_ERROR:
			printf("Error: couldn't connect to the host.\n");
			break;
		case READ_ERROR:
			printf("Error: couldn't read from the server.\n");
			break;
		case WRITE_ERROR:
			printf("Error: couldn't write to the server.\n");
			break;
		case PTHREAD_ERROR:
			printf("Error: couldn't create a thread.\n");
			break;
		case OUT_OF_MEMORY_ERROR:
			printf("Error: couldn't allocate enough memory.\n");
			break;
		default:
			printf("An unknown error occured.\n");
			break;
	}
}
void clean_up_ncurses(UserWindow window);

int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		print_error_message(INPUT_ERROR);
		return INPUT_ERROR;
	}
	
	//initialize ncurses requirements---------------------------------------
	UserWindow window = initializeNcurses();
	//----------------------------------------------------------------------

	//Set up a string which will be used by another function in the program
	srand(time(NULL));
	int error = 0;

	//Establish the connection to the server--------------------------------
	int clientSocket;
	if ((error = connect_to_server(argv[1], window, &clientSocket)) != 0) 
	{
		clean_up_ncurses(window);
		print_error_message(error);
		return error;
	}
	//----------------------------------------------------------------------

	//have the user choose a username---------------------------------------
	char *initial_username;
	int username_length;
	if ((error = get_username(clientSocket, window, &username_length, &initial_username)) != 0)
	{
		clean_up_ncurses(window);
		close(clientSocket);
		print_error_message(error);
		return error;
	}

	char username[BUFF_SIZE] = {0};
	strncpy(username, initial_username, BUFF_SIZE - 1);
	free(initial_username);
	//----------------------------------------------------------------------

	wprintw(window.output, "\nWelcome to the chatroom, %s! You may now begin chatting. (Type \"/cmd\" to list commands.)\n", username);
	wrefresh(window.output);

	//create the thread for receiving messages from the server--------------
	uint8_t *thread_continue = malloc(sizeof(uint8_t));
	*thread_continue = 1;
	pthread_t msg_thread;
	if ((error = create_receiving_thread(clientSocket, window, thread_continue, &msg_thread)) != 0)
	{
		clean_up_ncurses(window);
		close(clientSocket);
		print_error_message(error);
		return error;
	}
	//----------------------------------------------------------------------

	//get the user's messages and write them to the server------------------
	error = send_messages(clientSocket, username, username_length, window);
	//----------------------------------------------------------------------

	//clean up--------------------------------------------------------------
	print_error_message(error);
	*thread_continue = 0;
	pthread_join(msg_thread, NULL);
	free(thread_continue);
	clean_up_ncurses(window);
	close(clientSocket);
	//----------------------------------------------------------------------

	return NO_ERROR;
}

UserWindow initializeNcurses()
{
	initscr();
	cbreak();
	noecho();

	UserWindow window;

	window.output = newwin(LINES - 1, COLS, 0, 0);
	scrollok(window.output, TRUE);
	leaveok(window.output, TRUE);

	window.input = newwin(1, COLS, LINES - 1, 0);
	scrollok(window.input, TRUE);

	return window;
}

int connect_to_server(char *host, UserWindow window, int *clientSocket)
{
	wprintw(window.output, "Connecting to server...\n");
	wrefresh(window.output);

	struct sockaddr_in sad;
	int port = PORT_NUM;
	struct hostent *ptrh;

	//set up the socket and sockaddr
	*clientSocket = socket(PF_INET, SOCK_STREAM, 0);

	if (*clientSocket < 0)
	{
		return CONNECTION_ERROR;
	}

	memset((char *) &sad, 0, sizeof(sad));
	sad.sin_family = AF_INET;
	sad.sin_port = htons((u_short) port);
	ptrh = gethostbyname(host);
	memcpy(&sad.sin_addr, (*ptrh).h_addr, (*ptrh).h_length);
	//-------------------------------

	if (connect(*clientSocket, (struct sockaddr *) &sad, sizeof(sad)) < 0)
	{
		return CONNECTION_ERROR;
	}

	wprintw(window.output, "Successfully connected to server.\n");
	wrefresh(window.output);

	return NO_ERROR;
}

int get_username(int clientSocket, UserWindow window, int *length, char **username)
{
	wprintw(window.output, "What would you like your username to be? (Less than %d characters)\n", BUFF_SIZE);
	wrefresh(window.output);
	char *user_input = NULL;
	size_t size = 0;
	int chars_read = ncurses_getline(&user_input, &size, window.output);

	if (user_input == NULL)
	{
		return OUT_OF_MEMORY_ERROR;
	}

	*length = MIN(chars_read, BUFF_SIZE);
	//get rid of the newline, or make sure the string will
	//null terminate when placed into a buffer on the server
	user_input[*length - 1] = '\0';
	//send the desired username to the server; either write
	//all of the characters read or keep it to the maximum
	//username size
	write(clientSocket, user_input, *length);
	
	//read from the server to determine its response
	char from_server ='s';
	if (read(clientSocket, &from_server, 1) < 0)
	{
		free(user_input);
		return READ_ERROR;
	}

	//if the username was taken (indicated by receiving 't' from the server),
	//have the user try another
	while (from_server == 't')
	{
		wprintw(window.output, "Sorry, that username is already taken. Please try another.\n");
		wrefresh(window.output);
		chars_read = ncurses_getline(&user_input, &size, window.output);
		if (user_input == NULL)
		{
			free(user_input);
			return OUT_OF_MEMORY_ERROR;
		}

		*length = MIN(chars_read, BUFF_SIZE);

		user_input[*length - 1] = '\0';

		write(clientSocket, user_input, *length);
		if (read(clientSocket, &from_server, 1) < 0)
		{
			return READ_ERROR;
		}
	}

	*username = user_input;
	return NO_ERROR;	
}

int create_receiving_thread(int clientSocket, UserWindow window, uint8_t *cont_var, pthread_t *msg_thread)
{
	ThreadArguments *args = malloc(sizeof(ThreadArguments));
	args->fd = clientSocket;
	args->out_window = window.output;
	args->cont = cont_var;
	if (pthread_create(msg_thread, NULL, get_messages, args))
	{
		return PTHREAD_ERROR;
	}

	return NO_ERROR;
}

int send_messages(const int clientSocket, const char const *username, const int username_length, UserWindow window)
{
	char *user_input = NULL;
	size_t size = 0;
	int chars_read = 0;

	do
	{
		//read the user's input, store the number of characters read
		chars_read = ncurses_getline(&user_input, &size, window.input);

		if (user_input == NULL)
		{
			return OUT_OF_MEMORY_ERROR;	
		}

		//determine if they want the commands printed
		if (strcmp(user_input, "/cmd\n") == 0)
		{
			print_commands(window.output);
		}
		else if (strcmp(user_input, "/quit\n") == 0)
		{
			break;
		}
		else if (user_input[0] == '/')
		{
			execute_command(username, &user_input[1], clientSocket, window.output);
		}
		else
		{
			//if not, write the user's input to the server; note that, because
			//getline is used, it includes the trailing '\n'
			//first inform the server how long the message will be...
			int to_send = chars_read + username_length + strlen(": ");
			char *message = (char *) malloc((to_send + 1) * sizeof(char));
			strcpy(message, username);
			strcat(message, ": ");
			strcat(message, user_input);

			write(clientSocket, &to_send, sizeof(int));
			//...and then send the message
			write(clientSocket, message, to_send);
			free(message);
		}
	} while (strcmp(user_input, "/quit\n") != 0);

	free(user_input);

	return NO_ERROR;
}

void *get_messages(void *args)
{
	//get the value of the shared file descriptor for the clientSocket
	//the output window, and the shared flag indicating the thread should continue
	ThreadArguments *args_p = (ThreadArguments *) args;
	int clientSocket = args_p->fd;
	WINDOW *out_win = args_p->out_window;
	uint8_t *cont = args_p->cont;
	free(args_p);

	//continuously read from the server and print out what is received;
	//because TCP is being used, it is assured that that all data will be
	//received in the correct order; because no formatting is done, data is
	//only read and then printed it, it does not matter how many bytes are
	//read on each iteration, just that they are printed
	while (*cont)
	{
		char from_server[BUFF_SIZE + 1];
		int bytes_read = recv(clientSocket, from_server, BUFF_SIZE, MSG_DONTWAIT);
		if (bytes_read >= 0)
		{
			from_server[bytes_read] = '\0';
			wprintw(out_win, "%s", from_server);
			wrefresh(out_win);
		}
	}

	pthread_exit(0);
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

void execute_command(const char const *username, const char const *input, const int clientSocket, WINDOW *notification_win)
{
	const int WHISPER = WHISPER_INDICATOR;
	int input_len = strlen(input);
	if (input_len < 3)
	{
		wprintw(notification_win, "That is not a valid command.\n");
		wrefresh(notification_win);
		return;
	}

	if (strncmp(input, "me", 2) == 0)
	{
		if (input[3] == '\0')
			return;
		
		//username + ' ' + message ('\n' is implicitly included in the message)
		int message_size= strlen(username) + strlen(&input[3]) + 1;
		char *message = (char *) malloc((message_size + 1) * sizeof(char));
		strcpy(message, username);
		strcat(message, " ");
		strcat(message, &input[3]);

		write(clientSocket, &message_size, sizeof(int));
		write(clientSocket, message, message_size);

		free(message);
	}
	else if ((input_len > 5) && (strncmp(input, "roll", 4) == 0))
	{
		if (input[5] == '\0')
			return;

		char *dice_info = strdup(&input[5]);
		char *s_num_sides = dice_info;
		char *s_num_dice = strsep(&s_num_sides, "d");
		if (s_num_sides == NULL)
		{
			free(dice_info);
			wprintw(notification_win, "Correct usage of roll command: /roll [dice]d[sides]\n");
			wrefresh(notification_win);
			return;
		}

		int num_dice = atoi(s_num_dice);
		int num_sides = atoi(s_num_sides);
		if ((num_dice <= 0) || (num_sides <= 0))
		{
			free(dice_info);
			wprintw(notification_win, "Correct usage of roll command: /roll [dice]d[sides]\n");
			wrefresh(notification_win);
			return;
		}

		int message_size = strlen(username) + strlen(" rolled  dice with  sides.\n") + get_num_digits(num_dice) + get_num_digits(num_sides) + 1;
		//char **rolls = (char **) malloc(num_dice * sizeof(char*));
		int *rolls = (int *) malloc(num_dice * sizeof(int));
		int total = 0;
		int i;
		for (i = 0; i < num_dice; i++)
		{
			int roll = rand() % num_sides + 1;
			total += roll;
			rolls[i] = roll;
			message_size += strlen("Roll : \n") + get_num_digits(i + 1) + get_num_digits(roll);
		}

		message_size += strlen("Total: \n") + get_num_digits(total);
		char *message = (char *) malloc(message_size * sizeof(char));
		sprintf(message, "%s rolled %d dice with %d sides.\n", username, num_dice, num_sides);
		for (i = 0; i < num_dice; i++)
		{
			sprintf(message, "%sRoll %d: %d\n", message, i + 1, rolls[i]);
		}
		sprintf(message, "%sTotal: %d\n", message, total);

		message_size--;
		write(clientSocket, &message_size, sizeof(int));
		write(clientSocket, message, message_size);

		free(dice_info);
		free(rolls);
		free(message);
	}
	else if ((input_len > 8) && (strncmp(input, "whisper", 7) == 0))
	{
		char *input_copy = strdup(&input[8]);
		char *message_contents = input_copy;
		char *message_recipient = strsep(&message_contents, " ");

		if (message_contents == NULL)
		{
			free(input_copy);
			wprintw(notification_win, "Correct usage: /whisper [user] [message]\n");
			wrefresh(notification_win);
			return;
		}

		write(clientSocket, &WHISPER, sizeof(int));

		int recipient_size = strlen(message_recipient);
		write(clientSocket, &recipient_size, sizeof(int));
		write(clientSocket, message_recipient, recipient_size);

		int message_size = strlen(username) + strlen("Whisper from : ") + strlen(message_contents);
		char *message = (char *) malloc((message_size + 1) * sizeof(char));
		strcpy(message, "Whisper from ");
		strcat(message, username);
		strcat(message, ": ");
		strcat(message, message_contents);

		write(clientSocket, &message_size, sizeof(int));
		write(clientSocket, message, message_size);

		free(message);
		free(input_copy);
	}
	else
	{
		wprintw(notification_win, "That is not a valid command.\n");
	}

	wrefresh(notification_win);
}

void print_commands(WINDOW *out_win)
{
	wprintw(out_win, "\nThe commands are:\n/roll [dice]d[number] - roll [dice] number of dice with [number] sides\n/me [action] - outputs your username as performing the action\n/whisper [username] [message] - send [message] only to user [username]\n/quit - quit the program\n\n");
	wrefresh(out_win);
}

int get_num_digits(int i)
{
	int length = 1;
	while ((i /= 10) != 0)
	{
		length++;
	}
	return length;
}

void clean_up_ncurses(UserWindow window)
{
	delwin(window.output);
	delwin(window.input);
	endwin();
}
