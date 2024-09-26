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

int simulation_started = 0; // Flag per l'inizio della simulazione

pid_t master_child, activator, feeder, inhibitor; // PID dei processi figli

int N_ATOMI_INIT, ENERGY_DEMAND, SIM_DURATION, ENERGY_EXPLODE_THRESHOLD, inhib_value;

int sem_id_starting_simulation; // Semaforo per l'inizio della simulazione
int sem_id_stats;               // Semaforo per le statistiche
int sem_id_simulation_data;     // Semaforo per i dati della simulazione
int sem_id_message_queues;      // Semaforo per le code di messaggi
int sem_id_inhibitor;           // Semaforo per l'inibitore

struct stats *stats;                                 // Struttura per le statistiche
struct stats previous_stats = {0, 0, 0, 0, 0, 0, 0}; // Struttura per le statistiche precedenti
struct simulation_data *sim_data;                    // Struttura per i dati della simulazione
struct inhibitor_data *inhib_data;                   // Struttura per i dati dell'inibitore
struct n_atoms_data *n_atoms_data;                   // Struttura per il numero di atomi

int master_msg_queue;    // Coda di messaggi del master
int activator_msg_queue; // Coda di messaggi dell'attivatore
int inhibitor_msg_queue; // Coda di messaggi dell'inibitore

int shm_stats;     // Id della memoria condivisa per le statistiche
int shm_sim_data;  // Id della memoria condivisa per i dati della simulazione
int shm_inhibitor; // Id della memoria condivisa per l'inibitore
int shm_atoms;     // Id della memoria condivisa per il numero di atomi

char simulation_data_str[10];
char sem_id_simulation_data_str[10];
char min_n_atom_str[10];

// Funzione per liberare le risorse allocate
void free_resources()
{
    if (shmdt(stats) == -1) // Stacco la memoria condivisa delle statistiche
    {
        perror("Errore nella shmdt delle statistiche");
    }

    if (shmdt(sim_data) == -1) // Stacco la memoria condivisa dei dati della simulazione
    {
        perror("Errore nella shmdt dei dati della simulazione");
    }

    if (shmdt(n_atoms_data) == -1) // Stacco la memoria condivisa del numero di atomi
    {
        perror("Errore nella shmdt del numero di atomi");
    }

    if (shmdt(inhib_data) == -1) // Stacco la memoria condivisa dell'inibitore
    {
        perror("Errore nella shmdt dell'inibitore");
    }

    if (shmctl(shm_stats, IPC_RMID, NULL) == -1) // Rimuovo la memoria condivisa delle statistiche
    {
        perror("Errore nella rimozione della memoria condivisa delle statistiche");
    }

    if (shmctl(shm_sim_data, IPC_RMID, NULL) == -1) // Rimuovo la memoria condivisa dei dati della simulazione
    {
        perror("Errore nella rimozione della memoria condivisa dei dati della simulazione");
    }

    if (shmctl(shm_atoms, IPC_RMID, NULL) == -1) // Rimuovo la memoria condivisa del numero di atomi
    {
        perror("Errore nella rimozione della memoria condivisa del numero di atomi");
    }

    if (shmctl(shm_inhibitor, IPC_RMID, NULL) == -1) // Rimuovo la memoria condivisa dell'inibitore
    {
        perror("Errore nella rimozione della memoria condivisa dell'inibitore");
    }

    if (semctl(sem_id_starting_simulation, 0, IPC_RMID) == -1) // Rimuovo il semaforo per l'inizio della simulazione
    {
        perror("Errore nella rimozione del semaforo di inizializzazione della simulazione");
    }

    if (semctl(sem_id_stats, 0, IPC_RMID) == -1) // Rimuovo il semaforo per le statistiche
    {
        perror("Errore nella rimozione del semaforo delle statistiche");
    }

    if (semctl(sem_id_simulation_data, 0, IPC_RMID) == -1) // Rimuovo il semaforo per i dati della simulazione
    {
        perror("Errore nella rimozione del semaforo dei dati della simulazione");
    }

    if (semctl(sem_id_message_queues, 0, IPC_RMID) == -1) // Rimuovo il semaforo per le code di messaggi
    {
        perror("Errore nella rimozione del semaforo delle code di messaggi");
    }

    if (semctl(sem_id_inhibitor, 0, IPC_RMID) == -1) // Rimuovo il semaforo per l'inibitore
    {
        perror("Errore nella rimozione del semaforo dell'inibitore");
    }

    if (msgctl(master_msg_queue, IPC_RMID, NULL) == -1) // Rimuovo la coda di messaggi del master
    {
        perror("Errore nella rimozione della coda di messaggi del master");
    }

    if (msgctl(activator_msg_queue, IPC_RMID, NULL) == -1) // Rimuovo la coda di messaggi dell'attivatore
    {
        perror("Errore nella rimozione della coda di messaggi dell'attivatore");
    }

    if (msgctl(inhibitor_msg_queue, IPC_RMID, NULL) == -1) // Rimuovo la coda di messaggi dell'inibitore
    {
        perror("Errore nella rimozione della coda di messaggi dell'inibitore");
    }
}

