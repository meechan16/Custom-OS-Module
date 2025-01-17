#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/time.h>
#include <stdbool.h>
#include <semaphore.h>

#define MAX_PID 30

typedef struct task{
    pid_t pid; // PID of task
    int flag; // It's running stats -1 -> unused 0->ready 1->running 2->terminated
    int pri; // Priority 4 ->Highest  1->Lowest     default is 4
    double execTime ; //Execution time
    struct timeval startTime ; //Its starts time
    struct timeval termination_time ; //Termination time
    double waitTime ; // Its wait time
    char * command ; //Name of command or file
}task ;

typedef struct queue {
    sem_t sem ;
    int rear;
    int front;

    task * array[MAX_PID] ;

    task *(*dequeue)(struct queue *);
    void (*enqueue)(struct queue * , task *);
    bool (*is_empty)(struct queue *);
}queue;

task * dequeue(queue * q)
{
    if(q->is_empty(q)){
        return NULL ;
    }

    q->front = (q->front + 1) % MAX_PID ;
    return q->array[q->front] ;

}

void enqueue(queue * q , task * t)
{
    if((q->rear + 1)% MAX_PID == q->front)
    {
        printf("Queue is full") ;
        return ;
    }

    q->rear = (q->rear + 1) %  MAX_PID ;
    q->array[q->rear] = t ;


}

bool is_empty(queue * q) {
    return (q->rear == q->front);
}


typedef struct shm_t
{
    int ncpu; // No. of cpu
    unsigned int tslice ;  // Time Slice in RR
    int top; // Top element in PT
    int count ; // No. of items in terminatedTask
    int terminatedTop ; //Top element of terminatedTask
    task PT[MAX_PID] ; //Program table
    queue readyQueue1; // Ready queue with priority 1
    queue readyQueue2; // Ready queue with priority 2
    queue readyQueue3; // Ready queue with priority 3
    queue readyQueue4;// Ready queue with priority 4
    queue runningQueue;//Running processes queue
    task terminatedTask[100]; //List of terminated task
    pid_t scheduler_pid ; //Scheduler's PID
    sem_t history; // Semaphore for history
    sem_t sem; // Semaphore for manupulation in queues
    sem_t null_sem; // Semaphore for manupulation in PT


}shm_t ;

shm_t * shm ;

int shm_fd;
struct shmmem* setup() {
    char *shm_name = "PT"; // Program Table Name
    shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666); // File Descriptor
    if (shm_fd == -1) {
        perror("Shared memory failed");
        exit(1);
    }

    ftruncate(shm_fd, sizeof(struct shm_t)); // Setting size of shared memory
    
    struct shmmem *shm = mmap(NULL, sizeof(struct shm_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0); // Mapping space for shm
    if (shm == MAP_FAILED) {
        perror("Mapping Failed");
        exit(1);
    }

    return shm;
}


