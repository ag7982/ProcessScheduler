#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <ctype.h>
#include <sstream>
#include <deque>
#include <queue>
#include <vector>
using namespace std;

//////////// DATA STRUCTUrES AND GLOBAL VARIABLES ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef enum { STATE_RUNNING , STATE_READY, STATE_CREATED, STATE_BLOCKED, STATE_COMPLETED, STATE_PREEMPT } process_state;
typedef enum {TRANS_TO_READY,TRANS_TO_BLOCKED,TRANS_TO_RUNNING,TRANS_TO_PREEMPT,TRANS_TO_FINISHED}
transition;
int num_random;
double total_tat =0;
double total_cpu_util = 0;
double total_cpu_wait = 0;
bool preprio = false;
double total_io_util = 0;
int start_time= 0;
double throughput=0;
int finish_time=0;
int count_of_blocked = 0;
bool vflag;
vector <int> random_list;
int ofs=0;
int previous_time;
int count=0;
int current_time=0;
bool invoke_scheduler;

class process
{
    public:
        int at;
        int io_burst;
        int total_cpu;
        int total_cpu_o;
        int cpu_burst;
        int id;
        int state_ts = 0;
        int static_p;
        int dynamic_p;
        bool prem = false;
        int finishing_time;
        int cpu_wait_time;
        int burst = 0;
        int io_wait_time;
        int turn_around_time;
        int time_spent_in_state = 0;
        int input_quantum;
        process_state previous_state;
        process_state current_state;
    
};

class event
{
    public:
        process * proc;
        int timestamp;
        transition trans;
};

deque <process *> process_q;
deque <event *> event_q;
void add_to_eventq(event * e);
void create_event(process * p);
int get_random(int burst);
void create_process(int at, int tt, int cb, int ib, int input_quantum, int prio);
void print_processes();
void initialise_random_list(string rfile_path);


///////////////////////////////////////////////SCHEDULERS///////////////////////////////////////////////////////////////////////////////////////

class scheduler
{
    public:
    int input_quantum = input_quantum;
    deque <process *> ready_q;
    
    virtual void add_process(process * p) {};
    virtual process* get_next_process() {};
};
class SRTF: public scheduler
{
    public:
    int input_quantum = 10000;
    process* get_next_process()
    {
        if (ready_q.empty())
        {
            return nullptr;
        }
        process *p = ready_q.front();
        ready_q.pop_front();
        return p;
    }
    void add_process(process *p)
    {
        auto it = ready_q.begin();
        while (it!=ready_q.end())
        {
            if ((*it)->total_cpu>p->total_cpu)
            {
                ready_q.insert(it, p);
                return;
            }
            it++;
        }
        ready_q.push_back(p);
        return;
    }
};
class RR: public scheduler
{
    public:
     process* get_next_process()
    {
        if (ready_q.empty())
        {
            return nullptr;
        }
        process *p = ready_q.front();
        ready_q.pop_front();
        return p;
    }
     void add_process(process *p)
    {
        ready_q.push_back(p);
        return;
    }
};
class FIFO: public scheduler
{
    public:
    int input_quantum =10000;
     process* get_next_process()
    {
        if (ready_q.empty())
        {
            return nullptr;
        }
        process *p = ready_q.front();
        ready_q.pop_front();
        return p;
    }
     void add_process(process *p)
    {
        ready_q.push_back(p);
        return;
    }
};

class LCFS: public scheduler
{
    public:
    int input_quantum = 10000;
     process* get_next_process()
    {
        if (ready_q.empty())
        {
            return nullptr;
        }
        process *p = ready_q.back();
        ready_q.pop_back();
        return p;
    }
     void add_process(process *p)
    {
        ready_q.push_back(p);
        return;
    }
};

class PRIO: public scheduler
{
    public:
    int mx_prio;
    vector <deque<process*>> running;
    vector<deque<process*>> expired;

    PRIO(int m)
    {
        mx_prio = m;
        for (int i = 0;i<=mx_prio;i++)
        {
            running.push_back(deque <process*>());
            expired.push_back(deque <process*>());
        }
    }

    void add_process(process *p)
    {
        if (p->dynamic_p == -1)
        {
            p->dynamic_p = p->static_p - 1;
            if (vflag) {cout<<"psuhed process:"<<p->id<<" to expired q with new prio:"<<p->dynamic_p<<endl;}
            expired[p->dynamic_p].push_back(p);
            return;
        }
        else
        {
            // cout<<p->dynamic_p<<endl;
            running[p->dynamic_p].push_back(p);
                    // cout<<"hey"<<endl;

        }
        
        return;
    }

