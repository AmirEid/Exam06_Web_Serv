#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/ip.h>

// copy headers, extract_message and str_join function from the main.c given in the test
// We need an int counter to assign unique IDs to each connection.
// We need a counter to calculate the max fd for select.
// we need an array to store the connections IDs and an array to store the messages for each connection.
// We need 3 fd_set variables, one for the read fds(FDs to receive messages), one for the write fds(FDs to send messages) and one for the active fds(To store active and running FDs).
// we need a buffer to store the messages received and nother to store the messages we want to send to clients.


int count, max_fd = 0;
int ids[35536];
char *msgs[3556];

fd_set read_fd, write_fd, active_fd;
char read_buff[1000];
char write_buff[42];

int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}


//function to handle all types of errors. It will print the error message and exit with the given status code.
void error_handler(char *msg, int status){
    write(2, msg, strlen(msg));
    exit (status);
}

//To create a server that is listening, we need a socket(FD) which used for communication.
//You need to read the manual of FD_SET, FD_ZERO, FD_CLR, FD_ISSET, they are functions to manipulate the fd_set variables.
//we are using the max_fd variable to keep track of the maximum fd we have, because select needs to know the range of fds to check. We will update this variable whenever we add a new fd to the active_fd set.

int create_socket(){
    max_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (max_fd == -1)
        error_handler("Fatal error\n", 1);
    FD_ZERO(&active_fd);
    FD_SET(max_fd, &active_fd);
    return (max_fd);
}

//The second phase is to create a server, you will find hints in the main.c given with this exam.
// but after we have a socket we need to bind it to an address and a port. so use bind and listen functions. for that we need a struct sockaddr_in variable
// instead of SOMAXCONN you can use any number to specify the maximum number of pending connections in the queue. in the main.c it is 10.
void create_server(int fd, struct sockaddr_in *server, int port){
    bzero(server, sizeof(*server));
    server->sin_family = AF_INET; 
	server->sin_addr.s_addr = htonl(2130706433);
	server->sin_port = htons(port);
    if ((bind(fd, (const struct sockaddr *)server, sizeof(*server))) != 0)
        error_handler("Fatal error\n", 1);
    if (listen(fd, SOMAXCONN) != 0)
        error_handler("Fatal error\n", 1);
}

//This function is used to send messages to all clients except the one that sent the message.
//FD_ISSET is used to check if the fd is in the fd_set is ready to be used(read or write). Check the manual for more details.
//always use the manual in the exam. command: man or man 2
void notify_others(char *message, int current_fd){
    for (int fd = 0; fd <= max_fd; fd++){
        if(FD_ISSET(fd, &write_fd) && fd != current_fd)
            if (send(fd, message, strlen(message), 0) == -1)
                error_handler("Fatal error\n", 1);
    }
}

//This function is used to register a new client. so we increase the max_fd number and also the count. assign IDS, AND MESSAGE.
// We add the new fd to the active set. we use sprintf to write to the buffer. then send the buffer to all the other clients.
void register_client(int client_fd){
    if (client_fd > max_fd)
        max_fd = client_fd;
    ids[client_fd] = count++;
    msgs[client_fd] = NULL;
    FD_SET(client_fd, &active_fd);
    sprintf(write_buff, "server: client %d just arrived\n", ids[client_fd]);
    notify_others(write_buff, client_fd);
}

//Same logic. we use sprintf to write to the buffer, send the buffer to the other clients.
// Remove the fd from the active set, free the message connected to that client. then close the fd.
void remove_client(int fd){
    sprintf(write_buff, "server: client %d just left\n", ids[fd]);
    notify_others(write_buff, fd);
    FD_CLR(fd, &active_fd);
    free(msgs[fd]);
    close(fd);
}

//This function is used to send the messages received from a client to all other clients.
// we extract the messages from the buffer, we prepare the buffer with  the message, then send it and free the extracted message.
// The functions you got from the main.c use malloc. so you must free.
void send_message(int fd) {
    char *message;
    while (extract_message(&(msgs[fd]), &message)){
        sprintf(write_buff, "client %d: ", ids[fd]);
        notify_others(write_buff, fd);
        notify_others(message, fd);
        free(message);
    }
}

//Logic steps:
// check arguments
// create the server socket
// to bind it you need to prepare the sockaddr_in struct with the required values. such as TCP or UDP, port, ip address, and if it is ipV4 or 6
// always use printf after each step to make sure everything works fine. it should print on the server terminal.
// sometimes it wont work on the server terminal inside the while loop because of many reasons, but one of them is the server does not get the time to print
// now you make all the fd sets euqal inside the loop. so they get updated every itiration.
// now using select to monitor the fds. select will monitor read_fd to check if there is a new connection or a message to read, and monitor write_fd to check if there is a message to send.
// the other 2 NULL one is for handling exceptions, the other is for timeout.
// now we loop over all the fds to check which one is ready.
// the if condition is vital. because if the fd is not ready, FD_ISSET will return 0 and we will continue to check the next fd.
// this way we will not have problem with the fds that are not ready. we basically skip iterations for the fds that are not ready.
// next there are 2 cases. if the fd is the same as the server fd, it means a new connection is coming.
// if they are not equal (fd and server fd), it means there is a message to read or maybe a client disconnected.
// use accept function to accept the new connection. use recv function to read messages from the client or check if a client disconnected.

int main(int argc, char **argv) {
    if (argc != 2)
        error_handler("Wrong number of arguments\n", 1);
    int server_fd = create_socket();
    struct sockaddr_in server;
    create_server(server_fd, &server, atoi(argv[1]));
    printf("server _fd %d\n", server_fd);
    while(1){
        read_fd = write_fd = active_fd;
        if (select(max_fd + 1, &read_fd, &write_fd, NULL, NULL) == -1)
            error_handler("Fatal error\n", 1);
        for (int fd = 0; fd <= max_fd; fd++){
            if (!FD_ISSET(fd, &read_fd))
                continue;
            if (fd == server_fd){
                socklen_t len = sizeof(server);
                int client_fd = accept(server_fd, (struct sockaddr *)&server, &len);
                printf("New client connected: %d\n", client_fd);
                if (client_fd < 0)
                    error_handler("Fatal error\n", 1);
                register_client(client_fd);
                break;
            } else {
                int read_bytes = recv(fd, read_buff, 1000, 0);
                printf("message received: %s\n", read_buff);
                if (read_bytes <= 0){
                    remove_client(fd);
                    break;
                }
                read_buff[read_bytes] = '\0';
                msgs[fd] = str_join(msgs[fd], read_buff);
                send_message(fd);
            }
        }
    }
}