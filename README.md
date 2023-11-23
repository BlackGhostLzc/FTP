## 使用介绍
 
1. get : 客户端下载文件
2. mget : 客户端批量下载
3. rget : 递归下载文件(夹)

4. put : 客户端上传文件
5. mput : 客户端批量上传文件
6. rput : 递归上传文件(夹)

7. ls : 列出服务器当前路径的文件
8. lls : 列出客户端当前路径的文件

9. mkdir : 让服务器创建文件夹
10. lmkdir : 客户端创建文件夹

11. cd : 服务器进入目录
12. lcd : 客户端进入目录

--------


## 增加的功能
1. 支持多线程
这也引发了一个问题，多线程处理各客户端的cd命令会出现bug，线程之间共享一个 cwd (current working directory)。

2. 解决方法
为 client_info 添加一个字段 `char last_pwd[MAX_PATH]` 表示上一次处理客户端请求该线程所在的目录。
```c
struct client_info
{
	int sfd;
	int cid;
	char last_pwd[MAX_PATH];
};
```
然后还需要一把锁，在服务器处理cd命令的时候锁住这把锁，同时处理线程在执行命令前都需要`chdir(ci->last_pwd)`。
