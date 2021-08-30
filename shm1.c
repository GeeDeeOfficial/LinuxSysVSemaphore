#include <unistd.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
#include <stdio.h>
#include <errno.h>
#include <sys/mman.h>
#include <stdbool.h>
#include <string.h>
#include <sys/sem.h>

#define shmpath "shmtest"
#define semKey  (12345)
struct sembuf lockSembuf;
struct sembuf unlockSembuf;

int semid;

bool unlock();
bool lock();
int createSysVSemaphore();
void removeOnRestart();

void main()
{
    //unlink
    shm_unlink(shmpath);
    removeOnRestart();

    semid = createSysVSemaphore();
    
   lock();
    //shm
    void* addr;
    int size = 256;
    bool success = true;
    int fd = shm_open(shmpath, O_CREAT | O_EXCL | O_RDWR, 0666);
                                 //S_IRUSR | S_IWUSR);
    if (fd == -1)
    {
        if (EEXIST == errno)
        {
            fd = shm_open(shmpath, O_RDWR, 0666);
            if (fd == -1)
            {
                printf("attach failed\n");
                success = false;
            }
            else
            {
                printf("attach success\n");
            }
        }
        else
        {
            printf("shm_open fail\n");
            success = false;
        }
    }
    else
    {
        printf("create success\n");
        if (ftruncate(fd, size) == -1)
        {
            printf("ftrunc fail\n");
            success = false;
        }
        else
        {
            printf("ftrunc success\n");
        }
    }
    if(success)
    {
    	addr = mmap(NULL, size, PROT_WRITE, MAP_SHARED, fd, 0);
	    if (addr == MAP_FAILED)
	    {
            printf("mmap fail\n");
            success = false;
	    }
	    else
	    {
            printf("mmap success\n");	    
	    }
    }

    unlock();

    while(true)
    {   
        lock();
        memcpy(addr,"hello there\0", 12*4);
        unlock();
        sleep(3); 
        /*
        if (((char *)addr)[0] == 'w')
        {
            printf("stop\n");        
            lock();       
        }
        else
        {
            printf("running\n");
            continue;
        }
        */
    }   
}

int createSysVSemaphore()
{
    bool success;
    key_t key;
    int sem;
    
    union semun  
    {
        int val;
        struct semid_ds *buf;
        ushort array [1];
    } arg;
    
    //assume nothing with go wrong
    success = true;
   
    sem = semget (semKey, 1, 0666 | IPC_CREAT | IPC_EXCL);
    
    if (sem == -1)
    {
        if (EEXIST == errno)
        {
            //already exists attach to it
            sem = semget(semKey, 0, 0);
            if (sem == -1)
            {
                printf("sem attach error\n");
                success = false;
            }
            else
            {
                printf("attached semaphore\n");
             
                struct semid_ds semid_ds;
                arg.buf = & semid_ds;
                semid_ds.sem_otime = 0;
                
                while(arg.buf->sem_otime == 0)
                {
                    printf("waiting for semaphore initialization\n");
                    if (semctl(sem, 0, IPC_STAT, arg) == -1)
                    {
                        printf("semctl err\n");
                    }
                    usleep(10000);
                }
                printf("semaphore initialized\n");
            }
        }
        else
        {
            printf("failed to create semaphpre\n");
            success = false;
        }
    }   
    else
    {
        printf("created semaphore\n");
        //initialize semaphore
        arg.val = 1;
        sleep(100000);
        if (semctl (sem, 0, SETVAL, arg) == -1) 
        {
            printf("semctl err\n");
            success = false;
        }
    }

    if(success)
    {
        /* Initialize the semaphore. */
        lockSembuf.sem_num = 0;
        lockSembuf.sem_op = -1;
        lockSembuf.sem_flg = SEM_UNDO;
        
        unlockSembuf.sem_num = 0;
        unlockSembuf.sem_op = 1;
        unlockSembuf.sem_flg = SEM_UNDO;
    }
    
    return sem;
}

bool lock()
{
    if (semop(semid, &lockSembuf, 1) == -1)
    {
        return false;
    }
    else
    {
        return true;
    }
}

bool unlock()
{
    if (semop(semid, &unlockSembuf, 1) == -1)
    {
        return false;
    }
    else
    {
        return true;
    }
}

void removeOnRestart()
{
    int sem = semget(semKey, 0, 0);
    if (sem != -1)
    {
        printf("delete semaphore from previous run\n");
        if (semctl (sem, 0, IPC_RMID) == -1) 
        {
            perror ("semctl IPC_RMID"); 
        }
    }
}
