#ifndef _H_THREADPOOL
#define _H_THREADPOOL

#include <pthread.h>
#include <deque>
#include <vector>
#include <sys/epoll.h>

const int STARTED = 0;
const int STOPPED = 1;

class Mutex {
    public:
        Mutex() {
            pthread_mutex_init(&_lock, NULL);
            _is_locked = false;
        }
        ~Mutex() {
            while(_is_locked);
            unlock(); // Unlock Mutex after shared resource is safe
            pthread_mutex_destroy(&_lock);
        }
        void lock() {
            pthread_mutex_lock(&_lock);
            _is_locked = true;
        }
        void unlock() {
            _is_locked = false; // do it BEFORE unlocking to avoid race condition
            pthread_mutex_unlock(&_lock);
        }
        pthread_mutex_t* get_mutex_ptr() {
            return &_lock;
        }
    private:
        pthread_mutex_t _lock;
        volatile bool _is_locked;
};

class CondVar {
    public:
        CondVar() { pthread_cond_init(&m_cond_var, NULL); }
        ~CondVar() { pthread_cond_destroy(&m_cond_var); }
        void wait(pthread_mutex_t* mutex) {pthread_cond_wait(&m_cond_var, mutex); }
        void signal() { pthread_cond_signal(&m_cond_var); }
        void broadcast() { pthread_cond_broadcast(&m_cond_var); }
    private:
        pthread_cond_t m_cond_var;
};

class Task {
    public: 
        Task(void (*fn_ptr)(void*), void* arg); // pass a free function pointer
        ~Task();
        void run();
    private:
        void (*m_fn_ptr)(void*);
        void* m_arg;
};

typedef void (*ThreadStartCallback)();
typedef void (*ThreadExitCallback)();

class ThreadPool {
    public:
        ThreadPool();
        ~ThreadPool();
        int start();
        int start_threadpool();
        int destroy_threadpool();
        void* execute_thread();
        int add_task(Task *task);
        void set_thread_start_cb(ThreadStartCallback f);
        void set_thread_exit_cb(ThreadExitCallback f);
        void set_task_size_limit(int size);
        void set_pool_size(int pool_size);
        ThreadStartCallback m_scb;
        ThreadExitCallback m_exit_cb;
    private:
        int _pool_size;
        Mutex _task_mutex;
        CondVar _task_cond_var;
        std::vector<pthread_t> _threads; // storage for threads
        std::deque<Task *> _tasks;
        volatile int _pool_state;
        int _task_size_limit;
};

#endif /* _H_THREADPOOL */
