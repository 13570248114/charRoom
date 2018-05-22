#include "pubsub.h"
#include <muduo/base/ProcessInfo.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/EventLoopThread.h>

#include <iostream>
#include <sstream>

#include <stdio.h>

using namespace muduo;
using namespace muduo::net;
using namespace pubsub;

EventLoop* g_loop = NULL;
string g_topic;
string g_content;

void subscription(const string& topic, const string& content, Timestamp)
{
  printf("%s: %s\n", topic.c_str(), content.c_str());
}


void connection(PubSubClient* client)
{
  if (client->connected())
  {
  }
  else
  {
    g_loop->quit();
  }
}

int main(int argc, char* argv[])
{
  if (argc == 2)
  {
    string hostport = argv[1];
    size_t colon = hostport.find(':');
    if (colon != string::npos)
    {
      string hostip = hostport.substr(0, colon);
      uint16_t port = static_cast<uint16_t>(atoi(hostport.c_str()+colon+1));

      string name = ProcessInfo::username()+"@"+ProcessInfo::hostname();
      name += ":" + ProcessInfo::pidString();
   
      EventLoopThread loopThread;
      g_loop = loopThread.startLoop();
      PubSubClient client(g_loop, InetAddress(hostip, port), name);
      client.start();
      string line;
      string cmd;
      
      while (getline(std::cin, line)&&client.connected())
      {
          std::stringstream ss(line.c_str());
          ss>>cmd>>g_topic>>g_content;
            if(cmd=="pub")
                client.publish(g_topic, g_content);
            else if(cmd=="sub")
                client.subscribe(g_topic,subscription);         
      } 
      client.stop();
    }
    else
    {
      printf("Usage: %s hub_ip:port topic content\n", argv[0]);
    }
  }
  else
  {
    printf("Usage: %s hub_ip:port topic content\n"
           "Read contents from stdin:\n"
           "  %s hub_ip:port topic -\n", argv[0], argv[0]);
  }
}
