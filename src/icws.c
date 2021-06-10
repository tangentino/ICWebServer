#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>

char * port;
char * root;

void parse_argument(int argc, char **argv) {
    // Parsing command line argument to get port and root values
    static struct option long_options[] = {
        {"port",required_argument,NULL,'p'},
        {"root",required_argument,NULL,'r'},
        {NULL, 0, NULL, 0}
    };

	int ch = 0;

    while ((ch = getopt_long(argc, argv, "p:r:", long_options, NULL)) != -1) {
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

int main(int argc, char **argv) {
    parse_argument(argc,argv);
}
