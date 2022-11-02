#include "../include/link_layer.h"
#include "../include/macros.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>

unsigned char ns = 0, nr = 1; //ns começa a 0 e nr a 1
unsigned char control = 0; //campo de controlo

unsigned int tentativas;
unsigned int timeout;
char role;
int fd;

int reject = FALSE;

struct termios oldtio, newtio;

typedef enum {
    START,
    FLAG_RCV,
    A_RCV,
    C_RCV,
    BCC_OK,
} State; //trama

int alarmEnabled = FALSE;
int alarmCount = 0;

// Alarm functions 

void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;

    printf("Alarm #%d\n", alarmCount);
}

void activateAlarm(){ //start alarm
    alarm(timeout); //Temporizador
    alarmEnabled = TRUE;
}

void deactivateAlarm(){ //end alarm
    alarm(0);
    alarmEnabled = FALSE;
    alarmCount = 0;
}

//Fases do protocolo de Ligação de Dados

void ns_update(){ //change value I
    if(ns == I0){ //anterior 0
        ns = I1; //novo 1
    } else { //se anterior 1
        ns = I0; // novo 0
    }
}

void nr_update(){ //change value RR
    if(nr == 0){ //anterior 0
        nr = 1; //novo 1
    } else { //se anterior 1
        nr = 0; //novo 0
    }
}

//Maquina de estados para construção de trama
State state_machine(State current_state, int *finished, unsigned char value, unsigned char a, unsigned char c){
    switch (current_state){
        case START:
            if(value == FLAG){
                current_state = FLAG_RCV; //flag
            }
            break;
        case FLAG_RCV:
            if(value == FLAG){
                return current_state; //retorna estado atual
            }
            else if(value == a){
                current_state = A_RCV; //A
            }
            else
                current_state = START; //inicio
            break;
        case A_RCV:
            if(value == FLAG){
                current_state = FLAG_RCV; //flag
            }
            else if(value == c){
                current_state = C_RCV; //C
            }
            else{
                current_state = START; //inicio
            }
            break;
        case C_RCV:
            if(value == FLAG){
                current_state = FLAG_RCV; //flag
            } 
            else if(value == (a^c)){
                current_state = BCC_OK; //BCC
            }
            else{
                if(role == LlRx){
                    printf("Error");
                }
                current_state = START; //volta ao inicio
            }
            break;
        case BCC_OK:
            if(value == FLAG){
                *finished = TRUE; //fim 
            }
            else{
                current_state = START; //inicio
            }
            break;
    }
    return current_state; //retorna estado atual
}

//Trama
unsigned int buildFrame(unsigned char* updated_I, unsigned int I_size, const unsigned char *buf, int bufSize){
    updated_I[0] = FLAG; //F
    updated_I[1] = A_WRITE; //A
    unsigned char c;

    if(ns == 0){ //valor de I(NS=0)
        c = I0;
    } else { //valor de I(NS=1)
        c = I1;
    }

    updated_I[2] = c; //C
    updated_I[3] = BCC(A_WRITE,c); //BCC1

    //Dados
    int bcc2 = 0; //xor
    int start = 4;
    int extra = 0; // 0 como nos exemplos para acrescentar algo

    //Transparência - Mecanismo de byte stuffing

    for(int i = 0; i < bufSize; i++) {

        //Se ocorrer 0x7e então 0x7d 0x5e
        if(buf[i] == FLAG){ 
            updated_I[i+start+extra] = FLAG_P;
            updated_I[i+start+1+extra] = 0x5E;
            extra++;
        }

        else if(buf[i] == FLAG_P){ //Se ocorrer 0x7d então 0x7d 0x5d
            updated_I[i+start+extra] = FLAG_P;
            updated_I[i+start+1+extra] = 0x5D;
            extra++;
        }

        else{
            updated_I[i+start+extra] = buf[i];
        }
        bcc2 ^= buf[i];
    }

    //BCC2
    if(bcc2 == FLAG){ //Se ocorrer 0x7e então 0x7d 0x5e
        updated_I[I_size-3] = FLAG_P;
        updated_I[I_size-2] = 0x5E;
    }

    else if(bcc2 == FLAG_P){ //Se ocorrer 0x7d então 0x7d 0x5d
        updated_I[I_size-3] = FLAG_P;
        updated_I[I_size-2] = 0x5D;
    }

    else{

        I_size--;
        updated_I[I_size-2] = bcc2;
    }
    updated_I[I_size-1] = FLAG;
    return I_size;
}

