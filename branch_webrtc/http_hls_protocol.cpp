#include <iostream>
#include <map>

#include "common_define.h"
#include "global.h"
#include "http_hls_protocol.h"
#include "io_buffer.h"
#include "local_stream_center.h"
#include "rtmp_protocol.h"
#include "tcp_socket.h"
#include "util.h"

using namespace std;

HttpHlsProtocol::HttpHlsProtocol(Epoller* epoller, Fd* socket)
    :
    MediaSubscriber(kHttpHls),
    epoller_(epoller),
    socket_(socket),
    media_publisher_(NULL)
{
}

HttpHlsProtocol::~HttpHlsProtocol()
{
}

int HttpHlsProtocol::Parse(IoBuffer& io_buffer)
{
    uint8_t* data = NULL;

    int size = io_buffer.Read(data, io_buffer.Size());


    int r_pos = -1; // '\r'
    int n_pos = -1; // '\n'
    int m_pos = -1; // ':'

    bool key_value = false; // false:key, true:value

    string key;
    string value;

    map<string, string> header;

    for (int i = 0; i != size; ++i)
    {
        if (data[i] == '\r')
        {
            r_pos = i;
        }
        else if (data[i] == '\n')
        {
            if (i == r_pos + 1)
            {
                if (i == n_pos + 2) // \r\n\r\n
                {
                    cout << LMSG << "http done" << endl;

                    cout << LMSG << "app_:" << app_ << ",stream_:" << stream_ << ",ts_:" << ts_ << ",type_:" << type_ << endl;
                    if (! app_.empty() && ! stream_.empty())
                    {
                        media_publisher_ = g_local_stream_center.GetMediaPublisherByAppStream(app_, stream_);

                        if (media_publisher_ != NULL)
                        {
                            if (type_ == "ts")
                            {
                                const string& ts = media_publisher_->GetMediaMuxer().GetTs(Util::Str2Num<uint64_t>(ts_));
                                
                                if (! ts.empty())
                                {
                                    ostringstream os;

					                os << "HTTP/1.1 200 OK\r\n"
                                       << "Server: trs\r\n"
                                       << "Content-Type: application/x-mpegurl\r\n"
                                       << "Connection: keep-alive\r\n"
                                       << "Content-Length:" << ts.size() << "\r\n"
                                       << "\r\n";

					                GetTcpSocket()->Send((const uint8_t*)os.str().data(), os.str().size());
                                    GetTcpSocket()->Send((const uint8_t*)ts.data(), ts.size());
                                }
                                else
                                {
                                    ostringstream os;

					                os << "HTTP/1.1 404 Not Found\r\n"
                                       << "Server: trs\r\n"
                                       << "Connection: close\r\n"
                                       << "\r\n";

					                GetTcpSocket()->Send((const uint8_t*)os.str().data(), os.str().size());
                                }
                            }
                            else if (type_ == "m3u8")
                            {
                                string m3u8 = media_publisher_->GetMediaMuxer().GetM3U8();
                                
                                if (! m3u8.empty())
                                {
                                    ostringstream os;

					                os << "HTTP/1.1 200 OK\r\n"
                                       << "Server: trs\r\n"
                                       << "Content-Type: application/x-mpegurl\r\n"
                                       << "Connection: keep-alive\r\n"
                                       << "Content-Length:" << m3u8.size() << "\r\n"
                                       << "\r\n";

					                GetTcpSocket()->Send((const uint8_t*)os.str().data(), os.str().size());
                                    GetTcpSocket()->Send((const uint8_t*)m3u8.data(), m3u8.size());
                                }
                                else
                                {
                                    ostringstream os;

					                os << "HTTP/1.1 404 Not Found\r\n"
                                       << "Server: trs\r\n"
                                       << "Connection: close\r\n"
                                       << "\r\n";

					                GetTcpSocket()->Send((const uint8_t*)os.str().data(), os.str().size());
                                }
                            }
                        }
                        else
                        {
                            cout << LMSG << "can't find media source, app_:" << app_ << ",stream_:" << stream_ << endl;

                            g_media_center_mgr->GetAppStreamMasterNode(app_, stream_);
                            expired_time_ms_ = Util::GetNowMs() + 10000;

                            g_local_stream_center.AddAppStreamPendingSubscriber(app_, stream_, this);

                            /*
                            ostringstream os;

					        os << "HTTP/1.1 404 Not Found\r\n"
                               << "Server: trs\r\n"
                               << "Connection: close\r\n"
                               << "\r\n";

					        GetTcpSocket()->Send((const uint8_t*)os.str().data(), os.str().size());
                            */
                        }
                    }


                    return kSuccess;
                }
                else // \r\n
                {
                    key_value = false;

                    cout << LMSG << key << ":" << value << endl;

                    if (key.find("GET") != string::npos)
                    {
                        //GET /test.flv HTTP/1.1
                        int s_count = 0;
                        int x_count = 0; // /
                        int d_count = 0; // .
                        for (const auto& ch : key)
                        {
                            if (ch == ' ')
                            {
                                ++s_count;
                            }
                            else if (ch == '/')
                            {
                                ++x_count;
                            }
                            else if (ch == '.')
                            {
                                ++d_count;
                            }
                            else
                            {
                                if (x_count == 1)
                                {
                                    app_ += ch;
                                }
                                else if (x_count == 2)
                                {
                                    stream_ += ch;
                                }
                                else if (x_count == 3 && s_count < 2)
                                {
                                    if (d_count == 1)
                                    {
                                        type_ += ch;
                                    }
                                    else
                                    {
                                        ts_ += ch;
                                    }
                                }
                            }
                        }
                    }

                    header[key] = value;
                    key.clear();
                    value.clear();
                }
            }

            n_pos = i;
        }
        else if (data[i] == ':')
        {
            m_pos = i;
            key_value = true;
        }
        else if (data[i] == ' ')
        {
            if (m_pos + 1 == i)
            {
            }
            else
            {
                if (key_value)
                {
                    value += (char)data[i];
                }
                else
                {
                    key += (char)data[i];
                }
            }
        }
        else
        {
            if (key_value)
            {
                value += (char)data[i];
            }
            else
            {
                key += (char)data[i];
            }
        }
    }

    return kNoEnoughData;
}

