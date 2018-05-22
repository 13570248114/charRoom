# chatRoom
一个基于muduo网络库的简单聊天程序(必须先安装muduo库).
## 示例:
- 打开服务器(端口绑定在5555,开启3个IO线程):    
```
lin in ~/chatRoom on master λ ./hub 5555 3  
```
- 打开2个客户端(我指定了本地地址和端口5555),并都用sub命令进入名字为my的房间: 
```
lin in ~/chatRoom λ ./client 127.0.0.1:5555
20180522 02:58:09.443982Z  8294 INFO  TcpClient::TcpClient[lin@lin:8294] - connector 0xBC0E80 - TcpClient.cc:71
20180522 02:58:09.444103Z  8294 INFO  TcpClient::connect[lin@lin:8294] - connecting to 127.0.0.1:5555 - TcpClient.cc:109
sub my
```

```
lin in ~/chatRoom λ ./client 127.0.0.1:5555
20180522 03:02:02.008278Z  8387 INFO  TcpClient::TcpClient[lin@lin:8387] - connector 0x2594E80 - TcpClient.cc:71
20180522 03:02:02.008392Z  8387 INFO  TcpClient::connect[lin@lin:8387] - connecting to 127.0.0.1:5555 - TcpClient.cc:109
sub my
```
- 客户1在my的房间发表言论,客户1和客户2都收到消息:  

客户1:
```
pub my hello
my: hello
```
客户2:  
```
my: hello
```


