#include "mcc_generated_files/mcc.h"
#include "stdio.h"


char getU1( void) {
    if ( U1STAbits.OERR) U1STAbits.OERR= 0;
    while ( !U1STAbits.URXDA);  
    return U1RXREG;
}

void _ISR _T2Interrupt( void){
    static int i= 0;
    _RA2= !_RA2; 
    _RA1= !_RA1;
    if ( ++i == 10) {
        i= 0;
        _RA1= 0;
        _T2IE= 0;
        T2CONbits.TON= 0; 
    }
    _T2IF= 0;
}

void _ISR _T1Interrupt( void){
    _RB4= !_RB4;
    _RA1= !_RA1; //vibrator
    _T1IF= 0;
}

unsigned int getMuscleTone( void){
    AD1CON1bits.SAMP= 1;
    while (!AD1CON1bits.DONE);
    AD1CON1bits.DONE= 0;
    //printf("%u\r\n", ADC1BUF0);//(int) (ADC1BUF0/1024*3.3));
    return ADC1BUF0;
}

void completeSession( void){
    _RA4= 1;
    _RB5= 1;
    _RA2= 1;
    _RB4= 1;
    _RA1= 0; //vibrator
    T1CONbits.TON= 0;
}

void _ISR _CNInterrupt( void) {
    printf( "done\r\n");  
    completeSession();
    _CN6IE= 0;
    _CNIF = 0;
    _CNIE= 0; 
}

int main(void)
{
    SYSTEM_Initialize();
 
    while (1)
    {
        char command= getU1();
        if ( command == '1'){
            _RA4=!_RA4; 
            printf("%u\r\n", getMuscleTone());
        }
        if ( command == '3'){
            _RB5=!_RB5; 
            printf("%u\r\n", getMuscleTone());
        }
        if ( command == '5'){
            printf("ok\r\n");
            _T2IE= 1;
            _T2IF= 0;
            _RA2=!_RA2;
            _RA1= 1; //vibrator
            T2CONbits.TON= 1;
        }
        if ( command == '7'){
            printf("ok\r\n");
            _T1IE= 1;
            _T1IF= 0;
            _CNIP= 5;
            _CNIF= 0;
            _CN6IE= 1;
            _CNIE= 1;
            _RB4=!_RB4; 
            T1CONbits.TON= 1;
        } 
        if ( command == '9'){
            completeSession();            
        }
    }
    return -1;
}

/*
int putU1( int c) {
    while ( U1STAbits.UTXBF);
    U1TXREG = c;
    return c;
}
void putsU1( char *s) {
    while( *s) putU1( *s++);
    putU1( '\r');
    putU1( '\n');
}

void Delayms( unsigned t)
{
    T1CON = 0x8000;     
    while (t--) {
        TMR1 = 0;
        while ( TMR1 < (1000000UL/1000)); 
    }
}
*/
    //putsU1( ">"+command);
        /*
         * 
            //char c1,c2;
    //putsU1( "Press 1 or 2 to control Red LED");
    Delayms( 1000);
    //_RB5= 0;
    //putsU1( "Press 3 or 4 to control Green LED");
    Delayms( 1000);
    //_RA4= 0;
        printf("AT");
        Delayms( 2000);
        printf("AT+VERSION");
        //printf( "AT+NAMEiWaker");
        //_RA4=1;
        Delayms( 2000);
        //while( 1);
        //Delayms( 1000);
         */

        //printf( "Here is your data efi: %u\r\n", command);
        
        /*
        AD1CON1bits.SAMP= 1;
        while (!AD1CON1bits.DONE);
        AD1CON1bits.DONE= 0;
        printf("%u\r\n", ADC1BUF0);//(int) (ADC1BUF0/1024*3.3));
        */