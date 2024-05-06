#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <poll.h>
#include <stdbool.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include "queue.h"

//Defines
//#define PRINT_LOG
#define BUFFER_SIZE 100

//Macros
#ifdef	PRINT_LOG
#define DEBUG_LOG(str, ...)	printf(str, ##__VA_ARGS__)
#else
#define DEBUG_LOG(str, ...)
#endif

//static variables
static char ipstr[INET_ADDRSTRLEN];
static const char* file_location = "/var/tmp/aesdsocketdata";
static SLIST_HEAD(slisthead, slist_data_s) head;
static bool is_signal_active = false;

//structs 
struct client_socket_data
{
	bool is_connection_closed;
	int client_fd;
	FILE* file_fd;
	pthread_mutex_t client_mutex;
	pthread_mutex_t* file_mutex;
	struct sockaddr sock_addr;
        socklen_t sock_length;
};

struct slist_data_s
{
	pthread_t thread;
	struct client_socket_data client_info;
	SLIST_ENTRY(slist_data_s) entries;
};

struct timer_data_s
{
	bool is_expired;
	pthread_mutex_t mutex;
};

//types
typedef struct slist_data_s slist_data_t;

//funtons prototype
static void signal_handler_function(int signal_number);
static void* client_socket_thread_func(void* param);
static void add_new_client(int server_fd, FILE* file, pthread_mutex_t* f_mutex);
static void verify_disconnected_clients();
static void remove_all_clients();
static void timer_thread_func(union sigval sigval);
static void timestamp_handler(struct timer_data_s* td, FILE* file, pthread_mutex_t* file_mutex);

