#include "fd.h"
#include "http_hls_mgr.h"
#include "http_hls_protocol.h"
#include "rtmp_mgr.h"

HttpHlsMgr::HttpHlsMgr(Epoller* epoller, RtmpMgr* rtmp_mgr, ServerMgr* server_mgr)
    :
    epoller_(epoller),
    rtmp_mgr_(rtmp_mgr),
    server_mgr_(server_mgr)
{
}

HttpHlsMgr::~HttpHlsMgr()
{
}

int HttpHlsMgr::HandleRead(IoBuffer& io_buffer, Fd& socket)
{
	HttpHlsProtocol* http_protocol = GetOrCreateProtocol(socket);

    while (http_protocol->Parse(io_buffer) == kSuccess)
    {   
    }
}

int HttpHlsMgr::HandleClose(IoBuffer& io_buffer, Fd& socket)
{
	HttpHlsProtocol* http_protocol = GetOrCreateProtocol(socket);

    http_protocol->OnStop();

    delete http_protocol;
    fd_protocol_.erase(socket.GetFd());
}

int HttpHlsMgr::HandleError(IoBuffer& io_buffer, Fd& socket)
{
	HttpHlsProtocol* http_protocol = GetOrCreateProtocol(socket);

    http_protocol->OnStop();

    delete http_protocol;
    fd_protocol_.erase(socket.GetFd());
}

int HttpHlsMgr::HandleConnected(Fd& socket)
{
}

HttpHlsProtocol* HttpHlsMgr::GetOrCreateProtocol(Fd& socket)
{
    int fd = socket.GetFd();
    if (fd_protocol_.count(fd) == 0)
    {   
        fd_protocol_[fd] = new HttpHlsProtocol(epoller_, &socket, this, rtmp_mgr_, server_mgr_);
    }   

    return fd_protocol_[fd];
}