    process * get_next_process()
    {
        int i;
        // cout<<"hi"<<endl;
        for (int j =mx_prio;j>=0;j--)
        {
            if (!running[j].empty())
            {
                i=1;
                break;
            }
            else
            {
                i=0;
            }
        }
        if (i==0)
        {
            running.swap(expired);
            if (vflag) {cout<<"swapped expired and running"<<endl; }
        }
        for (int j=mx_prio;j>=0;j--)
        {
            if (!running[j].empty())
            {
                process * pp = running[j].front();
                if(vflag) cout<<"returned process: "<<pp->id<<endl;
                running[j].pop_front();
                return pp;
            }
        }
        return nullptr;
    }
};

class PPRIO: public scheduler
{
    public:
    int mx_prio;
    vector <deque<process*>> running;
    vector<deque<process*>> expired;

    PPRIO(int m)
    {
        mx_prio = m;
        for (int i = 0;i<=mx_prio;i++)
        {
            running.push_back(deque <process*>());
            expired.push_back(deque <process*>());
        }
    }

     void add_process(process *p)
    {
        if (p->dynamic_p == -1)
        {
            p->dynamic_p = p->static_p - 1;
            if (vflag) {cout<<"psuhed process:"<<p->id<<" to expired q with new prio:"<<p->dynamic_p<<endl;}
            expired[p->dynamic_p].push_back(p);
            return;
        }
        else
        {
            // cout<<p->dynamic_p<<endl;
            running[p->dynamic_p].push_back(p);
                    // cout<<"hey"<<endl;

        }
        
        return;
    }

     process * get_next_process()
    {
        int i;
        // cout<<"hi"<<endl;
        for (int j =mx_prio;j>=0;j--)
        {
            if (!running[j].empty())
            {
                i=1;
                break;
            }
            else
            {
                i=0;
            }
        }
        if (i==0)
        {
            running.swap(expired);
            if (vflag) {cout<<"swapped expired and running"<<endl; }
        }
        for (int j=mx_prio;j>=0;j--)
        {
            if (!running[j].empty())
            {
                process * pp = running[j].front();
                if(vflag) cout<<"returned process: "<<pp->id<<endl;
                running[j].pop_front();
                return pp;
            }
        }

        return nullptr;
    }
    
};
// CALLED FROM MAIN
scheduler * get_scheduler_object(char schedu, int input_quantum, int prio)
{
    scheduler * sched;
    switch (schedu)
    {
        case ('F'):
        {
            sched = new FIFO();
            cout<<"FCFS"<<endl;
            break;
        }
        case('L'):
        {
            sched = new LCFS();
            cout<<"LCFS"<<endl;
            break;
        }
        case('R'):
        {
            sched = new RR();
            cout<<"RR"<<" "<<input_quantum<<endl;
            break;
        }
        case('E'):
        {
           sched = new PPRIO(prio);
            cout<<"PREPRIO"<<" "<<input_quantum<<endl;
            preprio = true;
            break;
        }
        case('S'):
        {
            sched = new SRTF();
            cout<<"SRTF"<<endl;
            break;
        }
        case ('P'):
        {
            sched = new PRIO(prio);
            cout<<"PRIO"<<" "<<input_quantum<<endl;
            break;
        }
    }
    return sched;
}

/////////////////////////////////////////////////////////////DES and its functions ////////////////////////////////////////////////////////////////////////////

int get_next_event_time()
{
    
    if (event_q.empty())
    {
        return -1;
    }
    return event_q.front()->timestamp;
}
process * current_running_process;
void add_to_eventq(event* e)
{
    auto it = event_q.begin();
    while (it!=event_q.end())
    {
        if ((*it)->timestamp > e->timestamp )
        {
            event_q.insert(it, e);
            return;
        }
        it++;
    }
    event_q.push_back(e);
    if (vflag) cout<<"PUSHED AT END "<<e->trans<<":"<<e->proc->id<<": timestamp "<<e->timestamp<<endl;
    return;
}


