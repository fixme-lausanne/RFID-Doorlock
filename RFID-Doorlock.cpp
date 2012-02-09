#include "mbed.h"
#include "stdio.h"
#include "EthernetNetIf.h"
#include "HTTPClient.h"
#include "NTPClient.h"
#include <string>

#define DBNAME "/local/db.txt"
#define LOG "/local/logs.txt"
#define CARTESMAX 20
#define TAGMAX 15
#define MASTER "XXXXXXXXXX"
#define LOGLINEMAX 50
#define TWEETSIZE 141
#define HTTPRETRY 3
#define BASETIMEOUT 4000

LocalFileSystem local("local");

//RFID data & enable
Serial rfidIn(p9,p10);
DigitalOut enable(p11);

//H-Bridge controllers
DigitalOut a(p26);
DigitalOut b(p29);

//MBED Leds
DigitalOut readyLed(LED1);
DigitalOut greenLed(p23);
DigitalOut redLed(p22);
DigitalOut statusLed(LED2);
DigitalOut errorLed(LED4);


/*
 * 
        1
      2   6
        7
      4   8 
        9    3


*/

DigitalOut seg1(p5);
DigitalOut seg2(p6);
DigitalOut seg4(p7);
DigitalOut seg6(p8);
DigitalOut seg7(p12);
DigitalOut seg8(p13);
DigitalOut seg9(p14);

DigitalIn status(p16);
DigitalIn setTweet(p17);
DigitalIn up(p19);
DigitalIn down(p18);
DigitalIn sendTweet(p20);
EthernetNetIf eth;

int readDB(char tags[CARTESMAX][TAGMAX]);
int checkTag(char tags[CARTESMAX][TAGMAX], char tag[TAGMAX], int nbTags);
int giveResult(int statut);
int openDoor(int statut);
int logAttempt(char tag[TAGMAX], int valid);
int addTag(char tag[TAGMAX]);
void printTags(char tags[CARTESMAX][TAGMAX], int nbTags);
void dispDigit(int i);
int tweet(char tweetMsg[TWEETSIZE]);
int setupNetwork();

int main() {
    //INIT
    redLed = 1;
    a=1;
    b=1;
    dispDigit(11);
    enable = 1;
    printf("\n\r");

    //variables
    char tags[CARTESMAX][TAGMAX];
    char buffer[TAGMAX];
    char tweetMsg[TWEETSIZE];
    int valid = 0;
    int master = 0;
    int nbTags, hours, statusMove;
    
    //Baud settings for Serial
    rfidIn.baud(2400);
    
    //setup ethernet
    //redLed = !setupNetwork(eth);

    //Read tags & nbTags
    nbTags = readDB(tags);
    if(nbTags>0){
        printf("READ DB OK (%d tags)\n\r",nbTags);
        readyLed = 1;
        redLed = 0;
        enable = 0;
    } else {
        printf("READ DB NOT OK\n\r");
        while(1) {
                errorLed = !errorLed;
                wait(0.5);
        }
    }
    if (status) {
        hours = 1;
    } else {
        hours = 42;
    }
    statusMove = status;
        
    while(1) {
        dispDigit(hours);
        if (rfidIn.readable() && enable == 0 && master == 0 && !setTweet){
            enable = 1;
            rfidIn.scanf("%s", buffer);
            if(strcmp (buffer,MASTER) == 0){
                master = 1;
            } else {
                valid = checkTag(tags, buffer, nbTags);
                logAttempt(buffer, valid);
                printf("%s", buffer);
                printf("   >>> %d\n\r", valid);
                giveResult(valid);
                enable = openDoor(valid);
            }
        }
        if (master == 1 && !setTweet){
            printTags(tags, nbTags);
            wait(3);
            printf("Scan new card now!\n\r");
            enable = 0;
            while(1) { //issue if card is not readable
                if (rfidIn.readable()){
                    enable = 1;
                    rfidIn.scanf("%s", buffer);
                    addTag(buffer);
                    break;
                }
            }
            nbTags = readDB(tags);
            if(nbTags>0){
                printf("READ DB OK (%d tags)\n\r",nbTags);
            } else {
                printf("READ DB NOK\n\r");
                while(1){
                    errorLed = !errorLed;
                    wait(0.5);
                }
            }
            printTags(tags, nbTags);
            wait(3);
            master = 0;
            enable = 0;
        }
        if (status && !statusMove){
            hours = 1;
            statusMove = status;
        }
        if (!status && statusMove){
            hours = 42;
            statusMove = status;
        }
        if (status && up && hours < 9){
            wait(.3);
            hours++; 
        }
        if (status && down && hours > 1){
            wait(.3);
            hours--;
        }
        if (sendTweet){
            wait(.3);
            if(status){
                sprintf(tweetMsg, "do=custom&hours=%d",hours);
            } else {
                strcpy(tweetMsg, "do=close");
            }
            setupNetwork();
            giveResult(tweet(tweetMsg));
        }              
    }
}


//FUNCTIONS
int readDB(char tags[CARTESMAX][TAGMAX]) {
    FILE *fichier;
    int i;
    int result = 0;
    char ligne[TAGMAX];
    fichier = fopen(DBNAME,"r");
    if (fichier != NULL) {
        i=0;
        fgets (ligne, TAGMAX, fichier);
        while (!feof(fichier)) {
            sscanf (ligne, "%s", tags[i]);
            i++;
            fgets (ligne, TAGMAX, fichier);
        }
        result = i;
        fclose (fichier);
        
    }
    return result;
}

