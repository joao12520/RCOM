#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

int parseUrl(char** parsed, char* url) {
    char *user = "anonymus", *password = "", *host = "", *urlPath = "/";

    char *token = strtok(url, ":");
    if (strcmp(token,"ftp")==0 || token == NULL) {          //check if it is ftp
        if (strchr(token, '@') == NULL) {                   //no credentials
            token++;token++;
            host = strtok(NULL, "/");
            if ((urlPath = strtok(NULL, "\0")) == NULL) {   //no path
                urlPath = "/";
            }
        } else {                                            //credentials
            user = strtok(NULL, ":");
            user++;user++;
            password = strtok(NULL, "@");
            host = strtok(NULL, "/");
            if ((urlPath = strtok(NULL, "\0")) == NULL) {   //no path
                urlPath = "/";
            }
        }
    } else {
        host = url;
    }

    parsed[0] = user;
    parsed[1] = password;
    parsed[2] = host;
    parsed[3] = urlPath;

    return 0;
}

int main(int argc, char *argv[]) {
    struct hostent *h;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <address to get IP address>\n", argv[0]);
        exit(-1);
    }

    char* parsed[4];
    if (parseUrl(&*parsed, argv[1])) {
        fprintf(stderr, "Error parsing\n");
        exit(-1);
    }
    printf("User: %s\nPassword: %s\nHost: %s\nURL Path: %s\n", parsed[0], parsed[1], parsed[2], parsed[3]);

/**
 * The struct hostent (host entry) with its terms documented

    struct hostent {
        char *h_name;    // Official name of the host.
        char **h_aliases;    // A NULL-terminated array of alternate names for the host.
        int h_addrtype;    // The type of address being returned; usually AF_INET.
        int h_length;    // The length of the address in bytes.
        char **h_addr_list;    // A zero-terminated array of network addresses for the host.
        // Host addresses are in Network Byte Order.
    };

    #define h_addr h_addr_list[0]	The first address in h_addr_list.
*/
    if ((h = gethostbyname(argv[1])) == NULL) {
        herror("gethostbyname()");
        exit(-1);
    }

    printf("Host name  : %s\n", h->h_name);
    printf("IP Address : %s\n", inet_ntoa(*((struct in_addr *) h->h_addr)));


    int sockfd;
    struct sockaddr_in server_addr;

    /*server address handling*/
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(h->h_addr);    /*32 bit Internet address network byte ordered*/
    server_addr.sin_port = htons(21);        /*server TCP port must be network byte ordered */

    /*open a TCP socket*/
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        exit(-1);
    }
    /*connect to the server*/
    if (connect(sockfd,
                (struct sockaddr *) &server_addr,
                sizeof(server_addr)) < 0) {
        perror("connect()");
        exit(-1);
    }
    
    //login
    FILE *file = fdopen(sockfd, "r");
    char *line;
    size_t size;
    int code;
    char* login = "";
    while (code != 230){
        if (getline(&line, &size, file) != -1) {
            printf("%s\n", line);
            code = atoi(line);
        }
        
        if (code == 220) {                          // expecting username
            sprintf(login, "user %s\n", parsed[0]);
            write(sockfd, login, strlen(login));
        } else if (code == 331) {                   // expecting password
            sprintf(login, "pass %s\n", parsed[1]);
            write(sockfd, login, strlen(login));
        } else if (code == 230) {                   // login succsessful
            sprintf(login, "pasv\n");
            write(sockfd, login, strlen(login));    // entering passive mode
        } else {
            fprintf(stderr, "Error logging in\n");
            exit(-1);
        }
    }

    //download
    getline(&line, &size, file);
    printf("%s\n", line);
    code = atoi(line);
    if (code != 227) {
        fprintf(stderr, "Error entering passive mode\n");
        exit(-1);
    }

    char* adr = "";
    char* copy = "";
    strcpy(copy, line);
    char* token = strtok(copy, "(");
    token = strtok(NULL, ",");
    strcat(adr, token);
    token = strtok(NULL, ",");
    strcat(adr, token);
    token = strtok(NULL, ",");
    strcat(adr, token);
    token = strtok(NULL, ",");
    strcat(adr, token);

    int port = 0;
    token = strtok(NULL, ",");
    port += atoi(token) * 256;
    token = strtok(NULL, ",");
    port += atoi(token);
    
    int fdReq;
    /*server address handling*/
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(adr);    /*32 bit Internet address network byte ordered*/
    server_addr.sin_port = htons(port);        /*server TCP port must be network byte ordered */

    /*open a TCP socket*/
    if ((fdReq = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        exit(-1);
    }
    /*connect to the server*/
    if (connect(fdReq,
                (struct sockaddr *) &server_addr,
                sizeof(server_addr)) < 0) {
        perror("connect()");
        exit(-1);
    }

    char *retr = "retr ";
    strcat(retr, parsed[3]);
    strcat(retr, "\n");
    write(sockfd, retr, strlen(retr));
    if (getline(&line, &size, file) != -1) {
        printf("%s\n", line);
        code = atoi(line);
    }
    if (code != 150) {
        fprintf(stderr, "Error connecting\n");
            exit(-1);
    }

    char *filename = "";
    char *copyPath = "";
    strcpy(copyPath, parsed[3]);
    token = strtok(copyPath, "/");
    while (token != NULL) {
        strcpy(filename, copyPath);
        token = strtok(NULL, "/");
    }

    int fdRec = open(filename, O_RDWR | O_CREAT);
    char *buf = "";
    int bytes;
    while ((bytes = read(fdReq, buf, sizeof(buf)))) {
        write(fdRec, buf, bytes);
    }

    if (getline(&line, &size, file) != -1) {
        printf("%s\n", line);
        code = atoi(line);
    }
    if (code != 226) {
        fprintf(stderr, "Error downloading\n");
            exit(-1);
    }


    if (close(sockfd)<0) {
        perror("close()");
        exit(-1);
    }

    return 0;
}
