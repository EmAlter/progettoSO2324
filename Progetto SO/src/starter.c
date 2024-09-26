#include "functions.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>

// 1000000000 nanosecondi = 1 secondo

// Funzione che converte il valore inibitore in 0 o 1
char convert_inhib_value(char inhib_value)
{
    if (inhib_value == 's')
    {
        return '1';
    }
    else if (inhib_value == 'n')
    {
        return '0';
    }
}

// Funzione che controlla se i valori letti dal file sono numeri
int check_filedata_values(char *token)
{
    if (token == NULL)
    {
        return 0;
    }
    else
    {
        for (int i = 0; token[i] != '\0'; i++)
        {
            if (!isdigit(token[i]) && token[i] != '\n')
            {
                return 0;
            }
        }
    }
    return 1;
}

// Funzione che legge i dati dal file
char **read_data_from_file(char *sim_data_fields[])
{
    FILE *file;
    char buffer[200];
    int index = 1; // Il campo 0 è già stato inizializzato con "starter" (nome del programma)

    // Apre il file in lettura
    if ((file = fopen("../data/filedata.txt", "r")) == NULL)
    {
        perror("Errore nell'apertura del file: il file si deve trovare nella cartella data e chiamarsi filedata.txt\n");
        exit(EXIT_FAILURE);
    }

    printf("Lettura dati dal file...\n");

    // Legge il file riga per riga
    while (fgets(buffer, sizeof(buffer), file) != NULL)
    {
        char *token = strtok(buffer, ":");
        if (token != NULL)
        {
            token = strtok(NULL, "\n"); // Legge il valore intero dopo il carattere ':' (fino al carattere di newline) e lo assegna al campo puntato da field
            while (isspace(*token))
                token++; // Salta eventuali spazi bianchi (tabulazioni, spazi, ecc.)
            if (!check_filedata_values(token))
            {
                printf("Il campo %d contiene caratteri non validi\n", index);
                fclose(file);
                exit(EXIT_FAILURE);
            }
            sim_data_fields[index] = strdup(token);
            TEST_ERROR;
            index++; // Passa al campo successivo
        }

        else
        {
            printf("Il campo %d è vuoto\n", index);
            fclose(file);
            exit(EXIT_FAILURE);
        }
    }

    fclose(file);

    printf("Dati letti dal file.\n");
    printf("--------------------\n");

    return sim_data_fields;
}

// Funzione che esegue la simulazione scelta
void choose_simulation(int scelta, char *sim_data_fields[])
{
    int ex_master;
    switch (scelta)
    {
    case 1:                                // EXPLODE
        sim_data_fields[1] = "2";          // numero di atomi iniziali
        sim_data_fields[2] = "25";         // numero atomico minimo
        sim_data_fields[3] = "130";        // numero atomico massimo
        sim_data_fields[4] = "170";        // energia prelevata dal master
        sim_data_fields[5] = "100000";     // soglia di esplosione
        sim_data_fields[6] = "300";        // tempo di simulazione
        sim_data_fields[7] = "5";          // numero di nuovi atomi generati dal feeder
        sim_data_fields[8] = "1000000000"; // step di attivatore
        sim_data_fields[9] = "2000000000"; // step di alimentazione

        ex_master = execve("../bin/master", sim_data_fields, NULL);
        TEST_ERROR;
        break;

    case 2:                                // BLACKOUT
        sim_data_fields[1] = "4";          // numero di atomi iniziali
        sim_data_fields[2] = "13";         // numero atomico minimo
        sim_data_fields[3] = "47";         // numero atomico massimo
        sim_data_fields[4] = "139";        // energia prelevata dal master
        sim_data_fields[5] = "1500000";    // soglia di esplosione
        sim_data_fields[6] = "300";        // tempo di simulazione
        sim_data_fields[7] = "2";          // numero di nuovi atomi generati dal feeder
        sim_data_fields[8] = "3000000000"; // step di attivatore
        sim_data_fields[9] = "3000000000"; // step di alimentazione

        ex_master = execve("../bin/master", sim_data_fields, NULL);
        TEST_ERROR;
        break;

    case 3:                                // MELTDOWN
        sim_data_fields[1] = "20";         // numero di atomi iniziali
        sim_data_fields[2] = "1";          // numero atomico minimo
        sim_data_fields[3] = "20";         // numero atomico massimo
        sim_data_fields[4] = "1";          // energia prelevata dal master
        sim_data_fields[5] = "1500000";    // soglia di esplosione
        sim_data_fields[6] = "300";        // tempo di simulazione
        sim_data_fields[7] = "1";          // numero di nuovi atomi generati dal feeder
        sim_data_fields[8] = "2000000000"; // step di attivatore
        sim_data_fields[9] = "1";          // step di alimentazione

        ex_master = execve("../bin/master", sim_data_fields, NULL);
        TEST_ERROR;
        break;

    case 4:                                // TIMEOUT
        sim_data_fields[1] = "5";          // numero di atomi iniziali
        sim_data_fields[2] = "1";          // numero atomico minimo
        sim_data_fields[3] = "30";         // numero atomico massimo
        sim_data_fields[4] = "3";          // energia prelevata dal master
        sim_data_fields[5] = "100000";     // soglia di esplosione
        sim_data_fields[6] = "15";         // tempo di simulazione
        sim_data_fields[7] = "2";          // numero di nuovi atomi generati dal feeder
        sim_data_fields[8] = "1000000000"; // step di attivatore
        sim_data_fields[9] = "1000000000"; // step di alimentazione

        ex_master = execve("../bin/master", sim_data_fields, NULL);
        TEST_ERROR;
        break;

    case 5:

        sim_data_fields = read_data_from_file(sim_data_fields);

        /*for (int i = 1; i < 12; i++)
        {
            printf("Campo %d: %s\n", i, sim_data_fields[i]);
        }*/

        ex_master = execve("../bin/master", sim_data_fields, NULL);
        TEST_ERROR;
        break;

    default:
        printf("Scelta non valida\n");
        exit(EXIT_FAILURE);
    }
}

