/** \file wator.c
       \author Nicolò Tonci
     Si dichiara che il contenuto di questo file e' in ogni sua parte opera
     originale dell' autore.  */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "wator.h"

char cell_to_char (cell_t a){
	switch (a){
		case WATER :
			return 'W';
		case SHARK :
			return 'S';
		case FISH :
			return 'F';
		default :
			return '?';
	}
}

int char_to_cell (char c){
	switch(c){
		case 'W' :
			return WATER;
		case 'S' :
			return SHARK;
		case 'F' :
			return FISH;
		default :
		 return -1;
	}
}

planet_t * new_planet (unsigned int nrow, unsigned int ncol){
	int i = 0, j = 0;
	cell_t ** world = NULL;
	int ** bcount = NULL;
	int ** dcount = NULL;
	planet_t * planet = NULL;
	
	/* controllo se i valori in input sono accettabili */
	if(nrow == 0 || ncol == 0){
		errno = EINVAL;
		return NULL;
	}
	
	/* creo la matrice del mondo */
	world = (cell_t **) malloc(nrow * sizeof(cell_t *));
	for(i=0;i<nrow;i++)
		world[i] = (cell_t *) malloc(ncol * sizeof(cell_t));
		
	/* creo la matrice del contatore delle nascite */
	bcount = (int **) malloc(nrow * sizeof(int*));
	for(i=0;i<nrow;i++)
		bcount[i] = (int *) malloc(ncol * sizeof(int));
		
	/* creo la matrice del contatore delle morti */
	dcount = (int **) malloc(nrow * sizeof(int*));
	for(i=0;i<nrow;i++)
		dcount[i] = (int *) malloc(ncol * sizeof(int));
	
	
	/* inizializzo le matrici come richiesto nella specifica */
	for(i=0;i<nrow;i++)
		for(j=0;j<ncol;j++){
			world[i][j] = WATER;
			bcount[i][j] = 0;
			dcount[i][j] = 0;
		}
	
	/* alloco la struttura del pianeta e collego le sottostrutture allocate in precedenza */
	planet = (planet_t *) malloc(sizeof(planet_t));
	planet->w = world;
	planet->nrow = nrow;
	planet->ncol = ncol;
	planet->btime = bcount;
	planet->dtime = dcount;
	
	return planet;
}

void free_planet (planet_t* p){
	int i = 0;
	/* dealloco tutte le righe delle matrici */
	for(i=0;i<p->nrow;i++){
		free((p->w)[i]);
		free((p->btime)[i]);
		free((p->dtime)[i]);
	}
	/* dealloco l'array di puntatori alle righe */
	free(p->w);
	free(p->btime);
	free(p->dtime);
	/* dealloco il pianeta p */
	free(p);
}

int print_planet (FILE* f, planet_t* p){
	int i, j;
	/* stampo le dimensioni della matrice e nel contempo controllo che il puntatore al file sia corretto, se la stampa è andata a buon fine, altrimenti ritorno -1 */
	if ((fprintf(f,"%d\n%d\n", p->nrow, p->ncol)) < 0) 
		return -1;
	/* inizio a stampare la matrice */
	for(i=0;i<(p->nrow);i++){
		/* la prima stampa la faccio senza spazio prima, infatti j poi parte da 1 */
		fprintf(f,"%c", cell_to_char(p->w[i][0]));
		for(j=1;j<(p->ncol);j++)
			fprintf(f," %c", cell_to_char(p->w[i][j]));
		/* terminata ogni riga, stampo il ritorno carrello */
		fprintf(f,"\n");
	}
	return 0;
}

planet_t* load_planet (FILE* f){
	int nrow, ncol;
	int i = 0, j = 0;
	char tmp;
	planet_t * p = NULL;
	/* leggo le dimensione della matrice */
	fscanf(f,"%d\n", &nrow);
	fscanf(f,"%d\n", &ncol);
	/* chiamo la funzione di allocazione del pianeta con i parametri appena acquisiti (numero righe, numero colonne) */
	p = new_planet(nrow,ncol);
	/* comincio a leggere la configurazione iniziale del pianeta e settare l'opportuna matrice world */
	for(i=0;i<nrow;i++)
		for(j=0;j<ncol;j++){
			fscanf(f,"%c ", &tmp);
			p->w[i][j] = char_to_cell(tmp);
		}
	return p;
}

wator_t* new_wator (char* fileplan){
	FILE * f = NULL;
	FILE * conf = NULL;
	wator_t * wator = NULL;
	/* apro il file di configurazione iniziale passatomi per paramentro */
	f = fopen(fileplan, "r");
	/* apro il file di conf */
	conf = fopen("wator.conf","r");
	/* alloco la struttura del mondo acquatico */
	wator = (wator_t *) malloc(sizeof(wator_t));
	/* leggo i tre parametri del file wator.conf e li inserisco direttamente nella struttura */
	fscanf(conf,"sd %d\n", &(wator->sd));
	fscanf(conf,"sb %d\n", &(wator->sb));
	fscanf(conf,"fb %d\n", &(wator->fb));
	/* chiamo la funzione che mi inizializza il pianeta partendo da una configurazione su file opportunamente passato come parametro */
	wator->plan = load_planet(f);
	wator->nf = 0;
	wator->ns = 0;
	/* chiudo i file aperti precedentemente */
	fclose(conf);
	fclose(f);
	return wator;
}


