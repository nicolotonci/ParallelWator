/**
   \file
   \author lso15 

   \brief struttura ad albero relativa agli utenti connessiwator
*/ 
#ifndef __WATOR__H
#define __WATOR__H

#include <stdio.h>

/** file di configurazione */
static const char CONFIGURATION_FILE[] = "wator.conf";

/** tipo delle celle del pianeta:
   SHARK squalo
   FISH pesce
   WATER solo acqua

*/
typedef enum cell { SHARK, FISH, WATER } cell_t;


/** Tipo matrice acquatica che rappreseneta il pianeta */
typedef struct planet {
  /** righe */
  unsigned int nrow; 
  /** colonne */
  unsigned int ncol; 
  /** matrice pianeta */
  cell_t ** w;
  /** matrice contatori nascita (pesci e squali)*/
  int ** btime;
  /** matrice contatori morte (squali )*/
  int ** dtime;

} planet_t;

/** struttura che raccoglie le informazioni di simulazione */
typedef struct wator {
  /** sd numero chronon morte squali per digiuno */
  int sd; 
  /** sb numero chronon riproduzione squali */
  int sb; 
  /** fb numero chronon riproduzione pesci */
  int fb; 
  /** nf numero pesci*/
  int nf; 
  /** ns numero squali */
  int ns; 
  /** numero worker */
  int nwork;
  /** durata simulazione */
  int chronon;
  /** pianeta acquatico */
  planet_t* plan;
} wator_t;

/** trasforma una cella in un carattere
   \param a cella da trasformare

   \retval 'W' se a contiene WATER
   \retval 'S' se a contiene SHARK
   \retval 'F' se a contiene FISH
   \retval '?' in tutti gli altri casi
 */
char cell_to_char (cell_t a) ;

/** trasforma un carattere in una cella
   \param c carattere da trasformare

   \retval WATER se c=='W'
   \retval SHARK se c=='S'
   \retval FISH se c=='F'
   \retval -1 in tutti gli altri casi
 */
int char_to_cell (char c) ;

/** crea un nuovo pianeta vuoto (tutte le celle contengono WATER) utilizzando 
    la rappresentazione con un vettore di puntatori a righe
    \param nrow numero righe
    \param numero colonne

    \retval NULL se si sono verificati problemi nell'allocazione
    \retval p puntatore alla matrice allocata altrimenti
*/
planet_t * new_planet (unsigned int nrow, unsigned int ncol);

/** dealloca un pianeta (e tutta la matrice ...)
    \param p pianeta da deallocare

*/
void free_planet (planet_t* p);

/** stampa il pianeta su file secondo il formato di fig 2 delle specifiche, es

3
5
W F S W W
F S W W S
W W W W W

dove 3 e' il numero di righe (seguito da newline \n)
5 e' il numero di colonne (seguito da newline \n)
e i caratteri W/F/S indicano il contenuto (WATER/FISH/SHARK) separati da un carattere blank (' '). Ogni riga terminata da newline \n

    \param f file su cui stampare il pianeta (viene sovrascritto se esiste)
    \param p puntatore al pianeta da stampare

    \retval 0 se tutto e' andato bene
    \retval -1 se si e' verificato un errore (in questo caso errno e' settata opportunamente)

*/
int print_planet (FILE* f, planet_t* p);


/** inizializza il pianeta leggendo da file la configurazione iniziale

    \param f file da dove caricare il pianeta (deve essere gia' stato aperto in lettura)

    \retval p puntatore al nuovo pianeta (allocato dentro la funzione)
    \retval NULL se si e' verificato un errore (setta errno) 
            errno = ERANGE se il file e' mal formattato
 */
planet_t* load_planet (FILE* f);



/** crea una nuova istanza della simulazione in base ai contenuti 
    del file di configurazione "wator.conf"

    \param fileplan nome del file da cui caricare il pianeta

    \retval p puntatore alla nuova struttura che descrive 
              i parametri di simulazione
    \retval NULL se si e' verificato un errore (setta errno)
*/
wator_t* new_wator (char* fileplan);

/** libera la memoria della struttura wator (e di tutte le sottostrutture)
    \param pw puntatore struttura da deallocare

 */
void free_wator(wator_t* pw);