void simulation(scheduler * scheduler)
{
    event* evt;
    bool pre_empt = false;
    
    while( !event_q.empty() ) 
    {
        
        evt = event_q.front();
        event_q.pop_front();      
        process *current_process = new process();
        current_process = evt->proc; // this is the process the event works on
        current_time = evt->timestamp;
        current_process->time_spent_in_state = current_time - current_process->state_ts;
        current_process->state_ts = current_time;

        switch (evt->trans) 
        { 
            // which state to transition to?
            pre_empt =  false;
            
            case TRANS_TO_READY:
            {
                if (vflag) cout<<"PROCESS: "<<evt->proc->id<<" AT TIME: "<<current_time<<" in trans-to-ready"<<" rem_time: "<<evt->proc->total_cpu<<" prio:"<<evt->proc->dynamic_p<<endl;
                if (evt->proc->current_state == STATE_BLOCKED)
                { 
                    //check if pre-empted
                    count_of_blocked--;
                    if (count_of_blocked == 0)
                    {
                        total_io_util = total_io_util + current_time - start_time;
                    }
                    evt->proc->dynamic_p = current_process->static_p -  1;
                    evt->proc->previous_state = STATE_BLOCKED;
                    evt->proc->current_state = STATE_READY;

                }
                evt->proc->state_ts = current_time;
                evt->proc->current_state = STATE_READY;
                evt->proc->state_ts = current_time;
                scheduler->add_process(evt->proc);
                // cout<<evt->proc->time_spent_in_state<<endl;
                // must come from BLOCKED or from PREEMPTION
                // must add to run queue
                invoke_scheduler = true; // conditional on whether something is run
                break;
            }
            case TRANS_TO_RUNNING:
            {  
                int burst;
                if (evt->proc->current_state ==STATE_READY)
                {
                evt->proc->cpu_wait_time += evt->proc->time_spent_in_state;
                }   
                // evt->proc->cpu_wait_time += evt->proc->time_spent_in_state;
                if (evt->proc->burst == 0)
                {
                    // get new burst
                    // cout<<evt->proc->cpu_burst<<endl;
                    burst = get_random(evt->proc->cpu_burst);
                    // cout<<"got cpu burst:" <<burst<<endl;
                    evt->proc->burst = burst;
                }
                else
                {
                    burst = evt->proc->burst;
                }
                if (vflag) cout<<"PROCESS: "<<evt->proc->id<<" AT TIME: "<<current_time<<" in trans-to-run"<<" burst: "<<burst<<" rem_time:"<<evt->proc->total_cpu<<" prio:"<<evt->proc->dynamic_p<<endl;

                if (burst <= evt->proc->total_cpu)
                {
                    // total rem time is more than burst
                    if (burst > evt->proc->input_quantum)
                    {    // burst more than input_quantum have to pre-empt
                        pre_empt = true;
                        burst = evt->proc->input_quantum;
                        
                    }
                }
                else if (burst>evt->proc->total_cpu)
                {
                    burst = evt->proc->total_cpu;  

                    if (burst>evt->proc->input_quantum)
                    {
                        pre_empt = true;
                        burst = evt->proc->input_quantum;
                    }                  
                }
               
                evt->proc->burst -= burst;   
                evt->proc->total_cpu -= burst;
                event *ee = new event();
                ee->proc = evt->proc;
                ee->timestamp = current_time + burst;
                if (vflag) cout<<"PROCESS: "<<evt->proc->id<<" AT TIME: "<<current_time<<" in trans-to-run"<<" burst: "<<burst<<" rem_time:"<<evt->proc->total_cpu<<" prio:"<<evt->proc->dynamic_p<<endl;
                if (ee->proc->total_cpu > 0)
                {
                    if (pre_empt)
                    {
                        ee->proc->current_state = STATE_RUNNING;
                        ee->trans = TRANS_TO_PREEMPT;
                    }
                    else
                    {
                        ee->proc->current_state = STATE_RUNNING;
                        ee->trans = TRANS_TO_BLOCKED;
                    }
                }
                else if (ee->proc->total_cpu == 0)
                {
                    ee->proc->current_state = STATE_RUNNING;
                    ee->trans = TRANS_TO_FINISHED;
                }
                total_cpu_util += burst;
                add_to_eventq(ee);
                
                // create event for either preemption or blocking
                break;
            }
            case TRANS_TO_BLOCKED:
            {               
                //create an event for when process becomes READY again
                int io_b;
                io_b = get_random(evt->proc->io_burst);
                
                if (vflag) cout<<"PROCESS: "<<evt->proc->id<<" AT TIME: "<<current_time<<" in trans-to-blocked for time: "<<io_b<<endl;
                
                evt->proc->previous_state = evt->proc->current_state;
                evt->proc->current_state = STATE_BLOCKED;
                evt->timestamp = current_time + io_b;
                evt->trans = TRANS_TO_READY;
                if (count_of_blocked == 0)
                {
                    start_time = current_time;
                }
                count_of_blocked++;
                evt->proc->io_wait_time += io_b;
                add_to_eventq(evt);
                invoke_scheduler = true;
                current_running_process = nullptr;
                break;
            }
            case TRANS_TO_PREEMPT:
            {
                // add to runqueue (no event is generated)
                pre_empt = false;
                if (vflag) cout<<"PROCESS: "<<evt->proc->id<<" AT TIME: "<<current_time<<" in trans-to-preempt with prio ="<<evt->proc->dynamic_p<<endl;
                evt->proc->current_state =  STATE_PREEMPT;
                evt->proc->dynamic_p = evt->proc->dynamic_p - 1;
                scheduler->add_process(evt->proc);
                current_running_process = nullptr;
                invoke_scheduler = true;
                break;
            }
            case TRANS_TO_FINISHED:
            {
                if (vflag)
                { cout<<"PROCESS FINISHED: "<<evt->proc->id<<" AT TIME: "<<current_time<<endl; }
                evt->proc->finishing_time = current_time;
                invoke_scheduler = true;
                current_running_process = nullptr; 
                finish_time = current_time;  
                break;
            }
        }
        // remove current event object from Memory
        // delete evt; 
        if (invoke_scheduler)
        {
            if (preprio)
            {
                
                if (current_running_process!=nullptr)
                {
                    if (evt->proc->current_state == STATE_READY && evt->proc->dynamic_p > current_running_process->dynamic_p)
                    {
                        auto it = event_q.begin();
                        int time;
                        while (it!=event_q.end())
                        {
                            if ((*it)->proc->id == current_running_process->id)
                            {
                                time = (*it)->timestamp;
                                if (time!=current_time)
                                {
                                    event_q.erase(it);
                                }
                                break;
                            }
                            it++;
                        }
                        if (time!=current_time)
                        {
                            current_running_process->total_cpu = current_running_process->total_cpu +   (time - current_time);
                            current_running_process->burst += (time -current_time);
                            current_running_process->state_ts = current_time - evt->proc->state_ts;
                            event *eee = new event();
                            eee->proc = current_running_process;
                            eee->proc->prem = true;
                            eee->trans = TRANS_TO_PREEMPT;
                            eee->timestamp = current_time;
                            total_cpu_util-=(time -current_time);
                            add_to_eventq(eee);
                            if (vflag) cout<<"------------>"<<current_running_process->id<<" preempted by:"<<evt->proc->id<<endl;
                            current_running_process=nullptr;
                        }
                    }
                }
                
            }
            
            if (get_next_event_time() == current_time) 
            { 
                continue;
            }
            //process next event from Event queue
            invoke_scheduler = false; // reset global flag
            if (current_running_process== nullptr) 
            {                
                current_running_process = scheduler->get_next_process();
                if (current_running_process == nullptr) 
                {                
                    current_running_process = nullptr;
                    continue;
                }
                event *e = new event();
                e->proc = current_running_process;
                e->proc->previous_state = e->proc->current_state;
                e->proc->current_state = STATE_READY;
                e->trans = TRANS_TO_RUNNING;
                e->timestamp = current_time;
                add_to_eventq(e);    
            }
        }
    }
    return;
}
///////////////////////////////////////////////////////DES ENDS/////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////// HELPER FUNCTIONS /////////////////////////////////////////////////////////////////////

