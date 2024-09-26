#include "functions.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <string.h>
#include <time.h>

int number_of_atoms;            // Atomi da generare
long step_feeder;               // STEP_ALIMENTAZIONE
int n_atom_max;                 // Numero atomico massimo
int sem_id_starting_simulation; // Semaforo per l'inizio della simulazione
pid_t master_pid;               // PID del master

int shm_id_simulation_data;
int sem_id_simulation_data;
struct simulation_data *sim_data;

int master_msg_queue;
int sem_id_msg_queues;

char min_n_atom_str[10]; // Numero atomico minimo

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Errore nei parametri passati al feeder\n");
        fprintf(stderr, "Usage: %s <id memoria condivisa simulazione> <id semaforo memoria condivisa>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // argv[0] è il nome del programma
    // argv[1] è la memoria condivisa con i dati
    // argv[2] è il semaforo della memoria condivisa

    struct sigaction sa;

    // Imposta la gestione dei segnali
    sa.sa_sigaction = sigterm_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGTERM, &sa, NULL) == -1)
    {
        perror("Errore nella gestione di SIGTERM (feeder)");
        exit(EXIT_FAILURE);
    }

    // Ignora il segnale SIGTSTP
    sa.sa_handler = SIG_IGN;
    if (sigaction(SIGTSTP, &sa, NULL) == -1)
    {
        perror("Errore nella gestione di SIGTSTP (feeder)");
        exit(EXIT_FAILURE);
    }

    shm_id_simulation_data = atoi(argv[1]);
    sim_data = (struct simulation_data *)shmat(shm_id_simulation_data, NULL, 0);
    TEST_ERROR;

    sem_id_simulation_data = atoi(argv[2]);

    get_sem(sem_id_simulation_data, 0);
    
    master_pid = sim_data->pid_master;
    number_of_atoms = sim_data->N_NUOVI_ATOMI;
    step_feeder = sim_data->STEP_ALIMENTAZIONE;
    n_atom_max = sim_data->N_ATOM_MAX;

    sem_id_starting_simulation = sim_data->sem_id_starting_simulation;

    master_msg_queue = sim_data->message_queue_master;
    sem_id_msg_queues = sim_data->sem_id_queues;

    release_sem(sem_id_simulation_data, 0);

    char simulation_data_str[10];
    snprintf(simulation_data_str, sizeof(simulation_data_str), "%d", shm_id_simulation_data);

    char sem_id_simulation_data_str[10];
    snprintf(sem_id_simulation_data_str, sizeof(sem_id_simulation_data_str), "%d", sem_id_simulation_data);

    struct timespec attesa;
    attesa.tv_sec = step_feeder / 1000000000;
    attesa.tv_nsec = step_feeder % 1000000000;

    // printf("FEEDER: Tempo di attesa tra una generazione e l'altra: %ld nanosecondi\n", step_feeder);
    // printf("FEEDER: Generazione di %d atomi\n", number_of_atoms);

    while (1)
    {
        for (int i = 0; i < number_of_atoms; i++)
        {
            char atomic_number_str[10];
            int atomic_number = new_atomic_number(n_atom_max);
            snprintf(atomic_number_str, sizeof(atomic_number_str), "%d", atomic_number);

            pid_t atom = fork();
            if (atom == -1)
            {
                kill(master_pid, SIGUSR1); // Se la fork fallisce, meltdown
                exit(EXIT_FAILURE);
            }
            else if (atom == 0)
            {
                char *const argAtom[] = {"atom", atomic_number_str, simulation_data_str, sem_id_simulation_data_str, NULL};

                int ex_atom = execve("../bin/atom", argAtom, NULL);
                TEST_ERROR;
            }
        }

        nanosleep(&attesa, NULL); // Attende step_feeder nanosecondi
    }
}