// Funzione per la gestione dei segnali e terminazione dei figli nel master
void manage_signal(int signum, siginfo_t *info, void *ptr)
{
    char buffer[100];
    int len;

    write(STDOUT_FILENO, "-------------------------------------------------------\n", 57);

    switch (signum)
    {
    case SIGALRM:
        write(STDOUT_FILENO, "TIMEOUT RAGGIUNTO, sto terminando...\n", 38);
        break;
    case SIGUSR1:
        write(STDOUT_FILENO, "MELTDOWN RILEVATO, sto terminando...\n", 38);
        break;
    case SIGINT:
        write(STDOUT_FILENO, "Segnale di interruzione SIGINT rilevato, termino.\n", 51);
        break;
    default:
        write(STDOUT_FILENO, "Segnale sconosciuto rilevato\n", 30);
        break;
    }

    kill(0, SIGTERM);
    while (wait(NULL) > 0)
        ; // Attende la terminazione di tutti i processi figli
    free_resources();
    exit(EXIT_SUCCESS);
}

// Funzione per la stampa della durata della simulazione in minuti e secondi e dell'inizio della simulazione
void print_start(int seconds)
{
    char buffer[100];
    int len;

    if (seconds < 60)
    {
        len = snprintf(buffer, sizeof(buffer), "La simulazione durera' %d secondi\n", seconds);
        write(STDOUT_FILENO, buffer, len);
    }
    else
    {
        int minutes = seconds / 60;
        seconds = seconds % 60;
        snprintf(buffer, sizeof(buffer), "La simulazione durera' %d minuti e %d secondi\n", minutes, seconds);
        write(STDOUT_FILENO, buffer, strlen(buffer));
    }

    write(STDOUT_FILENO, "INIZIO SIMULAZIONE\n", 19);
}