void print_completed_process_summary(process * p)
{

    printf("%04d: %4d %4d %4d %4d %1d | %5d %5d %5d %5d\n", p->id,p->at, p->total_cpu_o,p->cpu_burst, p->io_burst, p->static_p,p->finishing_time,p->finishing_time-p->at, p->io_wait_time, p->cpu_wait_time );
    total_cpu_wait += p->cpu_wait_time;
    
    total_tat+=p->finishing_time-p->at;
    // cout<<total_tat<<"::"<<p->turn_around_time;;
    return;
}

void display_process_output()
{
    auto it = process_q.begin();
    while (it != process_q.end())
    {
        print_completed_process_summary(*(it));
        it++;
    }
    return;
}
void display_cpu_stats()
{
    
    double avg_cpuwait = double(total_cpu_wait) / process_q.size();
    double avg_tat = double(total_tat / process_q.size());
    double io_util = double(total_io_util/finish_time) * 100;
    // cout<<"total io util"<<total_io_util;
    double cpu_util = double(total_cpu_util/finish_time) * 100;
    throughput = (double(process_q.size()) / finish_time) * 100;  
    printf("SUM: %d %.2lf %.2lf %.2lf %.2lf %.3lf\n", finish_time, cpu_util, io_util, avg_tat, avg_cpuwait, throughput);  
    return;
}