void free_wator(wator_t* pw){
	/* dealloco il pianeta */
	free_planet(pw->plan);
	/* dealloco la struttura di simulazione  */
	free(pw);
}

int shark_rule1 (wator_t* pw, int x, int y, int *k, int* l){
	cell_t ** world = pw->plan->w;
	int nrow , ncol;
	nrow = pw->plan->nrow;
	ncol = pw->plan->ncol;
	/* Controllo se lo squalo può mangiare partendo da direzione est e procedendo in senso orario */
	/* Guardo a destra */
	if (world[x][(y+1) % ncol] == FISH){
		*k = x;
		*l = ((y+1) % ncol);
		world[x][y] = WATER;
		world[*k][*l] = SHARK;
		return EAT;
	}
	/* Guardo sopra */
	if (world[(x-1+nrow) % nrow][y] == FISH){
		*k = ((x-1+nrow) % nrow);
		*l = y;
		world[x][y] = WATER;
		world[*k][*l] = SHARK;
		return EAT;
	}
	/* Guardo a sinistra */
	if (world[x][(y-1+ncol) % ncol] == FISH){
		*k = x;
		*l = ((y-1+ncol) % ncol);
		world[x][y] = WATER;
		world[*k][*l] = SHARK;
		return EAT;
	}
	/* Guardo sotto */
	if (world[(x+1) % nrow][y] == FISH){
		*k = ((x+1) % nrow);
		*l = y;
		world[x][y] = WATER;
		world[*k][*l] = SHARK;
		return EAT;
	}
	/* A questo punto non avendo mangiato vedo se può spostarsi con lo stesso criterio con cui controlla se può mangiare */
	/* Guardo a destra */
	if (world[x][(y+1) % ncol] == WATER){
		*k = x;
		*l = ((y+1) % ncol);
		world[x][y] = WATER;
		world[*k][*l] = SHARK;
		return MOVE;
	}
	/* Guardo sopra */
	if (world[(x-1+nrow) % nrow][y] == WATER){
		*k = ((x-1+nrow) % nrow);
		*l = y;
		world[x][y] = WATER;
		world[*k][*l] = SHARK;
		return MOVE;
	}
	/* Guardo a sinistra */
	if (world[x][(y-1+ncol) % ncol] == WATER){
		*k = x;
		*l = ((y-1+ncol) % ncol);
		world[x][y] = WATER;
		world[*k][*l] = SHARK;
		return MOVE;
	}
	/* Guardo sotto */
	if (world[(x+1) % nrow][y] == WATER){
		*k = ((x+1) % nrow);
		*l = y;
		world[x][y] = WATER;
		world[*k][*l] = SHARK;
		return MOVE;
	}
	/* In questo caso è rimasto fermo */ 
	*k = x;
	*l = y;
	return STOP;
}

int shark_rule2 (wator_t* pw, int x, int y, int *k, int* l){
	int ** btime = pw->plan->btime;
	int ** dtime = pw->plan->dtime;
	cell_t ** world = pw->plan->w;
	int nrow , ncol;
	nrow = pw->plan->nrow;
	ncol = pw->plan->ncol;
	/* se btime(x,y) è minore di pw->sb incremento btime(x,y) */
	if (btime[x][y] < (pw->sb))
		btime[x][y]++;
	/* se btime(x,y) è uguale a pw->sb controllo partendo da direzione est in senso orario se vi è una cella libera (WATER) in cui generare un nuovo squalo */
	else if (btime[x][y] == (pw->sb)){
		/* Guardo a destra */
		if (world[x][(y+1) % ncol] == WATER){
			*k = x;
			*l = ((y+1) % ncol);
			world[*k][*l] = SHARK;
		} else
		/* Guardo sopra */
		if (world[(x-1+nrow) % nrow][y] == WATER){
			*k = ((x-1+nrow) % nrow);
			*l = y;
			world[*k][*l] = SHARK;
		} else
		/* Guardo a sinistra */
		if (world[x][(y-1+ncol) % ncol] == WATER){
			*k = x;
			*l = ((y-1+ncol) % ncol);
			world[*k][*l] = SHARK;
		} else
		/* Guardo sotto */
		if (world[(x+1) % nrow][y] == WATER){
			*k = ((x+1) % nrow);
			*l = y;
			world[*k][*l] = SHARK;
		}
		/* in ogni caso btime(x,y) viene azzerato se uguale a pw->sb */
		btime[x][y] = 0;
	}
	/* Trattazione della morte dello squalo	*/
	/* Se dtime(x,y) è minore di pw->sd dtime(x,y) viene incrementato */
	if (dtime[x][y] < (pw->sd))
		dtime[x][y]++;
	/* Se dtime(x,y) è uguale a pw->sd lo squalo muore */
	else if (dtime[x][y] == (pw->sd)){
		/* la cella attuale viene cosi occupata da acqua */
		world[x][y] = WATER;
		return DEAD;
	}
	return ALIVE;
}

