Chat Program
======

The files included in this repository, made up of a server, two implementations of a client, and some housekeeping items, are for a chat system written in C. After the server application begins running, up to ten users can use the the URL for the server to connect with unique usernames and begin chatting with one another using the client programs, using plain-text or some built-in commands, such as rolling dice or "whispering" to one another.

The client program allows for simultaneous input and output by making use of pthreads. An ncurses text-based user interface has been implemented as well. Though improvements can continue to be made to this interface, it right now allows for a clean and clear seperation between input and output. (And though I have heard that ncurses can have problems with pthreads, I have not yet run into that with my testing; I will have to see once I test with someone else and try truly simultaneous input and output.) Finally, the data sent between the server and the clients will also eventually be encrypted, probably using the RSA algorithm.
