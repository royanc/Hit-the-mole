#include <stdlib.h>
#include "bsp.h"
#include "mrfi.h"
#include "nwk_types.h"
#include "nwk_api.h"
#include "bsp_leds.h"
#include "bsp_buttons.h"
#include "app_remap_led.h"



/*************************************Functions declaretions*************************************/
static void linkTo(void);
static uint8_t sRxCallback(linkID_t);   // application Rx frame handler.
void toggleLED(uint8_t);
// reserve space for the maximum possible peer Link IDs
static linkID_t sLinkIDs[NUM_CONNECTIONS] = {0};
static int ids[NUM_CONNECTIONS] = {0};
int inArray(linkID_t who);


/*************************************Defines and globals**************************************/
#define SPIN_ABOUT_A_SECOND  NWK_DELAY(1000)
#define NUM_OF_SESSIONS 4
#define NUM_CONNECTIONS  2
int scores[NUM_CONNECTIONS * 4];
uint8_t received_msgs = 0, winner = 0;
uint8_t total_u1 = 0, total_u2 = 0;


void main(void) {
    BSP_Init();

    SMPL_Init(sRxCallback);

    /* never coming back... */
    linkTo();

    /* but in case we do... */
    while(1);
}

static void linkTo(){
    uint8_t msg[1], numConnections=0;
    uint16_t  c, winner;
    int tries =0;
    /* blink LEDs until we link successfully the players */
    while(numConnections < NUM_CONNECTIONS) {
        if(SMPL_SUCCESS == SMPL_LinkListen(&sLinkIDs[numConnections]))     //SMPL_Link()
            numConnections++;

        if (numConnections<1) toggleLED(1);
        if (numConnections<2) toggleLED(2);

    }


    /*Verify that the leds shut down*/
    if(BSP_LED2_IS_ON())
        toggleLED(2);
    if(BSP_LED1_IS_ON())
        toggleLED(1);


    /* turn on RX. default is RX off. */
    SMPL_Ioctl(IOCTL_OBJ_RADIO, IOCTL_ACT_RADIO_RXON, 0);       //Maybe not important

        while (received_msgs < 2){
            SPIN_ABOUT_A_SECOND;
            tries ++;
            if (tries > 60)
                break;
        }
        if (received_msgs == 2) {
        for (c = 0; c < 4; c++) {
            if (scores[c] > scores[4 + c])
                total_u1 += 1;
            else if (scores[c] < scores[4+c])
                total_u2 += 1;
        }


        if (total_u1 > total_u2)
            winner = ids[1];
        else if (total_u1 < total_u2)
            winner = ids[0];
        else
            winner = 3;

        received_msgs = 0;
        total_u1 = 0;
        total_u2 = 0;
        *msg = winner;
         SMPL_Send(sLinkIDs[0], msg, sizeof(msg));
         SMPL_Send(sLinkIDs[1], msg, sizeof(msg));
    }


}

void toggleLED(uint8_t which) {
    if(1 == which) {
        BSP_TOGGLE_LED1();
    }
    else if(2 == which) {
        BSP_TOGGLE_LED2();
    }
    return;
}


int inArray(linkID_t who) {
    int8_t i;

    for(i=NUM_CONNECTIONS-1; i>=0; i--) {
        if(sLinkIDs[i] == who) return i;
    }
    return -1;
}


/* handle received frames. */
static uint8_t sRxCallback(linkID_t port) {
    uint8_t msg[9], len;
    int  g=0, arrID = 0;


    if ((arrID = inArray(port)) >= 0) {
        if ((SMPL_SUCCESS == SMPL_Receive(sLinkIDs[arrID], msg, &len)) && len) {
            ids[arrID] = msg[0];
            for (g = 0; g < 4; g++)
                scores[arrID*4 + g] = (*(msg + 2 * g + 1)) * 256 + *(msg + 2 * g + 2);

            received_msgs += 1;
        }
    }

    return 0;
}
