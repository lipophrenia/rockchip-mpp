/*
 * Copyright 2015 Rockchip Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * File         : mpp_thread.h
 * Description  : thread library for different OS
 * Author       : herman.chen@rock-chips.com
 * Date         : 9:47 2015/7/27
 */

#ifndef __MPP_THREAD_H__
#define __MPP_THREAD_H__

#if defined(_WIN32) && !defined(__MINGW32CE__)

/*
 * NOTE: POSIX Threads for Win32
 * Downloaded from http://www.sourceware.org/pthreads-win32/
 */
#include "semaphore.h"
#include "pthread.h"
#pragma comment(lib, "pthreadVC2.lib")

/*
 * add pthread_setname_np for windows
 */
int pthread_setname_np(pthread_t thread, const char *name);

#else

#include <unistd.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/time.h>

#ifndef PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP
#define PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP PTHREAD_RECURSIVE_MUTEX_INITIALIZER
#endif

#endif

#define THREAD_NAME_LEN 16

typedef void *(*MppThreadFunc)(void *);

typedef enum {
    MPP_THREAD_UNINITED,
    MPP_THREAD_READY,
    MPP_THREAD_RUNNING,
    MPP_THREAD_WAITING,
    MPP_THREAD_STOPPING,
} MppThreadStatus;

#ifdef __cplusplus

#include "mpp_debug.h"

class Mutex;
class Condition;

/*
 * for shorter type name and function name
 */
class Mutex
{
public:
    Mutex();
    ~Mutex();

    void lock();
    void unlock();
    int  trylock();

    class Autolock
    {
    public:
        inline Autolock(Mutex* mutex, RK_U32 enable = 1) :
            mEnabled(enable),
            mLock(mutex) {
            if (mLock && mEnabled)
                mLock->lock();
        }
        inline ~Autolock() {
            if (mLock && mEnabled)
                mLock->unlock();
        }
    private:
        RK_S32 mEnabled;
        Mutex *mLock;
    };

private:
    friend class Condition;

    pthread_mutex_t mMutex;

    Mutex(const Mutex &);
    Mutex &operator = (const Mutex&);
};

inline Mutex::Mutex()
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&mMutex, &attr);
    pthread_mutexattr_destroy(&attr);
}
inline Mutex::~Mutex()
{
    pthread_mutex_destroy(&mMutex);
}
inline void Mutex::lock()
{
    pthread_mutex_lock(&mMutex);
}
inline void Mutex::unlock()
{
    pthread_mutex_unlock(&mMutex);
}
inline int Mutex::trylock()
{
    return pthread_mutex_trylock(&mMutex);
}

typedef Mutex::Autolock AutoMutex;


/*
 * for shorter type name and function name
 */
class Condition
{
public:
    Condition();
    Condition(int type);
    ~Condition();
    RK_S32 wait(Mutex& mutex);
    RK_S32 wait(Mutex* mutex);
    RK_S32 timedwait(Mutex& mutex, RK_S64 timeout);
    RK_S32 timedwait(Mutex* mutex, RK_S64 timeout);
    RK_S32 signal();
    RK_S32 broadcast();

private:
    pthread_cond_t mCond;
};

inline Condition::Condition()
{
    pthread_cond_init(&mCond, NULL);
}
inline Condition::~Condition()
{
    pthread_cond_destroy(&mCond);
}
inline RK_S32 Condition::wait(Mutex& mutex)
{
    return pthread_cond_wait(&mCond, &mutex.mMutex);
}
inline RK_S32 Condition::wait(Mutex* mutex)
{
    return pthread_cond_wait(&mCond, &mutex->mMutex);
}
inline RK_S32 Condition::timedwait(Mutex& mutex, RK_S64 timeout)
{
    return timedwait(&mutex, timeout);
}
inline RK_S32 Condition::timedwait(Mutex* mutex, RK_S64 timeout)
{
    struct timespec ts;

    clock_gettime(CLOCK_REALTIME_COARSE, &ts);

    ts.tv_sec += timeout / 1000;
    ts.tv_nsec += (timeout % 1000) * 1000000;
    /* Prevent the out of range at nanoseconds field */
    ts.tv_sec += ts.tv_nsec / 1000000000;
    ts.tv_nsec %= 1000000000;

    return pthread_cond_timedwait(&mCond, &mutex->mMutex, &ts);
}
inline RK_S32 Condition::signal()
{
    return pthread_cond_signal(&mCond);
}
inline RK_S32 Condition::broadcast()
{
    return pthread_cond_broadcast(&mCond);
}

class MppMutexCond
{
public:
    MppMutexCond() {};
    ~MppMutexCond() {};

    void    lock()      { mLock.lock(); }
    void    unlock()    { mLock.unlock(); }
    int     trylock()   { return mLock.trylock(); }
    void    wait()      { mCondition.wait(mLock); }
    RK_S32  wait(RK_S64 timeout) { return mCondition.timedwait(mLock, timeout); }
    void    signal()    { mCondition.signal(); }
    void    broadcast() { mCondition.broadcast(); }
    Mutex   *mutex()    { return &mLock; }

private:
    Mutex           mLock;
    Condition       mCondition;
};

