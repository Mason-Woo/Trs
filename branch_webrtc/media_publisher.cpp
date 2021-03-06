#include "http_flv_protocol.h"
#include "media_publisher.h"
#include "rtmp_protocol.h"
#include "server_protocol.h"

bool MediaPublisher::AddSubscriber(MediaSubscriber* subscriber)
{   
    if (subscriber_.count(subscriber))
    {   
        return false;
    }   

    int ret = OnNewSubscriber(subscriber);

    if (ret == kPending)
    {
        wait_header_subscriber_.insert(subscriber);
    }
    else if (ret == kSuccess)
    {
        subscriber_.insert(subscriber);
    }

    subscriber->SetPublisher(this);

    return true;
}

bool MediaPublisher::RemoveSubscriber(MediaSubscriber* subscriber)
{   
    if (subscriber_.find(subscriber) == subscriber_.end())
    {   
        return false;
    }   

    subscriber_.erase(subscriber);
    wait_header_subscriber_.erase(subscriber);

    return true;
}   

int MediaPublisher::OnNewSubscriber(MediaSubscriber* subscriber)
{
    cout << LMSG << endl;

    if (media_muxer_.HasMetaData())
    {    
        subscriber->SendMetaData(media_muxer_.GetMetaData());
    }    

    cout << LMSG << "audio header:" << media_muxer_.HasAudioHeader() << ",video_header:" << media_muxer_.HasVideoHeader() << endl;

    // 还未收齐音视频头,暂时挂起,收齐后再分发流
    if (! media_muxer_.HasAudioHeader() || ! media_muxer_.HasVideoHeader())
    {
        cout << LMSG << "will pending" << endl;
        return kPending;
    }

    subscriber->SendAudioHeader(media_muxer_.GetAudioHeader());
    subscriber->SendVideoHeader(media_muxer_.GetVideoHeader());

    auto media_fast_out = media_muxer_.GetFastOut();

    for (const auto& payload : media_fast_out)
    {    
        subscriber->SendMediaData(payload);
    }    

    return kSuccess;
}
