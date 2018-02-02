#include <signal.h>


#include <iostream>

#include "admin_mgr.h"
#include "any.h"
#include "base_64.h"
#include "bit_buffer.h"
#include "bit_stream.h"
#include "echo_mgr.h"
#include "epoller.h"
#include "http_file_mgr.h"
#include "http_flv_mgr.h"
#include "http_hls_mgr.h"
#include "local_stream_center.h"
#include "media_center_mgr.h"
#include "media_node_discovery_mgr.h"
#include "ref_ptr.h"
#include "rtmp_mgr.h"
#include "server_mgr.h"
#include "socket_util.h"
#include "ssl_socket.h"
#include "tcp_socket.h"
#include "timer_in_second.h"
#include "timer_in_millsecond.h"
#include "trace_tool.h"
#include "udp_socket.h"
#include "util.h"
#include "webrtc_mgr.h"
#include "web_socket_mgr.h"

#include "openssl/ssl.h"

using namespace any;
using namespace std;
using namespace socket_util;

static void sighandler(int sig_no)
{
    cout << LMSG << "sig:" << sig_no << endl;
	exit(0);
} 

LocalStreamCenter       g_local_stream_center;
NodeInfo                g_node_info;
Epoller*                g_epoll = NULL;
HttpFlvMgr*             g_http_flv_mgr = NULL;
HttpFlvMgr*             g_https_flv_mgr = NULL;
HttpHlsMgr*             g_http_hls_mgr = NULL;
HttpHlsMgr*             g_https_hls_mgr = NULL;
MediaCenterMgr*         g_media_center_mgr = NULL;
MediaNodeDiscoveryMgr*  g_media_node_discovery_mgr = NULL;
RtmpMgr*                g_rtmp_mgr = NULL;
ServerMgr*              g_server_mgr = NULL;
SSL_CTX*                g_ssl_ctx = NULL;

