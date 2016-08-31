#include "proxy_parse.h"
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>

#define Buf_Size_Initial 4096

void dealClient(int sockid) {
    char buffer[Buf_Size_Initial];
    int flag, check, ret;
    ret = recv(sockid, buffer, Buf_Size_Initial, 0);

}

int main(int argc, char * argv[]) {

    if (argc != 2) {
        printf("Arguements to be supplied as follows: %s <port>\n", argv[0]);
        printf("Aborting\n");
        return 1;
    }

	int sockid, newsockid, serverport, clientIP[4];
    struct sockaddr_in serv_addr, cli_addr;
    char buffer[4096], streamlen[5];
    int  n, m, i, size;
    unsigned int addrLen, client1, client2;

    // declaring socket
    sockid = socket(AF_INET, SOCK_STREAM, 0);

    if (sockid < 0) {
        printf("ERROR opening socket\n");
        return 0;
    }

    // Cleaning socket struct
    memset(&serv_addr, 0, sizeof(serv_addr));
    serverport = atoi(argv[1]);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(serverport);

    // Binding socket to address
    if (bind(sockid, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        printf("ERROR on binding\n");
        return 0;
    }
      
    // Starting Listenning for connections
    int status = listen(sockid,5);
    if (status < 0) {
        printf("Error on listen command\n");
        return 0;
    }
    addrLen = sizeof(cli_addr);

    while (1) {
        // Accept connection from the client 

        newsockid = accept(sockid, (struct sockaddr *)&cli_addr, &addrLen);

        if (newsockid < 0) {
            printf("ERROR on accept\n");
            continue;
        }

        //calculates client IP in little endian
        client1 = cli_addr.sin_addr.s_addr, i = 3;
        printf("Client connected from IP: ");
        while (client1 > 0) {
            client2 = client1 / (1 << 8*(i));
            clientIP[i] = client2;
            client1 -= client2 * ( 1 << 8*(i));
            i--;
        }

        /* Prints Client's IP Address */
        /*for (int i = 0; i < 4; ++i) {
            printf("%d%c", clientIP[i], (i == 3)?'\n':'.');
        }*/

        if (fork() == 0) {
            dealClient(newsockid);
            close(newsockid);
            break;
        } else {
            close(newsockid);
        }

    }

	return 0;
}
