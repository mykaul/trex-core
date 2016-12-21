/*
 Itay Marom
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2015-2015 Cisco Systems, Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#ifndef __TREX_WATCHDOG_H__
#define __TREX_WATCHDOG_H__

#include <string>
#include <vector>
#include <thread>
#include <mutex>

#include "mbuf.h"
#include "os_time.h"

/**
 * every thread creates its own monitor from its own memory
 * 
 * @author imarom (19-Jun-16)
 */
class TrexMonitor {
    friend class TrexWatchDog;
    
public:

    /**
    * create a monitor 
    * 
    * @author imarom (31-May-16)
    * 
    * @param name 
    * @param timeout 
    * 
    * @return int 
    */
    void create(const std::string &name, double timeout_sec);

    /**
     * disable the monitor for 'time_sec' 
     * by default it will disable it for a long period of time 
     * (forever) 
     * 
     */
    void disable(dsec_t time_sec = 1e9) {
        /* double writes are atomic on x86_64 (aligned to 8 bytes) */
        m_active_time_sec = now_sec() + time_sec;
    }

    /**
     * re-enable a monitor after it was disabled
     * 
     */
    void enable() {
        /* before enabling - must tickle o.w the watchdog might crash this thread */
        tickle();
        /* memory fence - make sure the main thread sees this by order */
        asm volatile("mfence" ::: "memory");
        m_active_time_sec = now_sec();
    }
    
    /**
     * tickle the monitor - this should be called from the thread 
     * to avoid the watchdog from detecting a stuck thread 
     * 
     * @author imarom (19-Jun-16)
     */
    void tickle() {
        /* to avoid useless writes - first check */
        if (!m_tickled) {
            m_tickled = true;
        }
    }

    const std::string &get_name() const {
        return m_name;
    }
    
    /* return how much time has passed since last tickle */
    dsec_t get_interval(dsec_t now) const {
        return (now - m_ts);
    }


    dsec_t get_timeout_sec() const {
        return m_timeout_sec;
    }


private:

    /**
     * called by the watchdog to reset the monitor for a new round
     * 
     */
    void reset(dsec_t now) {
        m_tickled = false;
        m_ts      = now;
    }

   
    pthread_t get_tid() const {
        return m_tid;
    }

   
    volatile bool is_active(dsec_t now) const {
        return ( (now - m_active_time_sec) > 0 );
    }

    volatile bool is_tickled() const {
        return m_tickled;
    }

    bool is_expired(dsec_t now) const {
        return ( get_interval(now) > m_timeout_sec );
    }


    /* write fields are first */
    volatile dsec_t  m_active_time_sec;
    volatile bool    m_tickled;
    int              m_handle;
    dsec_t           m_ts;
    double           m_timeout_sec;
    pthread_t        m_tid;
    std::string      m_name;


} __rte_cache_aligned;


/**
 * a watchdog is a list of registered monitors
 * 
 * @author imarom (19-Jun-16)
 */
class TrexWatchDog {
public:

    /**
     * singleton entry
     * 
     * @author imarom (19-Jun-16)
     * 
     * @return TrexWatchDog& 
     */
    static TrexWatchDog& getInstance() {
        static TrexWatchDog instance;

        return instance;
    }

    void init(bool enable);
   
    /**
     * add a monitor to the watchdog 
     * from now on this monitor will be watched 
     * 
     * @author imarom (19-Jun-16)
     * 
     * @param monitor - a pointer to the object
     * 
     */
    void register_monitor(TrexMonitor *monitor);


    /**
     * start the watchdog
     * 
     */
    void start();


    /**
     * stop the watchdog
     * 
     */
    void stop();


private:

    TrexWatchDog() {
        m_thread        = NULL;
        m_enable        = false;
        m_active        = false;
        m_mon_count     = 0;
    }

    void register_signal();
    void _main();

    static const int           MAX_MONITORS = 100;
    TrexMonitor               *m_monitors[MAX_MONITORS];
    volatile int               m_mon_count;
    std::mutex                 m_lock;

    bool                       m_enable;
    volatile bool              m_active;
    std::thread               *m_thread;

    static bool                g_signal_init;
};


#endif /* __TREX_WATCHDOG_H__ */
