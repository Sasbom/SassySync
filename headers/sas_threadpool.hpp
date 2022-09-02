#include <iostream>
#include <mutex>
#include <thread>
#include <vector>
#include <functional>
#include <condition_variable>
#include <queue>
#include <chrono>

#pragma once

#ifndef THR_HEADER
#define THR_HEADER

//heavily adapted from
//https://stackoverflow.com/questions/15752659/thread-pooling-in-c11

//variadic template forwarding from:
//https://en.cppreference.com/w/cpp/utility/forward
//https://stackoverflow.com/questions/6486432/variadic-template-templates-and-perfect-forwarding 

class threadPool
{
public:
    int max_threads = -1; //when not defined, allow for as many threads as possible.

    void Run();
    void addJob(std::function<void()> const& job);
    void Stop();
    bool isBusy();
    void waitForAll();

    threadPool(){};
    threadPool(size_t const& _max_threads): max_threads(_max_threads){};

    template<typename ... Args>
    void addJob_bind(Args&&... job_args);

    ~threadPool(){
        waitForAll();
        Stop();
        };

private:
    void threadLoop(int const& i);

    bool terminate_execution = false; // when threads ordered to stop, stop looking for jobs
    std::mutex queue_mutex; // makes sure that job queues are safely written to
    std::condition_variable mutex_condition; //allows thread to wait
    std::vector<std::thread> threads; //hold the threads.
    std::queue<std::function<void()>> job_queue;
    std::vector<bool> work;
};

inline void threadPool::Run()
{
    //support for custom amount of threads but cap at max capacity.
    uint32_t num_threads;
    auto concur_max = std::thread::hardware_concurrency();

    if (max_threads <= 0 || max_threads > concur_max) 
        num_threads = concur_max;
    else 
        num_threads = std::move(max_threads);

    threads.resize(num_threads);
    work.resize(num_threads);

    for(uint32_t i = 0; i < num_threads; i++)
    {
        int t_id = i;
        threads.at(i) = std::thread([this, &t_id](){this->threadLoop(t_id);}); //start up each thread with a loop process.
    }
}

inline void threadPool::threadLoop(int const& i)
{
    int thread_id = i;
    work[thread_id] = false;
    
    while (true)
    {
        std::function<void()> job_fromqueue;

        {
            std::unique_lock<std::mutex> lck(queue_mutex);
            mutex_condition.wait(lck, [this] {return !job_queue.empty() || terminate_execution;}); //wait for lock to unlock, and check for execution, or the job queue being empty.
                                                                                                  //if the job queue is empty, the thread will keep waiting.
            if (terminate_execution) return; //kill

            job_fromqueue = job_queue.front();
            job_queue.pop(); // remove last element from job stack.
        }
        
        //std::cout << "started job on thread " << thread_id << "\n";
        work[thread_id] = true;
        job_fromqueue(); //when we've excited the scope where we wait and gotten the job, we can run it as a function.
        //std::cout << "finished job on thread " << thread_id << "\n";
        work[thread_id] = false;
    }
}

//Describe the job as a lambda: addJob(  []{ function(arg1,arg2,arg3);} );
//Anything you need to add here needs to be callable.
inline void threadPool::addJob(std::function<void()> const& job)
{
    {
        std::unique_lock<std::mutex> lck(queue_mutex);
        job_queue.push(job);
    }
    mutex_condition.notify_one(); //condition variable notifies one thread to unlock from waiting state, and it will run it's job.
}

//constructs a callable using std::bind and adds it as a job. addJob_bind(fucntion,arg1,arg2)
template<typename ... Args>
inline void threadPool::addJob_bind(Args&&... job_args)
{
    {
        std::unique_lock<std::mutex> lck(queue_mutex);
        auto job = std::bind(std::forward<Args>(job_args)...); //make a callable by forwarding the arguments of this function into std::bind.
        job_queue.push(job);
    }
    mutex_condition.notify_one(); //condition variable notifies one thread to unlock from waiting state, and it will run it's job.
}


inline bool threadPool::isBusy()
{
    bool busy = false;
    
    {
        std::unique_lock<std::mutex> lck(queue_mutex); //safely access job_queue with a mutex
        //go through all threads and break off if even one of m is still busy.
        if(job_queue.empty()==false) return true;

        int c = 0;
        for(std::thread& t : threads)
        {
            if(work[c]==true)
            {
                busy = true; 
                break;
            }
            c++;
        }
    }
    return busy;
}

inline void threadPool::Stop()
{
    //std::cout << "stopping\n";
    {
        std::unique_lock<std::mutex> lck(queue_mutex); //safely access job_queue with a mutex
        terminate_execution = true;
    }
    mutex_condition.notify_all(); //wake up all threads that are sleeping with the internal state in the class that they should stop running.

    //join all threads with current one (effectively closing them.)
    for(std::thread& t : threads)
    {
        t.join();
    }
    threads.clear();
}

inline void threadPool::waitForAll()
{
    //nooby spinlock.
    while(true)
    {
        //std::cout << isBusy();
        if (isBusy()==true) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        else break;
    }
}

#endif