void init(shm_t * shm) {

    //Initialing default values of all queues, semaphores and mutex

    sem_init(&(shm->readyQueue1.sem) , 1 , 1) ;
    sem_init(&(shm->readyQueue2.sem) , 1 , 1) ;
    sem_init(&(shm->readyQueue3.sem) , 1 , 1) ;

    sem_init(&(shm->readyQueue4.sem) , 1 , 1) ;
    sem_init(&(shm->runningQueue.sem) , 1 , 1) ;
    sem_init(&(shm->null_sem) , 1 , 0) ;
    sem_init(&(shm->history),1,1);
    sem_init(&(shm->sem) , 1 , 1) ;
    shm->readyQueue4.front = 0 ;
    shm->readyQueue4.rear = 0  ;
    shm->readyQueue3.front = 0 ;
    shm->readyQueue3.rear = 0  ;
    shm->readyQueue2.front = 0;
    shm->readyQueue2.rear = 0  ;
    shm->readyQueue1.front = 0;
    shm->readyQueue1.rear = 0 ;

    shm->runningQueue.front = 0 ;
    shm->runningQueue.rear = 0 ;

    shm->readyQueue1.dequeue = dequeue ;
    shm->readyQueue1.enqueue = enqueue ;
    shm->readyQueue1.is_empty = is_empty ;

    shm->readyQueue2.dequeue = dequeue ;
    shm->readyQueue2.enqueue = enqueue ;
    shm->readyQueue2.is_empty = is_empty ;

    shm->readyQueue3.dequeue = dequeue ;
    shm->readyQueue3.enqueue = enqueue ;
    shm->readyQueue3.is_empty = is_empty ;

    shm->readyQueue4.dequeue = dequeue ;
    shm->readyQueue4.enqueue = enqueue ;
    shm->readyQueue4.is_empty = is_empty ;

    shm->runningQueue.dequeue = dequeue ;
    shm->runningQueue.enqueue = enqueue ;
    shm->runningQueue.is_empty = is_empty ;
    shm->count = 0 ;
    shm->top = 0 ;
    shm->terminatedTop  = 0 ;
    int j = 0 ;
    while(j < MAX_PID){
        shm->PT[j].pid = 0 ;
        shm->PT[j].flag = -1  ;
        shm->PT[j].waitTime = 0 ;
        shm->PT[j].execTime = 0 ;
        shm->PT[j].startTime = (struct timeval){0} ;
        shm->PT[j].termination_time = (struct timeval){0} ;
        shm->terminatedTask[j].flag = -1  ;
        shm->terminatedTask[j].pid = 0 ;
        shm->terminatedTask[j].waitTime = 0 ;
        shm->terminatedTask[j].execTime = 0 ;
        shm->terminatedTask[j].startTime = (struct timeval){0} ;
        shm->terminatedTask[j].termination_time = (struct timeval){0} ;
        shm->readyQueue1.array[j] = NULL;
        shm->readyQueue2.array[j] = NULL;
        shm->readyQueue3.array[j] = NULL;
        shm->readyQueue4.array[j] = NULL;
        shm->runningQueue.array[j] = NULL;
        j++ ;
    }

}

void displayHistory() {

    // printf("%-8s %-10s %-10s %-20s %-20s %-15s %-15s\n",
    //        "PID", "Priority", "Command", "Start Time", "Termination Time", "Exec. Time", "Waiting Time");

    const int n = shm->terminatedTop;
    for (int i = 0; i < n; i++) {

        printf("\n\n\n");
        // printf("%-8d  %-10d  %-10s  %-20ld  %-20ld  %-20f  %-20f",shm->terminatedTask[i].pid,shm->terminatedTask[i].pri,shm->terminatedTask[i].command, shm->terminatedTask[i].startTime.tv_sec, shm->terminatedTask[i].termination_time.tv_sec, shm->terminatedTask[i].execTime, shm->terminatedTask[i].waitTime);
        printf("PID: %d\n",shm->terminatedTask[i].pid);
        printf("Priority: %d\n",shm->terminatedTask[i].pri);
        printf("Command: %s\n",shm->terminatedTask[i].command);
        printf("Start time: %ld\n",shm->terminatedTask[i].startTime.tv_sec);
        printf("Termination time: %ld\n",shm->terminatedTask[i].termination_time.tv_sec);
        printf("Execution time: %f\n",shm->terminatedTask[i].execTime);
        printf("Waiting Time: %f\n",shm->terminatedTask[i].waitTime);
        printf("\n\n\n");


    }
    sem_post(&(shm->history));
}

void clean() {

      //Cleaing and destroying memory

    sem_destroy((&(shm->sem)));
    sem_destroy((&(shm->null_sem)));
    sem_destroy(&(shm->readyQueue1.sem));
    sem_destroy(&(shm->readyQueue2.sem));
    sem_destroy(&(shm->readyQueue3.sem));
    sem_destroy(&(shm->readyQueue4.sem));
    sem_destroy(&(shm->runningQueue.sem));
    munmap(shm, sizeof(shm_t));
    shm_unlink("PT");
    kill(getpid(), SIGKILL);
}

void scheduler_exit() {
   printf("in scheduler exit");
   kill(getpid(), SIGKILL);
}

