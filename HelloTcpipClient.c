#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUF_SIZE 1000

void error_handling(char *message);

int main(int argc, char *argv[])
{
    int sock;
    struct sockaddr_in serv_addr;
    char message[BUF_SIZE];
    int str_len;
    fd_set reads, cpy_reads;
    struct timeval timeout;

    if(argc != 3) {
        printf("Usage : %s <IP> <port>\n", argv[0]);
        exit(1);
    }

    sock = socket(PF_INET, SOCK_STREAM, 0);   
    if(sock == -1)
        error_handling("socket() error");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));
    
    if(connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("connect() error!");
    else
        puts("Connected...........");

    while(1) 
    {
        FD_ZERO(&reads);
        FD_SET(0, &reads); // 0은 표준 입력(stdin)
        FD_SET(sock, &reads);

        timeout.tv_sec = 5;
        timeout.tv_usec = 5000;

        int result = select(sock + 1, &reads, NULL, NULL, &timeout);
        if(result == -1)
            break;
        if(result == 0)
            continue;

        if(FD_ISSET(sock, &reads)) // 소켓에 데이터가 들어오면
        {
            str_len = read(sock, message, BUF_SIZE - 1);
            if(str_len == 0) // 서버로부터 연결 종료 메시지를 받으면
                break;

            message[str_len] = 0;
            printf("Message from server: %s", message);
        }

        if(FD_ISSET(0, &reads)) // 사용자로부터 입력이 있으면
        {
            str_len = read(0, message, BUF_SIZE);
            if(str_len > 0)
            {
                message[str_len] = 0;
                write(sock, message, str_len);
            }
        }
    }

    close(sock);
    return 0;
}

void error_handling(char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
