#include <bits/stdc++.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <mutex>
//最大消息长度
#define MAX_LEN 200
//定义颜色的个数
#define NUM_COLORS 6

using namespace std;
// 终端的结构体
struct terminal
{	
	//该终端的id编号便于管理
	int id;
	//该终端用户名
	string name;
	//该终端用于通信的套接字
	int socket;
	//接管该终端的线程
	thread th;
};
// 创建一个容器用于存放多个客户端
vector<terminal> clients;
//使用了ANSI转义码，在终端输出时改变文本颜色
string def_col = "\033[0m";
//定义了六种颜色，用于区分用户
string colors[] = {"\033[31m", "\033[32m", "\033[33m", "\033[34m", "\033[35m", "\033[36m"};
int seed = 0;
//线程锁
mutex cout_mtx, clients_mtx;
//随机生成颜色函数
string color(int code);
//设置名字函数
void set_name(int id, char name[]);
//确保在多线程执行下输出有序
void shared_print(string str, bool endLine);
//发给处理sender_id以外的所有人
void broadcast_message(string message, int sender_id);
void broadcast_message(int num, int sender_id);
//剔除id这个客户
void end_connection(int id);
//处理发送给用户的消息
void handle_client(int client_socket, int id);

int main()
{	
	//创建server_socket，并检查错误
	int server_socket;
	if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("socket error: ");
		exit(-1);
	}
	//将server_socket绑定在本地的指定端口
	struct sockaddr_in server;
	server.sin_family = AF_INET;
	//将主机的第10000号进程绑定到结构体上
	server.sin_port = htons(10000);
	//提供任何可用的ip地址
	server.sin_addr.s_addr = INADDR_ANY;
	bzero(&server.sin_zero, 0);
	if ((bind(server_socket, (struct sockaddr *)&server, sizeof(struct sockaddr_in))) == -1)
	{
		perror("bind error");
		exit(-1);
	}
	//监听客户端响应
	if ((listen(server_socket, 8)) == -1)
	{
		perror("listen error: ");
		exit(-1);
	}
	//创建client_socket用来承接监听到的连接请求
	struct sockaddr_in client;
	int client_socket;
	unsigned int len = sizeof(sockaddr_in);
	//输出欢迎语
	cout << colors[NUM_COLORS - 1] << "\n\t  ====== 欢迎来到聊天室 ======   " << endl
		 << def_col;

	while (1)
	{	
		//client_socket接收连接请求
		if ((client_socket = accept(server_socket, (struct sockaddr *)&client, &len)) == -1)
		{
			perror("accept error: ");
			exit(-1);
		}
		seed++;
		//创建一个线程来接管这个客户端
		thread t(handle_client, client_socket, seed);
		lock_guard<mutex> guard(clients_mtx);
		//将该终端插入clients列表中
		clients.push_back({seed, string("匿名"), client_socket, (move(t))});
	}
	//将clients列表中的所有客户端都连接进程，将主线程进行阻塞
	for (int i = 0; i < clients.size(); i++)
	{
		if (clients[i].th.joinable())
			clients[i].th.join();
	}
	//关闭server_socket
	close(server_socket);
	return 0;
}

string color(int code)
{
	return colors[code % NUM_COLORS];
}

void set_name(int id, char name[])
{
	for (int i = 0; i < clients.size(); i++)
	{
		if (clients[i].id == id)
		{
			clients[i].name = string(name);
		}
	}
}

void shared_print(string str, bool endLine = true)
{	
	//先上锁在进行输出操作
	lock_guard<mutex> guard(cout_mtx);
	cout << str;
	if (endLine)
		cout << endl;
}

void broadcast_message(string message, int sender_id)
{
	char temp[MAX_LEN];
	//将string类型的字符串转换成cstring类型
	strcpy(temp, message.c_str());
	//将消息发送到指定客户端
	for (int i = 0; i < clients.size(); i++)
	{
		if (clients[i].id != sender_id)
		{
			send(clients[i].socket, temp, sizeof(temp), 0);
		}
	}
}

void broadcast_message(int num, int sender_id)
{
	for (int i = 0; i < clients.size(); i++)
	{
		if (clients[i].id != sender_id)
		{
			send(clients[i].socket, &num, sizeof(num), 0);
		}
	}
}

void end_connection(int id)
{
	for (int i = 0; i < clients.size(); i++)
	{
		if (clients[i].id == id)
		{
			lock_guard<mutex> guard(clients_mtx);
			clients[i].th.detach();
			clients.erase(clients.begin() + i);
			close(clients[i].socket);
			break;
		}
	}
}

void handle_client(int client_socket, int id)
{
	char name[MAX_LEN], str[MAX_LEN];
	recv(client_socket, name, sizeof(name), 0);
	set_name(id, name);

	//发送加入消息
	string welcome_message = string(name) + string(" 加入了聊天");
	broadcast_message("#NULL", id);
	broadcast_message(id, id);
	broadcast_message(welcome_message, id);
	shared_print(color(id) + welcome_message + def_col);

	while (1)
	{
		int bytes_received = recv(client_socket, str, sizeof(str), 0);
		if (bytes_received <= 0)
			return;
		if (strcmp(str, "#exit") == 0)
		{
			//发送离开消息
			string message = string(name) + string(" 离开了");
			broadcast_message("#NULL", id);
			broadcast_message(id, id);
			broadcast_message(message, id);
			shared_print(color(id) + message + def_col);
			end_connection(id);
			return;
		}
		broadcast_message(string(name), id);
		broadcast_message(id, id);
		broadcast_message(string(str), id);
		shared_print(color(id) + name + " : " + def_col + str);
	}
}
