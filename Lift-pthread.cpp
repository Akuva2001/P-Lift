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

#define READ_DEBUG

bool get_int(char*& str, const char* eof, int& res){
    res = 0;
    for(; str!=eof && (*str<'0' || *str>'9') && *str!='-'; ++str);
    int k = 1; if (*str == '-'){++str; k=-1;}
    if (*str<'0' || *str>'9') return false;
    for(; str!=eof && *str>='0' && *str<='9'; ++str) res = 10*res + *str - '0';
    res*=k; return true;
}
typedef int Time_t;
const Time_t MAX_TIME = ~(1<<31);
bool get_time(char*& str, const char* eof, Time_t& res){
    int h, m, s; bool fl = get_int(str, eof, h) && get_int(str, eof, m) && get_int(str, eof, s);
    res = 3600*h + 60*m + s; return fl;
}
enum {
    UP, DOWN
};
class passenger{
    public:
    int from, to, updown;
    Time_t when;
};
bool get_passenger(char*& str, const char* eof, passenger& res){
    bool fl = get_time(str, eof, res.when) && get_int(str, eof, res.from) && get_int(str, eof, res.to);
    res.updown = (res.to>res.from)?UP:DOWN;
    return fl;
}
enum TYPES{
    PASSENGER, STAGE, LIFT, KILL_ME
};
class alarm_me{
    public:
    pthread_cond_t* cond;
    Time_t time;
    int type;
    alarm_me(pthread_cond_t* condd, Time_t timee, int typee){cond = condd;time = timee;type = typee;}
    friend bool operator < (const alarm_me &a, const alarm_me &b){//for priority_queue, it is > operator
        if (a.time<b.time) return !true;
        if (a.time>b.time) return !false;
        return a.type>b.type;
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
    priority_queue<alarm_me> qu_time;
    vector<pthread_t> lifts;
    vector<pthread_cond_t> lift_conds;
    vector<int> lift_tasks;
    queue<int> sleepy_lifts;
    bool inited = false;
    Building(){
        pthread_mutex_init(&admin_mut, NULL);
        pthread_cond_init(&admin_cond, NULL);
    }
    ~Building(){
        if (inited) for(int i=0; i<K; ++i) pthread_cond_destroy(&lift_conds[i]);
        pthread_cond_destroy(&admin_cond);
        pthread_mutex_destroy(&admin_mut);
    }
    bool init(char*& str, const char* eof){
        if (!(get_int(str, eof, N) && get_int(str, eof, K) && get_int(str, eof, C) &&
         get_time(str, eof, T_stage) && get_time(str, eof, T_open) && get_time(str, eof, T_idle) &&
         get_time(str, eof, T_close) && get_time(str, eof, T_in) && get_time(str, eof, T_out))) return false;;
        stages.resize(N+1);lifts.resize(K);lift_conds.resize(K);lift_tasks.resize(K);inited = true;
        for(int i=0; i<K; ++i) pthread_cond_init(&lift_conds[i], NULL);
        return true;;
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
    B->qu_time.push(alarm_me(&local_cond, MAX_TIME, KILL_ME));
    if(!B->init(str, eof)){fprintf(stderr, "file_reader: init Building\n"); exit(0);}
    #ifdef READ_DEBUG
    B->print();
    #endif
    //Building is inited
    passenger pg; bool fl = get_passenger(str, eof, pg);
    for(;fl;fl = get_passenger(str, eof, pg)){
        B->qu_time.push(alarm_me(&local_cond, pg.when, PASSENGER));
        pthread_cond_signal(&B->admin_cond);
        pthread_cond_wait(&local_cond, &B->admin_mut);
        B->stages[pg.from].q[pg.updown].push(pg.to);
    }
    munmap(mem, size);
    close(fd);
    //join me
    pthread_cond_signal(&B->admin_cond);
    pthread_mutex_unlock(&B->admin_mut);
    return NULL;
}

void* lift(void* adr){
    Building* B = ((pair<Building*, int> *)adr)->first;
    int th_number = ((pair<Building*, int> *)adr)->second;
    pthread_mutex_lock(&B->admin_mut);
    B->qu_time.push(alarm_me(&B->lift_conds[th_number], MAX_TIME, KILL_ME));
    B->sleepy_lifts.push(th_number);
    pthread_cond_signal(&B->admin_cond);
    pthread_cond_wait(&B->lift_conds[th_number], &B->admin_mut);//wait for first task
    for(;B->lift_tasks[th_number]!=0;){

        //do
        B->lift_tasks[th_number] = 0;
        B->sleepy_lifts.push(th_number);
        pthread_cond_signal(&B->admin_cond);
        pthread_cond_wait(&B->lift_conds[th_number], &B->admin_mut);//wait for first task
    }
    //join me
    pthread_cond_signal(&B->admin_cond);
    pthread_mutex_unlock(&B->admin_mut);
    return NULL;
}


void* time_admin(void* adr){
    Building B;
    B.file_name = *(string*)adr;
    pthread_mutex_lock(&B.admin_mut);
    pthread_create(&B.reader_tread, NULL, file_reader, &B);
    pthread_cond_wait(&B.admin_cond, &B.admin_mut);//Building is inited
    for (int i=0; i<B.K; ++i){
        pair<Building*, int> var = pair<Building*, int>(&B, i);
        pthread_create(&B.reader_tread, NULL, lift, &var);
        pthread_cond_wait(&B.admin_cond, &B.admin_mut);
    }
    for(;!B.qu_time.empty();){
        alarm_me alm = B.qu_time.top();B.qu_time.pop();
        pthread_cond_signal(alm.cond);
        pthread_cond_wait(&B.admin_cond, &B.admin_mut);
    }


    pthread_mutex_unlock(&B.admin_mut);
    pthread_join(B.reader_tread, NULL);
    for (int i=0; i<B.K; ++i) pthread_join(B.lifts[i], NULL);
    return NULL;
}




int main(){
    string filename("input.txt");
    pthread_t time_admin_thread;
    pthread_create(&time_admin_thread, NULL, time_admin, (void*)&filename);
    pthread_join(time_admin_thread, NULL);
    return 0;
}