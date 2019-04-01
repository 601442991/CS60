# UDP套接字编程

## 描述
《自顶向下方法（原书第6版）》第2.7.1节给出了一个使用Python的UDP套接字编程实例，实现了一个简单的UDP通信程序。

书中代码基于Python2，本文采用Python3，所以针对字符编码问题做了一些简单修改。

## 代码

客户端程序`UDPClient.py`创建一个UDP套接字，并在用户输入一段小写字母组成的字符串后，发送到指定服务器地址和对应端口，等待服务器返回消息后，将消息显示出来。

服务端程序`TCPServer.py`一直保持一个可连接的UDP套接字，在接收到字符串后，将其改为大写，然后向客户端返回修改后的字符串。 


**代码文件：**

[UDPClient.py](source/UDPClient.py)

[UDPServer.py](source/UDPServer.py)
