#include "proxy_parse.h"
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>

#define Buf_Size_Initial 1024
#define Ip_addr 1
#define Server_Name_addr 2

int sendErrorReply(int SockId, int errorID) {
    
    char buffer[Buf_Size_Initial];
    int len;
    len = sprintf(buffer, "HTTP/1.0 500 Internal Server Error\r\n");
    len += sprintf(buffer + len, "Connection: close\r\n");
    len += sprintf(buffer + len, "Content-Length: %d\r\n", 175);
    len += sprintf(buffer + len, "Content-Type: text/html\r\n\r\n");
    len += sprintf(buffer + len, "<HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>\r\n");
    len += sprintf(buffer + len, "<BODY><H1>500 Internal Server Error</H1>\r\n");
    len += sprintf(buffer + len, "The request could not be processed due to an Internal server error.\r\n");
    len += sprintf(buffer + len, "</BODY>\r\n");

    send(SockId, buffer, len, 0);
    
    return 0;
}

/*char *getIPbyname(ParsedRequest *req) {
    int flag;
    flag = hostAddressType(req);
    if (flag == Server_Name_addr) {
        struct hostent* destInfo = gethostbyname(req->host);
        if (destInfo != NULL) {

        }
    } else if (flag == Ip_addr) {
        return req->host;
    }
    return NULL;
}*/

int EstablishConnection(ParsedRequest *req, int *sockid) {
    if (req && req->host) {

        struct hostent* destInfo = gethostbyname(req->host);
        int destPort;

        *sockid = socket(AF_INET, SOCK_STREAM, 0);

        if (destInfo == NULL || *sockid < 0) {
            return -1;
        }

        if (!(req->port))
            destPort = 80;
        else 
            destPort = atoi(req->port);

        struct sockaddr_in serv_addr;

        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        bcopy((char *)destInfo->h_addr, (char *)&serv_addr.sin_addr.s_addr, destInfo->h_length);
        serv_addr.sin_port = htons(destPort);

        if (connect(*sockid, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
            return -1;
        }

    } else {
        return -1;
    }

    return 0;
}

int RecvFromClient(int SockId, char *buffer) {
    int len = 0, size = Buf_Size_Initial, flag = 1;
    char *pstn;
    if (buffer == NULL) 
        return -1;


    len = recv(SockId, buffer, size, 0);
    while (flag) {
        
        pstn = strstr(buffer, "\r\n\r\n");
        if(pstn) {
            if (pstn - buffer == len - 4) {
                break;
            }
        }

        if(len >= size - 512) {
            buffer = (char *)realloc(buffer, 2 * size * sizeof(char));
            size *= 2;
        }
        len += recv(SockId, buffer + len, size - len, 0);
     
    }
    if(len <= 0)
        return -1;

    return len;
}

int RecvFromServer(int SockId, char *buffer) {
    int len = 0, size = Buf_Size_Initial;
    if (buffer == NULL)
        return -1;

    len = recv(SockId, buffer, size, 0);
    while(len == size) {
        buffer = (char *) realloc (buffer, 2 * size * sizeof(char));
        len += recv(SockId, buffer + len, size, 0);
        size *= 2;
    }

    if (len <= 0) 
        return -1;

    return len;
}

int dealClient(int SockId) {
    char *buffer, *send_buf;
    int  destSockId, ret;
    size_t len;

    buffer = (char *)malloc(Buf_Size_Initial * sizeof(char));
    len = RecvFromClient(SockId, buffer);
    if (len <= 0) {
        free(buffer);
        sendErrorReply(SockId, 500);
        return -1;
    }

    //Create a ParsedRequest to use. This ParsedRequest
    //is dynamically allocated.
    ParsedRequest *req = ParsedRequest_create();
    ret = ParsedRequest_parse(req, buffer, len);
    if (ret < 0) {
        sendErrorReply(SockId, 500);
        return -1;
    }
    free(buffer);

    if(EstablishConnection(req, &destSockId) < 0) {
        sendErrorReply(SockId, 500);
        ParsedRequest_destroy(req);
        return -1;
    }
    printf("Hey from DC\n");

    if (ParsedHeader_set(req, "Connection", "close") < 0) {
        sendErrorReply(SockId, 500);
        ParsedRequest_destroy(req);
        return -1;
    }

    if (ParsedHeader_set(req, "Host", req->host) < 0) {
        ParsedRequest_destroy(req);
        sendErrorReply(SockId, 500);
        return -1;
    }

    len = ParsedRequest_totalLen(req);

    send_buf = (char *)malloc((len + 1) * sizeof(char));
    if (send_buf == NULL) {
        ParsedRequest_destroy(req);
        sendErrorReply(SockId, 500);
        return -1;
    }

    if (ParsedRequest_unparse(req, send_buf, len + 1) < 0) {
        free(send_buf);
        sendErrorReply(SockId, 500);
        return -1;
    }
    ParsedRequest_destroy(req);

    printf("%s\n", send_buf);
    if (send(destSockId, send_buf, len + 1, 0) <= 0) {
        sendErrorReply(SockId, 500);
        return -1;
    }
    free(send_buf);

    buffer = (char *)malloc(Buf_Size_Initial * sizeof(char));
    len = RecvFromServer(destSockId, buffer);
    if (len <= 0) {
        sendErrorReply(SockId, 500);
        free(buffer);
        return -1;
    }

    if (send(SockId, buffer, len, 0) <= 0) {
        sendErrorReply(SockId, 500);
        return -1;
    }    
    free(buffer);

    return 0;
}

int main(int argc, char * argv[]) {

    if (argc != 2) {
        printf("Arguements to be supplied as follows: %s <port>\n", argv[0]);
        printf("Aborting\n");
        return 1;
    }

	int sockid, newsockid, serverport, clientIP[4], i;
    struct sockaddr_in serv_addr, cli_addr;
    int  childcount = 0, temp;
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
    FILE *fp = fopen("proxy.log", "a");

    while (1) {
        // Accept connection from the client 

        newsockid = accept(sockid, (struct sockaddr *)&cli_addr, &addrLen);

        if (newsockid < 0) {
            printf("ERROR on accept\n");
            continue;
        }

        //calculates client IP in little endian
        client1 = cli_addr.sin_addr.s_addr, i = 3;
        fprintf(fp, "Client connected from IP: ");
        while (client1 > 0) {
            client2 = client1 / (1 << 8*(i));
            clientIP[i] = client2;
            client1 -= client2 * ( 1 << 8*(i));
            i--;
        }

        /* Prints Client's IP Address */
        for (int i = 0; i < 4; ++i) {
            fprintf(fp, "%d%c", clientIP[i], (i == 3)?'\n':'.');
        }

        temp = fork();
        childcount ++;
        if (temp == 0) {
            dealClient(newsockid);
            shutdown(newsockid, 2);
            close(newsockid);
            break;
        } else {
            close(newsockid);
            if (childcount >= 20) {
                wait(&temp);
                childcount --;
            }
        }

    }

	return 0;
}
