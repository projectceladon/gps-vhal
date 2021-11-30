/*
 * Copyright (C) 2020 The Android Open Source Project
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
// #define LOG_NDEBUG 0
#define LOG_NIDEBUG 0
#include "gnss_hw_conn.h"
#include <cutils/properties.h>
#include <cutils/sockets.h>
#include <fcntl.h>
#include <log/log.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include "gnss_hw_listener.h"

namespace {
constexpr char kCMD_QUIT = 'q';
constexpr char kCMD_START = 'a';
constexpr char kCMD_STOP = 'o';

int epollCtlAdd(int epollFd, int fd) {
    int ret;

    /* make the fd non-blocking */
    ret = TEMP_FAILURE_RETRY(fcntl(fd, F_GETFL));
    if (ret < 0) {
        return ret;
    }
    ret = TEMP_FAILURE_RETRY(fcntl(fd, F_SETFL, ret | O_NONBLOCK));
    if (ret < 0) {
        return ret;
    }

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = fd;

    return TEMP_FAILURE_RETRY(epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &ev));
}

int epollCtlRemove(int epollFd, int fd) {
    return TEMP_FAILURE_RETRY(epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, NULL));
}
}  // namespace

namespace ciccloud {

GnssHwConn::GnssHwConn(const DataSink* sink) {
    m_gsstLoopExit = false;
    m_gpsSocketServerFd.reset();

    m_tcpPort = 8766;
    char buf[PROPERTY_VALUE_MAX] = {
        '\0',
    };
    if (property_get("virtual.gps.tcp.port", buf, "") > 0) {
        m_tcpPort = atoi(buf);
    }
    ALOGI("Virtual gps will read with port '%u'", (unsigned int)m_tcpPort);

    m_needNotifyClientStart = 0;
    m_epollFd.reset(epoll_create1(0));
    if (!m_epollFd.ok()) {
        ALOGE("%s:%d: epoll_create1 failed", __PRETTY_FUNCTION__, __LINE__);
        return;
    }

    m_gpsSocketServerThread = std::thread([this]() { gpsSocketServerThread(this); });

    if (!::android::base::Socketpair(AF_LOCAL, SOCK_STREAM, 0,
                                     &m_callersFd, &m_threadsFd)) {
        ALOGE("%s:%d: Socketpair failed", __PRETTY_FUNCTION__, __LINE__);
        m_devFd.reset();
        return;
    }

    m_thread = std::thread([this, sink]() {
        sink->gnssStatus(ahg10::IGnssCallback::GnssStatusValue::ENGINE_ON);
        workerThread(this, sink);
        sink->gnssStatus(ahg10::IGnssCallback::GnssStatusValue::ENGINE_OFF);
    });
}

GnssHwConn::~GnssHwConn() {
    if (m_thread.joinable()) {
        sendWorkerThreadCommand(kCMD_QUIT);
        m_thread.join();
    }

    // kCMD_QUIT, P uses CMD_QUIT = 0. For compatibility, transfer kCMD_QUIT to CMD_QUIT.
    char cmd = 0;
    int ret = 0;
    if (m_clientFd.ok()) {
        ret = TEMP_FAILURE_RETRY(write(m_clientFd.get(), &cmd, 1));
        if (ret != 1)
            ALOGE("%s: could not notify client(%d) to quit: ret=%d: %s", __PRETTY_FUNCTION__, m_clientFd.get(), ret, strerror(errno));
        else
            ALOGI("%s Notify client(%d) to quit", __PRETTY_FUNCTION__, m_clientFd.get());
    } else {
        ALOGI("%s No client is connected. Do not need to send quit message.", __PRETTY_FUNCTION__);
    }

    m_gsstLoopExit = true;
    shutdown(m_gpsSocketServerFd.get(), SHUT_RDWR);
    m_gpsSocketServerFd.reset();
    shutdown(m_clientFd.get(), SHUT_RDWR);
    m_clientFd.reset();

    if (m_gpsSocketServerThread.joinable()) {
        m_gpsSocketServerThread.join();
    }
}

bool GnssHwConn::ok() const {
    return m_thread.joinable() && m_gpsSocketServerThread.joinable();
}

bool GnssHwConn::start() {
    // kCMD_START, P uses CMD_START = 1. For compatibility, transfer kCMD_START to CMD_START.
    char cmd = 1;
    int ret = 0;
    if (m_clientFd.ok()) {
        ret = TEMP_FAILURE_RETRY(write(m_clientFd.get(), &cmd, 1));
        if (ret != 1)
            ALOGE("%s: could not notify client(%d) to start: ret=%d: %s", __PRETTY_FUNCTION__, m_clientFd.get(), ret, strerror(errno));
        else
            ALOGV("%s Notify client(%d) to start", __PRETTY_FUNCTION__, m_clientFd.get());
    }
    m_needNotifyClientStart = true;

    return ok() && sendWorkerThreadCommand(kCMD_START);
}

bool GnssHwConn::stop() {
    // kCMD_STOP, P uses CMD_STOP = 2. For compatibility, transfer kCMD_STOP to CMD_STOP.
    char cmd = 2;
    int ret = 0;
    if (m_clientFd.ok()) {
        ret = TEMP_FAILURE_RETRY(write(m_clientFd.get(), &cmd, 1));
        if (ret != 1)
            ALOGE("%s: could not notify client(%d) to stop: ret=%d: %s", __PRETTY_FUNCTION__, m_clientFd.get(), ret, strerror(errno));
        else
            ALOGV("%s Notify client(%d) to stop", __PRETTY_FUNCTION__, m_clientFd.get());
    }
    m_needNotifyClientStart = false;

    return ok() && sendWorkerThreadCommand(kCMD_STOP);
}

void GnssHwConn::workerThread(void* paramGnssHwConn, const DataSink* sink) {
    GnssHwConn* pGnssHwConn = (GnssHwConn*)paramGnssHwConn;
    epollCtlAdd(pGnssHwConn->m_epollFd.get(), pGnssHwConn->m_threadsFd.get());

    GnssHwListener listener(sink);
    bool running = false;

    while (true) {
        struct epoll_event events[2];
        const int kTimeoutMs = 60000;
        const int n = TEMP_FAILURE_RETRY(epoll_wait(pGnssHwConn->m_epollFd.get(),
                                                    events, 2,
                                                    kTimeoutMs));
        if (n < 0) {
            ALOGE("%s:%d: epoll_wait failed with '%s'", __PRETTY_FUNCTION__, __LINE__, strerror(errno));
            continue;
        }

        for (int i = 0; i < n; ++i) {
            const struct epoll_event* ev = &events[i];
            const int fd = ev->data.fd;
            const int ev_events = ev->events;

            if (fd == pGnssHwConn->m_clientFd.get()) {
                if (ev_events & (EPOLLERR | EPOLLHUP)) {
                    ALOGV("%s:%d: epoll_wait: ev_events=%x GPS socket client may close. Remove pGnssHwConn->m_clientFd(%d) and reset it. Let client to reconnect.", __PRETTY_FUNCTION__, __LINE__, ev_events, pGnssHwConn->m_clientFd.get());
                    epollCtlRemove(pGnssHwConn->m_epollFd.get(), pGnssHwConn->m_clientFd.get());
                    shutdown(pGnssHwConn->m_clientFd.get(), SHUT_RDWR);
                    pGnssHwConn->m_clientFd.reset();
                    continue;
                } else if (ev_events & EPOLLIN) {
                    char buf[64];
                    while (true) {
                        int n = TEMP_FAILURE_RETRY(read(fd, buf, sizeof(buf)));
                        if (n > 0) {
                            ALOGV("%s:%d Received %d bytes: %s", __PRETTY_FUNCTION__, __LINE__, n, buf);
                            if (running) {
                                for (int i = 0; i < n; ++i) {
                                    listener.consume(buf[i]);
                                }
                            }
                        } else if (n == 0) {
                            ALOGV("%s:%d GPS socket client may close. Remove pGnssHwConn->m_clientFd(%d) and reset it. Let client to reconnect.", __PRETTY_FUNCTION__, __LINE__, pGnssHwConn->m_clientFd.get());
                            epollCtlRemove(pGnssHwConn->m_epollFd.get(), pGnssHwConn->m_clientFd.get());
                            shutdown(pGnssHwConn->m_clientFd.get(), SHUT_RDWR);
                            pGnssHwConn->m_clientFd.reset();
                        } else {
                            break;
                        }
                    }
                }
            } else if (fd == pGnssHwConn->m_threadsFd.get()) {
                if (ev_events & (EPOLLERR | EPOLLHUP)) {
                    ALOGE("%s:%d: epoll_wait: pGnssHwConn->m_threadsFd.get() has an error, ev_events=%x", __PRETTY_FUNCTION__, __LINE__, ev_events);
                    ::abort();
                } else if (ev_events & EPOLLIN) {
                    const int cmd = workerThreadRcvCommand(fd);
                    switch (cmd) {
                        case kCMD_QUIT:
                            return;

                        case kCMD_START:
                            if (!running) {
                                listener.reset();
                                sink->gnssStatus(ahg10::IGnssCallback::GnssStatusValue::SESSION_BEGIN);
                                running = true;
                            }
                            break;

                        case kCMD_STOP:
                            if (running) {
                                running = false;
                                sink->gnssStatus(ahg10::IGnssCallback::GnssStatusValue::SESSION_END);
                            }
                            break;

                        default:
                            ALOGE("%s:%d: workerThreadRcvCommand returned unexpected command, cmd=%d", __PRETTY_FUNCTION__, __LINE__, cmd);
                            ::abort();
                            break;
                    }
                }
            } else {
                ALOGE("%s:%d: epoll_wait() returned unexpected fd", __PRETTY_FUNCTION__, __LINE__);
            }
        }
    }
}

int GnssHwConn::workerThreadRcvCommand(const int fd) {
    char buf;
    if (TEMP_FAILURE_RETRY(read(fd, &buf, 1)) == 1) {
        return buf;
    } else {
        return -1;
    }
}

bool GnssHwConn::sendWorkerThreadCommand(char cmd) const {
    return TEMP_FAILURE_RETRY(write(m_callersFd.get(), &cmd, 1)) == 1;
}

void GnssHwConn::gpsSocketServerThread(void* paramGnssHwConn) {
    GnssHwConn* pGnssHwConn = (GnssHwConn*)paramGnssHwConn;
    int ret = 0;
    int so_reuseaddr = 1;
    int gpsSocketServerFd = -1;

    ALOGI("Constructing GPS socket server...");
    gpsSocketServerFd = socket(AF_INET, SOCK_STREAM, 0);
    if (gpsSocketServerFd < 0) {
        ALOGE("%s:%d Fail to construct tcp socket with error: %s", __PRETTY_FUNCTION__, __LINE__, strerror(errno));
        return;
    }

    if (setsockopt(gpsSocketServerFd, SOL_SOCKET, SO_REUSEADDR, &so_reuseaddr, sizeof(int)) < 0) {
        ALOGE("%s setsockopt(SO_REUSEADDR) failed. gpsSocketServerFd: %d\n", __PRETTY_FUNCTION__, gpsSocketServerFd);
        return;
    }
    pGnssHwConn->m_gpsSocketServerFd.reset(gpsSocketServerFd);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(pGnssHwConn->m_tcpPort);

    ret = bind(pGnssHwConn->m_gpsSocketServerFd.get(), (struct sockaddr*)&addr, sizeof(struct sockaddr_in));
    if (ret < 0) {
        ALOGE("%s Failed to bind server socket address %d, %s", __PRETTY_FUNCTION__, ret, strerror(errno));
        return;
    }

    ret = listen(pGnssHwConn->m_gpsSocketServerFd.get(), 5);
    if (ret < 0) {
        ALOGE("%s Failed to listen on server socket", __PRETTY_FUNCTION__);
        return;
    }

    while (!pGnssHwConn->m_gsstLoopExit) {
        socklen_t alen = sizeof(struct sockaddr_in);
        ALOGV("%s Wait a GPS client to connect...", __PRETTY_FUNCTION__);
        int clientFd = -1;
        clientFd = accept(pGnssHwConn->m_gpsSocketServerFd.get(), (struct sockaddr*)&addr, &alen);
        if (clientFd >= 0) {
            ALOGI("%s A GPS client connected to server. clientFd = %d", __PRETTY_FUNCTION__, clientFd);
            pGnssHwConn->m_clientFd.reset(clientFd);
            if (pGnssHwConn->m_epollFd.ok()) {
                ALOGV("%s register pGnssHwConn->m_clientFd(%d) to pGnssHwConn->m_epollFd(%d)", __PRETTY_FUNCTION__, pGnssHwConn->m_clientFd.get(), pGnssHwConn->m_epollFd.get());
                epollCtlAdd(pGnssHwConn->m_epollFd.get(), pGnssHwConn->m_clientFd.get());
            }

            //Android already triggered start command. Notify client to start when it connect to server.
            if (pGnssHwConn->m_needNotifyClientStart) {
                ALOGV("%s Android already triggered start command. Notify client to start when it connect to server.", __PRETTY_FUNCTION__);
                // kCMD_START, P uses CMD_START = 1. For compatibility, transfer kCMD_START to CMD_START.
                char cmd = 1;
                ret = TEMP_FAILURE_RETRY(write(pGnssHwConn->m_clientFd.get(), &cmd, 1));
                if (ret != 1)
                    ALOGE("%s: could not notify client(%d) to start: ret=%d: %s", __PRETTY_FUNCTION__, pGnssHwConn->m_clientFd.get(), ret, strerror(errno));
                else
                    ALOGV("%s Notify client(%d) to start", __PRETTY_FUNCTION__, pGnssHwConn->m_clientFd.get());
            }
        } else {
            ALOGV("%s GPS socket server maybe shutdown as quit command is got. Or else, error happen. pGnssHwConn->m_clientFd = %d %s.", __PRETTY_FUNCTION__, pGnssHwConn->m_clientFd.get(), strerror(errno));
        }
    }

    shutdown(pGnssHwConn->m_clientFd.get(), SHUT_RDWR);
    pGnssHwConn->m_clientFd.reset();
    shutdown(pGnssHwConn->m_gpsSocketServerFd.get(), SHUT_RDWR);
    pGnssHwConn->m_gpsSocketServerFd.reset();
    ALOGI("%s Quit", __PRETTY_FUNCTION__);
    return;
}

}  // namespace ciccloud
