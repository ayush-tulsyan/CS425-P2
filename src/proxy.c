#include "proxy_parse.h"
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>

#define Buf_Size_Initial 1024

char ErrorReply[3][378];
int ErrorLen[] = {109, 175, 141}; //[0] = 109, ErrorLen[1] = 175, ErrorLen[2] = 141;

void initiateErrorMsgs() {
    char ErrorMsg[3][22];
    sprintf(ErrorMsg[0], "Bad Request");
    sprintf(ErrorMsg[1], "Internal Server Error");
    sprintf(ErrorMsg[2], "Not Implemented");
    int ErrorId[] = {400, 500, 501};
    int len, i;
    for (i = 0; i < 3; ++i) {
        len = sprintf(ErrorReply[i], "HTTP/1.0 %d %s\r\n", ErrorId[i], ErrorMsg[i]);
        len += sprintf(ErrorReply[i] + len, "Content-Length: %d\r\n", ErrorLen[i]);
        len += sprintf(ErrorReply[i] + len, "Content-Type: text/html\r\n");
        len += sprintf(ErrorReply[i] + len, "Connection: close\r\n\r\n");
        len += sprintf(ErrorReply[i] + len, "<HEAD><TITLE>%d %s</TITLE></HEAD>\r\n", ErrorId[i], ErrorMsg[i]);
        len += sprintf(ErrorReply[i] + len, "<BODY><H1>%d %s</H1>\r\n", ErrorId[i], ErrorMsg[i]);
        switch(i) {
            case 0:
            len += sprintf(ErrorReply[i] + len, "Bad Request Received.\r\n");
            break;
            case 1:
            len += sprintf(ErrorReply[i] + len, "The request could not be processed due to an Internal server error.\r\n");
            break;
            case 2:
            len += sprintf(ErrorReply[i] + len, "The required method has not been Implemented.\r\n");
            break;
        }
        len += sprintf(ErrorReply[i] + len, "</BODY>\r\n%c", 0);
        ErrorLen[i] = len;
    }
    return;
}

int sendErrorReply(int SockId, int errorID) {
    
    int flag;
    switch(errorID) {
        case 400:
            flag = 0; break;
        case 500:
            flag = 1; break;
        case 501:
            flag = 2; break;
        default:
            return -1;
    }
    write(SockId, ErrorReply[flag], ErrorLen[flag]);
    printf("%s\n", ErrorReply[flag]);
    return 0;
}

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

int RecvFromClient(int SockId, char *buffer, int size) {
    int len = 0, flag = 1;
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

size_t RecvFromServer(int SockId, char *buffer, size_t size) {
    size_t len = 0, temp;
    if (buffer == NULL)
        return -1;

    temp = read(SockId, buffer, size);
    len = temp;
    char *index = buffer + len;
    while(temp != 0) {
        if(len >= size - 2 * Buf_Size_Initial) {
            buffer = (char *)realloc(buffer, 2 * size * sizeof(char));
            size *= 2;
        }        
        temp = read(SockId, index, size - len);
        len += temp;
        index += temp;
    }

    if (len <= 0) 
        return -1;

    return len;
}

int dealClient(int SockId) {
    char *buffer, *send_buf, *rep_buf;
    int  destSockId, ret, temp;
    size_t len;

    buffer = (char *)malloc(Buf_Size_Initial * sizeof(char));
    len = RecvFromClient(SockId, buffer, Buf_Size_Initial);
    if (len <= 0) {
        free(buffer);
        sendErrorReply(SockId, 500);
        return -1;
    }

    //Create a ParsedRequest to use. This ParsedRequest
    //is dynamically allocated.
    ParsedRequest *req = ParsedRequest_create();
    ret = ParsedRequest_parse(req, buffer, len);
    if (ret == -2) {
        sendErrorReply(SockId, 501);
        return -1;
    } else if (ret == -1) {
        sendErrorReply(SockId, 500);
        return -1;
    } else if (ret == -3) {
        sendErrorReply(SockId, 400);
    }
    free(buffer);

    if(EstablishConnection(req, &destSockId) < 0) {
        sendErrorReply(SockId, 500);
        ParsedRequest_destroy(req);
        return -1;
    }

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
    // printf("Hey from DC!\n");

    if (write(destSockId, send_buf, len + 1) <= 0) {
        sendErrorReply(SockId, 500);
        return -1;
    }
    free(send_buf);

    rep_buf = (char *)malloc(4 * Buf_Size_Initial * sizeof(char));
    len = RecvFromServer(destSockId, rep_buf, 4 * Buf_Size_Initial);
    if (len <= 0) {
        sendErrorReply(SockId, 500);
        free(rep_buf);
        return -1;
    }
    printf("%lu\n", len);
    for (long unsigned i = 0; i < len; ++i) {
        printf("%c", rep_buf[i]);
    }

    temp = len;
    while (temp > 0) {
        ret = write(SockId, rep_buf, temp);

        rep_buf += ret;
        temp -= ret;
    }
    // free(rep_buf - len);

    return 0;
}

int main(int argc, char * argv[]) {

    if (argc != 2) {
        printf("Arguements to be supplied as follows: %s <port>\n", argv[0]);
        printf("Aborting\n");
        return 1;
    }

    initiateErrorMsgs();
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
