#include <stdio.h>
#include <stdlib.h>
#include<stdbool.h>
#include <math.h>
#include <time.h>
#include<unistd.h>

#include<pthread.h>
#include<semaphore.h>
sem_t sofa, mutex, inTreatment, startTreatment, endTreatment, outsideClinic, cPayment, endPayment, recPayment, payLock;


// add semaphores here

// convenience
#define UP(x) sem_post(x)
#define DOWN(x) sem_wait(x)

#define N 10
#define MAX_QUEUE_SIZE N + 2

#define MAX(x,y) (((x)>(y)) ? (x) : (y))
size_t Occupancy = 0;

int x = 0;

// a queue implementation using a circular buffer (array)
typedef struct CircularBufferQueue {
    int arr[MAX_QUEUE_SIZE];
    size_t pushIdx, popIdx, currSize, maxSize;
}CircularBufferQueue;

// initialize sofa to size 4, Lobby to size max(1,N - 4)
CircularBufferQueue Sofa, Lobby;

// -- DECLARATIONS --

// required for queue
void push(CircularBufferQueue* b, const int e);
int pop(CircularBufferQueue* b);
void init_CircularBuffer(CircularBufferQueue* b, const size_t size);

// thread related

// thread functions
void* dentalAssistant(void* args);
void* patientRun(void* args);

// patient activity
void enterClinic(const int n);
void sitOnSofa(const int n);
void getTreatment(const int n);
void givePayment(const int n);
void outOfClinic(const int n);

// dental assistant activity
void treat(const int dn);
void dentistAcceptPayment(const int dn);
int main(void) {
    sem_init(&sofa, 0, 4);
    sem_init(&mutex, 0, 1);
    sem_init(&inTreatment, 0, 3);
    sem_init(&startTreatment, 0, 0);
    sem_init(&endTreatment, 0, 0);
    sem_init(&outsideClinic, 0, 0);
    sem_init(&cPayment, 0, 0);
    sem_init(&endPayment, 0, 0);
    sem_init(&recPayment, 0, 0);
    sem_init(&payLock, 0, 1);
    init_CircularBuffer(&Sofa, 4);
    // max for case of N < 4 (assumption: lobby cannot be empty)
    init_CircularBuffer(&Lobby, N);

    // create threads and lobby
    int i, arr[N + 2];
    pthread_t custs[N + 2], dents[3];
    // run the threads
    for (i = 0;i < N + 2;i++) {
        arr[i] = i;
        if (pthread_create(&custs[i], NULL, patientRun, &arr[i]) != 0) {
            printf("Error pthread create\n");exit(1);
        }
    }
    for (i = 0;i < 3;i++) {
        if (pthread_create(&dents[i], NULL, dentalAssistant, &arr[i]) != 0) {
            printf("Error pthread create\n");exit(1);
        }
    }for (i = 0; i < 3; i++) {
        //join specific thread of patient
        pthread_join(dents[i], NULL);
    }
    while (1); // wait indefinitely

    return 0;
}void push(CircularBufferQueue* b, const int e) {
    // code for pushing to queue (enqueue)
    if (b->currSize == b->maxSize) {
        fprintf(stderr, "ERROR -- pushing to a full buffer!\n"); exit(1);
    }
    // insert to push index (from 0 onwards)
    b->arr[b->pushIdx] = e;
    // set both push and pop to the same index
    // if size is 0
    if (b->currSize == 0) {
        b->popIdx = b->pushIdx;
    }b->pushIdx = (b->pushIdx + 1) % b->maxSize;
    // increment size
    b->currSize++;
}

int pop(CircularBufferQueue* b) {
    // code for popping from queue (dequeue)
    int tmp = b->arr[b->popIdx]; // grab element
    if (b->currSize == 0) {
        fprintf(stderr, "ERROR -- popping an empty buffer!\n"); exit(1);
    }
    // increment pop index % size
    b->popIdx = (b->popIdx + 1) % b->maxSize;
    // increment size
    b->currSize--;
    return tmp;
}void init_CircularBuffer(CircularBufferQueue* b, const size_t size) {
    // initialize a queue as a circular buffer
    b->currSize = b->popIdx = b->pushIdx = 0;
    b->maxSize = size;
}

// threads:

// dentist
void* dentalAssistant(void* args) {
    int p_num, d_num = *(int*)args;
    while (1) {
        DOWN(&startTreatment);
        // get patient inside
        // receive patient for treatment

        // do the treatment
        treat(d_num);
        // inform the rest that treatment is over
        UP(&endTreatment);

        DOWN(&cPayment);
        // accept the payment from patient
        dentistAcceptPayment(d_num);
        // inform payment is over
        UP(&payLock);
        // decrease occupancy (invite the patient to leave)
        UP(&inTreatment);
        DOWN(&mutex); Occupancy--;
        UP(&mutex);
        // allow another customer to enter
        UP(&outsideClinic);
    }
}
void* patientRun(void* args) {
    int p_num = *(int*)args;
    // atomic check of resource: patients


    while (1) {
        DOWN(&mutex);
        if (Occupancy < N) {
            Occupancy++; // increases occupancy
            UP(&mutex);
            // patient stands up (enters standing queue)
            DOWN(&mutex);
            push(&Lobby, p_num);UP(&mutex);
            // successfuly entered clinic
            enterClinic(p_num);
            // patient waits for place on sofa
            DOWN(&sofa);

            // if sofa is available, remove patient from queue
            DOWN(&mutex);
            p_num = pop(&Lobby);
            UP(&mutex);DOWN(&mutex);
            push(&Sofa, p_num);
            UP(&mutex);

            // currently on sofa
            sitOnSofa(p_num);

            // wait for treatment chair to free
            DOWN(&inTreatment);

            // get patient
            DOWN(&mutex);
            p_num = pop(&Sofa);
            UP(&mutex);
            getTreatment(p_num);

            // release sofa
            UP(&sofa);

            // patient gets up for treatment
            UP(&startTreatment);

            // treatment is over
            DOWN(&endTreatment);

            // wait for being allowed to pay
            DOWN(&payLock);
            // pay
            givePayment(p_num);
            // inform payment is over
            UP(&cPayment);sleep(1);

        }
        else {
            UP(&mutex);
            // wait outside until you can reenterush
            outOfClinic(p_num);
            // try to re-enter when possible
            DOWN(&outsideClinic);
        }
    }
}
void enterClinic(const int n) {

    printf("I'm Patient #%d, I got into the clinic\n", n);

    sleep(1);
}

void sitOnSofa(const int n) {


    printf("I'm Patient #%d, I'm sitting on the sofa\n", n);

    sleep(1);
}void getTreatment(const int n) {


    printf("I'm Patient #%d, I'm getting treatment\n", n);

    sleep(1);
}

void givePayment(const int n) {

    printf("I'm Patient #%d, I'm paying now\n", n);
    sleep(1);

}void outOfClinic(const int n) {


    printf("I'm Patient #%d, I'm out of clinic \n", n);

    sleep(1);

}

// dental assistant activity
void treat(const int dn) {


    printf("I'm Dental Hygienist #%d, I'm working now\n", dn);
    sleep(1);
}
void dentistAcceptPayment(const int dn) {


    printf("I'm Dental Hygienist #%d, I'm getting payment\n", dn);


    sleep(1);
}