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
#include <iostream>
#include <pthread.h>
#include <time.h>
#include <mutex>
#include <thread>
#include <string>
#include <cstring>
extern "C" {
    #include "parse.h"
    #include "pcsa_net.h"
}
#include "work_queue.cpp"

#define MAXBUF 8192

using namespace std;

typedef struct sockaddr SA;

char * port;
char * root;
int numThreads, timeout;

mutex mtx;
work_queue workQueue;

static const char* DAY_NAMES[] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static const char* MONTH_NAMES[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

char *getCurrentDateTime(){
    const int TIME = 29;
    time_t t;
    struct tm tm;
    char* buf = (char *)malloc(TIME+1);
    time(&t);
    gmtime_r(&t, &tm);
    strftime(buf, TIME+1, "%a, %d %b %Y %H:%M:%S GMT", &tm);
    memcpy(buf, DAY_NAMES[tm.tm_wday], 3);
    memcpy(buf, MONTH_NAMES[tm.tm_mon], 3);
    return buf;
}

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
	char *dateTime = getCurrentDateTime();
    sprintf(buf,
        "HTTP/1.1 200 OK\r\n"
		"Date: %s\r\n"
        "Server: ICWS\r\n"
        "Connection: close\r\n"
        "Content-Length: %lu\r\n"
        "Content-Type: %s\r\n"
		"Last-Modified: %s\r\n\r\n",
        time, size, mime, time
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

    int readRequest = read(connFd,buf,MAXBUF);
    mtx.lock();
    Request *request = parse(buf,readRequest,connFd);
    mtx.unlock();

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
        {"numThreads",required_argument,NULL,'n'},
        {"timeout",required_argument,NULL,'t'},
        {NULL, 0, NULL, 0}
    };

	int ch = 0;

    while ((ch = getopt_long(argc, argv, "p:r:n:t:", longOptions, NULL)) != -1) {
        switch(ch) {
            case 'p':
                port = optarg;
                break;
            case 'r':
                root = optarg;
                break;
            case 'n':
                numThreads = atoi(optarg);
                break;
            case 't':
                timeout = atoi(optarg);
                break;
        }
    }
}

void doWork() {
    while (true) {
        int w;
        if (workQueue.removeJob(&w)) {
            if (w < 0) {
                break;
            }
            serveHttp(w,root);
            close(w);
        }
        else {
            usleep(250000);
        }
    }
}

void server() {

    thread worker[numThreads];

    for (int i = 0; i < numThreads; i++) {
        worker[i] = thread(doWork);
    }

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
        workQueue.addJob(connFd);
    }
}

int main(int argc, char **argv) {
    parseArgument(argc,argv);
    server();
    return 0;
}
