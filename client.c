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
	int *cont;
} ThreadArguments;

void *get_messages(void *args);
void print_commands();
void execute_command(const char const *username, const char const *input, const int clientSocket);
int get_num_digits(int i);

int connect_to_server(char *server, int *clientSocket);
int get_username(int clientSocket, int *length, char **username);
int create_receiving_thread(int clientSocket, int *cont_var, pthread_t *msg_thread);
int send_messages(const int clientSocket, const char const *username, const int username_length);

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

int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		print_error_message(INPUT_ERROR);
		return INPUT_ERROR;
	}
	
	srand(time(NULL));
	int error = 0;

	//Establish the connection to the server--------------------------------
	int clientSocket;
	if ((error = connect_to_server(argv[1], &clientSocket)) != 0) 
	{
		print_error_message(error);
		return error;
	}
	//----------------------------------------------------------------------

	//have the user choose a username---------------------------------------
	char *initial_username;
	int username_length;
	if ((error = get_username(clientSocket, &username_length, &initial_username)) != 0)
	{
		close(clientSocket);
		print_error_message(error);
		return error;
	}

	char username[BUFF_SIZE] = {0};
	strncpy(username, initial_username, BUFF_SIZE - 1);
	free(initial_username);
	//----------------------------------------------------------------------

	printf("\nWelcome to the chatroom, %s! You may now begin chatting. (Type \"/cmd\" to list commands.)\n", username);

	//create the thread for receiving messages from the server--------------
	int *thread_continue = malloc(sizeof(int));
	*thread_continue = 1;
	pthread_t msg_thread;
	if ((error = create_receiving_thread(clientSocket, thread_continue, &msg_thread)) != 0)
	{
		close(clientSocket);
		print_error_message(error);
		return error;
	}
	//----------------------------------------------------------------------

	//get the user's messages and write them to the server------------------
	error = send_messages(clientSocket, username, username_length);
	//----------------------------------------------------------------------

	//clean up--------------------------------------------------------------
	//make sure to kill the thread before closing socket so that this thread
	//isn't switched out for the other and data read from the serveri
	print_error_message(error);
	*thread_continue = 0;
	pthread_join(msg_thread, NULL);
	free(thread_continue);
	close(clientSocket);
	//----------------------------------------------------------------------

	return 0;
}

int connect_to_server(char *host, int *clientSocket)
{
	printf("Connecting to server...\n");

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

	printf("Successfully connected to server.\n");

	return 0;
}

int get_username(int clientSocket, int *length, char **username)
{
	printf("What would you like your username to be? (Less than %d characters)\n", BUFF_SIZE);
	char *user_input = NULL;
	size_t size = 0;
	int chars_read = getline(&user_input, &size, stdin);

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
	char from_server;
	if (read(clientSocket, &from_server, 1) < 0)
	{
		free(user_input);
		return READ_ERROR;
	}

	//if the username was taken (indicated by receiving 't' from the server),
	//have the user try another
	while (from_server == 't')
	{
		printf("Sorry, that username is already taken. Please try another.\n");
		chars_read = getline(&user_input, &size, stdin);
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
	return 0;	
}

int create_receiving_thread(int clientSocket, int *cont_var, pthread_t *msg_thread)
{
	ThreadArguments *args = malloc(sizeof(ThreadArguments));
	args->fd = clientSocket;
	args->cont = cont_var;
	if (pthread_create(msg_thread, NULL, get_messages, args))
	{
		return PTHREAD_ERROR;
	}
	//thread isn't joined because it *should* terminate when main finishes
}

int send_messages(const int clientSocket, const char const *username, const int username_length)
{
	char *user_input = NULL;
	size_t size = 0;
	int chars_read = 0;

	do
	{
		//read the user's input, store the number of characters read
		chars_read = getline(&user_input, &size, stdin);

		if (user_input == NULL)
		{
			return OUT_OF_MEMORY_ERROR;	
		}

		//determine if they want the commands printed
		if (strcmp(user_input, "/cmd\n") == 0)
		{
			print_commands();
		}
		else if (strcmp(user_input, "/quit\n") == 0)
		{
			break;
		}
		else if (user_input[0] == '/')
		{
			execute_command(username, &user_input[1], clientSocket);
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

	return 0;
}

void *get_messages(void *args)
{
	//get the value of the shared file descriptor for the clientSocker
	ThreadArguments *args_p = (ThreadArguments *) args;
	int clientSocket = args_p->fd;
	int *cont = args_p->cont;
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
			printf("%s", from_server);
		}
	}

	pthread_exit(0);
}

void execute_command(const char const *username, const char const *input, const int clientSocket)
{
	const int WHISPER = WHISPER_INDICATOR;
	int input_len = strlen(input);
	if (input_len < 3)
	{
		printf("That is not a valid command.\n");
		return;
	}

	if (strncmp(input, "me", 2) == 0)
	{
		if (input[3] == '\0')
			return;
		
		//username + ' ' + message
		int message_size= strlen(username) + strlen(&input[3]) + 1;
		//message size plus '\0'
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
			printf("Correct usage of roll command: /roll [dice]d[sides]\n");
			return;
		}

		int num_dice = atoi(s_num_dice);
		int num_sides = atoi(s_num_sides);
		if ((num_dice <= 0) || (num_sides <= 0))
		{
			free(dice_info);
			printf("Correct usage of roll command: /roll [dice]d[sides]\n");
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

		message_size -= 1;
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
			printf("Correct usage: /whisper [user] [message]\n");
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
		printf("That is not a valid command.\n");
	}

}

void print_commands()
{
	printf("\nThe commands are:\n/roll [dice]d[number] - roll [dice] number of dice with [number] sides\n/me [action] - outputs your username as performing the action\n/whisper [username] [message] - send [message] only to user [username]\n/quit - quit the program\n\n");
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
