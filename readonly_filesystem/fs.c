/* System Programming Project 2
**Refik Ozgun Yesildag - 150150067
**Sinem Elif Haseki - 150160026
**Hakan Sarac - 150140061

** Compilation:
** gcc fs.c -o fs -ansi -std=c99 -g -ggdb -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -lfuse -lmagic -lansilove
**
** Mount: ./sysproj -d src dst Unmount: fusermount -u dst
*/
#define FUSE_USE_VERSION 26

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/xattr.h>
#include <dirent.h>
#include <unistd.h>
#include <fuse.h>
#include <magic.h>
#include <ansilove.h>

char* rw_path;

#define TXTS 2
#define NONTEXT 3
#define DIRECTT 4 

//for determining whether its a text file, nontext file or directory
int file_type_finder(char* path) { //returns 2 if text, 3 if nontext, 4 if directory, based on the definitions above
    const char *magic_full, *cmp;
    magic_t magic_cookie;
    magic_cookie = magic_open(MAGIC_MIME);

    if (magic_cookie == NULL) {
        printf("unable to initialize magic library\n");
        return 1;
    }

    if (magic_load(magic_cookie, NULL) != 0) {
        printf("cannot load magic database - %s\n", magic_error(magic_cookie));
        magic_close(magic_cookie);
        return 1;
    }

    magic_full = magic_file(magic_cookie, path);
    if (magic_full[0] == 't' && magic_full[1] == 'e') { //text/....
        magic_close(magic_cookie);
        return TXTS; //we didn't return 1 because the previous if case returns 1 on any error circumstance
    } 
    else if (magic_full[0] == 'a' && magic_full[1] == 'p') { //application/...
        cmp = strchr(magic_full, '/');
        if (cmp[1] == 'o') {// application/octet-stream
            magic_close(magic_cookie);
            return TXTS;
        }
        
        magic_close(magic_cookie);
        return NONTEXT; //if it's in the application section but not octet-stream
        
    }
    else if (magic_full[0] == 'i' && magic_full[1] == 'n' && magic_full[6] == 'd') {// inode (not image)
        magic_close(magic_cookie);
        return DIRECTT;
    }
    
    magic_close(magic_cookie);
    return NONTEXT; //else it's non-text file
}


char* delete_ext(const char * path){ // delete extension of given path and return the deleted path

    char * ext = strrchr(path, '.');
    int ext_len  = strlen(ext);
    int path_len = strlen(path);
    char * temp_path = (char*)malloc(path_len - ext_len + 1);

    strncpy(temp_path, path, path_len - ext_len);
    temp_path[path_len-ext_len] = '\0';
    

    return temp_path;

}

char* change_ext(const char * path, const char * ext){ // change the extension of given path to png
	
    if(path == NULL || strcmp(path,".") == 0 || strcmp(path,"..") == 0){
        char * temp_dir = (char*)malloc(strlen(path) + 1);
        strncpy(temp_dir, path, strlen(path));
        temp_dir[strlen(path)] = '\0';
        return temp_dir;
    }
    char* deleted_path = delete_ext(path); //call delete extension first
	//then add the desired extension to the given path
    char* temp_path = (char *)malloc(strlen(deleted_path) + strlen(ext) + 1);
    strncpy(temp_path, deleted_path, strlen(deleted_path));
    temp_path[strlen(deleted_path)]='\0';
    strcat(temp_path, ext);
    temp_path[strlen(deleted_path)+strlen(ext)] = '\0';
    
    temp_path[strlen(temp_path)] = '\0';
    return temp_path;
}


static char* translate_path(const char* path)
{

	char* rPath = malloc(sizeof(char) * (strlen(path) + strlen(rw_path) + 1));

	strcpy(rPath, rw_path);
	if (rPath[strlen(rPath) - 1] == '/') {
		rPath[strlen(rPath) - 1] = '\0';
	}
	strcat(rPath, path);

	return rPath;
}
char* path_finder(const char* path) // finds the exact (real) path of requested png file by checking its pure name
{
	struct dirent* de;
	DIR *direc;
    int i;
    char *srcfilename, *retval;
    char *pure_name = delete_ext(path);
    //get the string starting from /, subtract with the whole path to get its size, allocate memory
    //subtracting these from each other gives the number of chars before the / char
    char *directory = (char *)malloc( (strrchr(path, '/')-path) + 1); //+1 for \0
    //copy path until last / into directory
    for (i=0; i<=(strrchr(path, '/')-path); i++){
        directory[i]=path[i];
    }
	directory[(strrchr(path, '/')-path)] = '\0';
	direc = opendir(directory);

	if (direc == NULL)
		return "";

	while ((de = readdir(direc)) != NULL)
	{
		char * file_path = (char*)malloc(strlen(de->d_name)+strlen(directory)+6);
		strcpy(file_path, directory);
		file_path[strlen(file_path)] = '\0';
		strcat(file_path, "/");
		file_path[strlen(file_path)] = '\0';
		strcat(file_path, de->d_name);
		file_path[strlen(file_path)] = '\0';


		if (strstr(file_path, pure_name) != NULL)
		{
			free(pure_name);
			free(directory);
			closedir(direc);
			
            return file_path;
			
		}
		if(file_path) free(file_path);
	}

}

