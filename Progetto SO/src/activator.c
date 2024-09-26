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
#include <errno.h>

int total_atoms;     // Numero totale di atomi
long step_activator; // STEP_ACTIVATOR
pid_t master_pid;    // PID del master

int inhib_value;
int flag_inhib;
int sem_di_starting_simulation;

struct simulation_data *sim_data;
struct n_atoms_data *n_atoms_data;
struct inhibitor_data *inhibitor_data;
struct timespec delay;

int id_sim_data;
int sem_id_sim_data;

int master_msg_queue;
int activator_msg_queue;
int sem_id_msg_queues;

int shm_n_atoms;

int shm_inhibitor;
int sem_id_inhibitor;

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Errore nei parametri passati ad activator\n");
        fprintf(stderr, "Usage: %s <id memoria condivisa simulazione> <id semaforo memoria condivisa>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // argv[0] è il nome del programma
    // argv[1] id memoria condivisa
    // argv[2] id semaforo memoria condivisa

    id_sim_data = atoi(argv[1]);
    sem_id_sim_data = atoi(argv[2]);

    struct sigaction sa;

    // Imposta la gestione dei segnali
    sa.sa_sigaction = sigterm_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGTERM, &sa, NULL) == -1)
    {
        perror("Errore gestione SIGTERM (activator)");
        exit(EXIT_FAILURE);
    }

    // Ignora il segnale SIGTSTP
    sa.sa_handler = SIG_IGN;
    if (sigaction(SIGTSTP, &sa, NULL) == -1)
    {
        perror("Errore gestione SIGTSTP (activator)");
        exit(EXIT_FAILURE);
    }

    sim_data = (struct simulation_data *)shmat(id_sim_data, NULL, 0);
    TEST_ERROR;

    get_sem(sem_id_sim_data, 0);

    delay.tv_sec = sim_data->STEP_ATTIVATORE / 1000000000;
    delay.tv_nsec = sim_data->STEP_ATTIVATORE % 1000000000;

    sem_di_starting_simulation = sim_data->sem_id_starting_simulation;

    master_msg_queue = sim_data->message_queue_master;
    activator_msg_queue = sim_data->message_queue_activator;
    sem_id_msg_queues = sim_data->sem_id_queues;

    shm_n_atoms = sim_data->shm_n_atoms;

    inhib_value = sim_data->inhib_value;
    shm_inhibitor = sim_data->shm_inhibitor;
    sem_id_inhibitor = sim_data->sem_id_inhibitor;

    release_sem(sem_id_sim_data, 0);

    n_atoms_data = (struct n_atoms_data *)shmat(shm_n_atoms, NULL, 0);
    TEST_ERROR;

    inhibitor_data = (struct inhibitor_data *)shmat(shm_inhibitor, NULL, 0);
    TEST_ERROR;

    while (1)
    {
        int splits_failed = 0;

        get_sem(sem_id_sim_data, 1);
        total_atoms = n_atoms_data->n_atoms;
        release_sem(sem_id_sim_data, 1);

        int random_atoms_selected = random_splits(total_atoms);

        // printf("Attivatore: %d atomi selezionati\n", random_atoms_selected);

        for (int i = 0; i < random_atoms_selected; i++)
        {

            struct message msg_atom;
            msg_atom.type = 1;
            msg_atom.value = 0;
            msg_atom.second_value = 0;

            if (inhib_value == 1)
            {
                get_sem(sem_id_inhibitor, 0);

                if (inhibitor_data->flag_inhib == 1) // Verifica se l'inibitore è attivo
                {
                    if (inhibitor_data->split_probability == 0) // Verifica se la probabilità di scissione è 0
                    {
                        release_sem(sem_id_inhibitor, 0);
                        splits_failed++; // Incrementa il contatore delle scissioni fallite
                        continue;
                    }
                }

                release_sem(sem_id_inhibitor, 0);
            }

            get_sem(sem_id_msg_queues, 1);

            if (msgsnd(activator_msg_queue, &msg_atom, sizeof(msg_atom), 0) == -1) // Invia il messaggio agli atomi (scissione)
            {
                release_sem(sem_id_msg_queues, 1);
                perror("Errore nell'invio del messaggio activator (atom)");
                exit(EXIT_FAILURE);
            }

            release_sem(sem_id_msg_queues, 1);
        }

        struct message msg_master;
        msg_master.type = 3;
        msg_master.value = random_atoms_selected - splits_failed; // Numero di scissioni riuscite
        msg_master.second_value = splits_failed;                  // Numero di scissioni fallite

        get_sem(sem_id_msg_queues, 0);

        if (msgsnd(master_msg_queue, &msg_master, sizeof(msg_master), 0) == -1) // Invia il numero di scissioni riuscite e fallite al master
        {
            release_sem(sem_id_msg_queues, 0);
            perror("Errore nell'invio del messaggio activator (master)");
            exit(EXIT_FAILURE);
        }

        release_sem(sem_id_msg_queues, 0);

        // printf("Scissioni totali: %d\n", random_atoms_selected);
        // printf("Scissioni riuscite: %d\n", random_atoms_selected - splits_failed);
        // printf("Scissioni fallite: %d\n", splits_failed);

        // splits_failed = 0; // Resetta il contatore degli atomi non scissi

        nanosleep(&delay, NULL);
    }

    exit(EXIT_SUCCESS);
}