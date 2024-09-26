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
#include <signal.h>

pid_t inhibitor_child;          // PID del figlio inibitore
int flag_start_and_stop = 1;    // Flag per la pausa e la ripartenza del processo inibitore
int flag_sem[4] = {1, 0, 0, 0}; // Flag per i semafori

struct simulation_data *sim_data;
struct inhibitor_data *inhibitor_data;

int shm_id_simulation_data;
int sem_id_simulation_data;

int shm_id_inhibitor;
int sem_id_inhibitor;

int message_queue_master;
int message_queue_inhibitor;
int sem_id_queues;

// Funzione per "assorbire" energia
int absorb_energy(int energy)
{
    double percentage = random_inhib(1); // Il valore da assorbire è tra il 30% e lo 80% del valore totale

    int energy_to_withdraw = (int)(energy * percentage);

    // Viene assicurato che almeno un valore di energia venga sottratto
    if (energy_to_withdraw == 0 && energy > 0)
    {
        energy_to_withdraw = 1;
    }

    energy -= energy_to_withdraw;

    return energy;
}

// Handler per il segnale SIGTSTP, gestisce la pausa e la ripartenza del processo inibitore
void inhibitor_pause_and_stop_handler(int signum, siginfo_t *info, void *context)
{
    if (signum == SIGTSTP)
    {
        char buffer[100];
        int len;

        if (flag_start_and_stop == 1)
        {
            flag_start_and_stop = 0;

            snprintf(buffer, sizeof(buffer), " PROCESSO INIBITORE IN PAUSA\n");
            write(STDOUT_FILENO, buffer, strlen(buffer));

            kill(inhibitor_child, SIGSTOP);

            if (flag_sem[1] == 1)
            {
                release_sem(sem_id_queues, 2);
                flag_sem[1] = 0;
            }

            if (flag_sem[2] == 1)
            {
                release_sem(sem_id_queues, 0);
                flag_sem[2] = 0;
            }

            if (flag_sem[3] == 1)
            {
                release_sem(sem_id_inhibitor, 0);
                flag_sem[3] = 0;
            }

            get_sem(sem_id_inhibitor, 0);
            inhibitor_data->flag_inhib = 0;
            release_sem(sem_id_inhibitor, 0);

            sigset_t mask;
            sigemptyset(&mask);
            sigaddset(&mask, SIGTSTP);
            sigprocmask(SIG_UNBLOCK, &mask, NULL);

            pause();

            sigprocmask(SIG_BLOCK, &mask, NULL);
        }
        else
        {
            flag_start_and_stop = 1;

            snprintf(buffer, sizeof(buffer), " PROCESSO INIBITORE RIPARTITO\n");
            write(STDOUT_FILENO, buffer, strlen(buffer));

            kill(inhibitor_child, SIGCONT);

            get_sem(sem_id_inhibitor, 0);
            inhibitor_data->flag_inhib = 1;
            release_sem(sem_id_inhibitor, 0);
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Errore nei parametri passati ad inhibitor\n");
        fprintf(stderr, "Usage: %s <id memoria condivisa simulazione> <id semaforo memoria condivisa>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // argv[0] è il nome del programma
    // argv[1] è la memoria condivisa con i dati
    // argv[2] è il semaforo della memoria condivisa

    struct sigaction sa;
    sa.sa_sigaction = sigterm_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGTERM, &sa, NULL) == -1)
    {
        perror("Errore nella gestione di SIGTERM (inhibitor)");
        exit(EXIT_FAILURE);
    }

    struct sigaction sa2;
    sa2.sa_sigaction = inhibitor_pause_and_stop_handler;
    sa2.sa_flags = SA_SIGINFO;
    sigemptyset(&sa2.sa_mask);

    if (sigaction(SIGTSTP, &sa2, NULL) == -1)
    {
        perror("Errore nella gestione di SIGTSTP (inhibitor)");
        exit(EXIT_FAILURE);
    }

    shm_id_simulation_data = atoi(argv[1]);
    sem_id_simulation_data = atoi(argv[2]);

    sim_data = (struct simulation_data *)shmat(shm_id_simulation_data, NULL, 0);
    TEST_ERROR;

    get_sem(sem_id_simulation_data, 0);

    shm_id_inhibitor = sim_data->shm_inhibitor;
    sem_id_inhibitor = sim_data->sem_id_inhibitor;

    message_queue_master = sim_data->message_queue_master;
    message_queue_inhibitor = sim_data->message_queue_inhibitor;
    sem_id_queues = sim_data->sem_id_queues;

    release_sem(sem_id_simulation_data, 0);

    inhibitor_data = (struct inhibitor_data *)shmat(shm_id_inhibitor, NULL, 0);
    TEST_ERROR;
    
    /*------- FIGLIO INIBITORE -------*/
    /*------- Il figlio inibitore si occupa di decidere se bloccare o meno la scissione di un atomo -------*/
    inhibitor_child = fork();
    if (inhibitor_child == -1)
    {
        kill(getppid(), SIGUSR1);
        exit(EXIT_FAILURE);
    }
    else if (inhibitor_child == 0)
    {
        struct sigaction sa;

        sa.sa_sigaction = sigterm_handler;
        sa.sa_flags = SA_SIGINFO;
        sigemptyset(&sa.sa_mask);

        if (sigaction(SIGTERM, &sa, NULL) == -1)
        {
            perror("Errore nella gestione di SIGTERM (inhibitor_child)");
            exit(EXIT_FAILURE);
        }

        sa.sa_handler = SIG_IGN;
        if (sigaction(SIGTSTP, &sa, NULL) == -1)
        {
            perror("Errore nella gestione di SIGTSTP (inhibitor_child)");
            exit(EXIT_FAILURE);
        }

        struct sigaction sa2;

        sa2.sa_handler = SIG_DFL;
        sa2.sa_flags = 0;
        sigemptyset(&sa2.sa_mask);

        if (sigaction(SIGCONT, &sa2, NULL) == -1)
        {
            perror("Errore nella gestione di SIGCONT (inhibitor)");
            exit(EXIT_FAILURE);
        }

        struct timespec delay;
        delay.tv_sec = 0;
        delay.tv_nsec = 1000000; // 1 ms (deciso da me)

        struct inhibitor_data *inhibitor_data_child = (struct inhibitor_data *)shmat(shm_id_inhibitor, NULL, 0);
        TEST_ERROR;

        while (1)
        {

            double probability = 0.4; // probabilità del 40% (scelta da me) che l'inibitore non blocchi la scissione

            get_sem(sem_id_inhibitor, 0);
            flag_sem[3] = 1;

            if (random_inhib(0) < probability)
            {
                inhibitor_data_child->split_probability = 1;
            }
            else
            {
                inhibitor_data_child->split_probability = 0;
            }

            if (flag_sem[3] == 1)
            {
                release_sem(sem_id_inhibitor, 0);
            }
            flag_sem[3] = 0;

            nanosleep(&delay, NULL);
        }
    }

    /*------- PADRE INIBITORE -------*/
    /*------- Il padre "assorbe" energia da quella appena generata dagli atomi -------*/
    while (1)
    {
        get_sem(sem_id_queues, 2);
        flag_sem[1] = 1;

        struct message msg;
        if (msgrcv(message_queue_inhibitor, &msg, sizeof(msg), 0, IPC_NOWAIT) == -1)
        {
            if (errno == EAGAIN || errno == ENOMSG)
            {
                if (flag_sem[1] == 1)
                {
                    release_sem(sem_id_queues, 2);
                }
                flag_sem[1] = 0;
                continue;
            }
            else
            {
                if (flag_sem[1] == 1)
                {
                    release_sem(sem_id_queues, 2);
                }
                flag_sem[1] = 0;
                perror("Errore nella ricezione del messaggio inhibitor (atom)");
                exit(EXIT_FAILURE);
            }
        }

        if (flag_sem[1] == 1)
        {
            release_sem(sem_id_queues, 2);
        }
        flag_sem[1] = 0;

        int old_energy = msg.value;
        int energy = absorb_energy(old_energy);

        struct message msg_master;
        msg_master.type = 1;
        msg_master.value = energy;                     // Energia rimanente
        msg_master.second_value = old_energy - energy; // Energia assorbita (totale - energia rimanente)

        get_sem(sem_id_queues, 0);
        flag_sem[2] = 1;

        if (msgsnd(message_queue_master, &msg_master, sizeof(msg_master), 0) == -1) // Invia il messaggio con l'energia al master
        {
            release_sem(sem_id_queues, 0);
            perror("Errore nell'invio del messaggio inhibitor (master)");
            exit(EXIT_FAILURE);
        }

        if (flag_sem[2] == 1)
        {
            release_sem(sem_id_queues, 0);
        }
        flag_sem[2] = 0;

        // printf("Energia totale: %d\n", old_energy);
        // printf("Inibitore: Energia assorbita: %d\n", msg_master.second_value);
    }
}