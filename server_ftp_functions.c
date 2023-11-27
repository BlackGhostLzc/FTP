#include <server_ftp.h>

static size_t size_packet = sizeof(struct packet);

struct client_info *client_info_alloc(int s, int c)
{
	struct client_info *ci = (struct client_info *)malloc(sizeof(struct client_info));
	ci->sfd = s;
	ci->cid = c;
	return ci;
}

void command_pwd(struct packet *shp, struct packet *data, int sfd_client, char *lpwd)
{
	int x;
	shp->type = DATA;
	strcpy(shp->buffer, lpwd);
	data = htonp(shp);
	if ((x = send(sfd_client, data, size_packet, 0)) != size_packet)
		er("send()", x);
}

void command_cd(struct packet *shp, struct packet *data, int sfd_client, char *message)
{
	int x;
	shp->type = INFO;
	strcpy(shp->buffer, message);
	data = htonp(shp);
	if ((x = send(sfd_client, data, size_packet, 0)) != size_packet)
		er("send()", x);
}

void command_ls(struct packet *shp, struct packet *data, int sfd_client, char *lpwd)
{
	int x;
	shp->type = DATA;
	DIR *d = opendir(lpwd);
	if (!d)
		er("opendir()", (int)d);
	struct dirent *e;
	while (e = readdir(d))
	{
		sprintf(shp->buffer, "%s\t%s", e->d_type == 4 ? "DIR:" : e->d_type == 8 ? "FILE:"
																				: "UNDEF:",
				e->d_name);
		data = htonp(shp);
		if ((x = send(sfd_client, data, size_packet, 0)) != size_packet)
			er("send()", x);
	}
	send_EOT(shp, data, sfd_client);
}

void command_get(struct packet *shp, struct packet *data, int sfd_client)
{
	int x;
	FILE *f = fopen(shp->buffer, "rb"); // Yo!
	shp->type = INFO;
	shp->comid = GET;
	strcpy(shp->buffer, f ? "File found; processing" : "Error opening file.");
	// printpacket(shp, HP);
	data = htonp(shp);
	if ((x = send(sfd_client, data, size_packet, 0)) != size_packet)
		er("send()", x);
	if (f)
	{
		shp->type = DATA;
		send_file(shp, data, sfd_client, f);
		fclose(f);
	}
	send_EOT(shp, data, sfd_client);
}

void command_put(struct packet *shp, struct packet *data, int sfd_client)
{
	int x;
	FILE *f = fopen(shp->buffer, "wb");
	shp->type = INFO;
	shp->comid = PUT;
	strcpy(shp->buffer, f ? "Everything in order; processing" : "Error opening file for writing on server side.");
	// printpacket(shp, HP);
	data = htonp(shp);
	if ((x = send(sfd_client, data, size_packet, 0)) != size_packet)
		er("send()", x);
	if (f)
	{
		receive_file(shp, data, sfd_client, f);
		fclose(f);
	}
}

void command_mkdir(struct packet *shp, struct packet *data, int sfd_client)
{
	char message[LENBUFFER];
	DIR *d = opendir(shp->buffer);
	if (d)
	{
		strcpy(message, "already exists");
		closedir(d);
	}
	else if (mkdir(shp->buffer, 0777) == -1)
	{
		fprintf(stderr, "Wrong path.\n");
		strcpy(message, "fail");
	}
	else
		strcpy(message, "success");
	int x;
	shp->type = INFO;
	strcpy(shp->buffer, message);
	data = htonp(shp);
	if ((x = send(sfd_client, data, size_packet, 0)) != size_packet)
		er("send()", x);
}

void command_rm(struct packet *shp, struct packet *data, int sfd_client)
{
	char message[LENBUFFER];
	int ret = -1;

	struct stat path_stat;
	if (stat(shp->buffer, &path_stat) == 0)
	{
		if (S_ISREG(path_stat.st_mode))
		{
			printf("The path is a regular file.\n");
			ret = remove(shp->buffer);
		}
		else if (S_ISDIR(path_stat.st_mode))
		{
			printf("The path is a directory.\n");
			ret = rmdir(shp->buffer);
		}
		else
		{
			printf("The path is neither a regular file nor a directory.\n");
		}
	}
	else
	{
		printf("Unable to get the status of the path.\n");
	}

	if (ret == -1)
	{
		fprintf(stderr, "Wrong path.\n");
		strcpy(message, "fail");
	}
	else
		strcpy(message, "success");

	int x;
	shp->type = INFO;
	strcpy(shp->buffer, message);
	data = htonp(shp);
	if ((x = send(sfd_client, data, size_packet, 0)) != size_packet)
		er("send()", x);
}

void command_rget(struct packet *shp, struct packet *data, int sfd_client)
{
	static char lpwd[LENBUFFER];
	if (!getcwd(lpwd, sizeof lpwd))
		er("getcwd()", 0);
	int x;
	DIR *d = opendir(lpwd);
	if (!d)
		er("opendir()", (int)d);
	struct dirent *e;
	while (e = readdir(d))
		if (e->d_type == 4 && strcmp(e->d_name, ".") && strcmp(e->d_name, ".."))
		{
			shp->type = REQU;
			shp->comid = LMKDIR;
			strcpy(shp->buffer, e->d_name);
			data = htonp(shp);
			// fprintf(stderr, "LMKDIR: e->d_name = <%s>\n", e->d_name);
			// printpacket(shp, HP);
			if ((x = send(sfd_client, data, size_packet, 0)) != size_packet)
				er("send()", x);

			shp->type = REQU;
			shp->comid = LCD;
			strcpy(shp->buffer, e->d_name);
			data = htonp(shp);
			// fprintf(stderr, "LCD: e->d_name = <%s>\n", e->d_name);
			// printpacket(shp, HP);
			if ((x = send(sfd_client, data, size_packet, 0)) != size_packet)
				er("send()", x);
			if ((x = chdir(e->d_name)) == -1)
				er("chdir()", x);

			command_rget(shp, data, sfd_client);

			shp->type = REQU;
			shp->comid = LCD;
			strcpy(shp->buffer, "..");
			data = htonp(shp);
			// fprintf(stderr, "LCD: <..>\n");
			// printpacket(shp, HP);
			if ((x = send(sfd_client, data, size_packet, 0)) != size_packet)
				er("send()", x);
			if ((x = chdir("..")) == -1)
				er("chdir()", x);
		}
		else if (e->d_type == 8)
		{
			shp->type = REQU;
			shp->comid = GET;
			strcpy(shp->buffer, e->d_name);
			data = htonp(shp);
			// fprintf(stderr, "GET: e->d_name = <%s>\n", e->d_name);
			// printpacket(shp, HP);
			if ((x = send(sfd_client, data, size_packet, 0)) != size_packet)
				er("send()", x);
			if ((x = recv(sfd_client, data, size_packet, 0)) == 0)
				er("recv()", x);
			shp = ntohp(data);
			if (shp->type == REQU && shp->comid == GET)
				command_get(shp, data, sfd_client);
		}
	closedir(d);
}
