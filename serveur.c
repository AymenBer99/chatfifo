/* version 0.2 (PM, 13/5/19) :
	Le serveur de conversation
	- crée un tube (fifo) d'écoute (avec un nom fixe : ./ecoute)
	- gère un maximum de maxParticipants conversations : select
		* tube d'écoute : accepter le(s) nouveau(x) participant(s) si possible
			-> initialiser et ouvrir les tubes de service (entrée/sortie) fournis
		* tubes (fifo) de service en entrée -> diffuser sur les tubes de service en sortie
	- détecte les déconnexions lors du select
	- se termine à la connexion d'un client de pseudo "fin"
	Protocole
	- suppose que les clients ont créé les tube d'entrée/sortie avant
		la demande de connexion, nommés par le nom du client, suffixés par _C2S/_S2C.
	- les échanges par les tubes se font par blocs de taille fixe, dans l'idée d'éviter
	  le mode non bloquant
*/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include <stdbool.h>
#include <sys/stat.h>

#define MAXPARTICIPANTS 5			/* seuil au delà duquel la prise en compte de nouvelles
								 						 	   connexions sera différée */
#define TAILLE_MSG 128				/* nb caractères message complet (nom+texte) */
#define TAILLE_NOM 25					/* nombre de caractères d'un pseudo */
#define NBDESC FD_SETSIZE-1		/* pour le select (macros non definies si >= FD_SETSIZE) */
#define TAILLE_RECEPTION 512	/* capacité du tampon de messages reçus */

typedef struct ptp { 					/* descripteur de participant */
    bool actif;
    char nom [TAILLE_NOM];
    int in;		/* tube d'entrée (C2S) */
    int out;	/* tube de sortie (S2C) */
} participant;


participant participants [MAXPARTICIPANTS];

char buf[TAILLE_RECEPTION]; 	/* tampon de messages reçus/à rediffuser */
int nbactifs = 0;

void effacer(int i) { /* efface le descripteur pour le participant i */
    participants[i].actif = false;
    bzero(participants[i].nom, TAILLE_NOM*sizeof(char));
    participants[i].in = -1;
    participants[i].out = -1;
}

void diffuser(char *dep) { /* envoi du message référencé par dep à tous les actifs */
/* à faire */
	int err;
	for(int i=0; i<nbactifs; i++) {
		err = write(participants[i].out, dep, TAILLE_MSG);
		if(err == -1) {
			perror("error diffuser");
		}
	}
}

void desactiver (int p) {
	int decalage = 0;
	char tubeC2S [TAILLE_NOM+5];
	char tubeS2C [TAILLE_NOM+5];
	
	close(participants[p].in);
	close(participants[p].out);
	
	strcpy(tubeC2S, participants[p].nom);
	strcat(tubeC2S,"_C2S");
	strcpy(tubeS2C, participants[p].nom);
	strcat(tubeS2C, "_S2C");
	/* traitement d'un participant déconnecté (à faire) */
	close(participants[p].in);
	close(participants[p].out);
	for (int i=0; i<nbactifs-1; i++) {
		if(i==p && p != nbactifs) {
			decalage = 1;
		}
		participants[i].actif = participants[i+decalage].actif;
		strcpy(participants[i].nom, participants[i+decalage].nom);
		participants[i].in = participants[i+decalage].in;
		participants[i].out = participants[i+decalage].out;
	}
	effacer(nbactifs-1);
	nbactifs--; 
	remove(tubeS2C);
	remove(tubeC2S);
}

void ajouter(char *dep) { // traite la demande de connexion du pseudo référencé par dep
	/*  Si le participant est "fin", termine le serveur (et gère la terminaison proprement)
	Sinon, ajoute le participant de pseudo référencé par dep
	(à faire)
	*/
	char tubeS2C[TAILLE_NOM+5];
	char tubeC2S[TAILLE_NOM+5]; 
	//char rejoindre[TAILLE_MSG];
	if(strcmp(dep, "fin") == 0) {
		for(int i=0; i<=nbactifs; i++) {
			desactiver(i);
		}
		exit(1);
	} else {
		nbactifs++;
		//strcpy(tubeS2C, "./");
		//strcpy(tubeC2S, "./");
		strcpy(tubeS2C, dep);
		strcpy(tubeC2S, dep);
		strcat(tubeS2C, "_S2C");
		strcat(tubeC2S, "_C2S");
		
		participants[nbactifs-1].actif = true;
		strcpy(participants[nbactifs-1].nom, dep);
		participants[nbactifs-1].in = open(tubeC2S, O_RDONLY|O_NONBLOCK);
		participants[nbactifs-1].out = open(tubeS2C, O_WRONLY);
		
		//Message si client rejoint
		//strcpy(rejoindre, "[service] ");
		//strcat(rejoindre, dep);
		//strcat(rejoindre, " rejoint la conversation\n");
		//diffuser(rejoindre);
	}
}
	
