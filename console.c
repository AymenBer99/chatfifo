/* version 0.1 (PM, 12/5/19) :
	Le client de conversation
	- crée deux tubes (fifo) d'E/S, nommés par le nom du client, suffixés par _C2S/_S2C
	- demande sa connexion via le tube d'écoute du serveur (nom supposé connu),
		 en fournissant le pseudo choisi (max TAILLE_NOM caractères)
	- attend la réponse du serveur sur son tube _C2S
	- effectue une boucle : select sur clavier et S2C.
	- sort de la boucle si la saisie au clavier est "au revoir"
	Protocole
	- les échanges par les tubes se font par blocs de taille fixe TAILLE_MSG,
	- le texte émis via C2S est préfixé par "[pseudo] ", et tronqué à TAILLE_MSG caractères
Notes :
	-le  client de pseudo "fin" n'entre pas dans la boucle : il permet juste d'arrêter
		proprement la conversation.
*/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

#define TAILLE_MSG 128				/* nb caractères message complet ([nom]+texte) */
#define TAILLE_NOM 25					/* nombre de caractères d'un pseudo */
#define NBDESC FD_SETSIZE-1		/* pour le select (macros non definies si >= FD_SETSIZE) */
#define TAILLE_RECEPTION 512	/* capacité du tampon de messages reçus */
#define NB_LIGNES 20
#define TAILLE_SAISIE 512		/* capacité du tampon pour les lignes saisies 
					 (hypothèse : pas de caractère de plus de 4 octets) */

char discussion [NB_LIGNES] [TAILLE_MSG]; /* derniers messages reçus */

void afficher(int depart) { 
/* affiche les lignes de discussion[] en commençant par la plus ancienne (départ+1 % nbln)
  et en finissant par la plus récente (départ) */
	int i;
	system("clear");  /* nettoyage de l'affichage simple, à défaut d'être efficace (1) */
	printf("==============================(discussion)==============================\n");
	for (i=1; i<=NB_LIGNES; i++) {
		printf("%s\n", discussion[(depart+i)%NB_LIGNES]);
	}
	printf("------------------------------------------------------------------------\n");
}

