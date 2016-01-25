#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "wator_main.h"
#include "wator.h"

/*######## VARIABILI GLOBALI ########################################## */

/* Istanza di Wator */
wator_t * Wator = NULL;

/* Identificatori dei thread */
pthread_t dispatcher_tid;
pthread_t * worker_tids;
pthread_t collector_tid;

/* Numero di worker da istanziare */
int nwork = NWORK_DEF;
/* Numero di cicili (chronon) da eseguire nella simulazione */
int nchron = CHRON_DEF;

/* Numero ciclo attuale e relativo mutex*/
volatile int actual_nchron = 1;
pthread_mutex_t actual_nchron_mtx = PTHREAD_MUTEX_INITIALIZER;

/* Identificatore per la chiusura della simulazione e dell'intero processo (e relativo mutex) */
volatile int toClose = 0;
pthread_mutex_t toClose_mtx = PTHREAD_MUTEX_INITIALIZER;

/* Colonne e righe di resto nella divisione in sottomatrici */
int rest_columns = 0;
int rest_rows = 0;

/* Coda FIFO dei jobs - Mutex e condition variable (comunicazione tra dispatcher e worker(s) */
pthread_mutex_t queue_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;
job_t * head = NULL;

/* Matrice di check aggiornamento cella (stesse dimensioni di quella del pianeta) */
int ** check_up = NULL;

/* Barriere usate nei worker per la sincronizzazione delle operazioni */
pthread_barrier_t brr;
pthread_barrier_t syncc;

/* condition variable ALL TASKS COMPLETED - Comunicazione tra worker e collector */
volatile int tasks_cmp = 0;
pthread_mutex_t task_cmp_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t task_cmp_cond = PTHREAD_COND_INITIALIZER;

/* condition variable LAST CHRONON COMPLETED - Comunicazione tra collector e dispatcher */
volatile int chron_cmp = 0;
pthread_mutex_t chron_cmp_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t chron_cmp_cond = PTHREAD_COND_INITIALIZER;

/* ########### FINE VARIABILI GLOBALI ####################################################### */

/* funzione che ritorna 1 (true) se ho ricevuto SIGTERM o SIGINT, altrimenti 0 (false) */
volatile int closing(){
	int i = 0;
	pthread_mutex_lock(&toClose_mtx);
	i = toClose;
	pthread_mutex_unlock(&toClose_mtx);
	return i;
}

/* ritorna il chronon attuale (incrementato di i - usato nel ciclo for del dispatcher ) */ 
volatile int GetNchron(volatile int i) {
	volatile int actual;
	pthread_mutex_lock(&actual_nchron_mtx);
	actual = actual_nchron = actual_nchron + i;
	pthread_mutex_unlock(&actual_nchron_mtx);
	return actual;
}

/* ####### FUNZIONI CODA #################################################################################### */
void enqueue(int i, int j) {
	job_t * tmp = NULL;
	job_t * new = NULL;
	/* alloco la nuova struttura */
	new = (job_t *) malloc(sizeof(job_t));
	new->i = i; new->j = j; new->flag = 0; new->next_job = NULL;
	
	if (head == NULL)
		head = new;
	else {
		tmp = head;
		while (tmp->next_job != NULL)
			tmp = tmp->next_job;
		tmp->next_job = new;
	}
}

/* ritorna un job non assegnato e lo setta come assegnato */
job_t * assign(){
	job_t * p = NULL;
	pthread_mutex_lock(&queue_mtx);
	p = head;
	while(p->flag != 0 && p != NULL)
		p = p->next_job;
	
	p->flag = 1;
	pthread_mutex_unlock(&queue_mtx);
	return p;
}

/* setta il job nel parametro come completato */
void completed(job_t * job){
	job->flag = 2;
}

int isEmpty() {
	return (head==NULL);
}

/* ritorna 1 (true) se tutti i job nella coda sono stati assegnati, 0 (false) altrimenti  */
int allassigned() {
	
 	job_t * tmp = NULL;
 	pthread_mutex_lock(&queue_mtx);
 	tmp =head;
 	while(tmp != NULL){
		if(tmp->flag == 0){
			pthread_mutex_unlock(&queue_mtx);
			return 0;
			}
		tmp = tmp->next_job; 
 	}
 	pthread_mutex_unlock(&queue_mtx);
	return 1;
}