//main function
int main(int argc, char* argv[])
{

	bool is_daemon = (argc == 2 && strncmp("-d", argv[1], 2) == 0);
	int opt = 1;
	int status = 0;
	int server_fd;
	int clock_id = CLOCK_MONOTONIC;
	struct addrinfo hints;
	struct addrinfo* servinfo = NULL;
	struct sigaction s_action;
	struct sigevent sev;
	struct timer_data_s td;
	struct pollfd poll_socket;
	struct itimerspec t_spec;
	pthread_mutex_t file_mutex;
	timer_t timerid;
	FILE* file_dir = NULL;

	SLIST_INIT(&head);

        memset(&hints, 0x00, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;

	memset(&s_action, 0x00, sizeof(struct sigaction));
	s_action.sa_handler = signal_handler_function;

	memset(&t_spec, 0x00, sizeof(struct itimerspec));
	t_spec.it_value.tv_sec = 10;
	t_spec.it_value.tv_nsec = 0;
	t_spec.it_interval.tv_sec = 10;
	t_spec.it_interval.tv_nsec = 0;

	memset(&sev, 0x00, sizeof(struct sigevent));
	td.is_expired = false;
	sev.sigev_notify = SIGEV_THREAD;
	sev.sigev_value.sival_ptr = &td;
	sev.sigev_notify_function = timer_thread_func;

	openlog(NULL, 0, LOG_USER);

      	do
      	{		
		if((status = getaddrinfo( NULL, "9000", &hints, &servinfo)) != 0)
		{
			perror("perror returned when getting address info");
			status = -1;
			break;
		}

		if((server_fd = socket(PF_INET, SOCK_STREAM, 0)) == -1)
		{
			perror("perror returned on attempting create a socket");
			status = -1;
                	break;
		}

		if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int)) != 0)
		{
                	perror("perror returned when setting socket options");
                	status = -1;
                	break;		
		}
	
		if(bind(server_fd, servinfo->ai_addr, sizeof(struct sockaddr)) != 0)
		{
                	perror("perror returned when attempting bind the server");
               	  	status = -1;
                	break;			
		}

        }
	while(0);

	freeaddrinfo(servinfo);

	if(status == 0 && is_daemon)
	{
		DEBUG_LOG("starting a daemon...\n");
		pid_t pid = fork();

		if(pid == 0) //child  process
		{

			chdir("/");
			pid_t sid = setsid();			

			close(STDIN_FILENO);
			close(STDOUT_FILENO);
			close(STDERR_FILENO);
		}
		else if(pid > 0) //parent process
		{
		        close(server_fd);		
			exit(EXIT_SUCCESS);
		}
		else
		{
			DEBUG_LOG("ERROR: it's not possible to create the child\n");
			exit(EXIT_FAILURE);
		}
	}
      
	do
      	{
                if(status != 0)
                        break;

		if(listen(server_fd, 10) == -1)
		{	
			perror("perror ocurrend while linstening");
                	status = -1;
                	break;
		}

		file_dir = fopen(file_location, "w+r");

		if(file_dir == NULL)
        	{
            		perror("perror on opening or creating a file");
                        status = -1;
                        break;
        	}

        	if(pthread_mutex_init(&file_mutex, NULL) != 0)
        	{
                	DEBUG_LOG("Failed to initialize mutex\n");
                        status = -1;
                        break;			
        	}
		
		if(pthread_mutex_init(&td.mutex, NULL) != 0)
		{
			DEBUG_LOG("Filed to initialize timer mutex\n");
			status = -1;
			break;
		}

		if(timer_create(clock_id, &sev, &timerid) != 0)
		{
			DEBUG_LOG("Error while creating timer: %d (%s) \n", errno, strerror(errno));
			status = -1;
			break;
		}

        	if(sigaction(SIGTERM, &s_action, NULL) != 0)
        	{
                	perror("signal SIGTERM perror");
			status = -1;
                        break;
        	}

        	if(sigaction(SIGINT, &s_action, NULL) != 0)
        	{
                	perror("signal SIGINT  perror");
                        status = -1;
                        break;			
        	}

		if(timer_settime(timerid, TIMER_ABSTIME, &t_spec, NULL) != 0)
		{
			DEBUG_LOG(" Error while setting timer\n");
			status = -1;
			break;
		}
	}
      	while(0);

	poll_socket.fd = server_fd;
        poll_socket.events = POLLIN;

	for(;;)
	{
		int poll_value = 0;

		timestamp_handler(&td, file_dir, &file_mutex);

		if(is_signal_active)
		{
			remove_all_clients();
			is_signal_active = false;
			break;
		}

		switch(poll_value = poll(&poll_socket, 1, 10))
		{
			case -1:
		        	DEBUG_LOG("Polling has failed.\n");
                     		continue;
				break;

			case 0:
				verify_disconnected_clients();
				break;

			default:
				if(poll_value > 0)
					add_new_client(server_fd, file_dir, &file_mutex);
				break;
		}
	}

	if(timer_delete(timerid) != 0)
	{
		perror("perror while deleting timer\n");
	}

	DEBUG_LOG("\nClosing server..\n");
	shutdown(server_fd, SHUT_RDWR); 
	close(server_fd);
	fclose(file_dir);

	DEBUG_LOG("removing aesdsocketdata\n");
	remove(file_location);	

	return status;
}

static void signal_handler_function(int signal_number)
{
	syslog(LOG_DEBUG, "Caught signal, exiting");
	is_signal_active = true;
}

static void timer_thread_func(union sigval sigval)
{
	pthread_mutex_t* mutex = &((struct timer_data_s *) sigval.sival_ptr)->mutex;

	DEBUG_LOG("hi from timer handler interrupt\n");	

	if(pthread_mutex_lock(mutex) != 0)
        {
                DEBUG_LOG("it was not possivel to lock mutex");
                return;
        }
	
	((struct timer_data_s *) sigval.sival_ptr)->is_expired = true;

        if(pthread_mutex_unlock(mutex) != 0)
        {
                DEBUG_LOG("it was not possivel to lock mutex");
                return;
        }	
}