void handle_exit(int sig) {

    //Handling exit fo SIGNIT

    sem_wait(&(shm->history));
    sem_wait(&(shm->runningQueue.sem));
    while (!(shm->runningQueue.is_empty(&shm->runningQueue)))
    {

        task * t = shm->runningQueue.dequeue(&shm->runningQueue);
        if (t != NULL) {
            int pid = fork();
            if (pid < 0) {
                printf("Error");
            }
            else if (pid == 0) {
                char *args[] = { t->command , NULL};
                execvp(t->command,args);
                printf("Can't find command\n");
            }
            else {
                int wstatus;
                waitpid(pid, &wstatus , 0) ;

                printf("running queue");
                t->flag = 2;
                gettimeofday(&(t->termination_time),NULL);
                double turnaround = (t->termination_time.tv_sec - t->startTime.tv_sec)*1000.0
                        + (t->termination_time.tv_usec - t->startTime.tv_usec) / 1000.0;
                t->execTime = turnaround - t->waitTime;
                shm->terminatedTask[shm->terminatedTop] = *t;
                shm->terminatedTop++;
                kill(t->pid, SIGKILL);


            }
        }
    }
    sem_post(&(shm->runningQueue.sem));
    sem_wait(&(shm->readyQueue4.sem));
    while (!(shm->readyQueue4.is_empty(&shm->readyQueue4)))
    {
        task * t = shm->readyQueue4.dequeue(&shm->readyQueue4);
        if (t != NULL) {
            int pid = fork();
            if (pid < 0) {
                printf("Error");
            }
            else if (pid == 0) {
                char *args[] = { t->command , NULL};
                execvp(t->command,args);
                printf("Can't find command\n");
            }
            else {
                int wstatus;
                waitpid(pid, &wstatus , 0) ;
                t->flag = 2;
                gettimeofday(&(t->termination_time),NULL);
                double turnaround = (t->termination_time.tv_sec - t->startTime.tv_sec)*1000.0
                        + (t->termination_time.tv_usec - t->startTime.tv_usec) / 1000.0;
                t->execTime = turnaround - t->waitTime;
                shm->terminatedTask[shm->terminatedTop] = *t;
                shm->terminatedTop++;
                kill(t->pid, SIGKILL);
            }
        }
    }
    sem_post(&(shm->readyQueue4.sem));
    sem_wait(&(shm->readyQueue3.sem));
    while (!(shm->readyQueue3.is_empty(&shm->readyQueue3)))
    {
        task * t = shm->readyQueue3.dequeue(&shm->readyQueue3);
        if (t != NULL) {
            int pid = fork();
            if (pid < 0) {
                printf("Error");
            }
            else if (pid == 0) {
                char *args[] = { t->command , NULL};
                execvp(t->command,args);
                printf("Can't find command\n");
            }
            else {
                int wstatus;
                waitpid(pid, &wstatus , 0) ;
                t->flag = 2;
                gettimeofday(&(t->termination_time),NULL);
                double turnaround = (t->termination_time.tv_sec - t->startTime.tv_sec)*1000.0
                        + (t->termination_time.tv_usec - t->startTime.tv_usec) / 1000.0;
                t->execTime = turnaround - t->waitTime;
                shm->terminatedTask[shm->terminatedTop] = *t;
                shm->terminatedTop++;
                kill(t->pid, SIGKILL);
            }
        }
    }
    sem_post(&(shm->readyQueue3.sem));
    sem_wait(&(shm->readyQueue2.sem));
    while (!(shm->readyQueue2.is_empty(&shm->readyQueue2)))
    {
        task * t = shm->readyQueue2.dequeue(&shm->readyQueue2);
        if (t != NULL) {
            int pid = fork();
            if (pid < 0) {
                printf("Error");
            }
            else if (pid == 0) {
                char *args[] = { t->command , NULL};
                execvp(t->command,args);
                printf("Can't find command\n");
            }
            else {
                int wstatus;
                waitpid(pid, &wstatus , 0) ;
                t->flag = 2;
                gettimeofday(&(t->termination_time),NULL);
                double turnaround = (t->termination_time.tv_sec - t->startTime.tv_sec)*1000.0
                        + (t->termination_time.tv_usec - t->startTime.tv_usec) / 1000.0;
                t->execTime = turnaround - t->waitTime;
                shm->terminatedTask[shm->terminatedTop] = *t;
                shm->terminatedTop++;
                kill(t->pid, SIGKILL);
            }
        }
    }
    sem_post(&(shm->readyQueue2.sem));
    sem_wait(&(shm->readyQueue1.sem));
    while (!(shm->readyQueue1.is_empty(&shm->readyQueue1)))
    {
        task * t = shm->readyQueue1.dequeue(&shm->readyQueue1);
        if (t != NULL) {
            int pid = fork();
            if (pid < 0) {
                printf("Error");
            }
            else if (pid == 0) {
                char *args[] = { t->command , NULL};
                execvp(t->command,args);
                printf("Can't find command\n");
            }
            else {
                int wstatus;
                waitpid(pid, &wstatus , 0) ;
                t->flag = 2;
                gettimeofday(&(t->termination_time),NULL);
                double turnaround = (t->termination_time.tv_sec - t->startTime.tv_sec)*1000.0
                        + (t->termination_time.tv_usec - t->startTime.tv_usec) / 1000.0;
                t->execTime = turnaround - t->waitTime;
                shm->terminatedTask[shm->terminatedTop] = *t;
                shm->terminatedTop++;
                kill(t->pid, SIGKILL);
            }
        }
    }
    sem_post(&(shm->readyQueue1.sem));

    //Waiting for all queue to clear then exit


    printf("All tasks completed. Exiting...\n");

    if (sig == SIGINT) {
        wait(NULL);
        kill(shm->scheduler_pid, SIGTERM);
        displayHistory();
        clean();
    }
}


