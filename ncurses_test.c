#include <ncurses.h>
#include <stdlib.h>

#define BUFF_SIZE 64;
#define BACKSPACE 7

char *ncurses_getline();

int main()
{
	initscr();
	cbreak();
	keypad(stdscr, TRUE);
	
	noecho();
	while (1)
	{
		char *input = ncurses_getline();
		printw("%s\n", input);
		refresh();
	}

	getch();	
	
	
	endwin();

	return 0;
}

char *ncurses_getline()
{
	int alloc_size = BUFF_SIZE;
	int needed_size = 2; //one for '\n' and one for '\0'
	char *retVal = malloc(alloc_size * sizeof(char));

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
				retVal = realloc(retVal, alloc_size * sizeof(char));
			}
			retVal[needed_size - 3] = input;
			printw("%c", input);
		}
		refresh();
	}

	retVal[needed_size - 2] = '\n';
	retVal[needed_size - 1] = '\0';

	printw("\n\n");
	refresh();

	return retVal;
}