int HttpHlsProtocol::OnStop()
{
    return kSuccess;
}

int HttpHlsProtocol::OnPendingArrive()
{
    cout << LMSG << "pending done" << endl;

    cout << LMSG << "app_:" << app_ << ",stream_:" << stream_ << ",ts_:" << ts_ << ",type_:" << type_ << endl;

    if (! app_.empty() && ! stream_.empty())
    {
        media_publisher_ = g_local_stream_center.GetMediaPublisherByAppStream(app_, stream_);

        if (media_publisher_ != NULL)
        {
            if (type_ == "ts")
            {
                const string& ts = media_publisher_->GetMediaMuxer().GetTs(Util::Str2Num<uint64_t>(ts_));
                
                if (! ts.empty())
                {
                    ostringstream os;

	                os << "HTTP/1.1 200 OK\r\n"
                       << "Server: trs\r\n"
                       << "Content-Type: application/x-mpegurl\r\n"
                       << "Connection: keep-alive\r\n"
                       << "Content-Length:" << ts.size() << "\r\n"
                       << "\r\n";

	                GetTcpSocket()->Send((const uint8_t*)os.str().data(), os.str().size());
                    GetTcpSocket()->Send((const uint8_t*)ts.data(), ts.size());
                }
                else
                {
                    ostringstream os;

	                os << "HTTP/1.1 404 Not Found\r\n"
                       << "Server: trs\r\n"
                       << "Connection: close\r\n"
                       << "\r\n";

	                GetTcpSocket()->Send((const uint8_t*)os.str().data(), os.str().size());
                }
            }
            else if (type_ == "m3u8")
            {
                string m3u8 = media_publisher_->GetMediaMuxer().GetM3U8();
                
                ostringstream os;

	            os << "HTTP/1.1 200 OK\r\n"
                   << "Server: trs\r\n"
                   << "Content-Type: application/x-mpegurl\r\n"
                   << "Connection: keep-alive\r\n"
                   << "Content-Length:" << m3u8.size() << "\r\n"
                   << "\r\n";

	            GetTcpSocket()->Send((const uint8_t*)os.str().data(), os.str().size());

                if (! m3u8.empty())
                {
                    GetTcpSocket()->Send((const uint8_t*)m3u8.data(), m3u8.size());
                }
            }
        }
        else
        {
            cout << LMSG << "can't find media source, app_:" << app_ << ",stream_:" << stream_ << endl;

            ostringstream os;

	        os << "HTTP/1.1 404 Not Found\r\n"
               << "Server: trs\r\n"
               << "Connection: close\r\n"
               << "\r\n";

	        GetTcpSocket()->Send((const uint8_t*)os.str().data(), os.str().size());
        }
    }

    return kSuccess;
}
