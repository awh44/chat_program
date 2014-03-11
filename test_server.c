#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>

int main()
{
	struct sockaddr_in sad;
	struct sockaddr_in cad;
	int welcomeSocket, connectionSocket, clilen;
	
	int port = 9034;

	welcomeSocket = socket(PF_INET, SOCK_STREAM, 0);
	memset((char *) &sad, 0, sizeof(sad));
	sad.sin_family = AF_INET;
	sad.sin_addr.s_addr = INADDR_ANY;
	sad.sin_port = htons((u_short) port);
	bind(welcomeSocket, (struct sockaddr *) &sad, sizeof(sad));

	if (listen(welcomeSocket, 10) < 0)
		printf("Couldn't listen on socket");
	clilen = sizeof(cad);

	char buffer[128];
	strcpy(buffer, "Hello!");

	while (1)
	{	
		connectionSocket = accept(welcomeSocket, (struct sockaddr *) &cad, &clilen);

		//used to test the test on the client side for taken usernames
		int taken = 1;
		while (taken)
		{
			char username[128];
			read(connectionSocket, username, sizeof(username));
			printf("%s\n", username);
			if (strcmp(username, "taken_name") == 0)
			{
				strcpy(buffer, "taken");
				write(connectionSocket, buffer, sizeof(buffer));
			}
			else
			{
				taken = 0;
				strcpy(buffer, "success");
				write(connectionSocket, buffer, sizeof(buffer));
			}
		}
		//---------------------------------------------------------------
		
		//used to test reading from the server on client side
		printf("Testing writing. Type /quit to move on to test reading from client\n");
		int chars_read = 128;
		char *input = (char *) malloc(chars_read * sizeof(char));
		do
		{
			chars_read = getline(&input, &chars_read, stdin);
			write(connectionSocket, input, chars_read);
		} while (strcmp(input, "/quit\n") != 0);
		//----------------------------------------------------

		//used to test writing to the server on client side 
		chars_read = read(connectionSocket, buffer, sizeof(buffer));
		buffer[chars_read] = '\0';
		while (strcmp(buffer, "/quit\n") != 0)
		{
			printf("%s\n", buffer);
			chars_read = read(connectionSocket, buffer, sizeof(buffer));
			buffer[chars_read] = '\0';
		}
		//----------------------------------------------------

		close(connectionSocket);
	}

	close(welcomeSocket);

	return 0;
}