void submitScheduledTask(char*command, int priority) {

    pid_t pid = fork();

    if (pid < 0) {
        printf("Error while forking");
        exit(0);
    }

    if (pid == 0) {
        pid_t pid_grandchild = fork();
        if (pid_grandchild > 0) {
            kill(pid_grandchild, SIGSTOP); // Temporarily Stoping grandchild which will also going to execute our command later
        }
        if (pid_grandchild == -1) {
            printf("Error in Fork");
            exit(0);
        };
        if (pid_grandchild == 0) {
            //grandchild process going to run our command;
            char * args[] = {command, NULL};
            execvp(command,args);
            printf("Error while executing command %s\n", command);
        }
        else {
            task* t = (task* ) malloc(sizeof(task)); //Allocating space for task
            if (t == NULL) {
                printf("Malloc failed for task");
            }
            t->waitTime = 0.0;
            t->pid = pid_grandchild;
            t->pri = priority;
            t->flag = 0;
            t->command = command;
            gettimeofday(&t->startTime,NULL);
            // t->command = (char *) malloc(sizeof(char) * strlen(command));
            // if (t != NULL) {
            //     strcpy(t->command,command);
            // }
            //Find place for newly created task;
            sem_wait(&(shm->sem)); // Stopping any other process for making changes in process table
                int n = shm->top; //last process idx in process table
                int j = 0;
                while (shm->PT[n].flag != -1) { // -1 means block is unused
                    
                    n = (n+1) % MAX_PID;
                    if (j++ == MAX_PID) {
                        printf("Process Table is full\n");
                        exit(0);
                    }
                }
                shm->PT[n] = *t; // adding task to process table
                task * processaddress = &shm->PT[n];
                shm->count++;
                shm->top = n+1;
            sem_post(&(shm->sem));

            switch(priority) {

                case 1:
                    sem_wait(&(shm->readyQueue1.sem));
                    shm->readyQueue1.enqueue(&(shm->readyQueue1),processaddress);
                    sem_post(&(shm->readyQueue1.sem));
                    break;

                case 2:
                    sem_wait(&(shm->readyQueue2.sem));
                    shm->readyQueue2.enqueue(&(shm->readyQueue2),processaddress);
                    sem_post(&(shm->readyQueue2.sem));
                    break;
                case 3:
                    sem_wait(&(shm->readyQueue3.sem));
                    shm->readyQueue3.enqueue(&(shm->readyQueue3),processaddress);
                    sem_post(&(shm->readyQueue3.sem));
                    break;
                case 4:
                    sem_wait(&(shm->readyQueue4.sem));
                    shm->readyQueue4.enqueue(&(shm->readyQueue4),processaddress);
                    sem_post(&(shm->readyQueue4.sem));
                    break;
            }

            sem_post(&(shm->null_sem));

            int runningstatus;
            printf("\n %s is running with %d \n",t->command,t->pid);
            waitpid(t->pid,&runningstatus,0);
            printf("\nExecution of %s is completed with %d \n",t->command,t->pid);
            
            sem_wait(&(shm->sem));
                shm->PT[n].flag = 2;
                gettimeofday(&shm->PT[n].termination_time,NULL);
                shm->PT[n].command = command;
                double totaltime =  (shm->PT[n].termination_time.tv_sec - shm->PT[n].startTime.tv_sec)*1000 + (shm->PT[n].termination_time.tv_usec - shm->PT[n].startTime.tv_usec) / 1000;
                shm->PT[n].execTime = totaltime;
                shm->PT[n].flag = -1;
                shm->count -= 1;
                shm->terminatedTask[shm->terminatedTop] = shm->PT[n];
                shm->terminatedTop += 1;;
                // switch(priority) {
                //
                //     case 1:
                //         sem_wait(&(shm->readyQueue1.sem));
                //         shm->readyQueue1.dequeue(&(shm->readyQueue1));
                //         sem_post(&(shm->readyQueue1.sem));
                //         break;
                //     case 2:
                //         sem_wait(&(shm->readyQueue2.sem));
                //         shm->readyQueue2.dequeue(&(shm->readyQueue2));
                //         sem_post(&(shm->readyQueue2.sem));
                //         break;
                //     case 3:
                //         sem_wait(&(shm->readyQueue3.sem));
                //         shm->readyQueue3.dequeue(&(shm->readyQueue3));
                //         sem_post(&(shm->readyQueue3.sem));
                //         break;
                //     case 4:
                //         sem_wait(&(shm->readyQueue4.sem));
                //         shm->readyQueue4.dequeue(&(shm->readyQueue4));
                //         sem_post(&(shm->readyQueue4.sem));
                //         break;
                // }
                sem_wait(&(shm->runningQueue.sem));
                shm->runningQueue.dequeue(&(shm->runningQueue));
                sem_post(&(shm->runningQueue.sem));
                kill(shm->PT[n].pid, SIGTERM);
            sem_post(&(shm->sem));
            free(t);
            _exit(0);
        }
    }
}