int main (int argc, char *argv[]) {
	int i,nlus,necrits,res;
	int ecoute;					/* descripteur d'écoute */
	fd_set readfds; 		/* ensemble de descripteurs écoutés par le select */
	char * buf0; 				/* pour parcourir le contenu du tampon de réception */
	char bufDemandes [TAILLE_NOM*sizeof(char)*MAXPARTICIPANTS]; 
	/* tampon requêtes de connexion. Inutile de lire plus de MAXPARTICIPANTS requêtes */
	char * message;
	
	/* création (puis ouverture) du tube d'écoute */
	mkfifo("./ecoute",S_IRUSR|S_IWUSR); // mmnémoniques sys/stat.h: S_IRUSR|S_IWUSR = 0600
	ecoute=open("./ecoute",O_RDONLY);

	for (i=0; i<= MAXPARTICIPANTS; i++) {
		effacer(i);
	}
		
	while (true) {
		printf("participants actifs : %d\n",nbactifs);

		/* boucle du serveur : traiter les requêtes en attente 
				 * sur le tube d'écoute : lorsqu'il y a moins de MAXPARTICIPANTS actifs.
				 	ajouter de nouveaux participants et les tubes d'entrée.			  
				 * sur les tubes de service : lire les messages sur les tubes c2s, et les diffuser.
				   Note : tous les messages comportent TAILLE_MSG caractères, et les constantes
           			sont fixées pour qu'il n'y ait pas de message tronqué, ce qui serait  pénible 
           			à gérer. Enfin, on ne traite pas plus de TAILLE_RECEPTION/TAILLE_MSG*sizeof(char)
           			à chaque fois.
           		- dans le cas où la terminaison d'un participant est détectée, gérer sa déconnexion
			
			(à faire)
		*/
		FD_ZERO(&readfds);
		FD_SET(ecoute, &readfds);
		for(int j=0; j<nbactifs; j++) {
			FD_SET(participants[j].in, &readfds);
		}
		res = select(NBDESC, &readfds, NULL, NULL, NULL);
		if(res == -1) {
			perror("Select");
			exit(1);
		}
		if(res > 0) {
			//Si on recoit dans le tube ecoute
			if(FD_ISSET(ecoute, &readfds) != 0) {
				buf0 = malloc(TAILLE_RECEPTION);
				message = malloc(TAILLE_RECEPTION);
				bzero(buf0, TAILLE_RECEPTION);
				nlus = read(ecoute, buf0, TAILLE_RECEPTION);
				if(nlus == -1) {
					perror("read");
					exit(1);
				}
				if(strcmp(buf0,"fin") != 0) {
					ajouter(buf0);
				}
				strcpy(message, "[service] ");
				strcat(message, buf0);
				strcat(message, " rejoint la discussion.\n");
				diffuser(message);
				if(strcmp(buf0,"fin") == 0) {
					for (i=0; i< nbactifs; i++) {
						desactiver(i);
					}
					if (close(ecoute) == -1) {
						perror("close ecoute");
					}
					remove("ecoute");
				}
				free(buf0);
				free(message);
				message = NULL;
				buf0 = NULL;
			}
			//Si on recoit dans les tubes C2S
			for (int j = 0; j < nbactifs; j++) {
				if (FD_ISSET(participants[j].in, &readfds) != 0) {
					buf0 = malloc(TAILLE_RECEPTION);
					message = malloc(TAILLE_RECEPTION);
					bzero(buf0, TAILLE_RECEPTION);
					nlus = read(participants[j].in, buf0, TAILLE_RECEPTION);
					if(nlus == -1) {
						perror("error read");
						exit(1);
					}
					strcpy(message, "[");
					strcat(message, participants[j].nom);
					strcat(message, "] ");
					strcat(message, "au revoir\n");
					if(strcmp(buf0, message) == 0) {
						desactiver(j);
					}
					diffuser(buf0);
					free(buf0);
					free(message);
					message = NULL;
					buf0 = NULL;
				}
			}
		}
	}
}
