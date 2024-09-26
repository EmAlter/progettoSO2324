#ifndef PROGETTO_SO_FUNCTION_H
#define PROGETTO_SO_FUNCTION_H

#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <string.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <errno.h>

// Macro per la gestione degli errori (copiata dalle lezioni)
#define TEST_ERROR                                 \
    if (errno)                                     \
    {                                              \
        fprintf(stderr,                            \
                "%s:%d: PID=%5d: Error %d (%s)\n", \
                __FILE__,                          \
                __LINE__,                          \
                getpid(),                          \
                errno,                             \
                strerror(errno));                  \
        exit(EXIT_FAILURE);                        \
    }

struct message
{
    long type;
    int value;
    int second_value; // Può essere usato per passare un secondo valore (quando necessario)
};

struct simulation_data
{
    pid_t pid_master; // PID del master

    int MIN_N_ATOMICO;       // Numero atomico minimo
    int N_ATOM_MAX;          // Numero atomico massimo
    int N_NUOVI_ATOMI;       // Numero di nuovi atomi generati dal feeder
    long STEP_ATTIVATORE;    // Step di activator
    long STEP_ALIMENTAZIONE; // Step di feeder

    int sem_id_starting_simulation; // Id del semaforo per l'avvio della simulazione

    int message_queue_master;    // Coda di messaggi per il master
    int message_queue_activator; // Coda di messaggi per l'attivatore
    int message_queue_inhibitor; // Coda di messaggi per l'inibitore
    int sem_id_queues;           // Id dei semafori per le code di messaggi

    int shm_n_atoms; // Id della memoria condivisa per il numero di atomi

    int inhib_value;      // Flag inibitore s/n nella simulazione
    int shm_inhibitor;    // Id della memoria condivisa per l'inibitore
    int sem_id_inhibitor; // Id del semaforo per la memoria condivisa dell'inibitore
};

struct stats
{
    int n_activations;   // Numero di attivazioni
    int n_splits;        // Numero di scissioni
    int energy_produced; // Energia prodotta
    int energy_wasted;   // Energia sprecata
    int waste;           // Scorie

    int energy_absorbed; // Energia assorbita (inibitore)
    int splits_failed;   // Scissioni fallite (inibitore)
};

struct inhibitor_data
{
    int flag_inhib;        // Flag inibitore (attivo o non attivo)
    int split_probability; // Probabilità di scissione
};

struct n_atoms_data
{
    int n_atoms; // Numero di atomi
};

// Funzioni per la gestione dei semafori
int create_sem(int sem_num);
void get_sem(int sem_id, int sem_num);
void release_sem(int sem_id, int sem_num);

// Funzioni per la gestione degli atomi
int new_atomic_number(int n_atom_max);
int divide_atomic_number(int father_atomic_number);

// Funzioni per la gestione dell'energia
int min(int n1, int n2);
int max(int n1, int n2);
int energy_produced(int n1, int n2);

// Funzioni per la gestione delle probabilità
int random_splits(int n_atoms);
double random_inhib(int flag);

// Funzioni per la gestione dei segnali
void sigterm_handler(int signum, siginfo_t *info, void *context);

#endif // PROGETTO_SO_FUNCTION_H