#define STOP 0
#define EAT  1
#define MOVE 2
#define ALIVE 3
#define DEAD 4
/** Regola 1: gli squali mangiano e si spostano 
  \param pw puntatore alla struttura di simulazione
  \param (x,y) coordinate iniziali dello squalo
  \param (*k,*l) coordinate finali dello squalo (modificate in uscita)

  La funzione controlla i 4 vicini 
              (x-1,y)
        (x,y-1) *** (x,y+1)
              (x+1,y)

  Se una di queste celle contiene un pesce, lo squalo mangia il pesce e 
  si sposta nella cella precedentemente occupata dal pesce. Se nessuna 
  delle celle adiacenti contiene un pesce, lo squalo si sposta
  in una delle celle adiacenti vuote. Se ci sono piu' celle vuote o piu' pesci 
  la scelta e' casuale.
  Se tutte le celle adiacenti sono occupate da squali 
  non possiamo ne mangiare ne spostarci lo squalo rimane fermo.

  NOTA: la situazione del pianeta viene 
        modificata dalla funzione con il nuovo stato

  \retval STOP se siamo rimasti fermi
  \retval EAT se lo squalo ha mangiato il pesce
  \retval MOVE se lo squalo si e' spostato solamente
  \retval -1 se si e' verificato un errore (setta errno)
*/
int shark_rule1 (wator_t* pw, int x, int y, int *k, int* l);

/** Regola 2: gli squali si riproducono e muoiono
  \param pw puntatore alla struttura wator
  \param (x,y) coordinate dello squalo
  \param (*k,*l) coordinate dell'eventuale squalo figlio (

  La funzione calcola nascite e morti in base agli indicatori 
  btime(x,y) e dtime(x,y).

  == btime : nascite ===
  Se btime(x,y) e' minore di  pw->sb viene incrementato.
  Se btime(x,y) e' uguale a pw->sb si tenta di generare un nuovo squalo.
  Si considerano i 4 vicini 
              (x-1,y)
        (x,y-1) *** (x,y+1)
              (x+1,y)

  Se una di queste celle e' vuota lo squalo figlio viene generato e la occupa, se le celle sono tutte occupate da pesci o squali la generazione non avviene. 
  In entrambi i casi btime(x,y) viene azzerato.

  == dtime : morte dello squalo  ===
  Se dtime(x,y) e' minore di pw->sd viene incrementato.
  Se dtime(x,y) e' uguale a pw->sd lo squalo muore e la sua posizione viene 
  occupata da acqua.

  NOTA: la situazione del pianeta viene 
        modificata dalla funzione con il nuovo stato


  \retval DIED se lo squalo e' morto
  \retval ALIVE se lo squalo e' vivo
  \retval -1 se si e' verificato un errore (setta errno)
*/
int shark_rule2 (wator_t* pw, int x, int y, int *k, int* l);

/** Regola 3: i pesci si spostano

    \param pw puntatore alla struttura di simulazione
    \param (x,y) coordinate iniziali del pesce
    \param (*k,*l) coordinate finali del pesce

    La funzione controlla i 4 vicini 
              (x-1,y)
        (x,y-1) *** (x,y+1)
              (x+1,y)

     un pesce si sposta casualmente in una delle celle adiacenti (libera). 
     Se ci sono piu' celle vuote la scelta e' casuale. 
     Se tutte le celle adiacenti sono occupate rimaniamo fermi.

     NOTA: la situazione del pianeta viene 
        modificata dalla funzione con il nuovo stato

  \retval STOP se siamo rimasti fermi
  \retval MOVE se il pesce si e' spostato
  \retval -1 se si e' verificato un errore (setta errno)
*/
int fish_rule3 (wator_t* pw, int x, int y, int *k, int* l);

/** Regola 4: i pesci si riproducono
  \param pw puntatore alla struttura wator
  \param (x,y) coordinate del pesce
  \param (*k,*l) coordinate dell'eventuale pesce figlio 

  La funzione calcola nascite in base a btime(x,y) 

  Se btime(x,y) e' minore di  pw->sb viene incrementato.
  Se btime(x,y) e' uguale a pw->sb si tenta di generare un nuovo pesce.
  Si considerano i 4 vicini 
              (x-1,y)
        (x,y-1) *** (x,y+1)
              (x+1,y)

  Se una di queste celle e' vuota il pesce figlio viene generato e la occupa, se le celle sono tutte occupate da pesci o squali la generazione non avviene. 
  In entrambi i casi btime(x,y) viene azzerato.

  NOTA: la situazione del pianeta viene 
        modificata dalla funzione con il nuovo stato


  \retval  0 se tutto e' andato bene
  \retval -1 se si e' verificato un errore (setta errno)
*/
int fish_rule4 (wator_t* pw, int x, int y, int *k, int* l);


/** restituisce il numero di pesci nel pianeta
    \param p puntatore al pianeta
    
    \retval n (>=0) numero di pesci presenti
    \retval -1 se si e' verificato un errore (setta errno )
 */
int fish_count (planet_t* p);

/** restituisce il numero di squali nel pianeta
    \param p puntatore al pianeta
    
    \retval n (>=0) numero di squali presenti
    \retval -1 se si e' verificato un errore (setta errno )
 */
int shark_count (planet_t* p);


/** calcola un chronon aggiornando tutti i valori della simulazione e il pianeta
   \param pw puntatore al pianeta

   \return 0 se tutto e' andato bene
   \return -1 se si e' verificato un errore (setta errno)

 */
int update_wator (wator_t * pw);

#endif
