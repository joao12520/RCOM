// Read from serial port in non-canonical mode
//
// Modified by: Eduardo Nuno Almeida [enalmeida@fe.up.pt]

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include "macros.h"

volatile int STOP = FALSE;

/*void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;

    printf("Alarm #%d\n", alarmCount);
}*/

int main(int argc, char *argv[])
{
    // Program usage: Uses either COM1 or COM2
    const char *serialPortName = argv[1];

    if (argc < 2)
    {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS1\n",
               argv[0],
               argv[0]);
        exit(1);
    }

    // Open serial port device for reading and writing and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    int fd = open(serialPortName, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        perror(serialPortName);
        exit(-1);
    }

    struct termios oldtio;
    struct termios newtio;

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0; // Inter-character timer unused
    newtio.c_cc[VMIN] = 1;  // Blocking read until 5 chars received

    // VTIME e VMIN should be changed in order to protect with a
    // timeout the reception of the following character(s)

    // Now clean the line and activate the settings for the port
    // tcflush() discards data written to the object referred to
    // by fd but not transmitted, or data received but not read,
    // depending on the value of queue_selector:
    //   TCIFLUSH - flushes data received but not read.
    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");

    // Loop for input
    unsigned char buf[BUF_SIZE + 1] = {0}; // +1: Save space for the final '\0' char
    
    
    //(void)signal(SIGALRM, alarmHandler);
	
    while (STOP == FALSE)
    {	
        // Returns after 5 chars have been input
        int bytes = read(fd, buf, 5); //lê o que recebeu
        buf[bytes] = '\0'; // Set end of string to '\0', so we can printf
	    printf("Received %d bytes\n", bytes);
        
        sleep(1); //Wait until all bytes have been written to the serial port
        
        printf("Received trama: "); 
        for(int i = 0; i < bytes; i++){ //mostra o que recebeu
            printf("%x", buf[i]);
        }
        printf("\n");
        

        
        printf("\n");
        int written = write(fd,buf,5); //envia trama de volta
        printf("Sent back %d bytes\n", written);
	
	
        STOP = TRUE;
    }
    
    /*while (alarmCount < 4)
    {
        if (alarmEnabled == FALSE)
        {
            alarm(3); // Set alarm to be triggered in 3s
            
            // Returns after 5 chars have been input
        int bytes = read(fd, buf, 5);
        buf[bytes] = '\0'; // Set end of string to '\0', so we can printf
	printf("Received %d bytes\n", bytes);
        
        for(int i = 0; i < bytes; i++){
            printf("%x", buf[i]);
        }
        printf("\n");
        
        int written = write(fd,buf,BUF_SIZE);
        printf("Sent %d bytes\n", written);
            
            alarmEnabled = TRUE;
        }
    }

    printf("Ending program\n");*/

    // The while() cycle should be changed in order to respect the specifications
    // of the protocol indicated in the Lab guide

    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}
