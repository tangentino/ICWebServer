#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
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


char * errorMessage(char* buf, char* msg) {
    // Error response
    sprintf(buf, 
        "HTTP/1.1 %s\r\n"
        "Server: ICWS\r\n"
        "Connection: close\r\n\r\n",
        msg
    );
    return buf;
}

char * responseMessage(char* buf, unsigned long size, char* mime) {
    sprintf(buf,
        "HTTP/1.1 200 OK\r\n"
        "Server: ICWS\r\n"
        "Connection: close\r\n"
        "Content-Length: %lu\r\n"
        "Content-Type: %s\r\n\r\n",
        size, mime
    );
    return buf;
}

char * getMIMEType(char* extension) {
    // Convert file extension to MIME
    if (strcmp(extension,"html") == 0) {
        return "text/html";
    }
    else if (strcmp(extension,"css") == 0) {
        return "text/css";
    }
    else if (strcmp(extension,"plain") == 0) {
        return "text/plain";
    }
    else if (strcmp(extension,"javascript") == 0 || strcmp(extension,"js") == 0) {
        return "text/javascript";
    }
    else if (strcmp(extension,"jpg") == 0 || strcmp(extension,"jpeg") == 0) {
        return "image/jpg";
    }
    else if (strcmp(extension,"png") == 0) {
        return "image/png";
    }
    else if (strcmp(extension,"gif") == 0) {
        return "image/gif";
    }
    return "";
}

void respond(int connFd, char* url, char* mime, char* method) {

    char buf[MAXBUF];

    if ((strcasecmp(method,"GET") != 0) && strcasecmp(method,"HEAD") != 0) {
        // Method is not GET or HEAD
        errorMessage(buf, "501 Method Unimplemented");
        write_all(connFd, buf, strlen(buf));

    }
    else {
        int urlFd = open(url, O_RDONLY);
        
        if (urlFd < 0) {
            errorMessage(buf, "404 File Not Found");
            write_all(connFd, buf, strlen(buf));
            return;
        }
        struct stat fstatbuf;
        fstat(urlFd, &fstatbuf);
        responseMessage(buf,fstatbuf.st_size,mime);
        write_all(connFd, buf, strlen(buf));

        if (strcasecmp(method,"GET") == 0) {
            // If method is GET server also returns content body
            ssize_t numRead;
            while ((numRead = read(urlFd, buf, MAXBUF)) > 0) {
                write_all(connFd, buf, numRead);
            }
        }

        close(urlFd);
    }
}

void serveHttp(int connFd, char *rootFolder) {

    char buf[MAXBUF];

    Request *request = parse(buf,MAXBUF,connFd);

    char method[MAXBUF];
    char url[MAXBUF];
    //char version[MAXBUF] = request->http_version;

    char * fileExtension;
    char * mime;
    
    strcpy(method,request->http_method);
    
    // Url is <root>/<uri>
    strcpy(url,rootFolder);
    strcat(url,request->http_uri);

    fileExtension = strrchr(url,'.'); // Get file extension in url
    fileExtension++; // Increment pointer

    mime = getMIMEType(fileExtension);

    respond(connFd, url, mime, method);
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

    while (1) {
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
    loop();
    return 0;
}
