#include "../include/application_layer.h"
#include "../include/link_layer.h"
#include "../include/macros.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

extern int reject;

long int filesize(FILE *file){
    fseek(file, 0, SEEK_END); //pointer no final do ficheiro

    long int bytesSize = ftell(file); //pos atual do pointer

    fseek(file, 0, SEEK_SET); //pointer no inicio

    return bytesSize;
}

int hexBytes(long int bytes){
    int hex = 0;

    while(bytes != 0){
        bytes >>= 8;
        hex++;
    }
    return hex;
}

//Pacote de controlo como no slide 23
void controlPacket(unsigned char *CPacket, int filesizeHexSize, long int bytesSize, unsigned int size, const char *filename){
    CPacket[0] = 2; //start
    CPacket[1] = 0; //tamanho do ficheiro
    CPacket[2] = filesizeHexSize; //tamanho hexadecimal

    memcpy(&CPacket[3], &bytesSize, filesizeHexSize);//alocado em memoria

    CPacket[3+filesizeHexSize] = 1; //nome do ficheiro
    CPacket[3+filesizeHexSize+1] = size; //tamanho nome ficheiro

    memcpy(&CPacket[3+filesizeHexSize+2], filename, size);
}

//Pacote de dados como no slide 23
void dataPacket(unsigned char *data_packet, int size, unsigned int n, FILE *file_Tx){
    unsigned char buf[size];

    fread(buf, 1, size, file_Tx);

    data_packet[1] = n; //dados
    data_packet[2] = size / 256; 
    data_packet[3] = size % 256;

    for (int i=0; i<size; i++)
        data_packet[i+4] = buf[i];
}

int compareCPacket(unsigned char *packet, int filesizeHexSize, unsigned char filesizeHex[MAX_PAYLOAD_SIZE], unsigned char receivedFilename[MAX_PAYLOAD_SIZE], unsigned char size){
    if (packet[1] == 0) { // tipo do parâmetro->file_size_bytes (em string)
        if(filesizeHexSize != packet[2]){
            printf("Start and end are different\n"); //
            return -1;
        }

        for(int i = 0; i < filesizeHexSize; i++){
            if(filesizeHex[i] != packet[3+i]){
                printf("Start and end are different\n");
                return -1;
            }
        }
    }

    if (packet[3+filesizeHexSize] == 1){ // tipo do parâmetro->filename
        if(size != packet[3+filesizeHexSize+1]){
            printf("Start and end are different\n");
            return -1;
        }

        for(int i = 0; i < size; i++){
            if(receivedFilename[i] != packet[3+filesizeHexSize+2+i]){
                printf("Start and end are different\n");
                return -1;
            }
        }
    }

    return 0;
}

