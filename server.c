#include <stdio.h> 
#include <stdlib.h> 
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h> 
#include <sys/socket.h> 
#include <sys/epoll.h>
#include <errno.h>
#include <pthread.h>

// 상수 정의
#define BUF_SIZE 1000
#define EPOLLSIZE 50

// 함수 정의
void error_handling(char *buf);
void setnonblockingmode(int fd); // fd를 non-blcking 모드로 설정
void *handle_clnt(void *arg);
void send_msg(char *msg, int len); // 모든 클라이언트에게 메세지 전송 함수

int clnt_socks[EPOLLSIZE]; // 클라이언트 소켓 배열
int clnt_cnt = 0; // 현재 연결된 클라이언트 수
pthread_mutex_t mutx; // 클라이언트 소켓 배열 접근을 위한 뮤텍스

int main(int argc, char *argv[]){
    int serv_sock, clnt_sock; // 서버, 클라 소켓 변수
    struct sockaddr_in serv_adr, clnt_adr;
    socklen_t adr_sz;
    struct epoll_event *ep_events;
    struct epoll_event event;
    int epfd, event_cnt;

    // 포트번호를 입력하지 않고 실행했을 경우 실행
    if(argc != 2){
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }

    // 서버 소켓 생성 및 바인딩
    pthread_mutex_init(&mutx, NULL);
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family=AF_INET;
    serv_adr.sin_addr.s_addr=htonl(INADDR_ANY);
    serv_adr.sin_port=htons(atol(argv[1]));

    if(bind(serv_sock, (struct sockaddr*) &serv_adr, sizeof(serv_adr)) == -1){
        error_handling("bind() error");
    }
    if(listen(serv_sock, 5) == -1){
        error_handling("listen() error");
    }

    // 소켓 리슨 상태로 설정 및 epoll
    setnonblockingmode(serv_sock); // 서버 소켓을 non-blocking 모드 설정

    epfd=epoll_create(EPOLLSIZE); // epoll 인스턴스 생성
    ep_events=(struct epoll_event*)malloc(sizeof(struct epoll_event)*EPOLLSIZE); // ep_events 메모리 동적할당

    event.events=EPOLLIN;
    event.data.fd=serv_sock;
    epoll_ctl(epfd, EPOLL_CTL_ADD, serv_sock, &event); // 서버 소켓을 epoll 인스턴스에 추가(클라이언트 연결 요청 감지)

    // epoll 이벤트 루프
    while (1){
        event_cnt=epoll_wait(epfd, ep_events, EPOLLSIZE, -1); // i/o epoll 이벤트 발생까지 대기

        if(event_cnt==-1){
            puts("epoll_wait() error");
            break;
        }

        int i;
        for(i = 0; i < event_cnt; i++){
            if(ep_events[i].data.fd==serv_sock){ // 서버 소켓 이벤트 발생시
                adr_sz=sizeof(clnt_adr);
                clnt_sock=accept(serv_sock, (struct sockaddr*)&clnt_adr, &adr_sz); // 연결 수락 (accept)
                setnonblockingmode(clnt_sock); // 클라이언트 소켓을 non-blocking 모드 설정
                event.events=EPOLLIN | EPOLLET; // Edge Triggered 모드 설정
                event.data.fd=clnt_sock;
                epoll_ctl(epfd, EPOLL_CTL_ADD, clnt_sock, &event); // epoll 인스턴스에 추가

                // 뮤텍스 부분
                pthread_mutex_lock(&mutx);
                clnt_socks[clnt_cnt++] = clnt_sock; // 클라이언트 소켓을 배열에 추가 (클라이언트 수 증가)
                pthread_mutex_unlock(&mutx);

                printf("connected client: %d \n", clnt_sock); // 소켓 식벌 파일 디스크립터 출력
            }else{
                char msg[BUF_SIZE]; // 메시지 변수
                int str_len = read(ep_events[i].data.fd, msg, BUF_SIZE);
                if(str_len > 0){
                    msg[str_len] = '\0'; // 문자열 끝에 널 문자 추가
                    printf("Mesage from client %d: %s\n", ep_events[i].data.fd, msg);
                    send_msg(msg, str_len); // 모든 클라이언트에게 메시지 전송
                }else if (str_len == 0){
                    // 클라이언트 연결 종료 처리
                    pthread_mutex_lock(&mutx);
                    int j;
                    for(j = 0; j < clnt_cnt; j++){
                        if(clnt_socks[j] == ep_events[i].data.fd){
                            while (j < clnt_cnt - 1){
                                clnt_socks[j] = clnt_socks[j + 1];
                                j++;
                            }
                            break;
                        }
                    }
                    clnt_cnt--;
                    pthread_mutex_unlock(&mutx);
                    close(ep_events[i].data.fd);
                    printf("closed clinet: %d \n", ep_events[i].data.fd);
                }
            }
        }
    }

    // 리소스 정리
    close(serv_sock);
    close(epfd);
    free(ep_events);
    return 0;
}

// 파일 디스크립터를 논블로킹 모드로 설정
void setnonblockingmode(int fd){
    int flag = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flag | O_NONBLOCK);
}

// 에러 메시지 출력하고 종료하는 함수
void error_handling(char *buf){
    fputs(buf, stderr);
    fputc('\n', stderr);
    exit(1);
}

void *handle_clnt(void *arg){
    int clnt_sock = *((int*)arg);
    char msg[BUF_SIZE];
    int strLen = read(clnt_sock, msg, BUF_SIZE);

    if(strLen <= 0){
        pthread_mutex_lock(&mutx);
        int i;
        for(i = 0; i < clnt_cnt; i++){
            if(clnt_sock == clnt_socks[i]){
                while(i++ < clnt_cnt-1){
                    clnt_socks[i] = clnt_socks[i+1]; // 연결 수락되면 배열에 추가
                }
                break;
            }
        }

        clnt_cnt--;
        pthread_mutex_unlock(&mutx);
        close(clnt_sock);
        printf("closed client: %d \n", clnt_sock);
        return NULL;
    }

    send_msg(msg, strLen);
    return NULL;
}

// 모든 연결된 클라이언트에게 메시지 전송 함수
void send_msg(char *msg, int len){
    pthread_mutex_lock(&mutx);
    int i;
    for(i = 0; i < clnt_cnt; i++){
        write(clnt_socks[i], msg, len);
    }
    pthread_mutex_unlock(&mutx);
}