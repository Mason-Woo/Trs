#ifndef __RTMP_MGR_H__
#define __RTMP_MGR_H__

#include <map>
#include <string>

#include "socket_handle.h"
#include "timer_handle.h"

using std::map;
using std::string;

class Epoller;
class Fd;
class RtmpProtocol;
class ServerMgr;

class RtmpMgr : public SocketHandle, public TimerSecondHandle
{
public:
    RtmpMgr(Epoller* epoller, ServerMgr* server_mgr);
    ~RtmpMgr();

    virtual int HandleRead(IoBuffer& io_buffer, Fd& socket);
    virtual int HandleClose(IoBuffer& io_buffer, Fd& socket);
    virtual int HandleError(IoBuffer& io_buffer, Fd& socket);
    virtual int HandleConnected(Fd& socket);

    virtual int HandleTimerInSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count);

    RtmpProtocol* GetOrCreateProtocol(Fd& socket);

private:
    Epoller* epoller_;
    ServerMgr* server_mgr_;
    map<int, RtmpProtocol*> fd_protocol_;
};

#endif // __RTMP_MGR_H__
