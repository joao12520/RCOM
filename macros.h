//enum trama {FLAG, A_WRITE, C_SET, BCC, FLAG};

// Tramas I (informação)
#define SET_SIZE 5  // tamanho em bytes da trama SET
#define UA_SIZE 5  // tamanho em bytes da trama UA
#define BUF_SIZE 256


#define FLAG 0x7E //flag de inicio e fim

#define A_WRITE 0x03  
#define A_READ 0x01 

#define C_SET 0x03	
#define C_DISC 0x0B 
#define C_UA 0x07 
#define C_RR(r) ((0b10000101) &= (r) << (7)) // (0x05 OU 0x85) Campo de Controlo - RR (receiver ready / positive ACK))
#define C_REJ(r) ((0b10000001) &= (r) << (7)) // (0x01 OU 0x81) Campo de Controlo - REJ (reject / negative ACK))

#define FALSE 0
#define TRUE 1

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

