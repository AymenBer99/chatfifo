# chatfifo
Chat between less than 6 persons using a named pipe.
Contains two files : serveur.c and console.c

### serveur.c :
Opens the communication and allows users to connect to servers with the named pipe "ecoute"

### console.c :
It represents the user ans has to be executed with one parameter that represents the name of the person.
It asks to open the connexion with the server with the named pipe "ecoute".
The server then opens two named pipes to receive the messages sent from the user and to send the other users messages.
The user can disconnect by typing "au revoir"
