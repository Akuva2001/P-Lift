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
class passenger_class{
    public:
    int from, to, updown;
    Time_t come, come_in = -1, come_out = -1;
};
bool get_passenger_class(char*& str, const char* eof, passenger_class& res){
    bool fl = get_time(str, eof, res.come) && get_int(str, eof, res.from) && get_int(str, eof, res.to);
    res.updown = (res.to>res.from)?UP:DOWN;
    return fl;
}
enum TYPES{
    PASSENGER, LIFT, KILL_ME
};
enum{
    ALARM_ME, NOT_ALARM_ME
};
class alarm_class{
    public:
    pthread_cond_t* cond;
    Time_t time;
    int type; int* act;
    alarm_class(pthread_cond_t* condd, Time_t timee, int typee, int* actt = NULL){cond = condd;time = timee;type = typee;}
    friend bool operator < (const alarm_class &a, const alarm_class &b){//for priority_queue, it is > operator
        if (a.time<b.time) return !true;
        if (a.time>b.time) return !false;
        return a.type>b.type;
    }
};
class stage_class{
    public:
    queue<passenger_class> q[2];//0 - up, 1 - down
};
class lift_class;
class reader_class;
void* reader_fun(void* adr);
void* lift_fun(void* adr);
class Building{
    public:
    int N = 0, K = 0, C = 0;
    Time_t T_stage = 0, T_open = 0, T_idle = 0, T_close = 0, T_in = 0, T_out = 0, Time = 0;
    vector<stage_class> stages;
    pthread_mutex_t admin_mut;
    pthread_cond_t admin_cond;
    priority_queue<alarm_class> qu_time;
    map<int, int> sleepy_lifts;//stage_class, thread_number
    map<int, int> requests;//stage_class, updown
    Time_t World_time = 0;
    reader_class reader;
    vector<lift_class> lifts;
    vector<passenger_class> accepted_passengers;
    bool inited = false;
    Building(){
        pthread_mutex_init(&admin_mut, NULL);
        pthread_cond_init(&admin_cond, NULL);
    }
    ~Building(){
        pthread_cond_destroy(&admin_cond);
        pthread_mutex_destroy(&admin_mut);
    }
    bool init(char*& str, const char* eof){
        if (!(get_int(str, eof, N) && get_int(str, eof, K) && get_int(str, eof, C) &&
         get_time(str, eof, T_stage) && get_time(str, eof, T_open) && get_time(str, eof, T_idle) &&
         get_time(str, eof, T_close) && get_time(str, eof, T_in) && get_time(str, eof, T_out))) return false;;
        stages.resize(N+1);lifts.resize(K);inited = true;
        for(int i=0; i<K; ++i) lifts[i].passengers.resize(N+1);
        return true;;
    }
    void run(){
        pthread_mutex_lock(&admin_mut);
        pthread_create(&reader.thr, NULL, reader_fun, &reader);
        pthread_cond_wait(&admin_cond, &admin_mut);//Building is inited
        if(!inited) {pthread_join(reader.thr, NULL);return;}
        for (int i=0; i<K; ++i){
            lifts[i].th_number = i;
            pthread_create(&lifts[i].thr, NULL, lift_fun, &lifts[i]);
            pthread_cond_wait(&admin_cond, &admin_mut);
        }//Lifts is inited
        for(;!qu_time.empty();){
            alarm_class alm = qu_time.top();qu_time.pop();
            World_time = alm.time;
            if (alm.act != NULL && *alm.act == NOT_ALARM_ME){delete alm.act;continue;}
            if (alm.act != NULL) delete alm.act;
            pthread_cond_signal(alm.cond);
            pthread_cond_wait(&admin_cond, &admin_mut);
        }
        pthread_join(reader.thr, NULL);
        for (int i=0; i<K; ++i) pthread_join(lifts[i].thr, NULL);
        pthread_mutex_unlock(&admin_mut);
    }
    void print(){
        printf("N = %d, K = %d, C = %d,\nT_stage = %d T_open = %d, T_idle = %d,\nT_close = %d, T_in = %d, T_out = %d\n", N, K, C, T_stage, T_open, T_idle, T_close, T_in, T_out);
    }
};
class reader_class{
    char *str, *eof;
    public:
    Building *B;
    string filename;
    pthread_t thr;
    pthread_cond_t cond;
    reader_class(){pthread_cond_init(&cond, NULL);}
    ~reader_class(){pthread_cond_destroy(&cond);}
    void run(){
        int fd; if ((fd = open(filename.c_str(), O_RDONLY))<0){perror("reader_fun: open"); exit(0);}
        struct stat stat_buf; int status = stat(filename.c_str(), &stat_buf);
        int size = stat_buf.st_size; void* mem = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
        if (mem==NULL) {perror("reader_fun: mmap"); exit(0);}
        str = (char *) mem; eof = str+size;
        pthread_mutex_lock(&B->admin_mut);
        if(!B->init(str, eof)){fprintf(stderr, "reader_fun: init Building error\n"); exit(0);}
        #ifdef READ_DEBUG
        B->print();
        #endif
        //Building is inited
        passenger_class pg; bool fl = get_passenger_class(str, eof, pg);
        for(;fl;fl = get_passenger_class(str, eof, pg)){
            B->qu_time.push(alarm_class(&cond, pg.come, PASSENGER));
            pthread_cond_signal(&B->admin_cond);
            pthread_cond_wait(&cond, &B->admin_mut);
            B->stages[pg.from].q[pg.updown].push(pg);
            if(B->stages[pg.from].q[pg.updown].size()==1){//we are first on this stage_class and need to push the button
                if(!B->sleepy_lifts.empty()){
                    auto lb = B->sleepy_lifts.lower_bound(pg.from);
                    auto ub = B->sleepy_lifts.upper_bound(pg.from);
                    map<int,int>::iterator acc;
                    if(lb==B->sleepy_lifts.end()) acc = ub;//map is not empty, so one of them exists for sure 
                    else{
                        int dl = abs(lb->first-pg.from), du = abs(ub->first-pg.from);
                        if(dl<du) acc = lb; else if (dl>du) acc = ub;
                        else{if (pg.updown == UP) acc = lb; else acc = ub;}
                    }
                    B->lifts[acc->second].task = pg.to;
                    B->lifts[acc->second].updown = pg.updown;
                    B->qu_time.push(alarm_class(&B->lifts[acc->second].cond, pg.come, LIFT));
                    B->sleepy_lifts.erase(acc);
                } else B->requests.insert(pair<int,int>(pg.from, pg.updown));
            }
        }
        munmap(mem, size);
        close(fd);
        //join me
        pthread_cond_signal(&B->admin_cond);
        pthread_cond_destroy(&cond);
        pthread_mutex_unlock(&B->admin_mut);
    }
};
enum{
    WAIT, DOORS_OPENED, DOORS_CLOSED, PASSENGERS_EXCHANGE, SLEEP, LIFT_ERR, NOT_STARTED
};
class lift_class{
    public:
    Building* B;
    int th_number, my_stage = 1, updown = UP, task = 0;
    vector<queue<passenger_class>> passengers;
    pthread_t thr;
    pthread_cond_t cond;
    int state = NOT_STARTED;
    int number = 0;
    lift_class(){pthread_cond_init(&cond, NULL);}
    ~lift_class(){pthread_cond_destroy(&cond);}
    void alarm(Time_t when){
        B->qu_time.push(alarm_class(&cond, when, LIFT)); wait();
    }
    void wait(){
        pthread_cond_signal(&B->admin_cond);
        pthread_cond_wait(&cond, &B->admin_mut);
    }
    void open_doors(){
        if (state == WAIT || state == DOORS_OPENED) {state = DOORS_OPENED;return;}
        if (state == DOORS_CLOSED || state == SLEEP){alarm(B->World_time+B->T_open); state = DOORS_OPENED; return;}
        state = LIFT_ERR; fprintf(stderr, "open_doors_err\n");
    }
    void close_doors(){
        if (state == DOORS_CLOSED || state == SLEEP) {state = DOORS_CLOSED;return;}
        if (state == DOORS_OPENED || state == WAIT){alarm(B->World_time+B->T_close); state = DOORS_CLOSED; return;}
        state = LIFT_ERR; fprintf(stderr, "close_doors_err\n");
    }
    void get_out_passengers(){
        if (state!=DOORS_OPENED){state = LIFT_ERR; fprintf(stderr, "get_out_passengers_err\n");return;}
        for(;!passengers[my_stage].empty();){
            passenger_class pg = passengers[my_stage].front();passengers[my_stage].pop();--number;
            pg.come_out = B->World_time; B->accepted_passengers.push_back(pg);alarm(B->World_time+B->T_out);
        }
    }
    void get_passengers(){
        if (state!=DOORS_OPENED){state = LIFT_ERR; fprintf(stderr, "get_passengers_err\n");return;}
        for (;number<B->C && !B->stages[my_stage].q[updown].empty();){
            passenger_class pg = B->stages[my_stage].q[updown].front();B->stages[my_stage].q[updown].pop();
            pg.come_in = B->World_time + B->T_in;passengers[pg.to].push(pg);++number;
            alarm(B->World_time+B->T_in);
        }
    }
    void go_to_stage_by_steps(){
        if (my_stage == task) {fprintf(stderr, "go_to_stage_by_steps_err\n"); return;}
        close_doors();int dt=(my_stage>task)?-1:1;
        for(;my_stage+dt!=task;){
            my_stage+=dt; alarm(B->World_time+B->T_stage);
            if(!passengers[my_stage].empty() || (!B->stages[my_stage].q[updown].empty() && number<B->C)){open_doors();get_out_passengers(); get_passengers();close_doors();}
        }
        my_stage+=dt; alarm(B->World_time+B->T_stage);
    }
    void go_to_stage_directly(){
        if (my_stage == task) {fprintf(stderr, "go_to_stage_directly_err\n"); return;}
        close_doors();alarm(B->World_time + B->T_stage*abs(my_stage-task));my_stage = task;
    }
    void end(){
        int dt = (updown == UP)?1:-1;
        open_doors();get_out_passengers(); get_passengers();
        for (;number!=0;){
            close_doors();
            my_stage+=dt; alarm(B->World_time+B->T_stage);
            if(!passengers[my_stage].empty() || (!B->stages[my_stage].q[updown].empty() && number<B->C)){open_doors();get_out_passengers(); get_passengers();}
        }
        state = WAIT;
    }
    void sleep(){
        state = WAIT; task = 0;int* p = new int; *p = ALARM_ME;
        B->sleepy_lifts.insert(pair<int,int>(my_stage, th_number));
        B->qu_time.push(alarm_class(&cond, B->World_time + B->T_idle, LIFT, p));
        wait();
        if (task != 0) {*p = NOT_ALARM_ME;return;}
        close_doors();
        state = SLEEP;
        wait();
    }
    void run(){
        pthread_mutex_lock(&B->admin_mut);
        B->qu_time.push(alarm_class(&cond, MAX_TIME, KILL_ME));
        state = SLEEP;
        B->sleepy_lifts.insert(pair<int,int>(my_stage, th_number));
        wait();//wait for first task
        for(;task!=0;){
            if(task==my_stage) /*ok*/;
            else if(task>my_stage && updown == UP || task<my_stage && updown == DOWN) go_to_stage_by_steps();
            else go_to_stage_directly();
            end();
            //get new task
            task = 0;
            if(!B->requests.empty()){
                auto lb = B->requests.lower_bound(my_stage);
                auto ub = B->requests.upper_bound(my_stage);
                map<int,int>::iterator acc;
                if(lb==B->requests.end()) acc = ub;//map is not empty, so one of them exists for sure 
                else{
                    int dl = abs(lb->first-my_stage), du = abs(ub->first-my_stage);
                    if(dl<du) acc = lb; else if (dl>du) acc = ub;
                    else{if (ub->second == UP) acc = ub; else acc = lb;}
                }
                task = acc->first; updown = acc->second;
                B->requests.erase(acc);
            } else sleep(); //reader may give us new task
        }
        //join me, i haven't tasks
        pthread_cond_signal(&B->admin_cond);
        pthread_mutex_unlock(&B->admin_mut);
    }

};
void* reader_fun(void* adr){
    reader_class* R = (reader_class*)adr;
    R->run();
    return NULL;
}
void* lift_fun(void* adr){
    lift_class *L = (lift_class*)adr;
    L->run();
    return NULL;
}

int main(){
    Building B; B.reader.filename = string("input.txt");
    B.run();
    return 0;
}