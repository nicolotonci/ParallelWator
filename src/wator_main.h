/* Default # Workers */
#define NWORK_DEF 3

/* Default # Chronon */
#define CHRON_DEF 5

/* Parametri per il socket */
#define UNIX_PATH_MAX 13
#define SOCKNAME "./visual.sck"


/* Dimensioni delle sottomatrici da dare ad ogni worker
	 NB! devono essere multipli di 3 entrambe le dimensioni
 */
#define ROW 3
#define COL 6

/* Struttura della coda FIFO condivisa per assegnare ad ogni worker il suo job */
typedef struct job job_t;
struct job {
	/* coordinate di base della sottomatrice */
	int i;
	int j;
	unsigned char flag; /* if assigned 1, if completed 2 */
	job_t * next_job;
};




