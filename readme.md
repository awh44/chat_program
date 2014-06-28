Chat Program
======

The files included in this repository, made up of a server, two implementations of a client, and some housekeeping items, are for a chat system written in C. After the server application begins running, up to ten users can use the the URL for the server to connect with unique usernames and begin chatting with one another using the client programs, using plain-text or some built-in commands, such as rolling dice or "whispering" to one another.

The client program allows for simultaneous input and output by making use of pthreads. The beginnings of an ncurses user interface have been sowed as well, with some basic I/O functions set up. The actual interface remains suboptimal for the time being, to say the least, however.
