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

int ncurses_getline(char **output, size_t *alloced, WINDOW *in_win);

int main(int argc, char *argv[])
{
	initscr();
	cbreak();
	keypad(stdscr, TRUE);
	//scrollok(stdscr, TRUE);
	noecho();

	WINDOW *output_win = newwin(LINES - 1, COLS, 0, 0);
	scrollok(output_win, TRUE);
	leaveok(output_win, TRUE);

	WINDOW *input_win = newwin(1, COLS, LINES - 1, 0);
	scrollok(input_win, TRUE);
	char *input = NULL;
	size_t size = 0;
	int chars_read = ncurses_getline(&input, &size, input_win);

	while (strcmp(input, "quit\n") != 0)
	{
		wclear(input_win);
		wrefresh(input_win);
		wprintw(output_win, "%s", input);
		wrefresh(output_win);
		chars_read = ncurses_getline(&input, &size, input_win);
	}

	free(input);
	delwin(output_win);
	delwin(input_win);

	endwin(); //end ncurses

	return 0;
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

