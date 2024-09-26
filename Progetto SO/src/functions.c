#include "functions.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <string.h>
#include <time.h>

// Funzione per la creazione di un semaforo
int create_sem(int sem_num)
{
    int sem_id = semget(IPC_PRIVATE, sem_num, IPC_CREAT | 0666);
    if (sem_id == -1)
    {
        perror("Errore nella creazione del semaforo");
        exit(EXIT_FAILURE);
    }
    return sem_id;
}

// Funzione per l'ottenimento di un semaforo
void get_sem(int sem_id, int sem_num)
{
    struct sembuf sem_op = {sem_num, -1, SEM_UNDO};
    if (semop(sem_id, &sem_op, 1) == -1)
    {
        perror("Errore nella get del semaforo");
        exit(EXIT_FAILURE);
    }
}

// Funzione per il rilascio di un semaforo
void release_sem(int sem_id, int sem_num)
{
    struct sembuf sem_op = {sem_num, 1, SEM_UNDO};
    if (semop(sem_id, &sem_op, 1) == -1)
    {
        perror("Errore nella release del semaforo");
        exit(EXIT_FAILURE);
    }
}

// Genera un numero casuale tra 1 e n_atom_max
int new_atomic_number(int n_atom_max)
{
    return rand() % n_atom_max + 1;
}

// Divide il numero atomico del padre in un numero casuale tra 1 e father_atomic_number - 1
int divide_atomic_number(int father_atomic_number)
{
    return rand() % (father_atomic_number - 1) + 1;
}

// Restituisce il massimo tra due numeri
int max(int n1, int n2)
{
    if (n1 > n2)
        return n1;
    else
        return n2;
}

// Restituisce il minimo tra due numeri
int min(int n1, int n2)
{
    if (n1 < n2)
        return n1;
    else
        return n2;
}

// Funzione per il calcolo dell'energia prodotta sulla base di: energy(n1, n2) = n1*n2 − max(n1, n2)
int energy_produced(int n1, int n2)
{
    if (n1 == 1 || n2 == 1)
    {
        return 0;
    }

    return n1 * n2 - max(n1, n2);
}

// Funzione per il numero casuale di attivazioni
int random_splits(int n_atoms)
{
    if (n_atoms == 0)
    {
        return 0;
    }
    else
    {
        return rand() % n_atoms  + 1;
    }
}

// Funzione per la generazione di un numero casuale:
// flag = 0 -> genera un numero casuale tra 0 e 1
// flag = 1 -> genera un numero casuale tra 0.3 e 0.8
double random_inhib(int flag)
{
    if (flag == 0)
    {
        return (double)rand() / (double)RAND_MAX;
    }
    else
    {
        return (double)rand() / (double)RAND_MAX * 0.5 + 0.3;
    }
}

// Handler per il segnale SIGTERM, gestisce la terminazione del processo
void sigterm_handler(int signum, siginfo_t *info, void *context)
{
    if (signum == SIGTERM)
    {
        //printf("Processo terminato con PID %d\n", getpid());
        while (wait(NULL) > 0);
        exit(EXIT_SUCCESS);
    }
}
