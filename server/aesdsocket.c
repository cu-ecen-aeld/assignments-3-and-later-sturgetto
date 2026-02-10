#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <fcntl.h>

#ifndef USE_AESD_CHAR_DEVICE
#define USE_AESD_CHAR_DEVICE 1
#endif

char buffer[2048];
#ifdef USE_AESD_CHAR_DEVICE
int outfile = -1;
#else
FILE * outfile = NULL;
#endif
pthread_mutex_t iomtx;
int sockfd = -1, peerfd = -1;
timer_t gTimerid;

#ifndef USE_AESD_CHAR_DEVICE
void start_timer(void)
{
    struct itimerspec value;

    value.it_value.tv_sec = 1;
    value.it_value.tv_nsec = 0;

    value.it_interval.tv_sec = 10;
    value.it_interval.tv_nsec = 0;

    timer_create (CLOCK_REALTIME, NULL, &gTimerid);

    timer_settime (gTimerid, 0, &value, NULL);
}
#endif

typedef struct _thread_info
{
    pthread_t thread;
    int peerfd;
    bool finished;
    struct _thread_info *next;
} *pthread_info, thread_info;

pthread_info thread_start = NULL, thread_end = NULL;

static void handler(int signo)
{
    if (sockfd != -1) close(sockfd);
    if (peerfd != -1) close(peerfd);
#ifdef USE_AESD_CHAR_DEVICE
    if (outfile != -1) close(outfile);
#else
    if (outfile != NULL) fclose(outfile);
#endif
    signo = signo;
    printf("Exiting...");
    exit(0);
}

void * aesdsocket_thread(void * arg)
{
    pthread_info pti = (pthread_info)arg;
    int bytesrcvd = 0, retval;
    // acquire mutex
    pthread_mutex_lock(&iomtx);
#ifdef USE_AESD_CHAR_DEVICE
    // Open a file for append
    outfile = open("/dev/aesdchar", O_WRONLY);
    if (outfile == -1)
#else
    outfile = fopen("/var/tmp/aesdsocketdata", "a");
    if (outfile == NULL)
#endif
    {
        syslog(LOG_ERR, "Unable to output device for writing.");
        close(pti->peerfd);
        pti->finished = true;
        // release mutex
        pthread_mutex_unlock(&iomtx);
        return NULL;
    }
    // receive data on the socker ans write to temp file.
    do 
    {
        bytesrcvd = recv(pti->peerfd, buffer, 1024, 0);
#ifdef USE_AESD_CHAR_DEVICE
        retval = write(outfile, buffer, bytesrcvd);
#else
        retval = fwrite(buffer, 1, bytesrcvd, outfile);
#endif
        if (retval != bytesrcvd)
        {
            syslog(LOG_ERR, "Unable to write all data to output device.");
            close(pti->peerfd);
	    pti->finished = true;
            // release mutex
            pthread_mutex_unlock(&iomtx);
            return NULL;
        }
    } while (bytesrcvd == 1024);
#ifdef USE_AESD_CHAR_DEVICE
    close(outfile);
    outfile = -1;
#else
    fclose(outfile);
    outfile = NULL;
#endif
    // Allow other threads to write to the file
    // release mutex
    pthread_mutex_unlock(&iomtx);

    // Prevent other threads from writing while reading.
    // acquire mutex
    pthread_mutex_lock(&iomtx);
    // Transmit file back out.
#ifdef USE_AESD_CHAR_DEVICE
    // Open a file for append
    outfile = open("/dev/aesdchar", O_RDONLY);
    if (outfile == -1)
#else
    outfile = fopen("/var/tmp/aesdsocketdata", "r");
    if (outfile == NULL)
#endif
    {
        syslog(LOG_ERR, "Unable to open temp file for reading.");
        close(pti->peerfd);
	pti->finished = true;
        // release mutex
        pthread_mutex_unlock(&iomtx);
        return NULL;
    }
    do
    {
#ifdef USE_AESD_CHAR_DEVICE
        bytesrcvd = read(outfile, buffer, 1024);
#else
        bytesrcvd = fread(buffer, 1, 1024, outfile);
#endif
        retval = send(pti->peerfd, buffer, bytesrcvd, 0);
        if (retval != bytesrcvd)
        {
            syslog(LOG_ERR, "Unable to send all data.");
            close(pti->peerfd);
	    pti->finished = true;
            // release mutex
            pthread_mutex_unlock(&iomtx);
            return NULL;
        }
    } while (bytesrcvd == 1024);
#ifdef USE_AESD_CHAR_DEVICE
    close(outfile);
    outfile = -1;
#else
    fclose(outfile);
    outfile = NULL;
#endif
    // release mutex
    pthread_mutex_unlock(&iomtx);
    close(pti->peerfd);
    return NULL;
}