/* ritorna 1 (true) se tutti i job nella coda sono stati completatai, 0 (false) altrimenti */
int allcompleted() {
	job_t * tmp = head;
 	while(tmp != NULL){
		if(tmp->flag != 2)
			return 0;
		tmp = tmp->next_job; 
 	}
return 1;
}

/* azzera la coda */
void erase_queue() {
	job_t * tmp;
	while(head != NULL){
		tmp = head->next_job;
		free(head);
		head =  tmp;
	}
}	


/* ###### FINE FUNZIONI CODA ########################################################################### */

/* scrive nella cella [*k][*l] della matrice check_up il chronon attuale */
void update_check(int * k, int * l){
	if (*k != -1 && *l != -1){
		check_up[*k][*l] = GetNchron(0);
		*k = *l = -1;	
	}
}

/* applico alla cella [i][j] le regole di Wator se tale cella non è stata scritta nel chronon attuale */
void update_cell(int i, int j) {
	int k = 0, l = 0;
	/* se la cella che devo processare è stata scritta nel chronon attuale la ignoro */
	if (check_up[i][j] < GetNchron(0)){
		switch (Wator->plan->w[i][j]) {
			case SHARK:
				if (shark_rule2(Wator,i,j,&k,&l) == ALIVE) {
					update_check(&k,&l);
					shark_rule1(Wator,i,j,&k,&l);
				} 
				update_check(&k,&l);
				break;
			case FISH:
				fish_rule4(Wator,i,j,&k,&l);
				update_check(&k,&l);
				fish_rule3(Wator,i,j,&k,&l);
				update_check(&k,&l);
				break;
			default: break;
		} 
	}
}

void* dispatcher() {
	int i, j;
	int column_submatrix;
	int row_submatrix;
	
	/* numero di sottomatrici per gruppo di righe */
	column_submatrix = Wator->plan->ncol / COL;
	
	/* numero di sottomatrici per gruppo di colonne */
	row_submatrix = Wator->plan->nrow / ROW;
	
	/* colonne e righe di resto (valori possibili 0,1,2) */
	rest_columns = Wator->plan->ncol % COL; 
	rest_rows = Wator->plan->nrow % ROW;
	
	for(;GetNchron(0) <= nchron && !closing(); GetNchron(1)){
			/* inserisco nella coda i job (le sottomatrici) da far processare ai worker */
			pthread_mutex_lock(&queue_mtx);
			for(i=0;i<row_submatrix;i++)
				for(j=0;j<column_submatrix;j++){
					enqueue(i*ROW,j*COL);
				}
			/* segnalo ai worker in attesa che la coda è stata riempita */
			pthread_cond_broadcast(&queue_cond);
			pthread_mutex_unlock(&queue_mtx);
			
			/* prima di passare al prossimo ciclo attendo che il collector mi comunichi che il ciclo corrente sia stato completato */
			pthread_mutex_lock(&chron_cmp_mtx);
			while(!chron_cmp)
				pthread_cond_wait(&chron_cmp_cond, &chron_cmp_mtx);
			chron_cmp = 0;
			pthread_mutex_unlock(&chron_cmp_mtx);
	}
	/* a questo punto, cancello i worker */
	for(i=0;i<nwork;i++)
		pthread_cancel(worker_tids[i]);
	
	return (void *)0;
}

/* funzione di cleanup per i thread worker */
void cleanup_f(void * fw) {
	pthread_mutex_unlock(&queue_mtx); /* rilascio l'eventuale lock acquisito */
	fclose((FILE*) fw);
}

