#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>
#include <stdbool.h>

#define SERVER_PORT 21
#define IP_MAX_SIZE 16
#define FILENAME_MAX_SIZE 200
#define INVALID_INPUT_ERROR "Usage: ftp://[<user>:<password>@]<host>/<url-path>\n"

static char filename[500];

struct Input
{
    char *user;
    char *password;
    char *host;
    char *urlPath;
};

int parseUrl(char *url, char **uph, char **urlPath)
{
    if (strstr(url,"ftp://") != NULL) {
        if (strstr(url,"ftp://") != url) {
            printf(INVALID_INPUT_ERROR);
            return -1;
        } else {
            url += 6;
        }  
    }
    char *path = NULL;
    if ((path = strchr(url, '/')) != NULL) {
        path++;
        *uph = strtok(url, "/");
    } else {
        path = "";
    }
    *urlPath = path;
    return 0;
}

void getFilename(char *urlPath, char *filename)
{
    char aux[1000];
    strcpy(aux, urlPath);
    char *token1 = strtok(aux, "/");
    while (token1 != NULL)
    {
        strcpy(filename, token1);
        token1 = strtok(NULL, "/");
    }
}

int parseUserPasswordHost(char *uph, struct Input *input)
{
    if ((input->host = strchr(uph, '@')) != NULL) {
        input->host++;
        uph = strtok(uph, "@");
        
        if ((input->password = strchr(uph, ':')) != NULL) {
            input->password++;
            input->user = strtok(uph, ":");
        } else {
            return 1;
        }
    } else {
        input->user = "anonymous";
        input->password = "";
        input->host = uph;
    }
    return 0;
}

int parseInput(int argc, char **argv, struct Input *input)
{
    if (argc < 2)
    {
        printf("Error: Not enough arguments.\n");
        return -1;
    }

    // Parse input URL
    char *uph;
    if (parseUrl(argv[1], &uph, &input->urlPath) < 0)
    {
        return -1;
    }

    // Get filename from url path
    getFilename(input->urlPath, filename);

    // Parse user, password, and host
    return parseUserPasswordHost(uph, input);
}

int writeToSocket(int fd, const char *cmd, const char *info)
{
    // Write command to socket
    if (write(fd, cmd, strlen(cmd)) != strlen(cmd))
    {
        printf("Error writing command to socket\n");
        return -1;
    }

    // Write info to socket
    if (write(fd, info, strlen(info)) != strlen(info))
    {
        printf("Error writing info to socket\n");
        return -1;
    }

    // Write newline to socket
    if (write(fd, "\n", strlen("\n")) != strlen("\n"))
    {
        printf("Error writing newline to socket\n");
        return -1;
    }

    printf("%s%s\n\n", cmd, info);

    return 0;
}

int readFromSocket(char *result, FILE *sock)
{
    // Read the first three characters of the response
    int first = fgetc(sock);
    int second = fgetc(sock);
    int third = fgetc(sock);
    if (first == EOF || second == EOF || third == EOF)
    {
        return -1;
    }

    // Read the space character after the response code
    int space = fgetc(sock);
    if (space == EOF)
    {
        return -1;
    }

    // Read the rest of the response
    if (space != ' ')
    {
        do
        {
            printf("%s\n", fgets(result, FILENAME_MAX_SIZE, sock));
            if (result == NULL)
            {
                return -1;
            }
        } while (result[3] != ' ');
    }
    else
    {
        printf("%s\n", fgets(result, FILENAME_MAX_SIZE, sock));
    }

    // Convert response code to integer and return
    int zero = '0';
    return (first - zero) * 100 + (second - zero) * 10 + (third - zero);
}

int createSocket(const char *ip, int port)
{
    int sockfd;
    struct sockaddr_in server_addr;

    // Set up server address
    memset((char *)&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);

    // Create TCP socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("socket()");
        return -1;
    }

    // Connect to server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("connect()");
        return -1;
    }

    return sockfd;
}