void applicationLayer(const char *serialPort, const char *role, int baudRate, int nTries, int timeout, const char *filename)  //try to open files, send and receive them
{
    LinkLayer linkLayer;
    linkLayer.serialPort = serialPort; //porta de serie

    if (!strcmp(role, "tx"))
        linkLayer.role = LlTx;

    else if (!strcmp(role, "rx"))
        linkLayer.role = LlRx;

    linkLayer.baudRate = baudRate; //velocidade
    linkLayer.nRetransmissions = nTries; //tentativas
    linkLayer.timeout = timeout; //valor do temporizador

    if(llopen(linkLayer) == -1){
        printf("Failed llopen\n");
        return;
    }

    if(linkLayer.role == LlTx){ //todo o processo de envio de ficheiro

        FILE *file_Tx;
        file_Tx = fopen(filename, "r"); //ler

        if (file_Tx == NULL) {  //no file
            printf("Failed to open file\n");
            return;
        }

        long int bytesSize = filesize(file_Tx);

        int filesizeHexSize = hexBytes(bytesSize);

        unsigned int size = strlen(filename); //length

        unsigned int CPacket_size = 3 + filesizeHexSize + 2 + size; // 3->C1 T1 L1; 2->T2 L2; length(V1) = file_size_bytes; length(V2) = size

        unsigned char CPacket[CPacket_size];

        controlPacket(CPacket, filesizeHexSize, bytesSize, size, filename);

        if (llwrite(CPacket, CPacket_size) == -1){
            printf("Failed llwrite\n");
            return;
        }


        //iniciar construção do pacote de dados
        unsigned char data_packet[MAX_PAYLOAD_SIZE] = {0};
        data_packet[0] = 1; //C; valor 1 = dados
        unsigned int n = 0;
        int last_packet = FALSE;

        while (!last_packet){

            if (bytesSize > MAX_DATA_SIZE){
                bytesSize -= MAX_DATA_SIZE;
                size = MAX_DATA_SIZE;
            }

            else{
                last_packet = TRUE;
                size = bytesSize;
            }

            dataPacket(data_packet, size, n, file_Tx);

            n = (n+1)%255;
            int counter = 0;

            reject = FALSE;

            //se "while" só em vez de "do while" ele recebe um txt vazio em vez de gif
            do {
                if(llwrite(data_packet, (int) (size+4)) == -1){
                    printf("llwrite failed\n");
                    return;
                }
                counter++;
            } while (reject && counter < nTries); 
        }
        fclose(file_Tx); //close file

        //end control packet
        CPacket[0] = 3; //3 equivale a end

        if(llwrite(CPacket, CPacket_size) == -1){ //escrita falhou
            printf("Failed llwrite\n");
            return;
        }

        printf("\n");
        printf("File sented\n");
        printf("\n");
    }

    else if(linkLayer.role == LlRx) //todo o processo de receção de ficheiro
    {

        FILE *file_Rx;
        file_Rx = fopen(filename, "w+"); //para ler e escrever

        if (file_Rx == NULL) { //dont have a file
            printf("Failed to open file\n");
            return;
        }

        unsigned char buf[MAX_PAYLOAD_SIZE]={0};

        if(llread(buf) == -1){ //can't read control packet
            printf("Control packet failed\n");
            return;
        }

        unsigned char filesizeHexSize = 0; 
        unsigned char filenameSize;
        unsigned char filesizeHex[MAX_PAYLOAD_SIZE];
        unsigned char receivedFilename[MAX_PAYLOAD_SIZE];
        int num_seq = 0;

        //=2 equivale  start
        if(buf[0] == 2){
            
            if (buf[1] == 0) { //tamanho do ficheiro
                filesizeHexSize = buf[2]; //L1

                for(int i = 0; i < filesizeHexSize; i++){
                    filesizeHex[i] = buf[3+i];
                }
            }

            if (buf[3+filesizeHexSize] == 1){ //nome do ficheiro
                filenameSize = buf[3+filesizeHexSize+1];

                for(int i = 0; i < filenameSize; i++){
                    receivedFilename[i] = buf[3+filesizeHexSize+2+i];
                }
            }


            while(TRUE){

                unsigned char packet[MAX_PAYLOAD_SIZE]={0};

                if(llread(packet) == -1){
                    printf("Failed to read packet\n");
                    return;
                }

                // read end control packet
                if(packet[0] == 3){
                    if (compareCPacket(packet, filesizeHexSize, filesizeHex, receivedFilename, filenameSize)<0)
                        return;
                    break;
                }
                
                //de acordo com slide 23
                //=1 valor de dados
                if(packet[0] == 1){
                    unsigned char n, l1, l2;
                    unsigned int k;
                    n = packet[1];

                    if(n != num_seq){
                        printf("Failed to read packet\n");
                        continue;
                    }

                    num_seq = (num_seq+1)%255;
                    l2 = packet[2];
                    l1 = packet[3];
                    k = 256*l2 + l1;
                    unsigned char data[k];

                    for(int i = 0; i < k; i++){ //campo de dados do pacote
                        data[i] = packet[4+i];
                    }

                    fwrite(data, 1, k, file_Rx);
                }

            }
        }
        fclose(file_Rx);
        printf("\n");
        printf("File received\n");
        printf("\n");
    }

    if(llclose(0) == -1){
        printf("Failed llclose\n");
    }
}