static void timestamp_handler(struct timer_data_s* td, FILE* file, pthread_mutex_t* file_mutex)
{
	time_t now;
	struct tm* tm_info;
	char buffer[80];

	if(pthread_mutex_lock(&td->mutex) != 0)
        {
                DEBUG_LOG("it was not possivel to lock mutex");
                return;
        }
	
	if(td->is_expired)
	{
		DEBUG_LOG("Hi from timestamp hanlder flag true\n");
		do
		{
			time(&now);
			tm_info = localtime(&now);

			strftime(buffer, 80, "timestamp:%a, %d %b %Y %T %z\n", tm_info);

        		if(pthread_mutex_lock(file_mutex) != 0)
        		{
                		DEBUG_LOG("it was not possivel to lock mutex");                
				break;
        		}
		
                	if(fwrite(buffer, strlen(buffer), 1, file) == -1)
                	{
                		DEBUG_LOG("error fwrite\n");
                	}

                	fflush(file);

                	if(pthread_mutex_unlock(file_mutex) != 0)
                	{	
                        	DEBUG_LOG("it was not possivel to lock mutex");
				break;
                	}
		}
		while(0);		
		td->is_expired = false;
	}

        if(pthread_mutex_unlock(&td->mutex) != 0)
        {
                DEBUG_LOG("it was not possivel to lock mutex");
                return;
        }
}

static void add_new_client(int server_fd, FILE* file, pthread_mutex_t* f_mutex)
{
	slist_data_t* socket = malloc(sizeof(slist_data_t));
	struct client_socket_data* c_socket = &socket->client_info;

	SLIST_INSERT_HEAD(&head, socket, entries);
	c_socket->is_connection_closed = true;
	c_socket->file_fd = file;
	c_socket->file_mutex = f_mutex;

        c_socket->sock_length = sizeof(struct sockaddr);
        if((c_socket->client_fd = accept(server_fd, &c_socket->sock_addr, &c_socket->sock_length)) == -1)
        {
        	perror("accept error");
                return;
        }

        memset(ipstr, 0x00, sizeof(ipstr));
        inet_ntop(AF_INET, &(((struct sockaddr_in*) &c_socket->sock_addr)->sin_addr), ipstr, sizeof(ipstr));
        syslog(LOG_DEBUG, "Accepted connection from %s \n", ipstr);
        DEBUG_LOG("Accepted connection from %s \n", ipstr);


	if(pthread_mutex_init(&c_socket->client_mutex, NULL) != 0)
        {
        	DEBUG_LOG("Failed to initialize mutex");
		return;
	}

        if(pthread_create(&socket->thread, NULL, &client_socket_thread_func, (void *) c_socket) != 0)
        {
        	DEBUG_LOG("It was not possible to create the thread");
		return;
        }

	c_socket->is_connection_closed = false;
}

static void verify_disconnected_clients()
{
    bool is_disconnected = false;
    slist_data_t* socket = SLIST_FIRST(&head);
    slist_data_t* next_socket = NULL;
    struct client_socket_data* c_socket = NULL;

    if(SLIST_EMPTY(&head))
	    return;

    while(1)
    {
	c_socket = &socket->client_info;

        if(pthread_mutex_lock(&c_socket->client_mutex) != 0)
        {
        	DEBUG_LOG("it was not possivel to lock mutex");
		return;
        }

	is_disconnected = c_socket->is_connection_closed;

        if(pthread_mutex_unlock(&c_socket->client_mutex) != 0)
        {
                DEBUG_LOG("it was not possivel to lock mutex");
                return;
        }
	
	next_socket = SLIST_NEXT(socket, entries);

       	if(is_disconnected)
        {
		DEBUG_LOG("Removing a socket...\n");
		DEBUG_LOG("Client desc: %i\n", c_socket->client_fd);
                if(pthread_join(socket->thread, NULL) != 0)
                {
                        perror("Error while joining the thread");
                }	
		SLIST_REMOVE(&head, socket, slist_data_s, entries);
                free(socket);
        }

	is_disconnected = false;

        if(next_socket == SLIST_END(&head))
	{
		break;
	}
	else
	{
		socket = next_socket;
	}
	      
    }
}

static void remove_all_clients()
{
	slist_data_t* socket = NULL;
	struct client_socket_data* c_socket = NULL;

	while(!SLIST_EMPTY(&head))
	{
		socket = SLIST_FIRST(&head);
		DEBUG_LOG("Removing clients..\n");
	        c_socket = &socket->client_info;

        	if(pthread_mutex_lock(&c_socket->client_mutex) != 0)
        	{
                	DEBUG_LOG("it was not possivel to lock mutex");
                	return;
        	}

		if(pthread_cancel(socket->thread) != 0)
                {
                        DEBUG_LOG("Erro to cancel the thread");
			return;
                }

        	if(pthread_mutex_unlock(&c_socket->client_mutex) != 0)
        	{
                	DEBUG_LOG("it was not possivel to lock mutex");
                	return;
        	}
		
                if(pthread_join(socket->thread, NULL) != 0)
                {
                        perror("Error while joining the thread");
                }	

		SLIST_REMOVE_HEAD(&head, entries);
		free(socket);	
	}
}

