#include <commons.h>
#include <file_transfer_functions.h>
#include <pthread.h>
#include <time.h>
/*
	for:
		ctime()
*/

#include <sys/stat.h>
/*
	for:
		stat()
*/

#define ID "SERVER=> "

#define MAX_PATH 64
#define MAX_CLIENT_NUM 128
struct client_info
{
	int sfd;
	int cid;
	char last_pwd[MAX_PATH];
};

struct client_info *client_info_alloc(int, int);

void command_pwd(struct packet *, struct packet *, int, char *);
void command_cd(struct packet *, struct packet *, int, char *);
void command_ls(struct packet *, struct packet *, int, char *);
void command_get(struct packet *, struct packet *, int);
void command_put(struct packet *, struct packet *, int);
void command_mkdir(struct packet *, struct packet *, int);
void command_rget(struct packet *, struct packet *, int);
void command_rm(struct packet *, struct packet *, int);
