#include <server_ftp.h>

size_t size_sockaddr = sizeof(struct sockaddr), size_packet = sizeof(struct packet);

void *serve_client(void *);

pthread_mutex_t access_server_pwd_mutex = PTHREAD_MUTEX_INITIALIZER;

int main(void)
{
	// BEGIN: initialization
	struct sockaddr_in sin_server, sin_client;
	int sfd_server, sfd_client, x;
	short int connection_id = 0;
	pthread_t thread_id;

	if ((x = sfd_server = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
		er("socket()", x);

	memset((char *)&sin_server, 0, sizeof(struct sockaddr_in));
	sin_server.sin_family = AF_INET;
	sin_server.sin_port = htons(PORTSERVER);
	sin_server.sin_addr.s_addr = htonl(INADDR_ANY);

	if ((x = bind(sfd_server, (struct sockaddr *)&sin_server, size_sockaddr)) < 0)
		er("bind()", x);

	if ((x = listen(sfd_server, MAX_CLIENT_NUM)) < 0)
		er("listen()", x);

	printf(ID "FTP Server started up @ local:%d. Waiting for client(s)...\n\n", PORTSERVER);
	// END: initialization

	char initial_pwd[64];
	if (getcwd(initial_pwd, sizeof(initial_pwd)) != NULL)
	{
		printf("Current working directory: %s\n", initial_pwd);
	}

	while (1)
	{
		if ((x = sfd_client = accept(sfd_server, (struct sockaddr *)&sin_client, &size_sockaddr)) < 0)
			er("accept()", x);
		printf(ID "Communication started with %s:%d\n", inet_ntoa(sin_client.sin_addr), ntohs(sin_client.sin_port));
		fflush(stdout);

		struct client_info *ci = client_info_alloc(sfd_client, connection_id++);

		// strcpy(ci->last_pwd, "/home/blackghost/Desktop/project/FTP/bin/server");
		strcpy(ci->last_pwd, initial_pwd);
		// printf("%s\n", ci->last_pwd);
		// serve_client(ci);
		// 这里换成多线程处理虽然支持多用户同时访问，但就会有一些问题暴露出来了。
		// 例如，两个用户不会知道对方的cd命令，会改变server服务器的工作路径。
		// 如何保持各个独立的工作路径呢？
		// 简单一点， 一把大锁保平安
		pthread_create(&thread_id, NULL, (void *)(serve_client), (void *)(ci));
	}

	close(sfd_server);
	printf(ID "Done.\n");
	fflush(stdout);

	return 0;
}

void *serve_client(void *info)
{
	int sfd_client, connection_id, x;
	struct packet *data = (struct packet *)malloc(size_packet);
	struct packet *shp;
	char lpwd[LENBUFFER];
	struct client_info *ci = (struct client_info *)info;
	sfd_client = ci->sfd;
	connection_id = ci->cid;

	// ci->last_pwd保存的是上一次客户端访问服务器时服务器所在的目录，cd时要修改ci->last_pwd
	// 每条访问要上锁

	while (1)
	{
		if ((x = recv(sfd_client, data, size_packet, 0)) == 0)
		{
			fprintf(stderr, "client closed/terminated. closing connection.\n");
			break;
		}

		/*
		pwd_lock();
		chdir(ci->last_pwd);
		*/

		pthread_mutex_lock(&access_server_pwd_mutex);
		chdir(ci->last_pwd);

		shp = ntohp(data);

		if (shp->type == TERM)
			break;

		if (shp->conid == -1)
			shp->conid = connection_id;

		if (shp->type == REQU)
		{
			switch (shp->comid)
			{
			case PWD:
				if (!getcwd(lpwd, sizeof lpwd))
					er("getcwd()", 0);
				command_pwd(shp, data, sfd_client, lpwd);
				break;
			case CD:
				if ((x = chdir(shp->buffer)) == -1)
					fprintf(stderr, "Wrong path.\n");
				command_cd(shp, data, sfd_client, x == -1 ? "fail" : "success");
				// 得到当前目录， 并保存在ci->last_pwd中
				char cwd[64]; // 用于存储当前目录的数组
				if (getcwd(cwd, sizeof(cwd)) != NULL)
				{
					printf("Current working directory: %s\n", cwd);
				}
				strcpy(ci->last_pwd, cwd);
				break;
			case MKDIR:
				command_mkdir(shp, data, sfd_client);
				break;
			case LS:
				if (!getcwd(lpwd, sizeof lpwd))
					er("getcwd()", 0);
				command_ls(shp, data, sfd_client, lpwd);
				break;
			case GET:
				command_get(shp, data, sfd_client);
				break;
			case PUT:
				command_put(shp, data, sfd_client);
				break;
			case RGET:
				if (!getcwd(lpwd, sizeof lpwd))
					er("getcwd()", 0);
				command_rget(shp, data, sfd_client);
				send_EOT(shp, data, sfd_client);
				if ((x = chdir(lpwd)) == -1)
					fprintf(stderr, "Wrong path.\n");

			case RM:
				command_rm(shp, data, sfd_client);
				break;
			default:
				// print error
				break;
			}
		}
		else
		{
			// show error, send TERM and break
			fprintf(stderr, "packet incomprihensible. closing connection.\n");
			send_TERM(shp, data, sfd_client);
			break;
		}

		pthread_mutex_unlock(&access_server_pwd_mutex);
	}

	close(sfd_client);
	fflush(stdout);
}
