//
//
//

#include <stddef.h>     
#include <stdio.h>      
#include <stdlib.h>     
#include <windows.h>    
#include <conio.h>      

#define semaphore HANDLE

//wait for a semaphore
void wait(semaphore h) 
{
    WaitForSingleObject(h, MAXLONG);
}

//signal a semaphore
void signal(semaphore h) 
{
    ReleaseSemaphore(h, 1, NULL);
}

// create a semaphore with value v
semaphore create(int v) 
{
    return CreateSemaphore(NULL, (long)v, MAXLONG, NULL);
}

//
//	The size of the buliding
//
#define NLIFTS 4        /* the number of lifts in the buliding */
#define NFLOORS 20      /* the number of floors in the building */
#define NPEOPLE 20      /* the number of people in the building */
#define MAXNOINLIFT 10  /* maximum lift capacity */
//
//	Some times (all in milliseconds)
//
#define LIFTSPEED 50    /* the time it takes for the lift to move one floor */
#define GETINSPEED 50   /* the time it takes to get into the lift */
#define GETOUTSPEED 50  /* the time it takes to get out of the lift */
#define PEOPLESPEED 100 /* the maximum time a person spends on a floor */
//
//	Directions
//
#define UP 1
#define DOWN -1

#define rnd(i) (rand()%(i))  /* a random number from 0..i-1 inclusive */
int rand_seed = 123456;
#define randomise() srand(rand_seed);rand_seed=rand();
//
//	Information about a floor in the building
//
typedef struct 
{
    int waitingtogoup;    
    int waitingtogodown;  
    semaphore up_arrow;   
    semaphore down_arrow; 
} Floor_info;
//
//	Lift
//
typedef struct 
{
    int no;               
    int position;         
    int direction;        
    int peopleinlift;     
    int stops[NFLOORS];   /* for each stop how many people are waiting to get off */
    semaphore stopsem[NFLOORS]; 
} Lift_info;
//
//	Some global variables
//
Floor_info floor[NFLOORS];
Lift_info* global_lift_ptr;     // To pass the Lift_info pointer
semaphore mutex_for_ostream;    // To lock IO stream
//
//	print a character c on the screen at position (x,y)
//
void char_at_xy(int x, int y, char c) 
{
    wait(mutex_for_ostream);    //lock IO stream
    gotoxy(x, y);
    Sleep(1);        
    printf("%c", c);
    signal(mutex_for_ostream);  //release IO stream
}
//
//	Tell everybody that is waiting to go in a certain direction
// to get into the lift. If the lift was empty then move it in the correct direction.
//
void getintolift(Lift_info *l, int direction) 
{
    int *waiting;
    semaphore *s;
    if (direction == UP) 
    {  
        s = &floor[l->position].up_arrow;  
        waiting = &floor[l->position].waitingtogoup;  
    }
    else 
    {               
        s = &floor[l->position].down_arrow; 
        waiting = &floor[l->position].waitingtogodown; 
    }
    while (*waiting) 
    {    
        if (!l->peopleinlift)  /* if the lift is empty */
            l->direction = direction; /* set direction */
        if (l->peopleinlift < MAXNOINLIFT && *waiting) {
            l->peopleinlift++;  /* one more in the lift */
            char_at_xy(NLIFTS * 4 + floor[l->position].waitingtogodown + floor[l->position].waitingtogoup, NFLOORS - l->position, ' '); /* erase from screen */
            (*waiting)--;       /* one less waiting */
            Sleep(GETINSPEED);  /* wait a short time */

            global_lift_ptr = l;    // save this lift into shared data
            signal(*s);             // signal
        }
        else 
        {
            break;
        }
    }
}
//
//	A lift
//
unsigned long CALLBACK lift_thread(void *p) {
    Lift_info l;
    int i;
    int no = (int)p;

    l.no = no;          /* set the no for this lift */
    l.position = 0;     /* set the initial position */
    l.direction = 1;    /* and direction */
    l.peopleinlift = 0; /* empty */
    randomise();
    for (i = 0; i < NFLOORS; i++) {
        l.stops[i] = 0;   /* nobody is waiting in the lift to go anywhere */
        l.stopsem[i] = create(0);
    }
    Sleep(rnd(1000)); /* wait for a while */
    while (1) {        /* do forever */
        char_at_xy(no * 4 + 1, NFLOORS - l.position, 0xdb); /* draw current position of lift */
        Sleep(LIFTSPEED);  /* wait a while */
        while (l.stops[l.position] != 0) {  /* drop off all passengers */
            l.peopleinlift--;        /* one less in lift */
            l.stops[l.position]--;   /* one less waiting */
            Sleep(GETOUTSPEED);      /* wait a while */

            signal(l.stopsem[l.position]);  //signal
 
            if (!l.stops[l.position]) /* if it was the last one */
                char_at_xy(no * 4 + 1 + 2, NFLOORS - l.position, ' ');  /* remove the - */
        }
        if (l.direction == UP || !l.peopleinlift)   /* if the lift is going up or is empty */
            getintolift(&l, UP);                    /* pick up passengers waiting to go up */
        if (l.direction == DOWN || !l.peopleinlift) /* if the lift is going down or is empty */
            getintolift(&l, DOWN);                  /* pick up passengers waiting to go down */
        char_at_xy(no * 4 + 1, NFLOORS - l.position, (char)(l.direction + 1 ? ' ' : 0xb3));  /* erase the lift on the screen */
        l.position += l.direction;                  /* move it */
        if (l.position == 0 || l.position == NFLOORS - 1)  /* if it is at the top or bottom */
            l.direction = -l.direction;                     /* change direction */
    }
}
//
//	A person
//
unsigned long CALLBACK person_thread(void *p) {
    int from = 0, to; /* start on the ground floor */
    Lift_info *l;

    randomise();
    while (1) {     /* stay in the building forever */
        Sleep(rnd(PEOPLESPEED)); /* work for a while */
        do
            to = rnd(NFLOORS);
        while (to == from);	/* pick a different floor to go to*/
        if (to > from) {	/* if we are going up */
            floor[from].waitingtogoup++;
            char_at_xy(NLIFTS * 4 + floor[from].waitingtogoup + floor[from].waitingtogodown, NFLOORS - from, 0xdc);

            wait(floor[from].up_arrow);     //wait
        }
        else {  /* if we are going down */
            floor[from].waitingtogodown++;
            char_at_xy(NLIFTS * 4 + floor[from].waitingtogodown
                + floor[from].waitingtogoup, NFLOORS - from, 0xdc);

            wait(floor[from].down_arrow);   //wait
        }

        l = global_lift_ptr;                //get Lift pointer from shared data

        l->stops[to]++;  /* press the button for the floor we want */
        if (l->stops[to] == 1)  /* light up the button if we were the first */
            char_at_xy(l->no * 4 + 1 + 2, NFLOORS - to, '-');

        wait(l->stopsem[to]);               //wait

        from = to;  /* we have reached our destination */
    }
}
//
//	Print the building on the screen
//
void printbuilding(void) {
    int l, f;

    clrscr();                  /* clear the screen */
    _setcursortype(_NOCURSOR); // don't display the cursor
    printf("\xda");
    for (l = 0; l < NLIFTS - 1; l++)         /* roof */
        printf("\x0c4\x0c2\x0c4\x0c2");
    puts("\x0c4\x0c2\x0c4\x0bf");
    for (f = NFLOORS - 1; f >= 0; f--)
        for (l = 0; l < NLIFTS; l++) {       /* floors */
            printf("\x0b3\x0b3\x0b3 ");
            if (l == NLIFTS - 1)
                puts("\x0b3");
        }
    printf("Lift Simulation - Press CTRL-Break to exit");
}
//
//	main starts the threads and then just sits there.
//
int main() {
    unsigned long i, id;

    for (i = 0; i < NFLOORS; i++) {
        floor[i].waitingtogoup = 0;     /* initialise the building */
        floor[i].waitingtogodown = 0;
        floor[i].up_arrow = create(0);
        floor[i].down_arrow = create(0);
    }
    /* initialise any other semaphores */
    mutex_for_ostream = create(1);
    printbuilding();        /* print the buildong on the screen */
    for (i = 0; i < NLIFTS; i++)   /* create the lift threads */
        CreateThread(NULL, 0, lift_thread, (void *)i, 0, &id);

    for (i = 0; i < NPEOPLE; i++)  /* create the people threads */
        CreateThread(NULL, 0, person_thread, (void *)i, 0, &id);

    Sleep(86400000); /* go to sleep for 86400 seconds (one day) */
    return(1);
}
