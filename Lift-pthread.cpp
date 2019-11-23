#include <cstdio>
#include <cstring>
#include <iostream>     // std::cout
#include <algorithm>    // std::make_heap, std::pop_heap, std::push_heap, std::sort_heap
#include <vector>       // std::vector
#include <map>
#include <string>
#include <set>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <queue>
#include <pthread.h>
using namespace std;

int get_int(char*& str, const char* eof){
    int res = 0;
    for(; str!=eof && (*str<'0' || *str>'9'); ++str);
    for(; str!=eof && *str>='0' && *str<='9'; ++str) res = 10*res + *str - '0';
    return res;
}
typedef int Time_t;
Time_t get_time(char*& str, const char* eof){
    int h = get_int(str, eof), m = get_int(str, eof), s = get_int(str, eof);
    return 3600*h + 60*m + s;
}
class passenger{
    public:
    int from, to;
    Time_t when;
};
passenger get_passenger(char*& str, const char* eof){
    passenger res;
    res.when = get_time(str, eof); res.from = get_int(str, eof); res.to = get_int(str, eof);
    return res;
}

enum TYPES{
    LIFT_SLEEP, PASSENGER, LIFT
};

class cond_var{
    pthread_cond_t cond;
    pthread_mutex_t *mut;
    public:
    cond_var(pthread_mutex_t *mutex){
        mut = mutex;
        pthread_cond_init(&cond, NULL);
    }
    ~cond_var(){
        pthread_cond_destroy(&cond);
    }
    void wait(){
        pthread_mutex_lock(mut);
        pthread_cond_wait(&cond, mut);
        pthread_mutex_unlock(mut);
    }
    void notify(){
        pthread_mutex_lock(mut);
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(mut);
    }
};

class alarm_me{
    public:
    cond_var* cond;
    Time_t time;
    int type;
    alarm_me(cond_var* condd, Time_t timee, int typee){cond = condd;time = timee;type = typee;}
    bool operator < (alarm_me &b){
        if (time<b.time) return true;
        if (time>b.time) return false;
        return type<b.type;
    }
};


class stage{
    public:
    queue<int> q[2];//0 - up, 1 - down
};




class Building{
    public:
    int N = 0, K = 0, C = 0;
    Time_t T_stage = 0, T_open = 0, T_idle = 0, T_close = 0, T_in = 0, T_out = 0;
    vector<stage> stages;
    pthread_mutex_t admin_mut;
    pthread_cond_t admin_cond;
    string file_name;
    pthread_t reader_tread;
    vector<alarm_me> qu_time;
    vector<pthread_t> lifts;
    Building(){
        pthread_mutex_init(&admin_mut, NULL);
        pthread_cond_init(&admin_cond, NULL);
    }
    ~Building(){
        pthread_cond_destroy(&admin_cond);
        pthread_mutex_destroy(&admin_mut);
    }
    void init(char*& str, const char* eof){
        N = get_int(str, eof); K = get_int(str, eof); C = get_int(str, eof);
        T_stage = get_time(str, eof); T_open = get_time(str, eof); T_idle = get_time(str, eof);
        T_close = get_time(str, eof); T_in = get_time(str, eof); T_out = get_time(str, eof);
        stages.resize(N);lifts.resize(K);
    }
    void print(){
        printf("N = %d, K = %d, C = %d,\nT_stage = %d T_open = %d, T_idle = %d,\nT_close = %d, T_in = %d, T_out = %d\n", N, K, C, T_stage, T_open, T_idle, T_close, T_in, T_out);
    }
};

void* file_reader(void* adr){
    Building* B = (Building *)adr;
    int fd; if ((fd = open(B->file_name.c_str(), O_RDONLY))<0){perror("file_reader: open"); exit(0);}
    struct stat stat_buf; int status = stat(B->file_name.c_str(), &stat_buf);
    int size = stat_buf.st_size; void* mem = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    if (mem==NULL) {perror("file_reader: mmap"); exit(0);}
    char *str = (char *) mem; const char *eof = str+size;
    pthread_cond_t local_cond;
    pthread_cond_init(&local_cond, NULL);
    pthread_mutex_lock(&B->admin_mut);
    B->init(str, eof);
    pthread_cond_signal(&B->admin_cond);
    pthread_cond_wait(&local_cond, &B->admin_mut);





    pthread_mutex_unlock(&B->admin_mut);
    munmap(mem, size);
    close(fd);
    return NULL;
}


void* time_admin(void* adr){
    Building B;
    B.file_name = *(string*)adr;
    pthread_mutex_lock(&B.admin_mut);
    pthread_create(&B.reader_tread, NULL, file_reader, &B);
    pthread_cond_wait(&B.admin_cond, &B.admin_mut);
    
    pthread_mutex_unlock(&B.admin_mut);
    B.print();
    return NULL;
}




int main(){
    string filename("input.txt");
    pthread_t time_admin_thread;
    pthread_create(&time_admin_thread, NULL, time_admin, (void*)&filename);
    pthread_join(time_admin_thread, NULL);
    return 0;
}