//Stop and Wait ARQ (slide 31, data-link-layer)
int sendAck(){
    unsigned char ack[5];
    ack[0] = FLAG;
    ack[1] = A_WRITE;
    ack[4] = FLAG;

    if (nr == 0) {
        ack[2] = C_RR0;
        ack[3] = BCC(A_WRITE, C_RR0);
    } else if (nr == 1) {
        ack[2] = C_RR1;
        ack[3] = BCC(A_WRITE, C_RR1);
    }

    if(write(fd,ack,5) == -1){
        printf("failed writing\n");
        return -1;
    }

    return 0;
}

int sendNack(){
    unsigned char nack[5];
    nack[0] = FLAG;
    nack[1] = A_WRITE;
    nack[4] = FLAG;

    if(nr == 0){
        nack[2] = C_REJ0;
        nack[3] = BCC(A_WRITE, C_REJ0);
    }

    else if(nr == 1){
        nack[2] = C_REJ1;
        nack[3] = BCC(A_WRITE, C_REJ1);
    }

    if(write(fd,nack,5) == -1){
        printf("failed writing\n");
        return -1;
    }

    return 0;
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters) //identificador da ligação de dados
{
    tentativas = connectionParameters.nRetransmissions;
    timeout = connectionParameters.timeout;
    role = connectionParameters.role;

    //like write_noncanonical.c

    fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY);

    if (fd < 0){
        return -1;
    }

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = connectionParameters.baudRate | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0; // Inter-character timer unused
    newtio.c_cc[VMIN] = 0;  // Blocking read until 5 chars received
    // VTIME e VMIN should be changed in order to protect with a
    // timeout the reception of the following character(s)

    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");

    if(role == LlTx){ //sender
        unsigned char set_value[5]; //inicia trama
        set_value[0] = FLAG;
        set_value[1] = A_WRITE;
        set_value[2] = C_SET;
        set_value[3] = BCC(A_WRITE,C_SET);
        set_value[4] = FLAG;

        int finished = FALSE;

        (void)signal(SIGALRM, alarmHandler); //liga alarme
        State current_state = START; //estado

        while(!finished && alarmCount < tentativas)
        {
            if (!alarmEnabled)
            {
                current_state = START;
                if (write(fd, set_value, 5) == -1){
                    perror("Couldn't write\n");
                    return -1;
                }
                activateAlarm();
            }

            unsigned char value;
            int bytes = read(fd, &value, 1); //lê

            if(bytes == -1){
                perror("Failed to read");
                return -1;
            }

            current_state = state_machine(current_state, &finished, value, A_WRITE, C_UA);
        }
        deactivateAlarm();
    }

    else if(role == LlRx){ //receiver
        State current_state = START;
        int finished = FALSE;

        while(!finished){
            unsigned char value;

            int bytes = read(fd, &value, 1); //lê

            if(bytes == -1){
                perror("Couldn't read");
                return -1;
            }
            current_state = state_machine(current_state, &finished, value, A_WRITE, C_SET);
        }

        unsigned char ua[5];
        ua[0] = FLAG;
        ua[1] = A_WRITE;
        ua[2] = C_UA;
        ua[3] = BCC(A_WRITE, C_UA);
        ua[4] = FLAG;

        if (write(fd, ua, 5) == -1){
            perror("Couldn't write\n");
            return -1;
        }
    }
    return fd;
}