void scheduler() {
    sem_wait(&(shm->sem));
        const unsigned int time = shm->tslice;
        int ncpu = shm->ncpu;
    sem_post(&(shm->sem));
    
    while (true) {
        sem_wait(&(shm->null_sem));
        int i = 0;
        while (i < ncpu) {

            task * t;
            // Getting first task from queue by its priority level;
            if (shm->readyQueue4.front != shm->readyQueue4.rear) {
                sem_wait(&(shm->readyQueue4.sem));
                t = shm->readyQueue4.dequeue(&(shm->readyQueue4));
                sem_post(&(shm->readyQueue4.sem));
            }

            else if (shm->readyQueue3.front != shm->readyQueue3.rear) {
                sem_wait(&(shm->readyQueue3.sem));
                t = shm->readyQueue3.dequeue(&(shm->readyQueue3));
                sem_post(&(shm->readyQueue3.sem));
            }

            else if (shm->readyQueue2.front != shm->readyQueue2.rear) {
                sem_wait(&(shm->readyQueue2.sem));
                t = shm->readyQueue2.dequeue(&(shm->readyQueue2));
                sem_post(&(shm->readyQueue2.sem));
            }

            else  {
                sem_wait(&(shm->readyQueue1.sem));
                t = shm->readyQueue1.dequeue(&(shm->readyQueue1));
                sem_post(&(shm->readyQueue1.sem));
            }

            if (t == NULL) {
                break; // Break if task not found
            }

            sem_wait(&(shm->sem));
                t->flag = 1; // Mark it running
                kill(t->pid,SIGCONT); // Resuming this task as it was stop by parent in submittask function
                waitpid(t->pid,NULL,WNOHANG);
            sem_post(&(shm->sem));

            sem_wait(&(shm->runningQueue.sem));
                shm->runningQueue.enqueue(&(shm->runningQueue),t); // Adding task to running queue
            sem_post(&(shm->runningQueue.sem));
            
            i++;
        }

        usleep(time); //Allowing task to run for given timeslice
        sem_wait(&shm->readyQueue1.sem);
            int f1 = shm->readyQueue1.front;
            int r1 = shm->readyQueue1.rear;
        sem_post(&(shm->readyQueue1.sem));

        sem_wait(&shm->readyQueue2.sem);
            int f2 = shm->readyQueue2.front;
            int r2 = shm->readyQueue2.rear;
        sem_post(&(shm->readyQueue2.sem));

        sem_wait(&shm->readyQueue3.sem);
            int f3 = shm->readyQueue3.front;
            int r3 = shm->readyQueue3.rear;
        sem_post(&(shm->readyQueue3.sem));

        sem_wait(&shm->readyQueue4.sem);
            int f4 = shm->readyQueue4.front;
            int r4 = shm->readyQueue4.rear;
        sem_post(&(shm->readyQueue4.sem));
        // Adding waiting time for all another process
        while (f4 != r4) { //Ensuring highest priority queue execute first
            sem_wait(&(shm->readyQueue4.sem));
            task * t = shm->readyQueue4.array[f4];
            if (t != NULL) {
                sem_post(&(shm->readyQueue4.sem));
                sem_wait(&(shm->sem));
                    t->waitTime = t->waitTime + (double)time/1000;
                sem_post(&(shm->sem));
            }
            else {
                sem_post(&(shm->readyQueue4.sem));
                break;
            }
            f4 += 1;
            f4 %= MAX_PID;
        }

        while (f3 != r3) {
            sem_wait(&(shm->readyQueue3.sem));
            task * t = shm->readyQueue3.array[f3];
            if (t != NULL) {
                sem_post(&(shm->readyQueue3.sem));
                sem_wait(&(shm->sem));
                    t->waitTime = t->waitTime + (double)time/1000;
                sem_post(&(shm->sem));
            }
            else {
                sem_post(&(shm->readyQueue3.sem));
                break;
            }
            f3 += 1;
            f3 %= MAX_PID;
        }
        while (f2 != r2) {
            sem_wait(&(shm->readyQueue2.sem));
            task * t = shm->readyQueue2.array[f2];
            if (t != NULL) {
                sem_post(&(shm->readyQueue2.sem));
                sem_wait(&(shm->sem));
                t->waitTime = t->waitTime + (double)time/1000;
                sem_post(&(shm->sem));
            }
            else {
                sem_post(&(shm->readyQueue2.sem));
                break;
            }
            f2 += 1;
            f2 %= MAX_PID;
        }
        while (f1 != r1) { //Least Priority
            sem_wait(&(shm->readyQueue1.sem));
            task * t = shm->readyQueue1.array[f3];
            if (t != NULL) {
                sem_post(&(shm->readyQueue1.sem));
                sem_wait(&(shm->sem));
                t->waitTime = t->waitTime + (double)time/1000;
                sem_post(&(shm->sem));
            }
            else {
                sem_post(&(shm->readyQueue1.sem));
                break;
            }
            f1 += 1;
            f1 %= MAX_PID;
        }
        i = 0;
        while (i++ < ncpu) {

            task * t = shm->runningQueue.dequeue(&(shm->runningQueue));
            if (t == NULL) break;
            sem_wait(&(shm->sem));
            if (t->flag != -1) {
                    kill(t->pid,SIGSTOP); //Stop the process currently running process
            }
            int flag = t->flag;
            sem_post(&(shm->sem));

            if (flag == 1) { //if process is currently running
                sem_wait(&(shm->sem));
                t->flag = 0; // making it ready
                int priority = t->pri;
                sem_post(&(shm->sem));

                switch (priority) {
                    case 1:
                        sem_wait(&(shm->readyQueue1.sem));
                        shm->readyQueue1.enqueue(&(shm->readyQueue1),t);
                        if (shm->readyQueue1.front != shm->readyQueue1.rear) {
                            sem_post(&(shm->null_sem));
                        }
                        sem_post(&(shm->readyQueue1.sem));
                        break;
                    case 2:
                        sem_wait(&(shm->readyQueue2.sem));
                        shm->readyQueue2.enqueue(&(shm->readyQueue2),t);
                        if (shm->readyQueue2.front != shm->readyQueue2.rear) {
                            sem_post(&(shm->null_sem));
                        }
                        sem_post(&(shm->readyQueue2.sem));
                        break;
                    case 3:
                        sem_wait(&(shm->readyQueue3.sem));
                        shm->readyQueue3.enqueue(&(shm->readyQueue3),t);
                        if (shm->readyQueue3.front != shm->readyQueue3.rear) {
                            sem_post(&(shm->null_sem));
                        }
                        sem_post(&(shm->readyQueue3.sem));
                        break;
                    case 4:
                        sem_wait(&(shm->readyQueue4.sem));
                        shm->readyQueue4.enqueue(&(shm->readyQueue4),t);
                        if (shm->readyQueue4.front != shm->readyQueue4.rear) {
                            sem_post(&(shm->null_sem));
                        }
                        sem_post(&(shm->readyQueue4.sem));
                        break;
                }
            }
        }
    }
}

