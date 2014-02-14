#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#define BUFFSIZE 1024
#define BACKLOG 5
#define CLIENTS 5
#define PORT 9034
#define IP_ADDRESS "127.0.0.1" 
#define MAX_COMMAND_LENGTH 16

char * process(char * message);
char * roll(char * message, int num_pos);

int main(int argc, char ** argv){
	int sockfd, newfd, err_ret;
	struct sockaddr_in serv_addr, client_addr;

	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
		fprintf(stderr, "Creating socket failed\n");
		return errno;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);
	serv_addr.sin_addr.s_addr = inet_addr(IP_ADDRESS);


	if( bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1){
		fprintf(stderr, "Error binding socket\n");
		return errno;
	}

	if( listen(sockfd, BACKLOG) == -1){
		fprintf(stderr, "Error listening\n");
		return errno;
	}

	fprintf(stdout, "Starting to listen to socket.. \n");
	
	size_t sin_size = sizeof(struct sockaddr_in);
	if((newfd = accept(sockfd, (struct sockaddr *)&client_addr,
		(socklen_t *)&sin_size)) == -1){
			fprintf(stderr, "Trouble accepting connection\n");	
			return errno;
	}else{
		fprintf(stdout, "Connection Recieved\n");
	}
	
	while(1){
			char option[100];
			int bytesRecv;

			bytesRecv = recv(newfd, option, sizeof(option), 0);

			if(bytesRecv < 0){
				char * response = process(option);
				int status = send(newfd , response, sizeof(response),0);
				if(status < 0){
					fprintf(stdout, "Error sending Message");
					return errno;
				}
			}else{
				break;
			}
	}	
		return 0;
}


char * process(char * message)
{
	if (message == NULL)
		return;

	if (message[0] != '/')
	{
		return message;
	}
	else
	{
		char command[MAX_COMMAND_LENGTH];
		int i = 1;
		while ((message[i] != ' ') && (message[i] != '\n') && (i < MAX_COMMAND_LENGTH))
		{
			command[i - 1] = message[i];
			i++;									
		}
		command[i - 1] = '\0';
		if (strcmp(command, "me") == 0)
		{
			char * retVal = (char *)malloc(sizeof(char)*(MAX_COMMAND_LENGTH+30));
			retVal[0]='\0';
			strcat(retVal, "Austin ");
			strcat(retVal, message);
			return retVal;
		}
		else if(strcmp(command, "roll") == 0)	
		{	
			roll(message, i+1);
		}	
	}
}

char * roll(char *message, int num_pos)
{
	char number[MAX_COMMAND_LENGTH];
	int j = num_pos;
	while ((message[j] != '\n') && (j < MAX_COMMAND_LENGTH))
	{	
		number[j - num_pos] = message[j];
		j++;
	}
	number[j - num_pos] = '\0';
	int num = atoi(number);
	int rand_num = (rand() % num) + 1;
	char * result = (char *)malloc(sizeof(char)*(MAX_COMMAND_LENGTH+30));
	result[0] = '\0';
	strcat(result, "Austin rolled a ");
	strcat(result, number); 
	return result;
}