#ifndef USE_AESD_CHAR_DEVICE
void timer_callback(int sig)
{
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);

    pthread_mutex_lock(&iomtx);
    int length = strftime(buffer, sizeof(buffer), "timestamp:%a, %d %b %Y %T %z", timeinfo);
    buffer[length] = '\n';
    outfile = fopen("/var/tmp/aesdsocketdata", "a");
    if (outfile == NULL)
    {
        syslog(LOG_ERR, "Unable to open temp file for writing.");
        // release mutex
        pthread_mutex_unlock(&iomtx);
        return;
    }
    // receive data on the socker ans write to temp file.
    int retval = fwrite(buffer, 1, length+1, outfile);
    if (retval != length+1)
    {
        syslog(LOG_ERR, "Unable to write all data to temp file.");
        // release mutex
    }
    fclose(outfile);
    outfile = NULL;
    pthread_mutex_unlock(&iomtx);
}
#endif

int main(int argc, char* argv[])
{
    struct sockaddr_in addr;
    struct sockaddr_in addr_peer;
    socklen_t addrlen = 0;
    struct sigaction sa;

#ifndef USE_AESD_CHAR_DEVICE
    // Deleting the file helps the full-test.sh complete.
    if (system("rm /var/tmp/aesdsocketdata") != 0)
    {
       syslog(LOG_ERR, "Unable to delete temp file,");
    }
#endif

    // Handle the command line options
    if ((argc > 1) && (strcmp("-d", argv[1]) == 0))
    {
        int pid = fork();
        if (pid < 0) {
            syslog(LOG_ERR, "Unable to make a daemon");
            exit(-1);
        }
        if (pid > 0) {
            exit(0);
        }
    }

    // Register for SIGINT and SIGTERM signals
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        syslog(LOG_ERR, "Unable to register for SIGINT.");
        return -1;
    }
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        syslog(LOG_ERR, "Unable to register for SIGTERM.");
        return -1;
    }

#ifndef USE_AESD_CHAR_DEVICE
    signal(SIGALRM, timer_callback);
    start_timer();
#endif

    // Create a TCP socket
    sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        syslog(LOG_ERR, "Unable to open socket.");
        return -1;
    }

    // Set the socket to reuse the port. Helps full-test.sh complete.
    int optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

    // Bind to port 9000 on all interfaces
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(9000);

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) 
    {
        syslog(LOG_ERR, "Unable to bind to port 9000.");
        close(sockfd);
        return -1;
    }

    // Start listening
    if (listen(sockfd, SOMAXCONN) < 0)
    {
        syslog(LOG_ERR, "Unable to listen on socket.");
        close(sockfd);
        return -1;
    }

    memset(&addr_peer, 0, sizeof(addr_peer));
    pthread_mutex_init(&iomtx, NULL);
    while (1)
    {
	// Accept connections to socket.
        peerfd = accept(sockfd, (struct sockaddr *)&addr_peer, &addrlen);
        if (peerfd < 0)
        {
            syslog(LOG_ERR, "Unable to accept connection on socket.");
            break;
        }

        char buf[INET_ADDRSTRLEN]; // >16 bytes
        if (inet_ntop(AF_INET, &addr_peer.sin_addr, buf, INET_ADDRSTRLEN) != NULL)\
       	{
            syslog(LOG_DEBUG, "Accepted connection from %s:%d", buf, ntohs(addr_peer.sin_port));
        }
       	else
       	{
            syslog(LOG_ERR, "Invalid IPv4 address");
        }

        pthread_info new_ti = malloc(sizeof(thread_info));
	new_ti->peerfd = peerfd;
	new_ti->finished = false;
	new_ti->next = NULL;

        if (pthread_create(&(new_ti->thread), NULL, aesdsocket_thread, (void*)new_ti) != 0)
	{
            syslog(LOG_ERR, "Unable to create thread for attached socket.");
	    close(peerfd);
	    break;
        }
	// Add the new thread to the tail.
	if (thread_end == NULL)
	{
            thread_start = thread_end = new_ti;
	}
	else
	{
            thread_end->next = new_ti;
            thread_end = new_ti;
	}

        // Joined finished threads
        pthread_info thread_cur, thread_prev;
	thread_prev = NULL;
	thread_cur = thread_start;
	while (thread_cur != NULL)
	{
	    if (thread_cur->finished == true)
	    {
		if (thread_prev == NULL)
		{
		     thread_start = thread_cur->next;
		     if (thread_start == NULL) thread_end = NULL;
		}
		else
		{
		     if (thread_cur->next == NULL)
		     {
			 thread_end = thread_prev;
			 thread_prev->next = NULL;
		     }
		     else
		     {
			 thread_prev->next = thread_cur->next;
		     }
		}
		void * retval;
		pthread_join(thread_cur->thread, &retval);
		free(thread_cur);
	    }
	    thread_prev = thread_cur;
	    thread_cur = thread_cur->next;
	}
    }
    close(sockfd);
    pthread_mutex_destroy(&iomtx);
    sockfd = -1;

    return 0;
}
