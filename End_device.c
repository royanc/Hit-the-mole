#include "bsp.h"
#include "mrfi.h"
#include "nwk_types.h"
#include "nwk_api.h"
#include "bsp_leds.h"
#include "bsp_buttons.h"
#include "app_remap_led.h"
#include <time.h>



#define NUM_OF_SESSION 4
#define SPIN_ABOUT_A_SECOND  NWK_DELAY(350)


static void linkTo(void);
static linkID_t sLinkID1 = 0;

/* application Rx frame handler. */
static uint8_t sRxCallback(linkID_t);
void uart_init();
void print(char* char_arr);
void hboard(int r);
int divide(int arr);

uint8_t user_id = 13;
char board[] = "[ ] [ ] [ ] [ ]";
char instruction[]="the game called hit a mole , you need to press the buttom that the a mole appear . the option is 1,2,3,4. the winner is who is press faster.  Winner color is green";
char initial[] = "The initial board";

char enter[2] = "\n";
char enter1[2] = "\r";


/* application Rx frame handler. */
char *dirty_board;//ARRAY CHANGING
int random;
int user_time_arr[4];
int result[4];
uint8_t finish_session = 0;
int calc_time_barrier = 0;
int time_exceeded = 0;
volatile time_t times[4];
int ti = 0;
int getin = 1;



void main (void){

    int s;    // Index for num_of_sessions


    BSP_Init();

    // Timers and UART initialization
    srand(time(NULL));   // Initialization, should only be called once.

    WDTCTL = WDTPW | WDTHOLD;       // stop watchdog timer
    TACCTL0 = CCIE;                 // TACCR0 interrupt enabled
    TACCR0 = 2*12000;                 // ~ 1 sec

    BCSCTL1 = CALBC1_1MHZ; //set Basic Clock System Control 1 to - 1 Mhz
    DCOCTL = CALDCO_1MHZ;  //set DCO Clock Frequency Control to - 1 Mhz
    UCA0CTL1 |= UCSSEL_2 ;      //use SMCLK
    UCA0CTL1 |= UCSWRST ;     //USCI software reset
    P3SEL |=0x30;             //Pin P3.5 and P3.4 and P3.3 used by USART module
    UCA0BR0 = 104;            //DIVIDE CLOCK FOR BAUD RATE 9600
    UCA0BR1 = 0;              //DIVIDE CLOCK FOR BAUD RATE 9600
    UCA0MCTL = 2;             //UCBRSx=1
    UCA0CTL1 &= ~UCSWRST;     //initialize USCI
    UC0IE |= UCA0RXIE;        //enabling USCI_A0 RX interrupt

    BCSCTL3 |= LFXT1S_2 ;
    _BIS_SR(GIE);       //enable global interrupt and work in LPM3

    uart_init();

    print(instruction);
    print(enter);
    print(enter1);
    SPIN_ABOUT_A_SECOND;

    // Print the clean board for the first time
    print(enter);
    print(initial);
    print(enter);
    print(enter1);
    print(board);
    print(enter);
    print(enter1);
    SPIN_ABOUT_A_SECOND;

    for (s = 0; s < NUM_OF_SESSION; s++) {

        random = rand() % 4 + 1; //random between 1 to 4
        hboard(random);
        print(enter);
        print(enter1);
        print(dirty_board);

        // Start counting
        TACTL = TACLR + TASSEL_1 + MC_1;        // ACLK, upmode

        while (calc_time_barrier == 0);
        result[s] = divide(user_time_arr[s]);
        calc_time_barrier = 0;
        SPIN_ABOUT_A_SECOND;
    }
    finish_session = 1;

    // simpliCTi initialization
    BSP_Init();
    SMPL_Init(sRxCallback);

    /* never coming back... */
    linkTo();

    /* but in case we do... */
    while(1) ;
}



#pragma vector=USCIAB0RX_VECTOR
__interrupt void USCI0RX_ISR(void) {
    char valuchar;

    TACTL = MC_0;
    valuchar = UCA0RXBUF;
    UCA0TXBUF = valuchar;

    if (getin){
        if (time_exceeded){
            user_time_arr[ti++] = 30000;
            time_exceeded = 0;
        }
        else if ((valuchar - 48) == random)
            user_time_arr[ti++] = TAR;
        else
            user_time_arr[ti++] = 25000;

        // Barrier for waiting for calculating the time
        calc_time_barrier = 1;
    }
    getin = getin^1;
}


// it is timer interrupt handler
#pragma vector=TIMERA0_VECTOR
__interrupt void Timer_A(void) {
    time_exceeded = 1;
}



int divide(int num) {
    int count = 0;
    while (num > 256) {
        count += 1;
        num -= 256;
    }
    return count;
}


// Choosing the right board according to the random location of the mob
void hboard(int r) {
    if (r == 1)   dirty_board = "[x] [ ] [ ] [ ]";
    if (r == 2)   dirty_board = "[ ] [x] [ ] [ ]";
    if (r == 3)   dirty_board = "[ ] [ ] [x] [ ]";
    if (r == 4)   dirty_board = "[ ] [ ] [ ] [x]";
}


// UART initialization
void uart_init(){
    UCA0CTL1 |= UCSSEL_2; //use SMCLK as USCI 0 Clock Source
    UCA0BR0 = 104;
    UCA0BR1 = 0x00;  //1Mhz / 104 ~= 9600
    UCA0MCTL = 0x02; //UCBRS = 1, UCBRF = 0
    UCA0CTL1 &= ~UCSWRST; //start UART, initialize USCI state machine
    P3SEL |= 0x30;
}


static void linkTo(){
    uint8_t  msg[9], delay = 0;
    int i, offset;

    while (SMPL_SUCCESS != SMPL_Link(&sLinkID1))
        SPIN_ABOUT_A_SECOND;

    /* we're linked. */
    /*Turn on RX*/
    SMPL_Ioctl( IOCTL_OBJ_RADIO, IOCTL_ACT_RADIO_RXON, 0);


    for(i=0;i<11;i++)
        SPIN_ABOUT_A_SECOND;

    /* delay longer and longer -- then start over */
    delay = (delay + 1) & 0x03;

    /* put the sequence ID in the message */
    if (finish_session) {
        *msg = user_id;
        for (i = 0; i < 4; i++) {
            offset = 2 * i + 1;
            *(msg + offset) = result[i];
            *(msg + offset + 1) = user_time_arr[i] - result[i] * 256;
        }
        SMPL_Send(sLinkID1, msg, sizeof(msg));
        finish_session = 0;
    }
}


/* handle received frames. */
static uint8_t sRxCallback(linkID_t port){
    uint8_t msg[1],len;

    /* is the callback for the link ID we want to handle? */
    if (port == sLinkID1){
        /* yes. go get the frame. we know this call will succeed. */
        if ((SMPL_SUCCESS == SMPL_Receive(sLinkID1, msg, &len)) && len){
            if (*msg == user_id)
                BSP_TOGGLE_LED1();
            else
                BSP_TOGGLE_LED2();
        }
        return 1;
    }
    return 0;
}



//this function print number of loops in one second is
void print(char* char_arr){
    int i = 0;
    while (char_arr[i] != '\0') {
        while (!(IFG2 & UCA0TXIFG));
        UCA0TXBUF = char_arr[i];
        i++;
    }
}





