#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <dirent.h>

#define BUFSIZE 1024
#define BUFLEN 50

struct file_stat{
	int flag;				
	char filename[512];
	uid_t st_uid;
	gid_t st_gid;
	off_t st_size;
	mode_t st_mode;
};

static struct option long_options[] = {
		{"name", 1, 0, 0},
		{"size", 1, 0, 0},
		{"type", 1, 0, 0},
		{"uid",  1, 0, 0},
		{"gid",  1, 0, 0},
		{0,		 0, 0, 0},
};

void do_ls(struct file_stat *buf);

void do_name(struct file_stat *buf, char arg[]);
void do_size(struct file_stat *buf, char arg[]);
void do_type(struct file_stat *buf, char arg[]);
void do_uid(struct file_stat *buf, char arg[]);
void do_gid(struct file_stat *buf, char arg[]);

void do_print(struct file_stat *buf);


int main(int argc, char const *argv[])

{
	int opt = 0;
	int opt_index = 0;
	char opt_name[BUFSIZE] = {0};
	struct file_stat *buf = (struct file_stat *)malloc(BUFLEN * sizeof(struct file_stat));
	do_ls(buf);

	while (!(opt = getopt_long_only(argc, argv, "name", long_options, &opt_index))) {

		strncpy(opt_name, long_options[opt_index].name, strlen(long_options[opt_index].name));

		switch (opt) {
			case 0:
				if (strcmp(opt_name, "name") == 0) 
					{do_name(buf, optarg);} 
				else if (strcmp(opt_name, "size") == 0) 
					{do_size(buf, optarg);}
				 else if (strcmp(opt_name, "type") == 0) 
					{do_type(buf, optarg);}
				 else if (strcmp(opt_name, "uid") == 0)
					{do_uid(buf, optarg);}
				 else if (strcmp(opt_name, "gid") == 0) 
					{do_gid(buf, optarg);}
				
				break;
		}
		memset(opt_name, 0, strlen(opt_name));
	}
	do_print(buf);
}

void do_ls(struct file_stat *buf)
{
	DIR *dir_ptr;
	struct dirent *direntp;
	struct stat info;
	int i = 0;

	if ((dir_ptr = opendir(".")) == NULL)
		return;
	else {
		while ((direntp = readdir(dir_ptr)) != NULL) {
			stat(direntp->d_name, &info);
			buf[i].flag = 1;
			strcpy((&buf[i])->filename, direntp->d_name);
			buf[i].st_uid = info.st_uid;
			buf[i].st_gid = info.st_gid;
			buf[i].st_size = info.st_size;
			buf[i].st_mode = info.st_mode;
			i++;
		}
	}
}


void do_print(struct file_stat *buf)
{
	int i;
	for (i = 0; i < BUFLEN; i++) {
		if (buf[i].flag == 1 && strcmp((&buf[i])->filename, ".") != 0 && strcmp((&buf[i])->filename, "..") != 0) {
			printf("%s\n", (&buf[i])->filename);
		}
	}
}

void do_name(struct file_stat *buf, char arg[])
{
	int i;
	for (i = 0; i < BUFLEN; i++) {
		if (strstr((&buf[i])->filename, arg) == NULL)
			buf[i].flag = 0;
	}
}

void do_size(struct file_stat *buf, char arg[])
{
	int lmt_size = atoi(arg);
	int i;
	for (i = 0; i < BUFLEN; i++) {
		if ((int)(&buf[i])->st_size > lmt_size)
			buf[i].flag = 0;
	}
}


void do_type(struct file_stat *buf, char arg[])
{
	int i;
	if (strcmp(arg, "d") == 0) {
		// is dir
		for (i = 0; i < BUFLEN; i++){
			if (!S_ISDIR((&buf[i])->st_mode))
				buf[i].flag = 0;
		}

	} else if (strcmp(arg, "c") == 0) {
		// is char dev
		for (i = 0; i < BUFLEN; i++){
			if (!S_ISCHR((&buf[i])->st_mode))
				buf[i].flag = 0;
		}

	} else if (strcmp(arg, "b") == 0) {
		// is block dev
		for (i = 0; i < BUFLEN; i++){
			if (!S_ISBLK((&buf[i])->st_mode))
				buf[i].flag = 0;
		}
	} else if (strcmp(arg, "l") == 0) {
		// is sybolic link
		for (i = 0; i < BUFLEN; i++){
			if (!S_ISLNK((&buf[i])->st_mode))
				buf[i].flag = 0;
		}
	} else if (strcmp(arg, "-") == 0) {
		// is regular file
		for (i = 0; i < BUFLEN; i++){
			if (!S_ISREG((&buf[i])->st_mode))
				buf[i].flag = 0;
		}
	} else {
		printf("wrong para in -type, exiting\n");
		exit -1;
	}
}


void do_uid(struct file_stat *buf, char arg[])
{
	int uid = atoi(arg);
	int i;
	for (i = 0; i < BUFLEN; i++)
	{
		if ((int)(&buf[i])->st_uid != uid)
			buf[i].flag = 0;
	}
}

void do_gid(struct file_stat *buf, char arg[])
{
	int gid = atoi(arg);
	int i;
	for (i = 0; i < BUFLEN; i++)
	{
		if ((int)(&buf[i])->st_gid != gid)
			buf[i].flag = 0;
	}
}