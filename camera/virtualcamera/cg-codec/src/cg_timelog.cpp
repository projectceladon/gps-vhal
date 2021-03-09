/*
** Copyright 2018 Intel Corporation
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/syscall.h>

// #include <cutils/properties.h>
#include "cg_log.h"

#include "cg_timelog.h"

#define gettid() syscall(__NR_gettid)

#undef LOG_TAG
#define LOG_TAG "IRR_TimeLog"

using namespace std;

TimeLog::TimeLog(const char* name, int mode, unsigned long idx1, unsigned long idx2) {
#if IRR_TIME_LOG
    m_enter_name = name;
    m_begin_name = nullptr;

    m_mode = mode;
    m_idx1 = idx1;
    m_idx2 = idx2;

    if (!mode) {
        pid_t pid = getpid();
        pid_t tid = gettid();
        gettimeofday(&m_enter, NULL);
        long long timestamp = (long long)(m_enter.tv_sec) * 1000000 + (long long)m_enter.tv_usec;
#if IRR_TIME_LOG_VERBOSE
        ALOGI("pid = %d, tid = %d : %s : enter : idx1 = %ld, idx2 = %ld, timestamp = %lld us, %s",
              pid, tid, m_enter_name, m_idx1, m_idx2, timestamp, ctime(&(m_enter.tv_sec)));
#else
        ALOGI("pid = %d, tid = %d : %s : enter : idx1 = %ld, idx2 = %ld, timestamp = %lld us", pid,
              tid, m_enter_name, m_idx1, m_idx2, timestamp);
#endif
    }
#endif
}

TimeLog::~TimeLog() {
#if IRR_TIME_LOG
    if (!m_mode) {
        gettimeofday(&m_exit, NULL);
        long long timestamp = (long long)(m_exit.tv_sec) * 1000000 + (long long)m_exit.tv_usec;
        long long timestamp_prev =
            (long long)(m_enter.tv_sec) * 1000000 + (long long)m_enter.tv_usec;
        pid_t pid = getpid();
        pid_t tid = gettid();
#if IRR_TIME_LOG_VERBOSE
        ALOGI(
            "pid = %d, tid = %d : %s : exit : idx1 = %ld, idx2 = %ld, timestamp = %lld us, "
            "diff2enter = %lld us, %s",
            pid, tid, m_enter_name, m_idx1, m_idx2, timestamp, timestamp - timestamp_prev,
            ctime(&(m_exit.tv_sec)));
#else
        ALOGI("pid = %d, tid = %d : exit : idx1 = %ld, idx2 = %ld, timestamp = %lld us", pid, tid,
              m_enter_name, m_idx1, m_idx2, timestamp);
#endif
    }
#endif
}

void TimeLog::begin(const char* name, unsigned long idx1, unsigned long idx2) {
#if IRR_TIME_LOG
    m_begin_name = name;
    m_idx1 = idx1;
    m_idx2 = idx2;
    pid_t pid = getpid();
    pid_t tid = gettid();
    gettimeofday(&m_begin, NULL);
    long long timestamp = (long long)(m_begin.tv_sec) * 1000000 + (long long)m_begin.tv_usec;
#if IRR_TIME_LOG_VERBOSE
    ALOGI("pid = %d, tid = %d : %s : begin : idx1 = %ld, idx2 = %ld, timestamp = %lld us, %s", pid,
          tid, m_begin_name, m_idx1, m_idx2, timestamp, ctime(&(m_begin.tv_sec)));
#else
    ALOGI("pid = %d, tid = %d : %s : begin : idx1 = %ld, idx2 = %ld, timestamp = %lld us", pid, tid,
          m_begin_name, m_idx1, m_idx2, timestamp);
#endif
#endif
}

void TimeLog::end() {
#if IRR_TIME_LOG
    gettimeofday(&m_end, NULL);
    long long timestamp = (long long)(m_end.tv_sec) * 1000000 + (long long)m_end.tv_usec;
    long long timestamp_prev = (long long)(m_begin.tv_sec) * 1000000 + (long long)m_begin.tv_usec;
    pid_t pid = getpid();
    pid_t tid = gettid();
#if IRR_TIME_LOG_VERBOSE
    ALOGI(
        "pid = %d, tid = %d : %s : end : idx1 = %ld, idx2 = %ld, timestamp = %lld us, diff2begin = "
        "%lld us, %s",
        pid, tid, m_begin_name, m_idx1, m_idx2, timestamp, timestamp - timestamp_prev,
        ctime(&(m_end.tv_sec)));
#else
    ALOGI("pid = %d, tid = %d : %s : end : idx1 = %ld, idx2 = %ld, timestamp = %lld us", pid, tid,
          m_begin_name, m_idx1, m_idx2, timestamp);
#endif
#endif
}
