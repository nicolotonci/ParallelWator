#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "wator.h"
#include "visualizer.h"


int main(int argc, char *argv[]){
	/* descrittore dell'output */
	FILE *f;
	
	int sck; /* socket */
	int f_sck; /* file descriptor del socket */
	struct sockaddr_un saddr; /* socket address struct */
	
	int nrow, ncol; /* numero di righe e numero di colonne */
	int i,j; /* contatori di stampa */
	int readed; /* contatore di byte ricevuti sul socket ad ogni connessione */
	char * buff = NULL; /* puntatore al buffer della matrice */
	unsigned char flag = 0;
	
	if (argc != 2 || ((f= fopen(argv[1],"w")) == NULL))
		f = stdout;
	
	/*creazione del socket */
	unlink(SOCKNAME);
	strncpy(saddr.sun_path, SOCKNAME, UNIX_PATH_MAX);
	saddr.sun_family = AF_UNIX;
	if ((sck = socket(AF_UNIX,SOCK_STREAM,0)) < 0){
		perror("Error opening socket");
		return 1;
	}
	/* bind del server */
	if (bind(sck,(struct sockaddr *) &saddr, sizeof(saddr)) < 0){
		perror("Error on binding");
		return 1;
	}
	listen(sck,MAX_CONN);
	
	while (1) {
		/* mi blocco per accettare una connessione */
		/* quando istauro la connessione f_sck sarà il suo file descriptor */
		f_sck = accept(sck,NULL,0);
		/* lettura secondo il protocollo */
		read(f_sck,&nrow,4); /* 4 byte is the size of an int */
		read(f_sck,&ncol,4);
		buff = (char *) malloc(nrow*ncol); /* alloco memoria per il buffer: nrow*ncol bytes*/
		
		readed = 0;
		/* leggo e inserisco nel buffer esattamente nrow * ncol bytes */
		while(readed < nrow*ncol){
		readed += read(f_sck, buff+readed, (nrow*ncol - readed));
		}
		/* leggo il flag (di chiusura) */
		read(f_sck, &flag, 1); /* sizeof(unsigned char) = 1 */
		
		fprintf(f,"%d\n%d\n", nrow, ncol); /* stampo le prime due righe: nrow \n ncol \n */
		/* stampo la matrice ben formattata */
		for(i=0;i<nrow;i++){
			fprintf(f,"%c", buff[i*ncol]);
			for(j=1;j<ncol;j++)
				fprintf(f," %c", buff[i*ncol+j]);
			fprintf(f,"\n");
		}
		/* svuoto il buffer e chiudo il file descriptor della connesione */
		free(buff);
		close(f_sck);

		if (flag == 1) /* se il flag == 1 termino il processo */
			break;
	}
	
	/* terminazione processo */
	close(sck);
	fclose(f);
	unlink(SOCKNAME);
	return 0;
}
