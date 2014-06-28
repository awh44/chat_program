#include <ncurses.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>

#define BUFF_SIZE 128
#define BACKSPACE 7
#define SPACE_VALUE 32
#define DEL_VALUE 127

int ncurses_getline(char **output, size_t *alloced);

int main(int argc, char *argv[])
{
	initscr();
	cbreak();
	keypad(stdscr, TRUE);
	scrollok(stdscr, TRUE);
	noecho();

	char *input = NULL;
	size_t size = 0;
	int chars_read = ncurses_getline(&input, &size);

	while (strcmp(input, "quit\n") != 0)
	{
		printw("%s", input);
		refresh();
		chars_read = ncurses_getline(&input, &size);
	}

	free(input);

	endwin(); //end ncurses

	return 0;
}

int ncurses_getline(char **output, size_t *alloced)
{
	int alloc_size = *alloced;

	//if output == NULL, ignore the claimed *alloced size
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
	else if (alloc_size == 1)
	{
		//if the alloc_size is only 1, there's not enough room for the
		//required '\0' and '\n', so realloc to at least a size of two
		alloc_size++;
		*output = realloc(*output, alloc_size * sizeof(char));
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