int main (int argc, char *argv[]) {
	int i,nlus,necrits, n_modifiees;
	int finProgramme = 0;
	char * buf0;			/* pour parcourir le contenu reçu d'un tube */

	int ecoute, S2C, C2S;		/* descripteurs tubes de service*/
	int curseur = 0;		/* position (affichage) dernière ligne reçue et affichée */

	fd_set readfds;		/* ensemble de descripteurs écoutés par le select */

	char tubeC2S [TAILLE_NOM+5]; /* chemin tube de service client -> serveur = pseudo_c2s */
	char tubeS2C [TAILLE_NOM+5]; /* chemin tube de service serveur -> client = pseudo_s2c */
	char pseudo [TAILLE_NOM];
	char message [TAILLE_MSG];
	char saisie [TAILLE_SAISIE];	/* tampon recevant la ligne saisie au clavier */
	char buf [TAILLE_RECEPTION];	/* tampon recevant les messages du tube s2c */

	if (!((argc == 2) && (strlen(argv[1]) < TAILLE_NOM*sizeof(char)))) {
		printf("utilisation : %s <pseudo>\n", argv[0]);
		printf("Le pseudo ne doit pas dépasser 25 caractères\n");
		exit(1);
	}

	/* ouverture du tube d'écoute */
	ecoute = open("./ecoute",O_WRONLY);
	if (ecoute==-1) {
		printf("Le serveur doit être lance, et depuis le meme repertoire que le client\n");
		exit(2);
	}
	/* création des tubes de service (à faire) */
	strcpy(pseudo, argv[1]);
	strcpy(tubeC2S, pseudo);
	strcat(tubeC2S,"_C2S");
	strcpy(tubeS2C, pseudo);
	strcat(tubeS2C, "_S2C");
	if(mkfifo(tubeC2S,S_IRUSR|S_IWUSR) == -1) {
		perror("fifo c2s");
		exit(1);
	}
	if(mkfifo(tubeS2C,S_IRUSR|S_IWUSR) == -1) {
		perror("fifo s2c");
		exit(1);
	}
	/* connexion (à faire) */
	//On envoie le pseudo au serveur
	if((write(ecoute, pseudo, TAILLE_NOM)) == -1) {
		printf("Erreur connexion client");
	}

	if (strcmp(pseudo,"fin") == 0) { /* "console fin" provoque la terminaison du serveur */
		printf("fermeture du serveur et ");
	} else {
	
		/* client "normal" */
		/* initialisations (à faire) */
		/* ouverture des tubes de service seulement ici  car il faut que la connexion 
		au serveur ait eu lieu pour qu'il puisse effectuer l'ouverture des tubes de service
		de son côté, et ainsi permettre au client d'ouvrir les tubes sans être bloqué */
		S2C = open(tubeS2C, O_RDONLY|O_NONBLOCK);
		if(S2C == -1) {
			perror("S2C open");
		}
		C2S = open(tubeC2S, O_WRONLY);
		if(C2S == -1) {
			perror("C2S open");
		}
		
		//Boucle si saisie != au revoir ou si nouveau utilisateur != fin
		while (strcmp(saisie,"au revoir\n")!=0 && finProgramme == 0) {
			curseur = 1;
			FD_ZERO(&readfds);
			FD_SET(STDIN_FILENO, &readfds);
			FD_SET(S2C, &readfds);
			n_modifiees = select(NBDESC, &readfds, NULL, NULL, NULL);
			if(n_modifiees == -1) {
				perror("select");
			}
			if(n_modifiees > 0) {
				//Si on entre des donnees par STDIN
				if(FD_ISSET(0, &readfds) != 0) {
					bzero(saisie,TAILLE_SAISIE);
					nlus = read(STDIN_FILENO, saisie, TAILLE_SAISIE);
					if(nlus == -1) {
						perror("read");
					}
					strcpy(message,"[");
					strcat(message,pseudo);
					strcat(message,"] ");
					strcat(message,saisie);
					necrits = write(C2S, message, TAILLE_MSG);
					if(necrits == -1) {
						perror("write");
					}
				}
				//Si on recoit des donnees du serveur
				if(FD_ISSET(S2C, &readfds) != 0) {
					nlus = read(S2C, message, TAILLE_MSG);
					if(nlus == -1) {
						perror("read");
					}
					//Condition si on recoit utilisateur fin s'est connectée
					if(strcmp(message,"[service] fin rejoint la discussion.\n")==0) {
						finProgramme = 1;
					}
					strcat(discussion[curseur%NB_LIGNES], message);
					curseur++;
				}
				afficher(curseur);
			}
			/* boucle principale (à faire) :
				* récupérer les messages reçus éventuels, puis les afficher.
				- tous les messages comportent TAILLE_MSG caractères, et les constantes
				sont fixées pour qu'il n'y ait pas de message tronqué, ce qui serait
				pénible à gérer.
				- lors de l'affichage, penser à effacer les lignes avant de les affecter
				pour éviter l'affichages de caractères parasites si l'ancienne ligne est
				plus longue que la nouvelle.
				* récupérer la ligne saisie éventuelle, puis l'envoyer
			*/
		}
	}
	/* nettoyage des tubes de service (à faire) */
	close(C2S);
	close(S2C);
	remove(tubeC2S);
	remove(tubeS2C);
	close(ecoute);
	printf("fin client\n");
	return EXIT_SUCCESS;
}

/* Note
 * (1) :	Pour éviter un appel de system() à chaque affichage, une solution serait,
					à l'initialisation du main(), d'appeler system("clear > nettoie"), pour récupérer
					la séquence spécifique au terminal du client permettant d'effacer l'affichage, 
          puis d'ouvrir nettoie en écriture, pour en écrire le contenu à chaque affichage.
          Cependant, le surcoût n'est pas vraiment critique ici pour l'application.
          Le principe (donné dans le sujet) est que tous les processus ouvrent les tubes
          dans le même ordre, pour éviter un interblocage (méthode des classes ordonnées).
 */
