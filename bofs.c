/*
 * BFS - The basic filesystem for FUSE.

 */


#define FUSE_USE_VERSION 26

static const char* bofsVersion = "2018.12.12";

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
#include <tidy.h>
#include <tidybuffio.h>


// Global to store our read-write path
char *rw_path;
//Extension of the html files
char *html_extension = "html";
//tidied string
char *tidied;
//Expected config file name
char *sys1 = "/config.config\0";
//a index
int traveler = 0;
int globalBuffer = 0;

//Tidy function will be hereeeee
static int tidy_html(const char* temp,const char *path){
		//tidy the temp, malloc with its size tidied, copy temp to tidied!!!!!!!!!
		//return 1 if success, return 0 if error was occured
		
	TidyBuffer output = {0};   //buffer structure of tidy can be used 
	TidyBuffer errbuf = {0};
	int ctrl = -1; //Control

	TidyDoc tdoc = tidyCreate();	//First call for tidy API


	char *new_path = malloc(sizeof(char)*strlen(path)+13);  // 13 is config.config size because we will end it to the path
	strcpy(new_path,path);
	char *ret = strrchr(path, '/');
	int a = ret-path;
	new_path[a]='\0';                        //last occurence of / will be end of string so we can understand the path of config file
											  // according  to html file
	strcat(new_path,sys1);                  //strcat a string manipulation function to concatane strings

	ctrl = tidyLoadConfig(tdoc, new_path);    //Config file is expected to be in same folder with html file, with name config.config
	free(new_path);             //deallocation is important especially in system usage

	if( ctrl >= 0)	 	
		ctrl = tidySetErrorBuffer( tdoc, &errbuf );      // Capture diagnostics
	if ( ctrl >= 0 )
		ctrl = tidyParseString( tdoc, temp );           // Parse the input
	if ( ctrl >= 0 )
		ctrl = tidyCleanAndRepair( tdoc );               // Tidy it up!
	if ( ctrl >= 0 )
		ctrl = tidyRunDiagnostics( tdoc );               // Kvetch
	if ( ctrl > 1 )                                    // If error, forcee output.
		ctrl = ( tidyOptSetBool(tdoc, TidyForceOutput, yes) ? ctrl : -1 );
	if ( ctrl >= 0 )
		ctrl = tidySaveBuffer( tdoc, &output );          // Pretty Print

	if ( ctrl >= 0 )
	{
		if ( ctrl > 0 )
		printf( "\nDiagnostics:\n\n%s", errbuf.bp );   //Just warnings, error, mistaken part of html file is reported to there 
		traveler=0;
		while ((output.bp)[traveler]!='\0'){traveler=traveler+1;printf(":%d:%c",traveler,(output.bp)[traveler]);}  //To determine buffer size
		printf( "\nAnd here is the result:\n\n%s", output.bp );   //output of the tidy process
		tidied = malloc(sizeof(char)*traveler+1);              //we need to allocate and copy to tidied string, because we need to reach
		printf("memcopy");                                   //tidied string from read callback function
		memcpy(tidied,output.bp,traveler);
		printf( "\nAnd here is the result of malloc:\n\n%s, sayı:%d:", tidied,traveler );
}
else
	printf( "A severe error (%d) occurred.\n", ctrl );


tidyBufFree( &output );       //deallocation is important, tidied will be deallocate in read function
tidyBufFree( &errbuf );
tidyRelease( tdoc );
globalBuffer = traveler;

return 1;
}


// Translate an bofs path into it's underlying filesystem path
static char* translate_path(const char* path)
{

    char *rPath= malloc(sizeof(char)*(strlen(path)+strlen(rw_path)+1));
	

	printf("\n%s\n",path);
	printf("%s\nfilepath\n",rw_path);
    strcpy(rPath,rw_path);
    if (rPath[strlen(rPath)-1]=='/') {
        rPath[strlen(rPath)-1]='\0';
    }
    strcat(rPath,path);

    return rPath;
}


/******************************
*
* Callback FUNCTIONS for FUSE
*
*
*
******************************/