// Thread lock / signal is distinguished by its source
typedef enum MppThreadSignal_e {
    THREAD_WORK,        // for working loop
    THREAD_INPUT,       // for thread input
    THREAD_OUTPUT,      // for thread output
    THREAD_CONTROL,     // for thread async control (reset)
    THREAD_SIGNAL_BUTT,
} MppThreadSignal;

#define THREAD_NORMAL       0
#define THRE       0

class MppThread
{
public:
    MppThread(MppThreadFunc func, void *ctx, const char *name = NULL);
    ~MppThread() {};

    MppThreadStatus get_status(MppThreadSignal id = THREAD_WORK);
    void set_status(MppThreadStatus status, MppThreadSignal id = THREAD_WORK);
    void dump_status();

    void start();
    void stop();

    void lock(MppThreadSignal id = THREAD_WORK) {
        mpp_assert(id < THREAD_SIGNAL_BUTT);
        mMutexCond[id].lock();
    }

    void unlock(MppThreadSignal id = THREAD_WORK) {
        mpp_assert(id < THREAD_SIGNAL_BUTT);
        mMutexCond[id].unlock();
    }

    void wait(MppThreadSignal id = THREAD_WORK) {
        mpp_assert(id < THREAD_SIGNAL_BUTT);
        MppThreadStatus status = mStatus[id];

        mStatus[id] = MPP_THREAD_WAITING;
        mMutexCond[id].wait();

        // check the status is not changed then restore status
        if (mStatus[id] == MPP_THREAD_WAITING)
            mStatus[id] = status;
    }

    void signal(MppThreadSignal id = THREAD_WORK) {
        mpp_assert(id < THREAD_SIGNAL_BUTT);
        mMutexCond[id].signal();
    }

    Mutex *mutex(MppThreadSignal id = THREAD_WORK) {
        mpp_assert(id < THREAD_SIGNAL_BUTT);
        return mMutexCond[id].mutex();
    }

private:
    pthread_t       mThread;
    MppMutexCond    mMutexCond[THREAD_SIGNAL_BUTT];
    MppThreadStatus mStatus[THREAD_SIGNAL_BUTT];

    MppThreadFunc   mFunction;
    char            mName[THREAD_NAME_LEN];
    void            *mContext;

    MppThread();
    MppThread(const MppThread &);
    MppThread &operator=(const MppThread &);
};

#endif

/*
 * status transaction:
 *                  new
 *                   v
 *           MPP_THREAD_UNINITED
 *                   v
 *                 setup
 *                   v
 * delete <-  MPP_THREAD_READY  <-------------------+
 *                   v                              |
 *                 start                            |
 *                   v                              |
 *           MPP_THREAD_RUNNING -> stop -> MPP_THREAD_STOPPING
 *                   v                              |
 *                 wait                             |
 *                   v                              |
 *           MPP_THREAD_WAITING -> stop ------------+
 *
 */
typedef enum MppSThdStatus_e {
    MPP_STHD_UNINITED,
    MPP_STHD_READY,
    MPP_STHD_RUNNING,
    MPP_STHD_WAITING,
    MPP_STHD_STOPPING,
    MPP_STHD_BUTT,
} MppSThdStatus;

/* MppSThd for Mpp Simple Thread */
typedef void* MppSThd;
typedef void* MppSThdGrp;

typedef struct MppSThdCtx_t {
    MppSThd     thd;
    void        *ctx;
} MppSThdCtx;

typedef void *(*MppSThdFunc)(MppSThdCtx *);

MppSThd mpp_sthd_get(const char *name);
void mpp_sthd_put(MppSThd thd);

MppSThdStatus mpp_sthd_get_status(MppSThd thd);
const char* mpp_sthd_get_name(MppSThd thd);
RK_S32 mpp_sthd_get_idx(MppSThd thd);
RK_S32 mpp_sthd_check(MppSThd thd);

void mpp_sthd_setup(MppSThd thd, MppSThdFunc func, void *ctx);

void mpp_sthd_start(MppSThd thd);
void mpp_sthd_stop(MppSThd thd);
void mpp_sthd_stop_sync(MppSThd thd);

void mpp_sthd_lock(MppSThd thd);
void mpp_sthd_unlock(MppSThd thd);
int  mpp_sthd_trylock(MppSThd thd);

void mpp_sthd_wait(MppSThd thd);
void mpp_sthd_signal(MppSThd thd);
void mpp_sthd_broadcast(MppSThd thd);

/* multi-thread group with same callback and context */
MppSThdGrp mpp_sthd_grp_get(const char *name, RK_S32 count);
void mpp_sthd_grp_put(MppSThdGrp grp);

void mpp_sthd_grp_setup(MppSThdGrp grp, MppSThdFunc func, void *ctx);
MppSThd mpp_sthd_grp_get_each(MppSThdGrp grp, RK_S32 idx);

void mpp_sthd_grp_start(MppSThdGrp grp);
void mpp_sthd_grp_stop(MppSThdGrp grp);
void mpp_sthd_grp_stop_sync(MppSThdGrp grp);

#endif /*__MPP_THREAD_H__*/