void create_event(process* p)
{
    event * e=new event();
    e->proc=p;
    e->timestamp=p->at;
    e->proc->previous_state=STATE_CREATED;
    e->proc->current_state=STATE_READY;
    e->trans=TRANS_TO_READY;
    add_to_eventq(e);
    return;
}

int get_random(int burst) 
{    
    if (ofs==num_random)
    {
        ofs=0;
    }
    // cout<<"offset: "<<ofs<<endl;
    return 1 + (random_list[ofs++] % burst); 
}
void create_process(int at, int tt, int cb, int ib, int input_quantum, int prio)
{
    process * p=new process();
    p->at=int(at);
    p->total_cpu=tt;
    p->total_cpu_o=tt;
    p->cpu_burst=cb;
    p->io_burst=ib;
    p->id=count++;
    p->burst = 0;
    p->static_p = get_random(prio);
    p->dynamic_p = p->static_p - 1;
    p->input_quantum = input_quantum;
    // cout<<"process input_quantum"<<p->input_quantum;
    process_q.push_back(p);
    create_event(p);
    return;
}

void print_processes()
{
    std::deque<process *>::iterator it = process_q.begin();
    for (;it!=process_q.end();it++)
    {
        cout<<(*it)->at<<" "<<(*it)->total_cpu<<" "<<(*it)->cpu_burst<<" "<<(*it)->io_burst<<endl;

    }
    return;
}

void initialise_random_list(string rfile_path) //pushing random numbers into a list
{
    ifstream rfile;
    rfile.open(rfile_path);
    if (!rfile.is_open())
    {
        cout<<"Random File could not be opened"<<endl;
        return;
    }
    string line;
    getline(rfile, line);
    num_random=stoi(line);
    while (getline(rfile, line))
    {
        random_list.push_back(stoi(line));
    }
    return;
}


int main(int argc, char* argv[])
{
    vflag= false;
    int tflag=0;
    int eflag=0;
    char *cvalue=NULL;
    int index;
    int c;
    char sched;
    
    //reading command line arguments for -v -t -e and -s
    int prio=-1; //default value, will be overridden is prio is provided
    int input_quantum=-1;
    opterr = 0;
    while ((c=getopt(argc,argv,"vtes:"))!=-1)
        switch(c)
        {
        case 'v':
            vflag=true;
            break;
        case 't':
            tflag=1;
            break;
        case 'e':
            tflag=1;
            break;
        case 's':
            cvalue=optarg;
            sscanf(cvalue,"%c %d : %d", &sched, &input_quantum, &prio);
            break;
        default:
            abort();
      }
    
    if (vflag) { cout<<"Default values of quantum and prio set as 10000 and 4"<<endl;}
    if (input_quantum<0)
    {
        input_quantum = 10000;
    }
    if (prio<0)
    {
        prio = 4;
    }
    
    scheduler * s=get_scheduler_object(sched, input_quantum, prio);
    
    //reading input file and random file
    index = optind;
    string file(argv[index]);
    index++;
    string rfile_path(argv[index]);
    string line;
    ifstream myfile(file);
    
    initialise_random_list(rfile_path);
    
    //creating pcb objects from input file
    int at;
    int tt;
    int ib;
    int cb;
    if (!myfile.is_open())
    {
        cout<<"Input File could not be opened"<<endl;
    }
    else if (myfile.is_open())
    {
        
        while (getline(myfile,line))
        {
            if (line.length() == 0)
            {
                break;
            }
            char* c = strcpy(new char[line.length() + 1], line.c_str());
            sscanf(c, "%d %d %d %d",&at, &tt, &cb, &ib);
            create_process(at,tt,cb,ib,input_quantum, prio);
        }
        myfile.close();
    
    // print_processes();

    simulation(s);

    display_process_output();

    display_cpu_stats();
    }

    return 0;
}
