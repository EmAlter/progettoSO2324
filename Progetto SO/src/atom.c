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

pid_t master_pid;  // PID del master
int atomic_number; // Numero atomico dell'atomo
int min_n_atom;    // Numero atomico minimo

struct simulation_data *sim_data;
struct inhibitor_data *inhibitor_data;

int shm_id_simulation_data;
int sem_id_simulation_data;

int shm_id_inhibitor;
int sem_id_inhibitor;

int master_msg_queue;
int activator_msg_queue;
int inhibitor_msg_queue;

int sem_id_msg_queues;

// Funzione di scissione dell'atomo
void split()
{
    int fd[2];

    if (atomic_number <= min_n_atom) // Se il numero atomico è inferiore a min_n_atom, termina il processo
    {
        struct message msg;
        msg.type = 2;
        msg.value = 2;
        msg.second_value = 0;

        get_sem(sem_id_msg_queues, 0);

        if (msgsnd(master_msg_queue, &msg, sizeof(msg), 0) == -1)
        {
            release_sem(sem_id_msg_queues, 0);
            perror("Errore nell'invio del messaggio");
            exit(EXIT_FAILURE);
        }
        release_sem(sem_id_msg_queues, 0);

        // printf("ATOMO con PID %d terminato\n", getpid());

        exit(EXIT_SUCCESS);
    }

    pipe(fd);

    pid_t child_pid = fork();
    if (child_pid == 0) // Codice del processo figlio
    {

        close(fd[1]); // Chiude il lato di scrittura

        int received_atomic_number;
        read(fd[0], &received_atomic_number, sizeof(received_atomic_number));

        close(fd[0]); // Chiude il lato di lettura

        char son_atomic_number_str[10];
        snprintf(son_atomic_number_str, sizeof(son_atomic_number_str), "%d", received_atomic_number);

        char simulation_data_str[10];
        snprintf(simulation_data_str, sizeof(simulation_data_str), "%d", shm_id_simulation_data);

        char sem_id_simulation_data_str[10];
        snprintf(sem_id_simulation_data_str, sizeof(sem_id_simulation_data_str), "%d", sem_id_simulation_data);

        char *const argAtom[] = {"atom", son_atomic_number_str, simulation_data_str, sem_id_simulation_data_str, NULL};

        int ex_atom = execve("../bin/atom", argAtom, NULL);
        TEST_ERROR;
    }
    else if (child_pid > 0) // Codice del processo padre dopo la fork
    {

        close(fd[0]); // Chiude il lato di lettura

        // Aggiorno il numero atomico del padre e restituisco il numero atomico del figlio
        int son_atomic_number = divide_atomic_number(atomic_number); // divide il numero atomico del padre
        atomic_number -= son_atomic_number;                          // aggiorna il numero atomico del padre

        write(fd[1], &son_atomic_number, sizeof(son_atomic_number)); // Invia il numero atomico del figlio al processo figlio

        close(fd[1]); // Chiude il lato di scrittura

        int energy = energy_produced(son_atomic_number, atomic_number);

        // printf("ATOMO con PID %d ha prodotto %d di energia\n", getpid(), energy);

        struct message msg;
        msg.type = 1;
        msg.value = energy;
        msg.second_value = 0;

        get_sem(sem_id_inhibitor, 0);
        int flag_inhib = inhibitor_data->flag_inhib;
        release_sem(sem_id_inhibitor, 0);

        if (flag_inhib == 1)
        {
            get_sem(sem_id_msg_queues, 2);

            if (msgsnd(inhibitor_msg_queue, &msg, sizeof(msg), 0) == -1)
            {
                release_sem(sem_id_msg_queues, 2);
                perror("Errore nell'invio del messaggio atom (inhibitor) ENERGIA\n");
                exit(EXIT_FAILURE);
            }

            release_sem(sem_id_msg_queues, 2);
        }
        else
        {

            get_sem(sem_id_msg_queues, 0);

            if (msgsnd(master_msg_queue, &msg, sizeof(msg), 0) == -1)
            {
                release_sem(sem_id_msg_queues, 0);
                perror("Errore nell'invio del messaggio atom (master) ENERGIA\n");
                exit(EXIT_FAILURE);
            }
            //printf("ATOMO con PID %d ha inviato il messaggio al master ENERGIA\n", getpid());
            release_sem(sem_id_msg_queues, 0);
        }
    }

    else
    {
        // Errore nella fork (meltdown)
        kill(master_pid, SIGUSR1);
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        fprintf(stderr, "Errore nei parametri passati ad atom\n");
        fprintf(stderr, "Usage: %s <atomic_number> <id memoria condivisa simulazione> <id semaforo memoria condivisa>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // argv[0] = nome del programma
    // argv[1] = numero atomico
    // argv[2] = memoria condivisa dati simulazione
    // argv[3] = id del semaforo della memoria condivisa dati simulazione

    struct sigaction sa;

    // Imposta la gestione dei segnali
    sa.sa_sigaction = sigterm_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGTERM, &sa, NULL) == -1)
    {
        perror("Errore gestione SIGTERM (atom)");
        exit(EXIT_FAILURE);
    }

    // Ignora il segnale SIGTSTP
    sa.sa_handler = SIG_IGN;
    if (sigaction(SIGTSTP, &sa, NULL) == -1)
    {
        perror("Errore gestione SIGTSTP (atom)");
        exit(EXIT_FAILURE);
    }

    struct timespec delay;
    delay.tv_sec = 0;
    delay.tv_nsec = 10000000; // 10 ms di ritardo (scelti da me)

    atomic_number = atoi(argv[1]);

    shm_id_simulation_data = atoi(argv[2]);
    sim_data = (struct simulation_data *)shmat(shm_id_simulation_data, NULL, 0);
    TEST_ERROR;

    sem_id_simulation_data = atoi(argv[3]);

    get_sem(sem_id_simulation_data, 0);

    master_pid = sim_data->pid_master;
    min_n_atom = sim_data->MIN_N_ATOMICO;
    master_msg_queue = sim_data->message_queue_master;
    activator_msg_queue = sim_data->message_queue_activator;
    inhibitor_msg_queue = sim_data->message_queue_inhibitor;
    sem_id_msg_queues = sim_data->sem_id_queues;

    shm_id_inhibitor = sim_data->shm_inhibitor;
    sem_id_inhibitor = sim_data->sem_id_inhibitor;

    release_sem(sem_id_simulation_data, 0);

    struct message msg;
    msg.type = 2;
    msg.value = 1;
    msg.second_value = 0;

    get_sem(sem_id_msg_queues, 0);

    if (msgsnd(master_msg_queue, &msg, sizeof(msg), 0) == -1)
    {
        release_sem(sem_id_msg_queues, 0);
        perror("Errore nell'invio del messaggio atom (master_child)\n");
        exit(EXIT_FAILURE);
    }

    release_sem(sem_id_msg_queues, 0);

    inhibitor_data = (struct inhibitor_data *)shmat(shm_id_inhibitor, NULL, 0);
    TEST_ERROR;

    while (1)
    {
        get_sem(sem_id_msg_queues, 1);

        struct message msg;
        if (msgrcv(activator_msg_queue, &msg, sizeof(msg), 0, IPC_NOWAIT) == -1)
        {
            if (errno == EAGAIN || errno == ENOMSG)
            {
                release_sem(sem_id_msg_queues, 1);
                nanosleep(&delay, NULL); // Brevissima attesa per permettere ad altri atomi di ricevere il messaggio
                continue;
            }
            else
            {
                release_sem(sem_id_msg_queues, 1);
                perror("Errore nella ricezione del messaggio activator (atom)");
                exit(EXIT_FAILURE);
            }
        }

        release_sem(sem_id_msg_queues, 1);

        // printf("ATOMO con PID %d ha ricevuto il segnale dall'activator\n", getpid());
        split();
        nanosleep(&delay, NULL); // Brevissima attesa per permettere ad altri atomi di ricevere il messaggio
    }
}