static int bofs_getattr(const char *path, struct stat *st_data)
{
    int res;
    char *upath=translate_path(path);         //Getattr callback is important to know, determine file attributes, 

    res = lstat(upath, st_data);              //stats of the given path, 
    printf("path: %s upath: %s\n", path, upath); 
    int len = strlen(upath) + 1;
    
    char* my_extension = (char*) malloc(len * sizeof(char));
    my_extension = strrchr(upath,'.');
    if(my_extension!=NULL){								//For only html file increase
		
		if(strcmp(my_extension+1,html_extension)==0)
			st_data->st_size = globalBuffer; 		  //Tidy size increasing 	
	}
    free(upath);                             
    if(res == -1) {
        return -errno;                       //gives error   
    }
    return 0;
}

static int bofs_readlink(const char *path, char *buf, size_t size)
{
    int res;
    char *upath=translate_path(path);

    res = readlink(upath, buf, size - 1);
    printf("Read link....\n");
    free(upath);
    if(res == -1) {
        return -errno;
    }
    buf[res] = '\0';
    return 0;
}
//reading directories will be processed in, 
static int bofs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,off_t offset, struct fuse_file_info *fi)
{
    DIR *dp;
    struct dirent *de;
    int res;

    (void) offset;
    (void) fi;

    char *upath=translate_path(path);

    dp = opendir(upath);
    free(upath);
    if(dp == NULL) {
        res = -errno;
        return res;
    }

    while((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        if (filler(buf, de->d_name, &st, 0))
            break;
    }

    closedir(dp);
    return 0;
}

//read only, so making a directory is permitted
static int bofs_mkdir(const char *path, mode_t mode)
{
    (void)path;
    (void)mode;
    printf("Mkdir can not work\n\n");
    return -EROFS;
}
//read only, so removing a file is permitted
static int bofs_unlink(const char *path)
{
    (void)path;
    return -EROFS;
}
//read only, so removing a directory is permitted
static int bofs_rmdir(const char *path)
{
    (void)path;
    return -EROFS;
}
//read only, so symlink is permitted
static int bofs_symlink(const char *from, const char *to)
{
    (void)from;
    (void)to;
    return -EROFS;
}
//read only, so renaming is permitted
static int bofs_rename(const char *from, const char *to)
{
    (void)from;
    (void)to;
    return -EROFS;
}
//read only, so linking is permitted
static int bofs_link(const char *from, const char *to)
{
    (void)from;
    (void)to;
    return -EROFS;
}
//read only, so changing permission is permitted
static int bofs_chmod(const char *path, mode_t mode)
{
    (void)path;
    (void)mode;
    return -EROFS;

}
//open function is important and used mostly
static int bofs_open(const char *path, struct fuse_file_info *finfo)
{
    int res;

    /* read only controls are made with looking flags
     */
    int flags = finfo->flags;

    if ((flags & O_WRONLY) || (flags & O_RDWR) || (flags & O_CREAT) || (flags & O_EXCL) || (flags & O_TRUNC) || (flags & O_APPEND)) {
	printf("error opening\n");
        return -EROFS;
    }

    char *upath=translate_path(path);

    res = open(upath, flags);

    free(upath);
    if(res == -1) {   //that means error
        return -errno;
    }
    close(res);
    return 0;
}
//read is used to send user the data from file 
static int bofs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *finfo)
{
    int fd;
    int res;
    (void)finfo;
    
    char *temp;

    char *upath=translate_path(path);        //path translating
    fd = open(upath, O_RDONLY);
    if(fd == -1) {
        res = -errno;
        return res;
    }
	
    char *my_extension = strrchr(upath,'.');          //last occurence of ".", to determine html extension
    if(!my_extension){
 	res = pread(fd, buf, size, offset);             //if no, then continue to read fd->buffer 
        if(res == -1) {
            res = -errno;
        }
        free(upath);       //deallocate
        close(fd);
        return res;       
    }
    	
    if(strcmp(my_extension+1,html_extension)==0){   //it means html file, tidy function will be called
	printf("tidy function will work!\n\n");
	temp = malloc(sizeof(char)*size+1-offset);       //temp is used to take data from fd
	res = pread(fd, temp, size, offset);

        if(res == -1) {                  //means error
            res = -errno;
        }
        
        if(tidy_html(temp,upath)==1){
			if(tidy_html(temp,upath)==1){             // tidy is completed successly

				 printf("write:%s",tidied);
				 memcpy(buf,tidied,strlen(tidied));   //copy to buffer from tidied
			 res = strlen(tidied); 
			 free(tidied);	                 
			}
			else{
				memcpy(buf,temp,strlen(temp)+1);      //otherwise, copy the temp. In normal cases, this do not enter
				if(res == -1) {
					res = -errno;
				}
			}
			free(temp);   //deallocate is important
		}
    }	
    else{
	res = pread(fd, buf, size, offset);
	if(res == -1) {
            res = -errno;
        }
    
    }
    free(upath);
    
   
    close(fd);
    return res;
}
//no write to any fileee
static int bofs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *finfo)
{
    (void)path;
    (void)buf;
    (void)size;
    (void)offset;
    (void)finfo;
    return -EROFS;
}

