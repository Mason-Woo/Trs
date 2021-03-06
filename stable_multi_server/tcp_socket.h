#ifndef __TCP_SOCKET_H__
#define __TCP_SOCKET_H__

#include "epoller.h"
#include "io_buffer.h"
#include "fd.h"

class Epoller;
class SocketHandle;

enum ConnectStatus
{
    kDisconnected = -1,
    kConnecting = 0,
    kConnected = 1,
    kDisconnecting = 2,
};

class TcpSocket : public Fd
{
public:
    TcpSocket(Epoller* epoller, const int& fd, SocketHandle* handler);
    ~TcpSocket();

    void AsServerSocket()
    {
        server_socket_ = true;
    }

    virtual int OnRead();
    virtual int OnWrite();
    virtual int Send(const uint8_t* data, const size_t& len);

    void SetDisconnected()
    {
        connect_status_ = kDisconnected;
    }

    void SetConnecting()
    {
        connect_status_ = kConnecting;
    }

    void SetConnected()
    {
        connect_status_ = kConnected;
    }

    void SetDisconnecting()
    {
        connect_status_ = kDisconnecting;
    }

private:
    bool server_socket_;
    SocketHandle* handler_;
    IoBuffer      read_buffer_;
    IoBuffer      write_buffer_;

    int           connect_status_;
};

#endif // __TCP_SOCKET_H__
