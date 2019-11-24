#include <iostream>
#include <cstring>
#include <string>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>    // std::sort
#include <sys/stat.h>
#include <vector>
using namespace std;


typedef int Time_t;
const Time_t MAX_TIME = ~(1<<31);
class passenger{
    public:
    int from, to, updown;
    Time_t when;
};

int main(){
    int fd;
    if ((fd = open("input.txt", O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH))<0)
            perror("open");
    dup2(fd, 1);
    int N = 10, K = 4, C = 7;
    printf("10 4 7\n0:0:3 0:0:2 0:1:0 0:0:2 0:0:3 0:0:3\n");
    int n;
    int max_time1 = 60;
    cin>>n>>max_time1;
    vector<int> p(n);
    for(int i=0; i<n; ++i){
        p[i] = rand()%max_time1;
    }
    sort(p.begin(), p.end());
    for (int i=0; i<n; ++i){
        int from = rand()%N + 1, to = rand()%N;
        if (to==from-1) to=(to+1)%N+1; else to++;
        printf("%d:%d:%d %d %d\n", p[i]/3600, p[i]/60%60, p[i]%60, from, to);
    }
    
}