static int fs_getattr(const char* path, struct stat* st_data) { // get the attribute of the file 
	int res;
	char* upath = translate_path(path);

	if (strstr(upath, ".png") != NULL) //if upath contains the .png substring
	{
		char* exact_path = path_finder(upath);
		if (exact_path != NULL)
		{
			res = lstat(exact_path, st_data);
			if(upath) free(upath);
			if(exact_path) free(exact_path);
		}
	}
	else{
		res = lstat(upath, st_data);
		free(upath);
		if (res == -1) {
			return -errno;
		}
	}

	return 0;
}

static int fs_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi)
{
	DIR* dp;
	struct dirent* de;
	int res;

	(void)offset;
	(void)fi;

	char* upath = translate_path(path);
	dp = opendir(upath);
	if (dp == NULL) {
		res = -errno;
		return res;
	}

	while ((de = readdir(dp)) != NULL) { 
		struct stat st;
		memset(&st, 0, sizeof(st));
		st.st_ino = de->d_ino;
		st.st_mode = de->d_type << 12;
		
		
		char * changed_path = (char*)malloc(strlen(upath)+strlen(de->d_name) + 6); 
		strcpy(changed_path, upath);
		changed_path[strlen(changed_path)] = '\0';
		strcat(changed_path, "/");
		changed_path[strlen(changed_path)] = '\0';
		strcat(changed_path, de->d_name);
		changed_path[strlen(changed_path)] = '\0';
		
		//get the real path of the desired path first
		int type = file_type_finder(changed_path);

		if (changed_path) free(changed_path);

		//if it's real extension is convertible to png, change extension to png
		if (type == TXTS){
			char* changed_name = change_ext(de->d_name, ".png");
			
				if (filler(buf, changed_name, &st, 0)) {
					if (changed_name)free(changed_name);
					break;
				}
				
				if (changed_name) free(changed_name);
		}
		//if it's a directory, use the filler, can't change extension here
		else if(type == DIRECTT){
				
			if (filler(buf, de->d_name, &st, 0))
				break;
		}
	} //if it's neither a text file nor a directory, don't do anything
	
	if (upath)free(upath); //free upath after usage
	closedir(dp);
	return 0;
}

static int fs_open(const char* path, struct fuse_file_info* finfo){
	int res;

	int flags = finfo->flags;

	if ((flags & O_WRONLY) || (flags & O_RDWR) || (flags & O_CREAT) || (flags & O_EXCL) || (flags & O_TRUNC) || (flags & O_APPEND)) {
		return -EROFS;
	}

	char* upath = translate_path(path);

	//if upath contains the substring ".png"
	if (strstr(upath, ".png") != NULL){
		upath = path_finder(upath); //then find its real path
	}


	res = open(upath, flags);
	free(upath);
	
	if (res == -1) {
		return -errno;
	}
	
	close(res);
	
	return 0;
}

static int fs_read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* finfo)
{
	int fd;
	int res;
	(void)finfo;
	struct ansilove_ctx ctx;
	struct ansilove_options options;
	size_t length;
	char* upath = translate_path(path);

	if (strstr(upath, ".png") != NULL)
	{
		upath = path_finder(upath);
	}

	
	ansilove_init(&ctx, &options);
	ansilove_loadfile(&ctx, upath);
	ansilove_ansi(&ctx, &options);
	length = ctx.png.length;
	if (offset < length){ //check the offset value, read if length not exceeded
		if (offset + size > length){ //you may have started from a valid position, but when size is added it might exceed length
			size = length - offset; //if exceeded, assign the remaining to size 
		}
		memcpy(buf, (char*)ctx.png.buffer + offset, size);
	}
	else
		size = 0;
	
	if(upath) free(upath);
	ansilove_clean(&ctx);
	return size;
}


struct fuse_operations fs_oper = {
	.getattr = fs_getattr,
	.readdir = fs_readdir,
	.open = fs_open,
	.read = fs_read,
};

static int fs_parse_opt(void* data, const char* arg, int key,
	struct fuse_args* outargs)
{
	(void)data;

	switch (key)
	{
	case FUSE_OPT_KEY_NONOPT:
		if (rw_path == 0)
		{
			rw_path = strdup(arg);
			return 0;
		}
		else
		{
			return 1;
		}
	case FUSE_OPT_KEY_OPT:
		return 1;

	default:
		exit(1);
	}
	return 1;
}

int main(int argc, char* argv[])
{
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	int res;

	res = fuse_opt_parse(&args, &rw_path, NULL, fs_parse_opt);
	if (res != 0)
	{
		fprintf(stderr, "Invalid arguments\n");
		exit(1);
	}
	if (rw_path == 0)
	{
		fprintf(stderr, "Missing readwritepath\n");
		exit(1);
	}

	fuse_main(args.argc, args.argv, &fs_oper, NULL);

	return 0;
}