void* worker(void* wid){
	int i, j, ii, jj;
	FILE* fw = NULL;
	job_t * job = NULL;
	char filename[18];
	/* creazione della stringa: wator_worker_ + wid */
	sprintf(filename,"wator_worker_%d",(int) wid);
	/* apertura del file in scrittura */
	fw = fopen(filename,"w");
	/* setto la funzione di cleanup in caso di canecellazione */
	pthread_cleanup_push(cleanup_f,(void *)fw);
	
	while(1) {
		
		/* attendo che la coda sia riempita dal dispatcher */
		pthread_mutex_lock(&queue_mtx);
		while(isEmpty())
			pthread_cond_wait(&queue_cond,&queue_mtx);
		pthread_mutex_unlock(&queue_mtx);
		
		/* se tutti i job sono assegnati ne eseguo uno fittizio per ragioni di sincronizzazione */
		if (allassigned()) {
			pthread_barrier_wait(&syncc);
			for(i=0;i<9;i++)
				pthread_barrier_wait(&brr);
		} 
		/* altrimenti me ne faccio assegnare uno e inizio l'elaborazione */
		else {
			job = assign();
			
			/* attendo che tutti i worker abbiano un job (reale o fittizio) */
			pthread_barrier_wait(&syncc);
			/* chiave dell'algoritmo di sincronizzazione: processare righe % 3 = i e colonne % 3 = j per non incorrere in collisioni */
			for(i=0;i<3;i++){
				for(j=0;j<3;j++){
					for(ii=job->i+i;ii<job->i+ROW;ii+=3) /* salto riga di 3 in 3 */
						for(jj=job->j+j;jj<job->j+COL;jj+=3) /* salto colonna di 3 in 3 */
							update_cell(ii,jj); /* chiamo la funzione che applica le regole del Wator */
					/* sincronizzazione per cui: tutte le righe % 3 = i e tutte le colonne % 3 = j sono state processte */
					pthread_barrier_wait(&brr);
				}
			}
			pthread_mutex_lock(&queue_mtx);
			/* segnalo che ho completato la mia sottomatrice (il mio job) */
			completed(job);
			
			/* controllo se tutti i job sono stati completati, in tal caso azzero la coda e sveglio il collector */
			if(allcompleted()){
			erase_queue();
			pthread_mutex_lock(&task_cmp_mtx);
			tasks_cmp=1;
			pthread_cond_signal(&task_cmp_cond);
			pthread_mutex_unlock(&task_cmp_mtx);
			}
			pthread_mutex_unlock(&queue_mtx);
		}
	}
	
	pthread_cleanup_pop(0);
	fclose(fw);
	return (void*) 0;
}

void* collector() {
	int i, j;
	char n;
	unsigned char cls = 0;
	int sck; /* file_descriptor socket */
	struct sockaddr_un saddr; /* socket address struct */
	
	/* inizializzazione del socket */
	sck = socket(AF_UNIX,SOCK_STREAM,0);
	strncpy(saddr.sun_path, SOCKNAME, UNIX_PATH_MAX);
	saddr.sun_family = AF_UNIX; 
	
	while(1){
		/* attendo che i worker abbiano completato tutti i job */
		pthread_mutex_lock(&task_cmp_mtx);
		while(!tasks_cmp)
			pthread_cond_wait(&task_cmp_cond, &task_cmp_mtx);
		tasks_cmp = 0;
		pthread_mutex_unlock(&task_cmp_mtx);
		
		/* gestisco le colonne non processate ( quelle di resto ) */
		for(j=Wator->plan->ncol-rest_columns;j<Wator->plan->ncol;j++);
			for(i=0;i<Wator->plan->nrow;i++)
				update_cell(i,j);
				
		/* gestisco le righe non processate (quelle di resto) tagliate delle colonne processate poco sopra */
		for(i=Wator->plan->nrow-rest_rows;i<Wator->plan->nrow;i++)
			for(j=0;j<Wator->plan->ncol-rest_columns;j++)
				update_cell(i,j);
				
		
		/* invio al visualizer la matrice da stampare se è l'ultimo ciclo o se mi devo chiudere */	
		if ((GetNchron(0) % nchron == 0) || closing()){
			cls = 1;
			/* tento la connessione */
			while (connect(sck,(struct sockaddr*) &saddr, sizeof(saddr)) == -1){
					if (errno != ENOENT)
						exit(EXIT_FAILURE);
				}
			/* scrivo sulla socket secondo il protocollo documentato nella relazione */
			write(sck,&Wator->plan->nrow,4); /* 4 size of int */
			write(sck,&Wator->plan->ncol,4);
			for(i=0;i<Wator->plan->nrow;i++)
				for(j=0;j<Wator->plan->ncol;j++){
					n = cell_to_char(Wator->plan->w[i][j]);
					write(sck,&n,1);
				}
			/* scrivo il flag di chiusura */
			write(sck,&cls,1);
			/* chiusura del socket */
			close(sck);
		}
		
		/* comunico al dispatcher che il ciclo corrente è stato completato */
		pthread_mutex_lock(&chron_cmp_mtx);
		chron_cmp = 1;
		pthread_cond_signal(&chron_cmp_cond);
		pthread_mutex_unlock(&chron_cmp_mtx);
		
		/* se era l'ultimo ciclo (quindi ho inviato la matrice al visualizer e l'ho terminato) termino il thread collector */
		if (cls)
			break;
	}
	close(sck);
	return (void*) 0;
}