int main(int argc, char *argv[])
{
    if (argc != 11)
    {
        fprintf(stderr, "Errore nei parametri passati al master\n");
        fprintf(stderr, "Usage: %s <n_atoms_init> <n_atom_max> <energy_demand> <energy_explode_threshold> <min_n_atom> <sim_duration> <n_nuovi_atomi>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // argv[0] = nome del programma
    // argv[1] = atomi iniziali
    // argv[2] = numero atomico minimo
    // argv[3] = numero atomico massimo
    // argv[4] = energia richiesta
    // argv[5] = soglia di esplosione
    // argv[6] = durata simulazione
    // argv[7] = numero di nuovi atomi creati ad ogni iterazione
    // argv[8] = step attivatore
    // argv[9] = step alimentazione
    // argv[10] = valore dell'inibitore (decide se attivare o meno l'inibitore)

    N_ATOMI_INIT = atoi(argv[1]);
    ENERGY_DEMAND = atoi(argv[4]);
    ENERGY_EXPLODE_THRESHOLD = atoi(argv[5]);
    SIM_DURATION = atoi(argv[6]);
    inhib_value = atoi(argv[10]);

    struct sigaction sa;

    // Imposta la gestione dei segnali
    sa.sa_sigaction = manage_signal;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGALRM, &sa, NULL) == -1)
    {
        perror("Errore gestione segnale SIGALRM (master)");
        exit(EXIT_FAILURE);
    }

    if (sigaction(SIGUSR1, &sa, NULL) == -1)
    {
        perror("Errore gestione segnale SIGUSR1 (master)");
        exit(EXIT_FAILURE);
    }

    if (sigaction(SIGUSR2, &sa, NULL) == -1)
    {
        perror("Errore gestione segnale SIGUSR2 (master)");
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        perror("Errore gestione segnale SIGINT (master)");
        exit(EXIT_FAILURE);
    }

    // Ignora il segnale SIGTSTP
    sa.sa_handler = SIG_IGN;
    if (sigaction(SIGTSTP, &sa, NULL) == -1)
    {
        perror("Errore gestione segnale SIGTSTP (master)");
        exit(EXIT_FAILURE);
    }

    srand(time(NULL)); // indica il seed di randomizzazione (usato per la generazione di numeri casuali)

    /*---- CREAZIONE E INIZIALIZZAZIONE STRUTTURE DELLA SIMULAZIONE ----*/

    /*-- SEMAFORI --*/
    sem_id_starting_simulation = create_sem(4);
    sem_id_stats = create_sem(1);
    sem_id_simulation_data = create_sem(2);
    sem_id_message_queues = create_sem(3);
    sem_id_inhibitor = create_sem(1);

    // Imposto i semafori
    semctl(sem_id_starting_simulation, 0, SETVAL, 0); // semaforo indice 0 per l'attivatore
    semctl(sem_id_starting_simulation, 1, SETVAL, 0); // semaforo indice 1 per il feeder
    semctl(sem_id_starting_simulation, 2, SETVAL, 0); // semaforo indice 2 per il master
    semctl(sem_id_starting_simulation, 3, SETVAL, 0); // semaforo indice 3 per l'inibitore

    semctl(sem_id_stats, 0, SETVAL, 1); // semaforo indice 0 per le statistiche

    semctl(sem_id_simulation_data, 0, SETVAL, 1); // semaforo indice 0 per i dati della simulazione
    semctl(sem_id_simulation_data, 1, SETVAL, 1); // semaforo indice 1 per la memoria condivisa contenente il numero di processi atomo

    semctl(sem_id_message_queues, 0, SETVAL, 1); // semaforo indice 0 per la coda di messaggi del master
    semctl(sem_id_message_queues, 1, SETVAL, 1); // semaforo indice 1 per la coda di messaggi dell'attivatore
    semctl(sem_id_message_queues, 2, SETVAL, 1); // semaforo indice 2 per la coda di messaggi dell'inibitore

    semctl(sem_id_inhibitor, 0, SETVAL, 1);

    /*-- CODE DI MESSAGGI --*/
    master_msg_queue = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);
    TEST_ERROR;

    activator_msg_queue = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);
    TEST_ERROR;

    inhibitor_msg_queue = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);
    TEST_ERROR;

    /*-- MEMORIA CONDIVISA --*/
    // Creazione della memoria condivisa per le statistiche
    shm_stats = shmget(IPC_PRIVATE, sizeof(struct stats), IPC_CREAT | 0666);
    TEST_ERROR;

    stats = (struct stats *)shmat(shm_stats, NULL, 0);
    TEST_ERROR;

    stats->n_activations = 0;
    stats->n_splits = 0;
    stats->energy_produced = 0;
    stats->energy_wasted = 0;
    stats->waste = 0;
    stats->energy_absorbed = 0;
    stats->splits_failed = 0;

    // Creazione della memoria condivisa per l'inibitore
    shm_inhibitor = shmget(IPC_PRIVATE, sizeof(struct inhibitor_data), IPC_CREAT | 0666);
    TEST_ERROR;

    inhib_data = (struct inhibitor_data *)shmat(shm_inhibitor, NULL, 0);
    TEST_ERROR;

    inhib_data->flag_inhib = inhib_value;
    inhib_data->split_probability = 0;

    // Creazione della memoria condivisa per il numero di atomi
    shm_atoms = shmget(IPC_PRIVATE, sizeof(struct n_atoms_data), IPC_CREAT | 0666);
    TEST_ERROR;

    n_atoms_data = (struct n_atoms_data *)shmat(shm_atoms, NULL, 0);
    TEST_ERROR;

    n_atoms_data->n_atoms = 0;

    // Creazione della memoria condivisa per i dati della simulazione
    shm_sim_data = shmget(IPC_PRIVATE, sizeof(struct simulation_data), IPC_CREAT | 0666);
    TEST_ERROR;

    sim_data = (struct simulation_data *)shmat(shm_sim_data, NULL, 0);
    TEST_ERROR;

    sim_data->pid_master = getpid();
    sim_data->MIN_N_ATOMICO = atoi(argv[2]);
    sim_data->N_ATOM_MAX = atoi(argv[3]);
    sim_data->N_NUOVI_ATOMI = atoi(argv[7]);
    sim_data->STEP_ATTIVATORE = atol(argv[8]);
    sim_data->STEP_ALIMENTAZIONE = atol(argv[9]);
    sim_data->sem_id_starting_simulation = sem_id_starting_simulation;
    sim_data->message_queue_master = master_msg_queue;
    sim_data->message_queue_activator = activator_msg_queue;
    sim_data->sem_id_queues = sem_id_message_queues;
    sim_data->shm_n_atoms = shm_atoms;
    sim_data->inhib_value = inhib_value;
    sim_data->shm_inhibitor = shm_inhibitor;
    sim_data->sem_id_inhibitor = sem_id_inhibitor;

    snprintf(simulation_data_str, sizeof(simulation_data_str), "%d", shm_sim_data);                         // id memoria condivisa dati simulazione
    snprintf(sem_id_simulation_data_str, sizeof(sem_id_simulation_data_str), "%d", sem_id_simulation_data); // id semaforo memoria condivisa dati simulazione

    /*------ CREAZIONE PROCESSO MASTER_CHILD (USATO PER AGGIORNARE LE STATISTICHE) -------*/
    master_child = fork();
    if (master_child == -1)
    {
        kill(getpid(), SIGUSR1);
        exit(EXIT_FAILURE);
    }
    else if (master_child == 0)
    {
        struct sigaction sa;

        // Imposta la gestione dei segnali
        sa.sa_sigaction = sigterm_handler;
        sa.sa_flags = SA_SIGINFO;
        sigemptyset(&sa.sa_mask);

        if (sigaction(SIGTERM, &sa, NULL) == -1)
        {
            perror("Errore gestione SIGTERM (master_child)");
            exit(EXIT_FAILURE);
        }

        // Ignora il segnale SIGTSTP
        sa.sa_handler = SIG_IGN;
        if (sigaction(SIGTSTP, &sa, NULL) == -1)
        {
            perror("Errore gestione SIGTSTP (master_child)");
            exit(EXIT_FAILURE);
        }

        struct stats *child_stats = (struct stats *)shmat(shm_stats, NULL, 0);
        TEST_ERROR;

        struct simulation_data *child_sim_data = (struct simulation_data *)shmat(shm_sim_data, NULL, 0);
        TEST_ERROR;

        struct n_atoms_data *child_n_atoms_data = (struct n_atoms_data *)shmat(shm_atoms, NULL, 0);
        TEST_ERROR;

        int flag_starting_master = 0;

        while (1)
        {
            get_sem(sem_id_message_queues, 0);

            struct message msg;
            if (msgrcv(master_msg_queue, &msg, sizeof(msg), 0, IPC_NOWAIT) == -1)
            {
                if (errno == EAGAIN || errno == ENOMSG)
                {
                    release_sem(sem_id_message_queues, 0);
                    continue;
                }
                else
                {
                    printf("Errore nella lettura della coda di messaggi (master_child)\n");
                    exit(EXIT_FAILURE);
                }
            }

            release_sem(sem_id_message_queues, 0);

            switch (msg.type)
            {
            case 1:
                get_sem(sem_id_stats, 0);
                child_stats->energy_produced += msg.value;
                child_stats->n_splits++;
                child_stats->energy_absorbed += msg.second_value;
                release_sem(sem_id_stats, 0);

                if (flag_starting_master == 0)
                {
                    release_sem(sem_id_starting_simulation, 2);
                    flag_starting_master = 1;
                }
                break;
            case 2:
                if (msg.value == 1)
                {
                    get_sem(sem_id_simulation_data, 1);
                    n_atoms_data->n_atoms++;
                    release_sem(sem_id_simulation_data, 1);
                }
                else if (msg.value == 2)
                {
                    get_sem(sem_id_stats, 0);
                    child_stats->waste++;
                    release_sem(sem_id_stats, 0);

                    get_sem(sem_id_simulation_data, 1);
                    n_atoms_data->n_atoms--;
                    release_sem(sem_id_simulation_data, 1);
                }
                break;
            case 3:
                get_sem(sem_id_stats, 0);
                child_stats->n_activations += msg.value;
                child_stats->splits_failed += msg.second_value;
                release_sem(sem_id_stats, 0);
                break;
            default:
                // Dovrebbe essere impossibile finire qui
                break;
            }
        }
    }

    /*------ CREAZIONE PROCESSI ATOMI -------*/
    for (int i = 0; i < N_ATOMI_INIT; i++)
    {
        get_sem(sem_id_simulation_data, 0);
        int atomic_number = new_atomic_number(sim_data->N_ATOM_MAX); // nuovo numero atomico casuale (gestito dal padre)
        release_sem(sem_id_simulation_data, 0);

        char atomic_number_str[10];
        snprintf(atomic_number_str, sizeof(atomic_number_str), "%d", atomic_number);

        pid_t new_fork_atom = fork();

        if (new_fork_atom == -1)
        {
            kill(getpid(), SIGUSR1);
            exit(EXIT_FAILURE);
        }
        else if (new_fork_atom == 0)
        {

            // inviare id coda messaggi activator
            char *const argAtom[] = {"atom", atomic_number_str, simulation_data_str, sem_id_simulation_data_str, NULL};

            int ex_atom = execve("../bin/atom", argAtom, NULL);
            TEST_ERROR;
        }
    }

    /*------ CREAZIONE PROCESSO ATTIVATORE -------*/
    activator = fork();
    if (activator == -1)
    {
        kill(getpid(), SIGUSR1);
        exit(EXIT_FAILURE);
    }
    else if (activator == 0)
    {

        struct sigaction sa;
        sa.sa_sigaction = manage_signal;
        sa.sa_flags = SA_SIGINFO;

        if (sigaction(SIGTERM, &sa, NULL) == -1)
        {
            perror("Errore gestione SIGTERM (activator)");
            exit(EXIT_FAILURE);
        }

        // printf("Attendo il segnale di inizio simulazione ACTIVATOR\n");
        get_sem(sem_id_starting_simulation, 0); // attende il segnale di inizio simulazione

        char *const argActivator[] = {"activator", simulation_data_str, sem_id_simulation_data_str, NULL};

        int ex_activator = execve("../bin/activator", argActivator, NULL);
        TEST_ERROR;
    }

    /*------ CREAZIONE PROCESSO FEEDER -------*/
    feeder = fork();
    if (feeder == -1)
    {
        kill(getpid(), SIGUSR1);
        exit(EXIT_FAILURE);
    }
    else if (feeder == 0)
    {
        struct sigaction sa;
        sa.sa_sigaction = manage_signal;
        sa.sa_flags = SA_SIGINFO;

        if (sigaction(SIGTERM, &sa, NULL) == -1)
        {
            perror("Errore gestione SIGTERM (feeder)");
            exit(EXIT_FAILURE);
        }

        // printf("Attendo il segnale di inizio simulazione FEEDER\n");
        get_sem(sem_id_starting_simulation, 1); // attende il segnale di inizio simulazione

        char *const argFeeder[] = {"feeder", simulation_data_str, sem_id_simulation_data_str, NULL};

        int ex_feeder = execve("../bin/feeder", argFeeder, NULL);
        TEST_ERROR;
    }

    /*------ CREAZIONE PROCESSO INIBITORE -------*/
    if (inhib_value == 1)
    {
        inhibitor = fork();
        if (inhibitor == -1)
        {
            kill(getpid(), SIGUSR1);
            exit(EXIT_FAILURE);
        }
        else if (inhibitor == 0)
        {
            struct sigaction sa;
            sa.sa_sigaction = manage_signal;
            sa.sa_flags = SA_SIGINFO;

            if (sigaction(SIGTERM, &sa, NULL) == -1)
            {
                perror("Errore gestione SIGTERM (inhibitor)");
                exit(EXIT_FAILURE);
            }

            char *const argInhibitor[] = {"inhibitor", simulation_data_str, sem_id_simulation_data_str, NULL};

            // printf("Attendo il segnale di inizio simulazione INHIBITOR\n");
            get_sem(sem_id_starting_simulation, 3); // attende il segnale di inizio simulazione

            int ex_inhibitor = execve("../bin/inhibitor", argInhibitor, NULL);
            TEST_ERROR;
        }
    }

    /*-----INIZIO SIMULAZIONE-----*/
    struct timespec delay;
    delay.tv_sec = 1;
    delay.tv_nsec = 0;

    release_sem(sem_id_starting_simulation, 0); // invia il segnale di inizio simulazione (activator)
    release_sem(sem_id_starting_simulation, 1); // invia il segnale di inizio simulazione (feeder)
    if (inhib_value == 1)
    {
        release_sem(sem_id_starting_simulation, 3); // invia il segnale di inizio simulazione (inhibitor)
    }

    get_sem(sem_id_starting_simulation, 2); // attende il segnale di inizio simulazione (master_child)

    print_start(SIM_DURATION);
    alarm(SIM_DURATION);

    /*-----STAMPA DELLE STATISTCHE-----*/
    while (1)
    {
        nanosleep(&delay, NULL);

        get_sem(sem_id_simulation_data, 0);

        stats->energy_produced -= ENERGY_DEMAND;
        stats->energy_wasted += ENERGY_DEMAND;

        const char *separator = "-------------------------------------------------------\n";
        write(STDOUT_FILENO, separator, strlen(separator));

        char buffer[256];
        int len;

        len = snprintf(buffer, sizeof(buffer), "Numero di attivazioni   %9d (Ultimo secondo: %d)\n", stats->n_activations, stats->n_activations - previous_stats.n_activations);
        write(STDOUT_FILENO, buffer, len);

        len = snprintf(buffer, sizeof(buffer), "Numero di scissioni:    %9d (Ultimo secondo: %d)\n", stats->n_splits, max(0, stats->n_splits - previous_stats.n_splits));
        write(STDOUT_FILENO, buffer, len);

        len = snprintf(buffer, sizeof(buffer), "Energia prodotta:       %9d (Ultimo secondo: %d)\n", stats->energy_produced, max(0, stats->energy_produced - previous_stats.energy_produced));
        write(STDOUT_FILENO, buffer, len);

        len = snprintf(buffer, sizeof(buffer), "Energia consumata:      %9d (Ultimo secondo: %d)\n", stats->energy_wasted, ENERGY_DEMAND);
        write(STDOUT_FILENO, buffer, len);

        len = snprintf(buffer, sizeof(buffer), "Scorie                  %9d (Ultimo secondo: %d)\n", stats->waste, max(0, stats->waste - previous_stats.waste));
        write(STDOUT_FILENO, buffer, len);

        if (inhib_value == 1) // Stampa delle statistiche dell'inibitore
        {
            write(STDOUT_FILENO, "\n", 1);

            len = snprintf(buffer, sizeof(buffer), "Energia assorbita:      %9d (Ultimo secondo: %d)\n", stats->energy_absorbed, max(0, stats->energy_absorbed - previous_stats.energy_absorbed));
            write(STDOUT_FILENO, buffer, len);

            len = snprintf(buffer, sizeof(buffer), "Scissioni non avvenute: %9d (Ultimo secondo: %d)\n", stats->splits_failed, max(0, stats->splits_failed - previous_stats.splits_failed));
            write(STDOUT_FILENO, buffer, len);
        }

        if (stats->energy_produced < 0) // Se l'energia prodotta è minore di 0, si verifica blackout
        {
            release_sem(sem_id_simulation_data, 0);

            write(STDOUT_FILENO, separator, strlen(separator));
            write(STDOUT_FILENO, "BLACKOUT RILEVATO, sto terminando...\n", strlen("BLACKOUT RILEVATO, sto terminando...\n"));

            kill(0, SIGTERM);
            while (wait(NULL) > 0)
                ; // Attende la terminazione di tutti i processi figli
            free_resources();
            exit(EXIT_SUCCESS);
        }
        else if (stats->energy_produced > ENERGY_EXPLODE_THRESHOLD) // Se l'energia prodotta è maggiore della soglia di esplosione, si verifica explode
        {
            release_sem(sem_id_simulation_data, 0);

            write(STDOUT_FILENO, separator, strlen(separator));
            write(STDOUT_FILENO, "ESPLOSIONE RILEVATA, sto terminando...\n", strlen("ESPLOSIONE RILEVATA, sto terminando...\n"));

            kill(0, SIGTERM);
            while (wait(NULL) > 0)
                ; // Attende la terminazione di tutti i processi figli
            free_resources();
            exit(EXIT_SUCCESS);
        }

        release_sem(sem_id_simulation_data, 0);

        previous_stats = *stats;
    }

    return 0;
}