int main(int argc, char* argv[])
{
    string raw = "Man is distinguished, not only by his reason, but by this singular passion from other animals, which is a lust of the mind, that by a perseverance of delight in the continued and indefatigable generation of knowledge, exceeds the short vehemence of any carnal pleasure.";

    string base64;

    Base64::Encode(raw, base64);

    cout << base64 << endl;

    string tmp;
    Base64::Decode(base64, tmp);

    cout << raw << endl;
    cout << tmp << endl;

    cout << raw.size() << endl;
    cout << tmp.size() << endl;

    assert(raw == tmp);

    FILE* server_private_key_file = fopen("server.key", "r");
    if (server_private_key_file == NULL)
    {
        cout << LMSG << endl;
        return -1;
    }

    // Open ssl init
	SSL_load_error_strings();
    int ret = SSL_library_init();

    assert(ret == 1);

    g_ssl_ctx = SSL_CTX_new(SSLv23_method());

    assert(g_ssl_ctx != NULL);

	string server_crt = "";
	string server_key = "";

    int server_crt_fd = open("server.crt", O_RDONLY, 0664);
    if (server_crt_fd < 0)
    {
        cout << LMSG << "open server.crt failed" << endl;
        return -1;
    }

    while (true)
    {
        char buf[4096];
        int bytes = read(server_crt_fd, buf, sizeof(buf));

        if (bytes < 0)
        {
            cout << "read server.crt failed" << endl;
            return -1;
        }
        else if (bytes == 0)
        {
            break;
        }

        server_crt.append(buf, bytes);
    }


    int server_key_fd = open("server.key", O_RDONLY, 0664);
    if (server_key_fd < 0)
    {
        cout << LMSG << "open server.key failed" << endl;
        return -1;
    }

    while (true)
    {
        char buf[4096];
        int bytes = read(server_key_fd, buf, sizeof(buf));

        if (bytes < 0)
        {
            cout << "read server.key failed" << endl;
            return -1;
        }
        else if (bytes == 0)
        {
            break;
        }

        server_key.append(buf, bytes);
    }

	BIO *mem_cert = BIO_new_mem_buf((void *)server_crt.c_str(), server_crt.length());
    assert(mem_cert != NULL);
    X509 *cert= PEM_read_bio_X509(mem_cert,NULL,NULL,NULL);
    assert(cert != NULL);    
    SSL_CTX_use_certificate(g_ssl_ctx, cert);
    X509_free(cert);
    BIO_free(mem_cert);    
    
    BIO *mem_key = BIO_new_mem_buf((void *)server_key.c_str(), server_key.length());
    assert(mem_key != NULL);
    RSA *rsa_private = PEM_read_bio_RSAPrivateKey(mem_key, NULL, NULL, NULL);
    assert(rsa_private != NULL);
    SSL_CTX_use_RSAPrivateKey(g_ssl_ctx, rsa_private);
    RSA_free(rsa_private);
    BIO_free(mem_key);

    ret = SSL_CTX_check_private_key(g_ssl_ctx);

    // parse args
    map<string, string> args_map = Util::ParseArgs(argc, argv);

    uint16_t rtmp_port              = 1935;
    uint16_t http_file_port         = 8666;
    uint16_t https_flv_port         = 8743;
    uint16_t http_flv_port          = 8787;
    uint16_t https_hls_port         = 8843;
    uint16_t http_hls_port          = 8888;
    string server_ip                = "";
    uint16_t server_port            = 10001;
    uint16_t admin_port             = 11000;
    uint16_t web_socket_port        = 8901;
    uint16_t ssl_web_socket_port    = 8943;
    uint16_t echo_port              = 11345;
    uint16_t webrtc_port            = 11445;
    bool daemon                     = false;

    auto iter_server_ip     = args_map.find("server_ip");
    auto iter_rtmp_port     = args_map.find("rtmp_port");
    auto iter_http_flv_port = args_map.find("http_flv_port");
    auto iter_http_hls_port = args_map.find("http_hls_port");
    auto iter_server_port   = args_map.find("server_port");
    auto iter_admin_port    = args_map.find("admin_port");
    auto iter_daemon        = args_map.find("daemon");

    if (iter_server_ip == args_map.end())
    {
        cout << "Usage:" << argv[0] << " -server_ip <xxx.xxx.xxx.xxx> -server_port [xxx] -http_flv_port [xxx] -http_hls_port [xxx] -daemon [xxx]" << endl;
        return 0;
    }

    server_ip = iter_server_ip->second;

    if (iter_rtmp_port != args_map.end())
    {
        if (! iter_rtmp_port->second.empty())
        {
            rtmp_port = Util::Str2Num<uint16_t>(iter_rtmp_port->second);
        }
    }

    if (iter_http_flv_port != args_map.end())
    {
        if (! iter_http_flv_port->second.empty())
        {
            http_flv_port = Util::Str2Num<uint16_t>(iter_http_flv_port->second);
        }
    }

    if (iter_http_hls_port != args_map.end())
    {
        if (! iter_http_hls_port->second.empty())
        {
            http_hls_port = Util::Str2Num<uint16_t>(iter_http_hls_port->second);
        }
    }

    if (iter_server_port != args_map.end())
    {
        if (! iter_server_port->second.empty())
        {
            server_port = Util::Str2Num<uint16_t>(iter_server_port->second);
        }
    }

    if (iter_admin_port != args_map.end())
    {
        if (! iter_admin_port->second.empty())
        {
            admin_port = Util::Str2Num<uint16_t>(iter_admin_port->second);
        }
    }

    if (iter_daemon != args_map.end())
    {
        int tmp = Util::Str2Num<int>(iter_daemon->second);

        daemon = (! (tmp == 0));
    }

    if (daemon)
    {
        Util::Daemon();
    }

	IpStr2Num(server_ip, g_node_info.ip);
    g_node_info.port.push_back(server_port);
    g_node_info.type          = RTMP_NODE;
    g_node_info.start_time_ms = Util::GetNowMs();
    g_node_info.pid           = getpid();

	signal(SIGUSR1, sighandler);
    signal(SIGPIPE,SIG_IGN);

    Log::SetLogLevel(kLevelDebug);

    DEBUG << argv[0] << " starting..." << endl;

    Epoller epoller;
    g_epoll = &epoller;

    // === Init Timer ===
    TimerInSecond timer_in_second(&epoller);
    TimerInMillSecond timer_in_millsecond(&epoller);

    // === Init Server Stream Socket ===
    int server_stream_fd = CreateNonBlockTcpSocket();

    ReuseAddr(server_stream_fd);
    Bind(server_stream_fd, "0.0.0.0", server_port);
    Listen(server_stream_fd);
    SetNonBlock(server_stream_fd);

    ServerMgr server_mgr(&epoller);
    timer_in_second.AddTimerSecondHandle(&server_mgr);
    g_server_mgr = &server_mgr;

    TcpSocket server_stream_socket(&epoller, server_stream_fd, &server_mgr);
    server_stream_socket.EnableRead();
    server_stream_socket.AsServerSocket();

    // === Init Server Rtmp Socket ===
    int server_rtmp_fd = CreateNonBlockTcpSocket();

    ReuseAddr(server_rtmp_fd);
    Bind(server_rtmp_fd, "0.0.0.0", rtmp_port);
    Listen(server_rtmp_fd);
    SetNonBlock(server_rtmp_fd);

    RtmpMgr rtmp_mgr(&epoller, &server_mgr);
    timer_in_second.AddTimerSecondHandle(&rtmp_mgr);
    g_rtmp_mgr = &rtmp_mgr;

    TcpSocket server_rtmp_socket(&epoller, server_rtmp_fd, &rtmp_mgr);
    server_rtmp_socket.EnableRead();
    server_rtmp_socket.AsServerSocket();

    // === Init Server Http Flv Socket ===
    int server_http_flv_fd = CreateNonBlockTcpSocket();

    ReuseAddr(server_http_flv_fd);
    Bind(server_http_flv_fd, "0.0.0.0", http_flv_port);
    Listen(server_http_flv_fd);
    SetNonBlock(server_http_flv_fd);

    HttpFlvMgr http_flv_mgr(&epoller);

    g_http_flv_mgr = &http_flv_mgr;

    TcpSocket server_http_flv_socket(&epoller, server_http_flv_fd, &http_flv_mgr);
    server_http_flv_socket.EnableRead();
    server_http_flv_socket.AsServerSocket();

    // === Init Server Https Flv Socket ===
    int server_https_flv_fd = CreateNonBlockTcpSocket();

    ReuseAddr(server_https_flv_fd);
    Bind(server_https_flv_fd, "0.0.0.0", https_flv_port);
    Listen(server_https_flv_fd);
    SetNonBlock(server_https_flv_fd);

    HttpFlvMgr https_flv_mgr(&epoller);

    g_https_flv_mgr = &https_flv_mgr;

    SslSocket server_https_flv_socket(&epoller, server_https_flv_fd, &https_flv_mgr);
    server_https_flv_socket.EnableRead();
    server_https_flv_socket.AsServerSocket();

    // === Init Server Http Hls Socket ===
    int server_http_hls_fd = CreateNonBlockTcpSocket();

    ReuseAddr(server_http_hls_fd);
    Bind(server_http_hls_fd, "0.0.0.0", http_hls_port);
    Listen(server_http_hls_fd);
    SetNonBlock(server_http_hls_fd);

    HttpHlsMgr http_hls_mgr(&epoller, &rtmp_mgr, &server_mgr);

    g_http_hls_mgr = &http_hls_mgr;

    TcpSocket server_http_hls_socket(&epoller, server_http_hls_fd, &http_hls_mgr);
    server_http_hls_socket.EnableRead();
    server_http_hls_socket.AsServerSocket();

    // === Init Server Https Hls Socket ===
    int server_https_hls_fd = CreateNonBlockTcpSocket();

    ReuseAddr(server_https_hls_fd);
    Bind(server_https_hls_fd, "0.0.0.0", https_hls_port);
    Listen(server_https_hls_fd);
    SetNonBlock(server_https_hls_fd);

    HttpHlsMgr https_hls_mgr(&epoller, &rtmp_mgr, &server_mgr);

    g_https_hls_mgr = &https_hls_mgr;

    SslSocket server_https_hls_socket(&epoller, server_https_hls_fd, &https_hls_mgr);
    server_https_hls_socket.EnableRead();
    server_https_hls_socket.AsServerSocket();

    // === Init Admin Socket ===
    int admin_fd = CreateNonBlockTcpSocket();

    ReuseAddr(admin_fd);
    Bind(admin_fd, "0.0.0.0", admin_port);
    Listen(admin_fd);
    SetNonBlock(admin_fd);

    AdminMgr admin_mgr(&epoller);

    TcpSocket admin_socket(&epoller, admin_fd, &admin_mgr);
    admin_socket.EnableRead();
    admin_socket.AsServerSocket();

    // === Init WebSocket Socket ===
    int web_socket_fd = CreateNonBlockTcpSocket();

    ReuseAddr(web_socket_fd);
    Bind(web_socket_fd, "0.0.0.0", web_socket_port);
    Listen(web_socket_fd);
    SetNonBlock(web_socket_fd);

    WebSocketMgr web_socket_mgr(&epoller);

    TcpSocket web_socket_socket(&epoller, web_socket_fd, &web_socket_mgr);
    web_socket_socket.EnableRead();
    web_socket_socket.AsServerSocket();

    // === Init SSL WebSocket Socket ===
    int ssl_web_socket_fd = CreateNonBlockTcpSocket();

    ReuseAddr(ssl_web_socket_fd);
    Bind(ssl_web_socket_fd, "0.0.0.0", ssl_web_socket_port);
    Listen(ssl_web_socket_fd);
    SetNonBlock(ssl_web_socket_fd);

    WebSocketMgr ssl_web_socket_mgr(&epoller);

    SslSocket ssl_web_socket_socket(&epoller, ssl_web_socket_fd, &ssl_web_socket_mgr);
    ssl_web_socket_socket.EnableRead();
    ssl_web_socket_socket.AsServerSocket();

    // === Init Server Http File Socket ===
    int server_http_file_fd = CreateNonBlockTcpSocket();

    ReuseAddr(server_http_file_fd);
    Bind(server_http_file_fd, "0.0.0.0", http_file_port);
    Listen(server_http_file_fd);
    SetNonBlock(server_http_file_fd);

    HttpFileMgr http_file_mgr(&epoller);

    TcpSocket server_http_file_socket(&epoller, server_http_file_fd, &http_file_mgr);
    server_http_file_socket.EnableRead();
    server_http_file_socket.AsServerSocket();

    // === Init Udp Echo Socket ===
    int echo_fd = CreateNonBlockUdpSocket();

    ReuseAddr(echo_fd);
    Bind(echo_fd, "0.0.0.0", echo_port);
    Listen(echo_fd);
    SetNonBlock(echo_fd);

    EchoMgr echo_mgr(&epoller);

    UdpSocket server_echo_socket(&epoller, echo_fd, &echo_mgr);
    server_echo_socket.EnableRead();

    // === Init Udp Echo Socket ===
    int webrtc_fd = CreateNonBlockUdpSocket();

    ReuseAddr(webrtc_fd);
    Bind(webrtc_fd, "0.0.0.0", webrtc_port);
    Listen(webrtc_fd);
    SetNonBlock(webrtc_fd);

    WebrtcMgr webrtc_mgr(&epoller);

    UdpSocket server_webrtc_socket(&epoller, webrtc_fd, &webrtc_mgr);
    server_webrtc_socket.EnableRead();

    // === Init Media Center ===
    MediaCenterMgr media_center_mgr(&epoller);
    timer_in_second.AddTimerSecondHandle(&media_center_mgr);
    g_media_center_mgr = &media_center_mgr;

    // === Init Media Node Discovery ===
    MediaNodeDiscoveryMgr media_node_discovery_mgr(&epoller);
    g_media_node_discovery_mgr = &media_node_discovery_mgr;
    media_node_discovery_mgr.ConnectNodeDiscovery("127.0.0.1", 16001);

    timer_in_second.AddTimerSecondHandle(&media_node_discovery_mgr);


    // Event Loop
    while (true)
    {
        epoller.Run();
    }

    return 0;
}