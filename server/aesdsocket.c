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

char buffer[2048];
FILE * outfile = NULL;
int sockfd = -1, peerfd = -1;

static void handler(int signo)
{
    if (sockfd != -1) close(sockfd);
    if (peerfd != -1) close(peerfd);
    if (outfile != NULL) fclose(outfile);
    signo = signo;
    printf("Exiting...");
    exit(0);
}

int main(int argc, char* argv[])
{
    int retval = 0;
    struct sockaddr_in addr;
    struct sockaddr_in addr_peer;
    socklen_t addrlen = 0;
    struct sigaction sa;

    // Deleting the file helps the full-test.sh complete.
    if (system("rm /var/tmp/aesdsocketdata") != 0)
    {
       syslog(LOG_ERR, "Unable to delete temp file,");
    }

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

    while (1)
    {
	// Accept connections to socket.
        peerfd = accept(sockfd, (struct sockaddr *)&addr_peer, &addrlen);
        if (peerfd < 0)
        {
            syslog(LOG_ERR, "Unable to accept connection on socket.");
            close(sockfd);
            return -1;
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


        int bytesrcvd = 0;
        // Open a file for append
        outfile = fopen("/var/tmp/aesdsocketdata", "a");
        if (outfile == NULL)
	{
            syslog(LOG_ERR, "Unable to open temp file for writing.");
            close(peerfd);
            close(sockfd);
            return -1;
	}
	// receive data on the socker ans write to temp file.
        do 
	{
            bytesrcvd = recv(peerfd, buffer, 1024, 0);
            retval = fwrite(buffer, 1, bytesrcvd, outfile);
	    if (retval != bytesrcvd)
	    {
                syslog(LOG_ERR, "Unable to write all data to temp file.");
                close(peerfd);
                close(sockfd);
                return -1;
	    }
	} while (bytesrcvd == 1024);
	fclose(outfile);

        outfile = NULL;
	// Transmit file back out.
        outfile = fopen("/var/tmp/aesdsocketdata", "r");
	if (outfile == NULL)
	{
            syslog(LOG_ERR, "Unable to open temp file for reading.");
            close(peerfd);
            close(sockfd);
            return -1;
	}
	do
	{
	    bytesrcvd = fread(buffer, 1, 1024, outfile);
	    retval = send(peerfd, buffer, bytesrcvd, 0);
	    if (retval != bytesrcvd)
	    {
                syslog(LOG_ERR, "Unable to send all data.");
                close(peerfd);
                close(sockfd);
                return -1;
	    }

	} while (bytesrcvd == 1024);
	fclose(outfile);
	outfile = NULL;
        close(peerfd);
	peerfd = -1;
    }
    close(sockfd);
    sockfd = -1;

    return 0;
}