//write acceses will be permitted
static int bofs_access(const char *path, int mode)
{
    int res;
    char *upath=translate_path(path);

    if (mode & W_OK)
        return -EROFS;

    res = access(upath, mode);
    free(upath);
    if (res == -1) {
        return -errno;
    }
    return res;
}

//operations
struct fuse_operations bofs_oper = {
    .getattr     = bofs_getattr,
    .readlink    = bofs_readlink,
    .readdir     = bofs_readdir,
    .mkdir       = bofs_mkdir,
    .symlink     = bofs_symlink,
    .unlink      = bofs_unlink,
    .rmdir       = bofs_rmdir,
    .rename      = bofs_rename,
    .link        = bofs_link,
    .chmod       = bofs_chmod,
    .open        = bofs_open,
    .read        = bofs_read,
    .write       = bofs_write,
    .access      = bofs_access,

};
//just for other options
enum {
    KEY_HELP,
    KEY_VERSION,
};
//for mountng usage is neeeded
static void usage(const char* progname)
{
    fprintf(stdout,
            "usage: %s readwritepath mountpoint [options]\n"
            "\n"
            "   Mounts readwritepath as a read-only mount at mountpoint\n"
            "\n"
            "general options:\n"
            "   -o opt,[opt...]     mount options\n"
            "   -h  --help          print help\n"
            "   -V  --version       print version\n"
            "\n", progname);
}
//parsing the command to fuse
static int bofs_parse_opt(void *data, const char *arg, int key,
                          struct fuse_args *outargs)
{
    (void) data;

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
    case KEY_HELP:
        usage(outargs->argv[0]);
        exit(0);
    case KEY_VERSION:
        fprintf(stdout, "BOFS version %s\n", bofsVersion);
        exit(0);
    default:
        fprintf(stderr, "see `%s -h' for usage\n", outargs->argv[0]);
        exit(1);
    }
    return 1;
}
//options
static struct fuse_opt bofs_opts[] = {
    FUSE_OPT_KEY("-h",          KEY_HELP),
    FUSE_OPT_KEY("--help",      KEY_HELP),
    FUSE_OPT_KEY("-V",          KEY_VERSION),
    FUSE_OPT_KEY("--version",   KEY_VERSION),
    FUSE_OPT_END
};
//main function
int main(int argc, char *argv[])
{
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv); //converting to fuse args
    int res;

    res = fuse_opt_parse(&args, &rw_path, bofs_opts, bofs_parse_opt);  //arsing the input
    if (res != 0)
    {
        fprintf(stderr, "Invalid arguments\n");   //ınvalid arguments
        fprintf(stderr, "see `%s -h' for usage\n", argv[0]);
        exit(1);
    }
    if (rw_path == 0)    //no readwirte path
    {
        fprintf(stderr, "Missing readwritepath\n");
        fprintf(stderr, "see `%s -h' for usage\n", argv[0]);
        exit(1);
    }

    fuse_main(args.argc, args.argv, &bofs_oper, NULL);  //fuse main

    return 0;
}
