Ana Vukojevic 2/9/2020

CLIENT
----------------------
The client has 5 commands:
Get <filename>- get a file from the server
Put <filename>- put a file into the server
ls - list files in directory of server
delete <filename> - delete file in server
exit - exit gracefully 

to run:
run make all (using the client/makefile)
then use command:
./client <server ip or hostname> <port number>


SERVER
---------------------
Server accepts all 5 of the client's commands

to run:
run make all (using the server/makefile)
then use command:
./server <port number>

use a port number over 5000
To get the hostname or ip address for the client:
hostname -> returns hostname of machine
curl ifconfig.me -> returns public ip address