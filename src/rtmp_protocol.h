#ifndef __RTMP_PROTOCOL_H__
#define __RTMP_PROTOCOL_H__

#include <stdint.h>
#include <stddef.h>

#include <deque>
#include <map>
#include <sstream>
#include <set>

#include "ref_ptr.h"
#include "socket_util.h"

using std::deque;
using std::map;
using std::string;
using std::ostringstream;
using std::set;

class Epoller;
class Fd;
class IoBuffer;
class StreamMgr;
class TcpSocket;

enum HandShakeStatus
{
    kStatus_0 = 0,
    kStatus_1,
    kStatus_2,
    kStatus_Done,
};

enum RtmpMessageType
{
    kSetChunkSize = 1,
    kUserControlMessage = 4,
    kWindowAcknowledgementSize = 5,
    kSetPeerBandwidth = 6,

    kAudio        = 8,
    kVideo        = 9,

    kAmf3Command = 17,
    kMetaData    = 18,
    kAmf0Command = 20,
};

enum RtmpRole
{
    // other_server --> me --> client

    kUnknownRole = -1,
    kClientPush  = 0,
    kPushServer  = 1,
    kPullServer  = 2,
    kClientPull  = 3,
};

struct RtmpUrl
{
    std::string ip;
    uint16_t port;
    std::string app;
    std::string stream_name;
};

struct RtmpMessage
{
    RtmpMessage()
        :
        timestamp(0),
        timestamp_delta(0),
        timestamp_calc(0),
        message_length(0),
        message_type_id(0),
        message_stream_id(0)
    {
    }

    string ToString() const
    {
        ostringstream os;

        os << "timestamp:" << timestamp
           << ",timestamp_delta:" << timestamp_delta
           << ",timestamp_calc:" << timestamp_calc
           << ",message_length:" << message_length
           << ",message_type_id:" << (uint16_t)message_type_id
           << ",message_stream_id:" << message_stream_id
           << ",msg:" << (uint64_t)msg
           << ",len:" << len;

        return os.str();
    }

    uint32_t timestamp;
    uint32_t timestamp_delta;
    uint32_t timestamp_calc;
    uint32_t message_length;
    uint8_t  message_type_id;
    uint32_t message_stream_id;

    uint8_t* msg;
    uint32_t len;
};

class RtmpProtocol
{
public:
    RtmpProtocol(Epoller* epoller, Fd* socket, StreamMgr* stream_mgr);
    ~RtmpProtocol();

    bool IsServerRole()
    {
        return role_ == kClientPull || role_ == kClientPush;
    }

    bool IsClientRole()
    {
        return role_ == kPushServer || role_ == kPullServer;
    }

    void SetClientPush()
    {
        role_ = kClientPush;
    }

    void SetPushServer()
    {
        role_ = kPushServer;
    }

    void SetPullServer()
    {
        role_ = kPullServer;
    }

    void SetClientPull()
    {
        role_ = kClientPull;
    }

    void SetRole(const RtmpRole& role)
    {
        role_ = role;
    }

    void SetRtmpSrc(RtmpProtocol* src)
    {
        rtmp_src_ = src;
    }

    int Parse(IoBuffer& io_buffer);
    int OnStop();
    int OnConnected();

    int EveryNSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count);

    int HandShakeStatus0();
    int HandShakeStatus1();
    int SetOutChunkSize(const uint32_t& chunk_size);
    int SetWindowAcknowledgementSize(const uint32_t& ack_window_size);
    int SetPeerBandwidth(const uint32_t& ack_window_size, const uint8_t& limit_type);
    int SendUserControlMessage(const uint16_t& event, const uint32_t& data);
    int SendConnect(const string& url);
    int SendCreateStream();
    int SendPublish();
    int SendAudio(const RtmpMessage& audio);
    int SendVideo(const RtmpMessage& video);

    int ConnectForwardServer(const string& ip, const uint16_t& port);

    static int ParseRtmpUrl(const string& url, RtmpUrl& rtmp_url);

    TcpSocket* GetTcpSocket()
    {
        return (TcpSocket*)socket_;
    }

    bool CanPublish()
    {
        return can_publish_;
    }

    bool RemoveForward(RtmpProtocol* protocol)
    {
        if (rtmp_forwards_.find(protocol) == rtmp_forwards_.end())
        {
            return false;
        }

        rtmp_forwards_.erase(protocol);

        return true;
    }

    bool AddRtmpPlayer(RtmpProtocol* protocol)
    {
        if (rtmp_player_.count(protocol))
        {
            return false;
        }

        rtmp_player_.insert(protocol);

        OnNewRtmpPlayer(protocol);

        return true;
    }

    bool RemovePlayer(RtmpProtocol* protocol)
    {
        if (rtmp_player_.count(protocol) == 0)
        {
            return false;
        }

        rtmp_player_.erase(protocol);

        return true;
    }

private:
    int OnRtmpMessage(RtmpMessage& rtmp_msg);
    int OnNewRtmpPlayer(RtmpProtocol* protocol);
    int SendRtmpMessage(const uint8_t& message_type_id, const uint8_t* data, const size_t& len);
    int SendMediaData(const RtmpMessage& media);

private:
    Epoller* epoller_;
    Fd* socket_;
    StreamMgr* stream_mgr_;
    HandShakeStatus handshake_status_;

    int role_;

    uint32_t in_chunk_size_;
    uint32_t out_chunk_size_;

    map<uint32_t, RtmpMessage> csid_head_;

    string app_;
    string tc_url_;
    string stream_name_;

    map<uint64_t, Payload> video_queue_;
    map<uint64_t, Payload> audio_queue_;

    uint32_t video_fps_;
    uint32_t audio_fps_;

    uint64_t video_frame_recv_;
    uint64_t audio_frame_recv_;

    uint64_t last_key_video_frame_;
    uint64_t last_key_audio_frame_;

    uint64_t last_calc_fps_ms_;
    uint64_t last_calc_video_frame_;
    uint64_t last_calc_audio_frame_;

    set<RtmpProtocol*> rtmp_forwards_;
    set<RtmpProtocol*> rtmp_player_;
    
    RtmpProtocol* rtmp_src_;

    string metadata_;
    string aac_header_;
    string avc_header_;

    string last_send_command_;

    bool can_publish_;

    uint64_t video_frame_send_;
    uint64_t audio_frame_send_;
    uint32_t last_video_timestamp_;
    uint32_t last_video_timestamp_delta_;
    uint32_t last_audio_timestamp_;
    uint32_t last_audio_timestamp_delta_;
    uint32_t last_video_message_length_;
    uint32_t last_audio_message_length_;
    uint8_t last_message_type_id_;
};

#endif // __RTMP_PROTOCOL_H__
