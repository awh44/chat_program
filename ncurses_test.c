#include <ncurses.h>
#include <stdlib.h>

#define BUFF_SIZE 64;
#define BACKSPACE 7

int ncurses_getline(char **output, size_t *alloced);

int main()
{
	initscr();
	cbreak();
	keypad(stdscr, TRUE);
	
	noecho();
	while (1)
	{
		char *line = NULL;
		size_t size = 0;
		int chars_read = ncurses_getline(&line, &size);
		printw("%s\n", line);
		refresh();
	}	
	
	endwin();

	return 0;
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
