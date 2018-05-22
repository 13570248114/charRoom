#include "codec.h"

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpServer.h>
#include <muduo/base/Mutex.h>
#include <muduo/base/ThreadLocalSingleton.h>

#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>
#include <boost/unordered_set.hpp>
#include <boost/circular_buffer.hpp>

#include <map>
#include <set>
#include <stdio.h>
#include <unistd.h>

using namespace muduo;
using namespace muduo::net;

namespace pubsub
{

typedef std::set<string> ConnectionSubscription;

class Topic : public muduo::copyable
{
 public:
  Topic(const string& topic)
    : topic_(topic)
  {
  }

  void add(const TcpConnectionPtr& conn)
  {
    audiences_.insert(conn);
    /*
    if (lastPubTime_.valid())
    {
      conn->send(makeMessage());
    }
    */
  }

  void remove(const TcpConnectionPtr& conn)
  {
    audiences_.erase(conn);
  }

  void publish(const string& content, Timestamp time)
  {
    content_ = content;
    lastPubTime_ = time;
    string message = makeMessage();
    for (std::set<TcpConnectionPtr>::iterator it = audiences_.begin();
         it != audiences_.end();
         ++it)
    {
      (*it)->send(message);
    }
  }

 private:

  string makeMessage()
  {
    return "pub " + topic_ + "\r\n" + content_ + "\r\n";
  }

  string topic_;
  string content_;
  Timestamp lastPubTime_;
  public:
  std::set<TcpConnectionPtr> audiences_;
};

class PubSubServer : boost::noncopyable
{
 public:
  PubSubServer(muduo::net::EventLoop* loop,
               const muduo::net::InetAddress& listenAddr)
    : server_(loop, listenAddr, "PubSubServer"),
      connectionBuckets_(idleNum)
  {
    server_.setConnectionCallback(
        boost::bind(&PubSubServer::onConnection, this, _1));
    server_.setMessageCallback(
        boost::bind(&PubSubServer::onMessage, this, _1, _2, _3));
    EventLoop::Functor f = boost::bind(&PubSubServer::onTimer,this);
    loop->runEvery(600.0,f);
    connectionBuckets_.resize(idleNum);
   // loop_->runEvery(1.0, boost::bind(&PubSubServer::timePublish, this));
  }

  void setThreadNum(int numThreads)
  {
    server_.setThreadNum(numThreads);
  }

  void start()
  {
    server_.setThreadInitCallback(boost::bind(&PubSubServer::threadInit, this, _1));
    server_.start();
  }

 private:
  void onConnection(const TcpConnectionPtr& conn)
  {
    if (conn->connected())
    {
      conn->setContext(ConnectionSubscription());
      EntryPtr entry(new Entry(conn));
      connectionBuckets_.back().insert(entry);
      WeakEntryPtr WeakEntry(entry);
      conn_map_weak[conn]=WeakEntry;
    }
    else
    {
      const ConnectionSubscription& connSub
        = boost::any_cast<const ConnectionSubscription&>(conn->getContext());
      // subtle: doUnsubscribe will erase *it, so increase before calling.
      for (ConnectionSubscription::const_iterator it = connSub.begin();
           it != connSub.end();)
      {
        doUnsubscribe(conn, *it++);
      }

      WeakEntryPtr weakEntry = conn_map_weak[conn];
      LOG_DEBUG << "Entry use_count = " << weakEntry.use_count();
    }
  }

  void onMessage(const TcpConnectionPtr& conn,
                 Buffer* buf,
                 Timestamp receiveTime)
  {
    ParseResult result = kSuccess;
    while (result == kSuccess)
    {
      string cmd;
      string topic;
      string content;
      result = parseMessage(buf, &cmd, &topic, &content);
      if (result == kSuccess)
      {
        if (cmd == "pub")
        {
          doPublish(conn->name(), topic, content, receiveTime);
        }
        else if (cmd == "sub")
        {
          LOG_INFO << conn->name() << " subscribes " << topic;
          doSubscribe(conn, topic);
        }
        else if (cmd == "unsub")
        {
          doUnsubscribe(conn, topic);
        }
        else
        {
          conn->shutdown();
          result = kError;
        }
      }
      else if (result == kError)
      {
        conn->shutdown();
      }
    }
    WeakEntryPtr weakEntry = conn_map_weak[conn];
    EntryPtr entry(weakEntry.lock());
    if(entry)
    {
        connectionBuckets_.back().insert(entry);
    }
  }