int checkTag(char tags[CARTESMAX][TAGMAX], char tag[TAGMAX], int nbTags) {
    for (int i = 0;i < nbTags;++i){
        if (strcmp(tag, tags[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

int giveResult(int statut){
    switch ( statut ){
        case 0:
            redLed = 1;
            wait(0.5);
            redLed = 0;
            break;
         case 1:
            greenLed = 1;
            wait(0.5);
            greenLed = 0;
            break;
    }
    return NULL;
}

int openDoor(int statut){
    if (statut == 1){
        a = 0;
        wait(0.5);
        a = 1;
        wait(5);
        b = 0;
        wait(0.5);
        b = 1;
    }
    return 0;
}

int logAttempt(char tag[TAGMAX], int valid){
    FILE *fichier;
    char ligne[LOGLINEMAX];
    fichier = fopen(LOG, "a");
    if (fichier == NULL) {
        printf("cant find %s\n\r",LOG);
    } else {
        sprintf(ligne, "%s >>> %i\n", tag, valid);
        fputs(ligne, fichier);
        fclose(fichier);
    }
    return NULL;
}

int addTag(char tag[TAGMAX]){
    FILE *fichier;
    char ligne[TAGMAX];
    int result = 0;
    fichier = fopen(DBNAME, "a");
    if (fichier != NULL) {
        sprintf(ligne, "%s\n", tag);
        fputs(ligne, fichier);
        fclose(fichier);
        result = 1;
        printf("Added %s on db.txt\n\r",ligne);
    } else {
         printf("cant find %s\n\r",DBNAME);
    }
    return result;
}

void printTags(char tags[CARTESMAX][TAGMAX], int nbTags){
    printf("List of cards allowed:\n\r");
    for(int i = 0;i < nbTags;++i){
        printf("%s\n\r",tags[i]);
    }
}

void dispDigit(int i){
    switch(i){
        case 1: seg1 = 1;
                seg2 = 1;
                seg4 = 1;
                seg6 = 0;
                seg7 = 1;
                seg8 = 0;
                seg9 = 1;
                break;
        case 2: seg1 = 0;
                seg2 = 1;
                seg4 = 0;
                seg6 = 0;
                seg7 = 0;
                seg8 = 1;
                seg9 = 0;
                break;
        case 3: seg1 = 0;
                seg2 = 1;
                seg4 = 1;
                seg6 = 0;
                seg7 = 0;
                seg8 = 0;
                seg9 = 0;
                break;
        case 4: seg1 = 1;
                seg2 = 0;
                seg4 = 1;
                seg6 = 0;
                seg7 = 0;
                seg8 = 0;
                seg9 = 1;
                break;
        case 5: seg1 = 0;
                seg2 = 0;
                seg4 = 1;
                seg6 = 1;
                seg7 = 0;
                seg8 = 0;
                seg9 = 0;
                break;
        case 6: seg1 = 0;
                seg2 = 0;
                seg4 = 0;
                seg6 = 1;
                seg7 = 0;
                seg8 = 0;
                seg9 = 0;
                break;
        case 7: seg1 = 0;
                seg2 = 1;
                seg4 = 1;
                seg6 = 0;
                seg7 = 1;
                seg8 = 0;
                seg9 = 1;
                break;
        case 8: seg1 = 0;
                seg2 = 0;
                seg4 = 0;
                seg6 = 0;
                seg7 = 0;
                seg8 = 0;
                seg9 = 0;
                break;
        case 9: seg1 = 0;
                seg2 = 0;
                seg4 = 1;
                seg6 = 0;
                seg7 = 0;
                seg8 = 0;
                seg9 = 0;
                break;
        case 0: seg1 = 0;
                seg2 = 0;
                seg4 = 0;
                seg6 = 0;
                seg7 = 1;
                seg8 = 0;
                seg9 = 0;
                break;
        case 42:seg1 = 0;
                seg2 = 0;
                seg4 = 0;
                seg6 = 1;
                seg7 = 1;
                seg8 = 1;
                seg9 = 0;
                break;
        default: seg1 = 1;
                seg2 = 1;
                seg4 = 1;
                seg6 = 1;
                seg7 = 1;
                seg8 = 1;
                seg9 = 1;
                break;
    }
}

int tweet(char tweetMsg[TWEETSIZE]){
    int success = 0;
    HTTPClient twitter;
    twitter.setTimeout(BASETIMEOUT);
    char url[177];
    sprintf(url, "http://fixme.ch/cgi-bin/twitter.pl?%s", tweetMsg);
    
    HTTPResult r;
    for (int trying = 0; !success && (trying < HTTPRETRY);++trying) {
        r = twitter.get(url, NULL);
        if( r == HTTP_OK ) {
            printf("Tweet sent with success!\n\r");
            success = 1;
        } else {
            printf("Problem during tweeting, return code %d\n", r);
            success = 0;
        }
    }
    return success;
}

int setupNetwork() {
    int result = 1;
    printf("Init\n\r");
    if(eth.getIp().isNull()){
        printf("Setting up...\n\r");
        //the eth.setup without parameter can be called, the timout will
        //be automatically set.
        //see : http://mbed.org/cookbook/Ethernet
        if(eth.setup() == ETH_TIMEOUT){
            printf("Error %d in setup.\n\r", ethErr);
            result = 0;
        }
    }
    
    return result;
}


