#include <openssl/md5.h>
#include <unistd.h>
#include<stdio.h>
#include<string.h>
#include<fcntl.h>
#include<sys/stat.h>
#include <inttypes.h>
#include<sys/types.h>
#include<errno.h>
/* 
 * structure declare
 */
struct Index{//use 30 bytes per index
		unsigned char MD5[MD5_DIGEST_LENGTH] ;
		unsigned char fid;
		unsigned char offset[5];//maxfilesize is 1T; use fixxed offset;
		unsigned int size;	//object size limit : 1G
		int collision;
};
struct Config{
		char PATH[50];
		int FILENUM;
		off_t MAXFILESIZE;
		int BUCKETSIZE;
		size_t BUCKETNUM;
		unsigned int OBJSIZE;
		unsigned char curFid;
		off_t offset;
};
typedef struct Index Index;
typedef struct Config Config;
/*
 * global config varible declare
 */
Config DB;


/*
 * function declare
 */
int init();
unsigned char *md5(char *in , unsigned char * out);
off_t bytesToOffset(unsigned char *bytesNum,int size);
int readString(char **src , char * dst , int length);//src wiil move to new location
int getVariable(void *start, int size , int nnum , char *filename);
int saveVariable(void *start, int size , int nnum , char *filename);
int putObject(void *start , off_t size , char *fileprefix , unsigned char *fid , off_t *offset );
int getObject(void *start , off_t size , char *fileprefix , unsigned char fid , off_t offset );
size_t receiveObj(int fd,char * obj);
/*
 *main function
*/
int main()
{
		init();	//get DB setting 

		unsigned char out[MD5_DIGEST_LENGTH];
		Index indexTable[DB.BUCKETNUM];//TODO use malloc and check
		char obj[DB.OBJSIZE];//TODO use malloc and check
		
		
		char str[]="abcdefg\n";
		char str2[1000];
		char str3[] = "yoyoABCCC";
		int n;
		off_t app;
		unsigned char off[]={5,253,222,132,221};
		app = bytesToOffset(off,5);
		n = receiveObj(STDIN_FILENO,str2);
		printf("%s\n[%d]",str2,n);
		
		
		putObject(str , strlen(str) , DB.PATH , &DB.curFid , &DB.offset);
		putObject(str3 , strlen(str3) , DB.PATH , &DB.curFid , &DB.offset );
		getObject(str2 , 17 , DB.PATH , (unsigned char) 0 , 0);
		//char str[8];//="abcdefg\n";
		md5(str,out);
        for(n=0; n<MD5_DIGEST_LENGTH; n++)
                printf("%02x", out[n]);
        printf("\n[%s]\n",str);
		return 0;
}
/* function implement*/
int init(){//TODO read from file;
		strcpy (DB.PATH ,"db");
		DB.FILENUM = 5;
		DB.MAXFILESIZE = 274877906944;
		DB.BUCKETSIZE = 0;
		DB.BUCKETNUM = 100000;
		DB.OBJSIZE = 10000;
		DB.curFid = 0;
		DB.offset = 0;
}
size_t receiveObj(int fd,char * obj){
		size_t bytes;
		char *ptr = obj;
		char buf[512];
		//bytes=read(STDIN_FILENO, buf, 512);
		bytes=read( fd , buf, 512);
		while(bytes > 0)
		{
				strncpy(ptr,buf,bytes);
				ptr+=bytes;
				bytes=read(fd, buf, 512);
		}
		*ptr = '\0';
		return (ptr-obj);

}
unsigned char *md5(char *in,unsigned char * out){
		int n;
        MD5_CTX c;
        char buf[512];
        ssize_t bytes;
        //unsigned char out[MD5_DIGEST_LENGTH];

        MD5_Init(&c);
        bytes=readString(&in , buf, 512);
        //bytes=read(STDIN_FILENO, buf, 512);
        while(bytes > 0)
        {
                MD5_Update(&c, buf, bytes);
				bytes=readString(&in , buf, 512);
                //bytes=read(STDIN_FILENO, buf, 512);
        }

        MD5_Final(out, &c);

        //printf("%d\n",MD5_DIGEST_LENGTH);

        return out;

}
off_t bytesToOffset(unsigned char *bytesNum,int size){
		int i=0;
		off_t sum = 0;
		for(i=0;i<size;i++){
				sum = sum<<8;
				sum = sum | bytesNum[i];
		}
		return sum;
}
int readString(char **src , char * dst , int length){
		char *cur = *src;
		char *head = *src;
		while( ( (cur-head) < length ) && ( *cur != '\0') && ( *cur != EOF )  ){
				cur++;
		}
		memcpy(dst,*src,(cur-head));
		*src = cur;
		return (cur-head);
}
//int getlocate
int saveVariable(void *start, int size , int nnum , char *filename){
		int fd;
		ssize_t verify =-1;
		fd = open(filename,O_WRONLY|O_CREAT|O_TRUNC , S_IWUSR | S_IRUSR);
		if(fd<0){//TODO error handle
				fprintf(stderr , "save error\n");
				return -1;
		}
		verify = write(fd , start,(ssize_t)size * nnum);
		close(fd);
		return verify;

}
int getVariable(void *start, int size , int nnum , char *filename){
		int fd;
		ssize_t verify=-1;
		fd = open(filename , O_RDONLY);
		verify=read(fd , start , size * nnum);
		if(verify != (size * nnum)){
				fprintf(stderr , "get error\n");
				//TODO some error handle
		}
		return verify;
}
int putObject(void *start , off_t size , char *fileprefix , unsigned char *fid , off_t *offset ){
		//TODO check file size
		//TODO return type turn to Index, or put to Index ary out side
		int fileId = 0;
		char filename[50]={};
		int fd;
		off_t verify=-1;
		fileId = fileId | *fid ;
		sprintf(filename,"%s%04d",fileprefix,fileId);
		fd = open(filename,O_WRONLY|O_CREAT , S_IWUSR | S_IRUSR  );
		//lseek;
		lseek(fd,*offset,SEEK_SET);
		verify = write(fd , start,size );
		if(verify != ( size )){
				fprintf(stderr , "putObj error\n");
				//TODO some error handle
		}
		*offset += verify;
		return verify;
		

}

int getObject(void *start , off_t size , char *fileprefix , unsigned char fid , off_t offset ){
		int fileId = 0;
		char filename[50]={};
		int fd;
		off_t verify=-1;
		fileId = fileId | fid ;
		sprintf(filename,"%s%04d",fileprefix,fileId);
		fd = open(filename,O_RDONLY);
		//lseek;
		lseek(fd,offset,SEEK_SET);
		verify = read(fd , start,size );
		if(verify != ( size )){
				fprintf(stderr , "getObj error[%d](%s)!\nread: %d\n{%s}\n", errno , filename ,verify,start);
				return -1;
				//TODO some error handle
		}
		return 0;
		

}