  typedef std::map<string, Topic> topicsMap;
  typedef ThreadLocalSingleton<topicsMap> LocalTopicMap_;
  void threadInit(EventLoop* loop)
  {
    assert(LocalTopicMap_::pointer()==NULL);
    LocalTopicMap_::instance();
    assert(LocalTopicMap_::pointer()!=NULL);

    MutexLockGuard lock(mutex_);
    loops_.insert(loop);
  }


  void timePublish()
  {
    Timestamp now = Timestamp::now();
    doPublish("internal", "utc_time", now.toFormattedString(), now);
  }

  void doSubscribe(const TcpConnectionPtr& conn,
                   const string& topic)
  {
    ConnectionSubscription* connSub
      = boost::any_cast<ConnectionSubscription>(conn->getMutableContext());

    connSub->insert(topic);
    getTopic(topic).add(conn);
    LOG_INFO<<CurrentThread::tid()<<" : "<<getTopic(topic).audiences_.size()<<"\n";
    for(auto it = getTopic(topic).audiences_.begin();it!=getTopic(topic).audiences_.end();++it)
        LOG_INFO<<(*it)->peerAddress().toIpPort()<<"\n";
  }

  void doUnsubscribe(const TcpConnectionPtr& conn,
                     const string& topic)
  {
    LOG_INFO << conn->name() << " unsubscribes " << topic;
    getTopic(topic).remove(conn);
    // topic could be the one to be destroyed, so don't use it after erasing.
    ConnectionSubscription* connSub
      = boost::any_cast<ConnectionSubscription>(conn->getMutableContext());
    connSub->erase(topic);
  }

  void doPublish(const string& source,
                 const string& topic,
                 const string& content,
                 Timestamp time)
  {
    MutexLockGuard lock(mutex_);
    for(std::set<EventLoop*>::iterator it = loops_.begin();it!=loops_.end();++it)
    {
        EventLoop::Functor f = boost::bind(&PubSubServer::doPublishInLoop,this,source,topic,content,time);
        (*it)->queueInLoop(f);
    }
    LOG_DEBUG;
  }

  void doPublishInLoop(const string& source,
                 const string& topic,
                 const string& content,
                 Timestamp time)
  {
        getTopic(topic).publish(content,time);
  }


  Topic& getTopic(const string& topic)
  {
    std::map<string, Topic>::iterator it = LocalTopicMap_::instance().find(topic);
    if (it == LocalTopicMap_::instance().end())
    {
      it = LocalTopicMap_::instance().insert(make_pair(topic, Topic(topic))).first;
    }
    return it->second;
  }

  void onTimer()
  {
     connectionBuckets_.push_back(Bucket());
  }

  typedef boost::weak_ptr<muduo::net::TcpConnection> WeakTcpConnectionPtr;
  struct Entry
  {
      explicit Entry(const WeakTcpConnectionPtr& weakConn)
      : weakConn_(weakConn)
    {
    }

     ~Entry()
     {
        muduo::net::TcpConnectionPtr conn = weakConn_.lock();
        if (conn)
        {
            conn->shutdown();
        }
     }

     WeakTcpConnectionPtr weakConn_;
  };
  typedef boost::shared_ptr<Entry> EntryPtr;
  typedef boost::weak_ptr<Entry> WeakEntryPtr;
  typedef boost::unordered_set<EntryPtr> Bucket;
  typedef boost::circular_buffer<Bucket> WeakConnectionList;

  TcpServer server_;
  std::set<EventLoop*> loops_;
  MutexLock mutex_;
  WeakConnectionList connectionBuckets_;
  const static int idleNum = 6;
  std::map<TcpConnectionPtr,WeakEntryPtr> conn_map_weak;
};

}

int main(int argc, char* argv[])
{
  if (argc > 1)
  {
    uint16_t port = static_cast<uint16_t>(atoi(argv[1]));
    EventLoop loop;
    pubsub::PubSubServer server(&loop, InetAddress(port));
    if (argc > 2)
    {
       server.setThreadNum(atoi(argv[2]));
    }
    server.start();
    loop.loop();
  }
  else
  {
    printf("Usage: %s pubsub_port [inspect_port]\n", argv[0]);
  }
}