int main(int argc, char **argv)
{
    // Parse input
    struct Input input;
    int parse_input_result = parseInput(argc, argv, &input);
    if (parse_input_result < 0)
    {
        fprintf(stderr, "Error parsing input\n");
        exit(-1);
    }
    printf("User: %s\nPassword: %s\nHost: %s\nURL Path: %s\n", input.user, input.password, input.host, input.urlPath);

    // Get IP address of host
    struct hostent *h;
    h = gethostbyname(input.host);
    if (h == NULL)
    {
        herror("gethostbyname()");
        exit(-1);
    }
    char ip[IP_MAX_SIZE];
    strcpy(ip, inet_ntoa(*((struct in_addr *)h->h_addr)));
    printf("IP Address : %s\n\n", ip);

    // Create control connection
    int control_connection_fd = createSocket(ip, SERVER_PORT);
    if (control_connection_fd < 0)
    {
        fprintf(stderr, "Error creating control connection\n");
        return -1;
    }
    FILE *control_connection_file = fdopen(control_connection_fd, "r+");

    // Read initial response from control connection
    char result[500];
    int response_code = readFromSocket(result, control_connection_file);
    if (response_code < 0)
    {
        fprintf(stderr, "Error reading from control connection\n");
        return -1;
    }
    if (response_code != 220)
    {
        fprintf(stderr, "Bad response from control connection\n");
        return -1;
    }

    // Send user command to control connection
    int write_result = writeToSocket(control_connection_fd, "user ", input.user);
    if (write_result != 0)
    {
        fprintf(stderr, "Error writing to control connection\n");
        return -1;
    }

    // Read response to user command from control connection
    response_code = readFromSocket(result, control_connection_file);
    if (response_code < 0)
    {
        fprintf(stderr, "Error reading from control connection\n");
        return -1;
    }
    if (response_code != 331 && response_code != 230)
    {
        fprintf(stderr, "Bad response from control connection\n");
        return -1;
    }

    // Send pass command to control connection
    write_result = writeToSocket(control_connection_fd, "pass ", input.password);
    if (write_result != 0)
    {
        fprintf(stderr, "Error writing to control connection\n");
        return -1;
    }

    // Read response to pass command from control connection
    response_code = readFromSocket(result, control_connection_file);
    if (response_code < 0)
    {
        fprintf(stderr, "Error reading from control connection\n");
        return -1;
    }
    if (response_code != 230)
    {
        fprintf(stderr, "Bad response from control connection\n");
        return -1;
    }

    // Send pasv command to control connection
    write_result = writeToSocket(control_connection_fd, "pasv ", "");
    if (write_result != 0)
    {
        fprintf(stderr, "Error writing to control connection\n");
        return -1;
    }

    // Read response to pasv command from control connection
    response_code = readFromSocket(result, control_connection_file);
    if (response_code < 0)
    {
        fprintf(stderr, "Error reading from control connection\n");
        return -1;
    }
    if (response_code != 227)
    {
        fprintf(stderr, "Bad response from control connection\n");
        return -1;
    }

    // Parse port number from pasv response
    int a, b, c, d, e, f;
    sscanf(result, "Entering Passive Mode (%d,%d,%d,%d,%d,%d).", &a, &b, &c, &d, &e, &f);
    int real_port = e * 256 + f;

    // Create data connection
    int data_connection_fd = createSocket(ip, real_port);
    if (data_connection_fd < 0)
    {
        fprintf(stderr, "Error creating data connection\n");
        return -1;
    }
    FILE *data_connection_file = fdopen(data_connection_fd, "r+");

    // Send retr command to control connection
    write_result = writeToSocket(control_connection_fd, "retr ", input.urlPath);
    if (write_result != 0)
    {
        fprintf(stderr, "Error writing to control connection\n");
        return -1;
    }

    // Read response to retr command from control connection
    response_code = readFromSocket(result, control_connection_file);
    if (response_code < 0)
    {
        fprintf(stderr, "Error reading from control connection\n");
        return -1;
    }
    if (response_code != 150)
    {
        fprintf(stderr, "Bad response from control connection\n");
        return -1;
    }

    // Open file to write data to
    FILE *myFile = fopen(filename, "w");

    // Read data from data connection and write to file
    int size;
    while (true)
    {
        unsigned char final_result[300];
        bool end;
        size = fread(final_result, 1, 300, data_connection_file);
        if (size < 0)
            return -1;
        if (size < 300)
            end = true;
        fwrite(final_result, 1, size, myFile);
        if (end)
            break;
    }

    // Close file and connections
    fclose(myFile);
    fclose(data_connection_file);
    close(data_connection_fd);

    // Read response from control connection after closing data connection
    response_code = readFromSocket(result, control_connection_file);
    if (response_code < 0)
    {
        fprintf(stderr, "Error reading from control connection\n");
        return -1;
    }
    if (response_code != 226)
    {
        fprintf(stderr, "Bad response from control connection\n");
        return -1;
    }

    // Close control connection
    fclose(control_connection_file);
    close(control_connection_fd);

    return 0;
}