int main()
{
    int scelta;
    char inhib_value;
    char *sim_data_fields[12];

    sim_data_fields[0] = "./master";
    sim_data_fields[11] = NULL;

    printf("POSSIBILI SIMULAZIONI:\n");

    printf("1) Simulazione 1:\n");
    printf("RISULTATO ATTESO: Terminazione per Explode\n");
    printf("\n");
    printf("Numero di nuovi atomi: 5\n");
    printf("Numero minimo atomico: 25\n");
    printf("Numero massimo atomico: 130\n");
    printf("Energia prelevata dal master: 170\n");
    printf("Energia oltre la quale l'atomo esplode: 100000\n");
    printf("Durata della simulazione in secondi: 30\n");
    printf("Atomi che immette il processo alimentazione: 5\n");
    printf("Step di attivatore: 1 secondo\n");
    printf("Step di alimentazione: 2 secondi\n");

    printf("--------------------\n");

    printf("2) Simulazione 2:\n");
    printf("RISULTATO ATTESO: Terminazione per Blackout\n");
    printf("\n");
    printf("Numero di nuovi atomi: 4\n");
    printf("Numero minimo atomico: 13\n");
    printf("Numero massimo atomico: 47\n");
    printf("Energia prelevata dal master: 139\n");
    printf("Energia oltre la quale l'atomo esplode: 150000\n");
    printf("Durata della simulazione in secondi: 300\n");
    printf("Atomi che immette il processo alimentazione: 2\n");
    printf("Step di attivatore: 3 secondi\n");
    printf("Step di alimentazione: 3 secondi\n");

    printf("--------------------\n");

    printf("3) Simulazione 3:\n");
    printf("RISULTATO ATTESO: Terminazione per Meltdown\n");
    printf("\n");
    printf("Numero di nuovi atomi: 20\n");
    printf("Numero minimo atomico: 1\n");
    printf("Numero massimo atomico: 20\n");
    printf("Energia prelevata dal master: 1\n");
    printf("Energia oltre la quale l'atomo esplode: 150000\n");
    printf("Durata della simulazione in secondi: 300\n");
    printf("Atomi che immette il processo alimentazione: 1\n");
    printf("Step di attivatore: 2 secondi\n");
    printf("Step di alimentazione: 1 nanosecondo\n");

    printf("--------------------\n");

    printf("4) Simulazione 4:\n");
    printf("RISULTATO ATTESO: Terminazione per Timeout\n");
    printf("\n");
    printf("Numero di nuovi atomi: 5\n");
    printf("Numero minimo atomico: 1\n");
    printf("Numero massimo atomico: 30\n");
    printf("Energia prelevata dal master: 3\n");
    printf("Energia oltre la quale l'atomo esplode: 100000\n");
    printf("Durata della simulazione in secondi: 15\n");
    printf("Atomi che immette il processo alimentazione: 2\n");
    printf("Step di attivatore: 1 secondo\n");
    printf("Step di alimentazione: 1 secondo\n");

    printf("--------------------\n");

    printf("5) Simulazione con i dati letti dal file\n");

    printf("--------------------\n");
    printf("Inserisci il numero corrispondente alla simulazione che vuoi eseguire (1/2/3/4/5): ");
    scanf("%d", &scelta);
    if (scelta < 1 || scelta > 5)
    {
        printf("Scelta non valida\n");
        exit(EXIT_FAILURE);
    }

    printf("Vuoi il processo inibitore in questa simulazione? (s/n): ");
    scanf(" %c", &inhib_value);
    if (inhib_value != 's' && inhib_value != 'n')
    {
        printf("Scelta non valida\n");
        exit(EXIT_FAILURE);
    }

    char char_str[2];
    char_str[0] = convert_inhib_value(inhib_value);
    char_str[1] = '\0'; // Aggiunge il terminatore di stringa

    // Assegna la stringa di caratteri all'indice 10 dell'array
    sim_data_fields[10] = malloc(sizeof(char) * 2);
    if (sim_data_fields[10] == NULL)
    {
        fprintf(stderr, "Errore di allocazione della memoria\n");
        exit(EXIT_FAILURE);
    }
    strcpy(sim_data_fields[10], char_str);

    printf("--------------------\n");
    if (scelta != 5)
    {
        printf("Hai scelto la simulazione %d\n", scelta);
    }
    else
    {
        printf("Hai scelto la simulazione con i dati dal file\n");
    }
    if (inhib_value == 's')
    {
        printf("Il processo Inibitore verrà avviato\n");
    }
    else if (inhib_value == 'n')
    {
        printf("Il processo Inibitore non verrà avviato\n");
    }
    printf("--------------------\n");

    choose_simulation(scelta, sim_data_fields);

    free(sim_data_fields[10]);

    return 0;
}