int fish_rule3 (wator_t* pw, int x, int y, int *k, int* l){
	cell_t ** world = pw->plan->w;
	int nrow , ncol;
	nrow = pw->plan->nrow;
	ncol = pw->plan->ncol;
	/* Controllo se il pesce può spostarsi, partendo da direzione est e procedendo in sesno orario */
	if (world[x][(y+1) % ncol] == WATER){
			*k = x;
			*l = ((y+1) % ncol);
			world[x][y] = WATER;
			world[*k][*l] = FISH;
			return MOVE;
	}
	/** Guardo sopra */
	if (world[(x-1+nrow) % nrow][y] == WATER){
		*k = ((x-1+nrow) % nrow);
		*l = y;
		world[x][y] = WATER;
		world[*k][*l] = FISH;
		return MOVE;
	}
	/** Guardo a sinistra */
	if (world[x][(y-1+ncol) % ncol] == WATER){
		*k = x;
		*l = ((y-1+ncol) % ncol);
		world[x][y] = WATER;
		world[*k][*l] = FISH;
		return MOVE;
	}
	/** Guardo sotto */
	if (world[(x+1) % nrow][y] == WATER){
		*k = ((x+1) % nrow);
		*l = y;
		world[x][y] = WATER;
		world[*k][*l] = FISH;
		return MOVE;
	}
	/* In questo caso il pesce è rimasto fermo */
	*k = x;
	*l = y;
	return STOP;
}

int fish_rule4 (wator_t* pw, int x, int y, int *k, int* l){
	int ** btime = pw->plan->btime;
	cell_t ** world = pw->plan->w;
	int nrow , ncol;
	nrow = pw->plan->nrow;
	ncol = pw->plan->ncol;
	/* se btime(x,y) è minore di pw->sb btime(x,y) viene incrementato */
	if (btime[x][y] < (pw->sb))
		btime[x][y]++;
	/* se btime(x,y) è uguale a pw->sb controllo partendo in direzione est in senso orario se vi è una cella libera (WATER) in cui generare un nuovo pesce */
	else if (btime[x][y] == (pw->sb)) {
		if (world[x][(y+1) % ncol] == WATER){
			*k = x;
			*l = ((y+1) % ncol);
			world[*k][*l] = FISH;
		} else
		/** Guardo sopra */
		if (world[(x-1+nrow) % nrow][y] == WATER){
			*k = ((x-1+nrow) % nrow);
			*l = y;
			world[*k][*l] = FISH;
		} else
		/** Guardo a sinistra */
		if (world[x][(y-1+ncol) % ncol] == WATER){
			*k = x;
			*l = ((y-1+ncol) % ncol);
			world[*k][*l] = FISH;
		} else
		/** Guardo sotto */
		if (world[(x+1) % nrow][y] == WATER){
			*k = ((x+1) % nrow);
			*l = y;
			world[*k][*l] = FISH;
		}
		/* in ogni caso btime(x,y) viene azzerato se uguale a pw->sb */
		btime[x][y] = 0;
	}
	return 0;
}

int fish_count (planet_t* p){
	int i, j, fish = 0;
	/* scorro la matrice e incremento la variabile fish solo in presenza di un pesce */
	for(i=0;i<p->nrow;i++)
		for(j=0;j<p->ncol;j++)
			if(p->w[i][j] == FISH)
				fish++;
	return fish;
}

int shark_count (planet_t* p){
	int i, j, shark = 0;
	/* scorro la matrice e incremento la variabile shark solo in presenza di uno squalo */
	for(i=0;i<p->nrow;i++)
		for(j=0;j<p->ncol;j++)
			if(p->w[i][j] == SHARK)
				shark++;
	return shark;
}

int update_wator (wator_t * pw){
	cell_t ** world = pw->plan->w;
	int i ,j, k, l;
	/* inizio a scorrere la matrice */
	for(i=0;i<pw->plan->nrow;i++)
		for(j=0;j<pw->plan->ncol;j++){
			/* se nella cella corrente vi è uno squalo applico le regole riferite agli squali */
			if (world[i][j] == SHARK)
				switch(shark_rule2(pw,i,j,&k,&l)){
					case -1: return -1;
					case ALIVE: 
						if (shark_rule1(pw,i,j,&k,&l) == -1) return -1;
				}
			/* se nella cella corrente vi è un pesce applico le regole riferite ai pesci */
			if (world[i][j] == FISH){
				if (fish_rule4(pw,i,j,&k,&l) == -1) return -1;
				if (fish_rule3(pw,i,j,&k,&l) == -1) return -1;
			}
		}
	return 0;
}
