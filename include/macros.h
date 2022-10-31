// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source
#define BUF_SIZE 256 //tamanho do buf
#define MAX_DATA_SIZE (MAX_PAYLOAD_SIZE)

#define FLAG 0x7E
#define FLAG_P 0x7D //escape; PPP mechanism

#define A_WRITE 0x03 
#define A_READ 0x01

//Control
#define C_UA 0x07
#define C_SET 	0x03
#define C_DISC 	0x0B
#define C_RR0	0x05 
#define C_RR1	0x85 
#define C_REJ0	0x81 
#define C_REJ1	0x01 

#define BCC(x,y) (x^y)

#define FALSE 0
#define TRUE 1

//numeracao da trama de info
#define I0 0x00 // NS = 0
#define I1 0x40 // NS = 1