// Alarm example
//
// Modified by: Eduardo Nuno Almeida [enalmeida@fe.up.pt]

#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include "macros.h"

int alarmEnabled = FALSE;
int alarmCount = 1;

// Alarm function handler
void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;

    printf("Alarm #%d\n", alarmCount);
}

//Start Alarm
void activateAlarm(){
    alarm(timeout);
    alarmEnabled = TRUE;
}

//End Alarm
void deactivateAlarm(){
    alarm(0);
    alarmEnabled = FALSE;
    alarmCount = 0;
}

int main()
{
    // Set alarm function handler
    (void) signal(SIGALRM, alarmHandler); //install alarm

    while (alarmCount < 4) //reescrever para comparar com nr de tentativas e se ja mandou ou nao
    {
        if (alarmEnabled == FALSE)
        { //deve começar a máquina de estados e ver se falha
            alarm(3); // Set alarm to be triggered in 3s
            alarmEnabled = TRUE;
        }
        activateAlarm();
    }
    deactivateAlarm();

    printf("Ending program\n");

    return 0;
}