////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize) //número de caracteres escritos
{
    reject = FALSE;
    deactivateAlarm();

    int count = 0;
    for (int i = 0; i < bufSize; i++) { 
        if (buf[i] == FLAG || buf[i] == FLAG_P) {
            count += 1;
        }
    }

    unsigned int I_size = 6 + bufSize + count + 1;

    unsigned char updated_I[I_size];

    //Trama de informação
    I_size = buildFrame(updated_I, I_size, buf, bufSize);

    int finished = FALSE;
    (void) signal(SIGALRM, alarmHandler);
    State current_state = START;

    alarmEnabled = FALSE;

    while (!finished && alarmCount < tentativas) {
        if (!alarmEnabled){
            if(write(fd, updated_I, I_size) == -1){ //tenta escrever trama de informação
                perror("Couldn't write\n");
                return -1;
            }
            current_state = START; //inicio

            activateAlarm();
        }

        unsigned char value;
        int bytes = read(fd, &value, 1); //lê

        if(bytes == -1){
            perror("Couldn't read");
            continue;
        }

        switch (current_state){ //Trama S + uma maquina de estados para leitura
            case START:
                if(value == FLAG){
                    current_state = FLAG_RCV;
                }
                break;
            case FLAG_RCV:
                if(value == FLAG){
                    continue;
                }
                else if(value == A_WRITE){
                    current_state = A_RCV;
                }
                else{
                    current_state = START;
                }
                break;
            case A_RCV:
                if(value == FLAG){
                    current_state = FLAG_RCV;
                }
                else if(value == C_RR0){
                    if (ns==0)
                        reject = TRUE;
                    else{
                        control = value;
                        current_state = C_RCV;
                        reject = FALSE;
                    }
                } else if(value == C_RR1){
                    if (ns==1)
                        reject = TRUE;
                    else{
                        control = value;
                        current_state = C_RCV;
                        reject = FALSE;
                    }
                }
                else if (value == C_REJ0 || value == C_REJ1){
                    read(fd, &value, 1);
                    reject = TRUE;
                    return 0;
                }
                else{
                    current_state = START;
                }
                break;
            case C_RCV:
                if(value == FLAG){
                    current_state = FLAG_RCV;
                }
                else if(value == (BCC(A_WRITE,control))){
                    current_state = BCC_OK;
                }
                else{
                    current_state = START;
                }
                break;
            case BCC_OK:
                if(value == FLAG){
                    finished = TRUE;
                }
                else{
                    current_state = START;
                }
                break;
        }
    }

    if(!finished){
        printf("I can not wait anymore\n");
        printf("Nothing received\n");
        return -1;
    }
    ns_update();
    
    deactivateAlarm();

    return I_size;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet) //comprimento do array (número de caracteres lidos)
{
    int finished = FALSE;
    (void)signal(SIGALRM, alarmHandler);
    State current_state = START;

    int PPP_on = FALSE; //Flag P on
    int bcc2 = 0;
    int size_data = 0;
    char c;
    unsigned char value;

    while(!finished) {
        int bytes = read(fd, &value, 1); //lê

        if (bytes == 0){
            continue;
        }
        if(bytes == -1){
            perror("Failed to read");
            return -1;
        }

        switch (current_state){ //leitura da trama I 
            case START:
                if(value == FLAG){
                    current_state = FLAG_RCV;
                }
                break;
            case FLAG_RCV:
                if(value == FLAG){
                    continue;
                }
                else if(value == A_WRITE){
                    current_state = A_RCV;
                }
                else{
                    current_state = START;
                }
                break;
            case A_RCV:
                if(value == FLAG){
                    current_state = FLAG_RCV;
                }
                else if(value == I0) {
                    if (nr == 0) {  //Discards duplicate
                        if (sendNack() == -1)
                            return -1;
                        current_state = START;
                    }
                    else {
                        c = value;
                        current_state = C_RCV;
                    }
                }else if (value == I1){
                    if (nr == 1) {  //Discards duplicate
                        if (sendNack() == -1)
                            return -1;
                        current_state = START;
                    }
                    else {
                        c = value;
                        current_state = C_RCV;
                    }
                }
                break;
            case C_RCV:
                if(value == FLAG){
                    current_state = FLAG_RCV;
                }
                else if(value == (BCC(A_WRITE,c))){ // BCC1
                    current_state = BCC_OK;
                }
                else{
                    printf("Failed\n");
                    current_state = START;
                }
                break;

            case BCC_OK:
                if(value != FLAG){
                    //Destuffing
                    if(value == FLAG_P){
                        PPP_on = TRUE;
                        continue;
                    }

                    if(PPP_on){
                        if(value == 0x5E){
                            packet[size_data] = FLAG;
                            size_data++;
                        }
                        else if(value == 0x5D){
                            packet[size_data] = FLAG_P;
                            size_data++;
                        }
                        PPP_on = FALSE;
                    }

                    else {
                        packet[size_data] = value;
                        size_data++;
                    }
                }
                else{
                    finished = TRUE;
                }
                current_state = BCC_OK;
                break;
        }
    }

    bcc2 = packet[size_data-1];
    size_data--;
    unsigned char new_bcc2 = 0;

    for (int i=0; i<size_data; i++) {
        new_bcc2 ^= packet[i];

    }

    if (new_bcc2 == bcc2) { //Positive answer
        if (sendAck() == -1)
            return -1;
    }

    else {
        printf("Failed\n"); //Negative answer
        if (sendNack() == -1)
            return -1;
    }

    nr_update();
    return size_data;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics) //valor positivo em caso de sucesso e negativo em caso de erro
{
    deactivateAlarm();

    if(role == LlTx){
        unsigned char disc_value[5];
        disc_value[0] = FLAG;
        disc_value[1] = A_READ;
        disc_value[2] = C_DISC;
        disc_value[3] = BCC(A_READ,C_DISC);
        disc_value[4] = FLAG;

        int finished = FALSE;
        (void) signal(SIGALRM, alarmHandler);
        State current_state = START;

        while(!finished && alarmCount < tentativas )
        {
            if (!alarmEnabled)
            {
                current_state = START;
                if (write(fd, disc_value, 5) == -1){
                    perror("Couldn't write\n");
                    return -1;
                }
                activateAlarm();
            }

            unsigned char value;

            int bytes = read(fd, &value, 1);

            if(bytes == -1){
                perror("Couldn't read\n");
                return -1;
            }

            current_state = state_machine(current_state, &finished, value, A_READ, C_DISC);
        }
        deactivateAlarm();

        unsigned char ua_value[5];
        ua_value[0] = FLAG;
        ua_value[1] = A_READ;
        ua_value[2] = C_UA;
        ua_value[3] = BCC(A_READ,C_UA);
        ua_value[4] = FLAG;

        if (write(fd, ua_value, 5) == -1){
            perror("Couldn't write\n");
            return -1;
        }

    }

    else if(role == LlRx){
        State current_state = START;
        int finished = FALSE;

        while(!finished){
            unsigned char value;

            int bytes = read(fd, &value, 1);

            if(bytes == -1){
                perror("Failed to read\n");
                return -1;
            }
            current_state = state_machine(current_state, &finished, value, A_READ, C_DISC);
        }

        unsigned char disc[5];
        disc[0] = FLAG;
        disc[1] = A_READ;
        disc[2] = C_DISC;
        disc[3] = BCC(A_READ, C_DISC);
        disc[4] = FLAG;

        if (write(fd, disc, 5) == -1){
            perror("Couldn't write\n");
            return -1;
        }

        current_state = START;
        finished = FALSE;

        while(!finished){
            unsigned char value;

            int bytes = read(fd, &value, 1);

            if(bytes == -1){
                perror("Failed to read\n");
                return -1;
            }
            current_state = state_machine(current_state, &finished, value, A_READ, C_UA);
        }
    }

    if (tcsetattr(fd, TCSANOW, &oldtio) == -1) {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd); //close file

    return 1;
}
