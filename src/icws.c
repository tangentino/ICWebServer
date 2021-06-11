#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <netdb.h>
#include "parse.h"
#include "pcsa_net.h"

#define MAXBUF 1024

typedef struct sockaddr SA;

char * port;
char * root;

void serveHttp(int connFd, char *rootFolder) {

    char buf[MAXBUF];

    Request *request = parse(buf,MAXBUF,connFd);

    char method[MAXBUF] = request->http_method;
    char url[MAXBUF] = request->http_url;
    char version[MAXBUF] = request->http_version;

    char newPath[80];

    if (strcasecmp(method,"GET") == 0) {

    }
    else {
        char *msg = "501 Method Unimplemented";
        write_all(connFd, msg, strlen(msg));
    }
}

void parseArgument(int argc, char **argv) {

    // Parsing command line argument to get port and root values
    static struct option longOptions[] = {
        {"port",required_argument,NULL,'p'},
        {"root",required_argument,NULL,'r'},
        {NULL, 0, NULL, 0}
    };

	int ch = 0;

    while ((ch = getopt_long(argc, argv, "p:r:", longOptions, NULL)) != -1) {
        switch(ch) {
            case 'p':
                port = optarg;
                break;
            case 'r':
                root = optarg;
                break;
        }
    }
}

void loop() {

    int listenFd = open_listenfd(port);

    while (true) {
        struct sockaddr_storage clientAddr;
        socklen_t clientLen = sizeof(struct sockaddr_storage);

        int connFd = accept(listenFd, (SA *) &clientAddr, &clientLen);
        if (connFd < 0) {
            fprintf(stderr, "Failed to accept\n");
            continue;
        }

        char hostBuf[MAXBUF], svcBuf[MAXBUF];

        if (getnameinfo((SA *) &clientAddr, clientLen, hostBuf, MAXBUF, svcBuf, MAXBUF, 0) == 0) {
            printf("Connection from %s:%s\n", hostBuf, svcBuf);
        }
        else {
            printf("Connection from ?UNKNOWN?\n");
        }
        serveHttp(connFd,root);
        close(connFd);
    }
}

int main(int argc, char **argv) {

    parseArgument(argc,argv);

    return 0;
}