int main(int argc, char *argv[]) {
	int i, pid, sig;
	FILE *f = NULL;
	char dumpfile[30];
	sigset_t set;
	
	/* check sui parametri */
	if (argc == 2 || argc == 4 || argc == 6 || argc == 8){
			if ((f=fopen(argv[1],"r")) == NULL)
				return 1;
			fclose(f);
			for(i=2; i<argc; i+=2)
				if (!strcmp(argv[i],"-n") && atoi(argv[i+1]) > 0)
						nwork = atoi(argv[i+1]);
				else if (!strcmp(argv[i],"-v") && atoi(argv[i+1]) > 0)
						nchron = atoi(argv[i+1]);
				else if (!strcmp(argv[i],"-f"))
						strcpy(dumpfile,argv[i+1]);
				else
					return 1;
		}
		else
			return 1;
	
	/* attivo il processo visualizer */
	pid = fork();
	if (pid == 0)
		execl("./visualizer","visualizer",dumpfile,(char*) NULL);
	
	/* creo una istanza di Wator */
	Wator = new_wator(argv[1]);
	
	/* alloco la matrice di check_up */
	check_up = (int **) malloc(Wator->plan->nrow*sizeof(int*));
	for(i=0; i<Wator->plan->nrow; i++)
		check_up[i] = (int *) malloc(Wator->plan->ncol*sizeof(int));
	
	/* apro il file di check per le invocazioni di SIGUSR1 */
	f = fopen("wator.check","w");
	
	/* gestione segnali */
	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGUSR1);
	sigaddset(&set, SIGTERM);
	pthread_sigmask(SIG_BLOCK, &set, 0);
	
	/* inizializzo le barriere dei workers in base al numero di workers */
	pthread_barrier_init(&brr, NULL, nwork);
	pthread_barrier_init(&syncc, NULL, nwork);
							
	/* creo il thread DISPATCHER */
	if ( pthread_create(&dispatcher_tid, NULL, dispatcher, NULL) != 0)
			return 1;
	
	/* creo i nwork thread WORKER */
	worker_tids = (pthread_t *) malloc(nwork*sizeof(pthread_t));
	for(i=0;i<nwork;i++)
		if ( pthread_create(&worker_tids[i], NULL, worker, (void *) i) != 0)
			return 1;
	
	/* creo il thread COLLECTOR */	
	if ( pthread_create(&collector_tid, NULL, collector, NULL) != 0)
			return 1;
	
	/* gestione segnali */	
	sigfillset(&set);
	
	while(!closing()) {
	sigwait(&set, &sig);
		switch(sig){
			case SIGINT: case SIGTERM:
				pthread_mutex_lock(&toClose_mtx);
				toClose = 1;
				pthread_mutex_unlock(&toClose_mtx);
				break;
			case SIGUSR1: 
				print_planet(f, Wator->plan);
				break;
		}
	}
	/* fase di chiusura del processo WATOR - in questo punto ho sicuro ricevuto un segnale di SIGINT o SIGTERM */
	fclose(f);
	
	pthread_join(dispatcher_tid, NULL);
	pthread_join(collector_tid, NULL);

	return 0;
}