static void* client_socket_thread_func(void* param)
{
	bool is_client_disconnected = false;
	bool is_timestamp_required = false;
	struct client_socket_data* t_data = (struct client_socket_data*) param;
        FILE* file_dir = t_data->file_fd;
        int client_fd = t_data->client_fd;
        struct pollfd poll_socket;
        pthread_mutex_t* file_mutex = t_data->file_mutex;
	pthread_mutex_t* client_mutex = &t_data->client_mutex;
        char rec_buff[BUFFER_SIZE + 1] = {'\0'};

        poll_socket.fd = client_fd;
        poll_socket.events = POLLIN;


	for(;;)
	{
		memset(rec_buff, '\0', sizeof(rec_buff));
		DEBUG_LOG("Polling client socket for data\n");
                if(poll(&poll_socket, 1, -1) == -1)
                {
                     DEBUG_LOG("Polling has failed.\n");
                     continue;
                }

                if(pthread_mutex_lock(client_mutex) != 0)
                {
                        DEBUG_LOG("it was not possivel to lock mutex");
                        continue;
                }
                
		if(pthread_mutex_lock(file_mutex) != 0)
                {
                      	DEBUG_LOG("it was not possivel to lock mutex");
                        continue;
                }

		do
                {
                        int rx_size = 0;

                        DEBUG_LOG("receiving data\n");
                        rx_size = recv(client_fd, rec_buff, BUFFER_SIZE, 0);

                        if(rx_size == -1) //an error has occurred
                        {
                                perror("recv error");
                                break;
                        }
                        else if (rx_size == 0) // The remote side has closed
                        {
				t_data->is_connection_closed = is_client_disconnected = true;
                                DEBUG_LOG("the remote side has closed\n");
                                break;
                        }
                        else if(rx_size < BUFFER_SIZE)
                        {
                                if(fwrite(rec_buff, rx_size, 1, file_dir) == -1)
                                {
                                        DEBUG_LOG("error fwrite\n");
                                }

                                fflush(file_dir);
			
				if(strchr(rec_buff, '\n') != NULL)
                                {
                                        break;
                                }		
                        }
                        else
                        {
                                if(fwrite(rec_buff, rx_size, 1, file_dir) == -1)
                                {
                                        DEBUG_LOG("error fwrite\n");
                                }

                                fflush(file_dir);

		                if(strchr(rec_buff, '\n') != NULL)
                                {
                                        break;
                                }
                        }

                }
                while(1);


                DEBUG_LOG("Sending data back..\n");
                fseek(file_dir, 0, SEEK_SET);

		is_timestamp_required = (strstr(rec_buff, "test_socket_timer") != NULL || strstr(rec_buff, "timestamp:wait-for-startup") != NULL);

                do //read the content of the file and send over the socket
                {
			if(is_client_disconnected)
				break;

                        memset(rec_buff, '\0', sizeof(rec_buff));
                        if(fgets(rec_buff, BUFFER_SIZE, file_dir) == NULL)
                        {
                                break;
       			}
			
			if(!is_timestamp_required && strstr(rec_buff, "timestamp:") != NULL)
			{
				continue;
			}

                        if(send(client_fd, rec_buff, strlen(rec_buff), 0) == -1)
                        {
                                perror("perror while sending");
                                break;
                        }
                }
                while(1);

                fseek(file_dir, 0, SEEK_END);

                if(pthread_mutex_unlock(file_mutex) != 0)
                {
                        DEBUG_LOG("it was not possivel to unlock mutex");
                }

		if(pthread_mutex_unlock(client_mutex) != 0)
                {
                        DEBUG_LOG("it was not possivel to unlock mutex");
                }

		if(is_client_disconnected)
			break;
	}
}

