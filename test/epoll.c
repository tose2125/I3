#include <sys/epoll.h>
#include <string.h> // memset
#include <unistd.h> // stdin_fileno
#include <stdio.h>  // printf

#define EPOLL_LIMIT 2
#define EPOLL_TIMEOUT 5
#define N 8

int main(int argc, char const *argv[])
{
    char data[N];

    int epfd = epoll_create(EPOLL_LIMIT);

    struct epoll_event stdin_event;
    memset(&stdin_event, 0, sizeof(stdin_event));
    stdin_event.events = EPOLLIN;
    stdin_event.data.fd = STDIN_FILENO;
    epoll_ctl(epfd, EPOLL_CTL_ADD, STDIN_FILENO, &stdin_event);

    int available;
    struct epoll_event current_events[EPOLL_LIMIT];
    while ((available = epoll_wait(epfd, current_events, EPOLL_LIMIT, 1000 * EPOLL_TIMEOUT)) > 0)
    {
        for (int i = 0; i < available; i++)
        {
            printf("EPOLL fd: %d\n", current_events[i].data.fd);
            int n = read(current_events[i].data.fd, data, N);
            fwrite(data, sizeof(char), n, stdout);
            putchar('\n');
        }
    }

    printf("EPOLL timeout\n");

    close(epfd);

    return 0;
}