int execute_command(char ** args) {
    if (strcmp(args[0], "submit") == 0) {
        if (args[2] != NULL) {
            submitScheduledTask(args[1], atoi(args[2]));
        } else {
            submitScheduledTask(args[1], 4);
        }
    } else {
        pid_t pid = fork();
        if (pid < 0) {
            printf("Error in Fork");
            exit(0);
        }
        if (pid == 0) {
            char *argvs[] = {args[0], NULL};
            execvp(argvs[0], argvs); //Executing command without submit to scheduler
            printf("Error in execvp");
        } else {
            wait(NULL); //In parent
        }
    }
    return 1;
}

void start_shell() {
    int status = fork();
    shm = setup();
    if (status < 0) {
        printf("Error in fork");
        exit(0);
    }
    if (status == 0) {
        int scheduler_pid = fork();
        if (scheduler_pid < 0) {
            printf("Error in fork Scheduler");
            exit(0);
        }
        if (scheduler_pid == 0) { // In the child
            struct sigaction action;
            memset(&action, 0, sizeof(action));
            action.sa_handler = scheduler_exit;
            sigaction(SIGINT, &action, NULL);

            int scheduler_fd = shm_open("PT",O_RDWR,0666);
            //
            shm = mmap(NULL, sizeof(shm_t), PROT_READ | PROT_WRITE, MAP_SHARED, scheduler_fd, 0);
            scheduler();
        }
        else {
            //In the parent
            shm->scheduler_pid = scheduler_pid;
        }
    }
    else {
        struct sigaction actionv;
        memset(&actionv, 0, sizeof(actionv));
        actionv.sa_handler = handle_exit;
        sigaction(SIGINT, &actionv, NULL);
        char cwd[1024];
        getcwd(cwd, sizeof(cwd));
        do {

            printf("atos@UNI:%s$ ", cwd); // Custom prompt
            char * command = NULL; //Reading command from user input
            ssize_t buffsize = 0;
            ssize_t read = getdelim(&command,&buffsize,'\n',stdin);
            if (read != -1) {
                if (command[read-1] == '\n') {
                    command[read-1] = '\0';
                }
            }
            char *inputc[100];
            int cnt = 0;
            char *tok = strtok(command, " "); //Spliting command
            while (tok != NULL) {
                inputc[cnt++] = tok;
                tok = strtok(NULL, " ");
            }
            inputc[cnt] = NULL;

            status = execute_command(inputc); //Executing command

        } while(status);

    }
}



int main(int argc , char * argv[] ) {
    shm = setup(); // SHM setup
    // shm->ncpu = 1 ;
    shm->ncpu = atoi(argv[1]) ; //Reading ncpu
    // shm->tslice = 0.0001;
    shm->tslice = (unsigned int )atoi(argv[2]); //Reading tsilce
    init(shm); //Initializing default values
    start_shell(); //Starting shell

}
