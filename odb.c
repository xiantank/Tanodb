#include <openssl/md5.h>
#include <unistd.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<fcntl.h>
#include<sys/stat.h>
#include <inttypes.h>
#include<sys/types.h>
#include<errno.h>
#include<getopt.h>
const int INDEX_OFFSET_LENGTH = 5;
/* 
 * structure declare
 */
struct Index{//use 30 bytes per index
		unsigned char MD5[MD5_DIGEST_LENGTH] ;
		unsigned char fid;
		unsigned char offset[5];//maxfilesize is 1T; use fixxed offset;
		unsigned int size;	//object size limit : 1G
		int collision;
		unsigned char indexFlag;// 1:delete 2:collision
};
typedef enum{
		INDEX_DELETE = 1,
		INDEX_COLLISION = 2,
		INDEX_EXIST = (1<<7)
}IndexFlag;
typedef enum{
		ARG_DEFAULT = 1,
		ARG_INIT = (1<<1),
		ARG_PATH = (1<<2),
		ARG_MAXOBJSIZE = (1<<3),
		ARG_BUCKETSIZE = (1<<4),
		ARG_BUCKETNUM = (1<<)5,
		ARG_DBFILESIZE = (1<<6),
		ARG_DBFILENUM = (1<<7),
}ArgFlag;

struct Config{
		char PATH[100];
		int FILENUM;
		off_t MAXFILESIZE;
		int BUCKETSIZE;
		off_t BUCKETNUM;
		unsigned int OBJSIZE;
		unsigned char curFid;
		off_t offset;
		int isFull; //TODO
};
typedef struct Index Index;
typedef struct Config Config;
/*
 * global config varible declare
 */
Config DB;
unsigned char buffer[100000];
/*
 * function declare
 */
int init();
int getVariable(void *start, int size , int nnum , char *filename);
int putObject(void *start , off_t size , char *fileprefix , unsigned char *fid , off_t *offset );
int getObject(void *start , off_t size , char *fileprefix , unsigned char fid , off_t offset );
size_t receiveObj(int fd,unsigned char * obj);
off_t getIndex(unsigned char *md5 , Index * indexTable);
off_t getItem( Index *indexTable , unsigned char *md5out , unsigned char* buffer);
off_t putItem(Index * indexTable , unsigned char *buffer , off_t size  );
void saveDB();
void saveIndex(Index *indexTable);
/*
 * tool function
*/
int saveVariable(void *start, int size , int nnum , char *filename);
unsigned char *md5(char *in , unsigned char * out);
int readString(char **src , char * dst , int length);//src wiil move to new location
off_t bytesToOffset(unsigned char *bytesNum,int size);
void offsetTobytes(unsigned char *c_offset , off_t offset, int length);
off_t getHV(unsigned char *md5);
off_t findIndex(off_t hv , Index *indexTable);
off_t findLocate(off_t hv , Index *indexTable);
off_t updateIndex(Index *indexTable , unsigned char *md5 , unsigned char fid , off_t offset , off_t size );


/*
 *main function
*/
int main(int argc , char *argv[])
{

		int c=0;
		char* const short_options = "idf:s:n:b:o:p:";
		struct option long_options[] = {
				{ "init" , 0 , NULL , 'i' },
				{ "default" , 0 , NULL , 'd' },
				{ "fnum" , 1 , NULL , 'f' },
				{ "fsize" , 1 , NULL , 's' },
				{ "bnum" , 1 , NULL , 'n' },
				{ "bsize" , 1 , NULL , 'b' },
				{ "maxobjsize" , 1 , NULL , 'o' },
				{ "path" , 1 , NULL , 'p' },
				{ 0 , 0 , 0 , 0}
		};
		int argFlag=0;//fsnbopid ;  8bits!;  76543210 ;128  64  32  16  8  4  2  1
		while((c = getopt_long (argc, argv, short_options, long_options, NULL)) != -1){
				switch(c){
						case 'f' : 
							DB.FILENUM = atoi(optarg);
							argFlag = argFlag | ARG_DBFILENUM;
							break;
						case 's' : 
							DB.MAXFILESIZE = (off_t)atol(optarg);
							argFlag = argFlag | ARG_DBFILESIZE;
							break;
						case 'n' : 
							DB.BUCKETNUM = (size_t)atol(optarg);
							argFlag = argFlag | ARG_BUCKETNUM;
							break;
						case 'b' : 
							DB.BUCKETSIZE = atoi(optarg);
							argFlag = argFlag | ARG_BUCKETSIZE;
							break;
						case 'o' : 
							DB.OBJSIZE = (unsigned int)atoi(optarg);
							argFlag = argFlag | ARG_MAXOBJSIZE;
							break;
						case 'p' : 
							strcpy(DB.PATH,optarg);
							argFlag = argFlag | ARG_PATH;
							break;
						case 'i' : 
							if(argFlag!=0){
									fprintf(stderr,
										"argument init must placed at the top\n%s --init [options]\n",argv[0]);
									exit(1);
							}
							init();	//init DB setting 
							argFlag = argFlag | ARG_INIT;
							break;
						case 'd' : 
							if(argFlag != 2){//NOT REALLY USE!!!
									fprintf(stderr,
									"argument init must placed at the top\n%s --init --default [options]\n",argv[0]);
									exit(1);
							}
							init();	//init DB setting 
							argFlag = argFlag | ARG_DEFAULT;
							break;
				}
		}
		if(argFlag & ARG_INIT){//that is must init
				saveDB();
				exit(0);

		}else if(argFlag == (ARG_PATH)){//only arg:path
				char dbini[100];
				sprintf(dbini,"%sdb.ini",DB.PATH);
				getVariable( (void *)&DB, sizeof(DB) , 1 , dbini);
		}else {
				fprintf(stderr,"arg error!%x",argFlag);
				exit(1);
				//TODO usage();
		}

		/* test */

		unsigned char md5out[MD5_DIGEST_LENGTH];
		Index indexTable[DB.BUCKETNUM];//TODO use malloc and check and memset(indexTable , 0)
		memset(indexTable , 0 , sizeof(Index)*DB.BUCKETNUM);
		//char obj[DB.OBJSIZE];//TODO use malloc and check


		off_t bytes=0,index=0,n;
		bytes = receiveObj(STDIN_FILENO,buffer);
		n = putItem(indexTable , buffer , bytes);
		fprintf(stderr , "[%zd]",n);
		md5(buffer,md5out);
		memset(buffer , 0 ,sizeof(buffer));
		bytes = getItem(indexTable , md5out , buffer);
		write(STDOUT_FILENO , buffer , bytes );
		fprintf(stderr , "[%zd]",bytes);

		return 0;
		index = getIndex(md5out , indexTable);
		if(index < 0 ){
				putObject(buffer , bytes , DB.PATH , &DB.curFid , &DB.offset);
				getObject(buffer , bytes , DB.PATH , DB.curFid , (off_t)0);
				write(STDOUT_FILENO , buffer , bytes );
		}
		return 0;
}
/* function implement*/
int init(){//TODO read from file;
		strcpy (DB.PATH ,"./");
		DB.FILENUM = 5;
		DB.MAXFILESIZE = 274877906944;
		DB.BUCKETSIZE = 0;
		DB.BUCKETNUM = 100000;
		DB.OBJSIZE = 10000;
		DB.curFid = 0;
		DB.offset = 0;

		return 1;
}
off_t getHV(unsigned char *md5){
		off_t tmp=0;
		tmp = bytesToOffset(&md5[0],8) ^ bytesToOffset(&md5[8],8);
		if(tmp < 0 ) tmp = (tmp * -1);
		return tmp % DB.BUCKETNUM;
}
off_t getIndex(unsigned char *md5 , Index * indexTable){
		off_t hv = getHV(md5);

		return findIndex(hv , indexTable);
		
}
off_t findIndex(off_t hv , Index *indexTable){
		if( indexTable[hv].indexFlag & INDEX_EXIST){// check is INDEX_EXIST //TODO set flag
				if(indexTable[hv].indexFlag & INDEX_DELETE){// INDEX_DELETE){ //TODO set flag
				}
				else if(memcmp(indexTable[hv].MD5 , md5 , 16)){
						return hv;
				}
				return findIndex(indexTable[hv].collision , indexTable);
		}
		else {
				return -1;
		}
}
size_t receiveObj(int fd,unsigned char * obj){
		off_t bytes;
		unsigned char *ptr = obj;
		unsigned char buf[512];
		//bytes=read(STDIN_FILENO, buf, 512);
		bytes=read( fd , buf, 512);
		while(bytes > 0)
		{
				memcpy(ptr,buf,bytes);
				ptr+=bytes;
				bytes=read(fd, buf, 512);
		}
		*ptr = '\0';
		return (ptr-obj);

}
unsigned char *md5(char *in,unsigned char * out){
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
				fprintf(stderr , "save error[%d]\n",fd);
				return -1;
		}
		verify = write(fd , start,(ssize_t)size * nnum);
		close(fd);
		return verify;
}
void saveDB(){
		char dbini[100];
		int tmp=-2;
		sprintf(dbini,"%sdb.ini",DB.PATH);
		tmp = saveVariable( (void *) &DB, sizeof(Config) , 1 , dbini);
		if(tmp != sizeof(Config) ){
				fprintf(stderr , "[%d] saveDB() error\n",tmp);
				exit(2);
		}

}
void saveIndex(Index *indexTable){
		char indexFile[100];
		int tmp=-2;
		sprintf(indexFile,"%sdb.inx",DB.PATH);
		tmp = saveVariable( (void *) &indexTable, sizeof(Index) , DB.BUCKETNUM , indexFile);
		if(tmp != (sizeof(Index) * DB.BUCKETNUM) ){
				fprintf(stderr , "[%d] saveIndex() error\n",tmp);
				exit(2);
		}
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
		saveDB();
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
				fprintf(stderr , "getObj error[%d](%s)!\nread: %zd\n{%s}\n", errno , filename ,verify, (char*)start);
				return -1;
				//TODO some error handle
		}
		return verify;
}
off_t getItem( Index *indexTable , unsigned char *md5out , unsigned char* buffer){
		//TODO maybe use filename to get md5out
		off_t index=-1,bytes = 0;
		
		index = getIndex(md5out , indexTable);
		if(index == -1 ) return -1;
		//TODO if( md5 != iT.md5 && iT.flag==COLLISION)
		bytes = getObject(buffer , indexTable[index].size , DB.PATH , indexTable[index].fid ,(off_t)indexTable[index].offset);
		if(bytes != indexTable[index].size){//TODO error handle
		}
		return index;


}
off_t putItem(Index * indexTable ,unsigned char *buffer , off_t size  ){
		unsigned char md5out[MD5_DIGEST_LENGTH];
		off_t index = -1 , startOffset , bytes;
		md5(buffer , md5out);
		index = getIndex(md5out , indexTable);
		if(index != -1){//it found in table
				return index;
		}
		startOffset = DB.offset;
		bytes = putObject( buffer , size , DB.PATH , &DB.curFid , &DB.offset);
		index = updateIndex(indexTable , md5out , DB.curFid , startOffset , size);
		if(bytes != indexTable[index].size){//TODO error handle
		}
		return index;


}
off_t updateIndex(Index *indexTable , unsigned char *md5 , unsigned char fid , off_t offset , off_t size ){

		off_t locate =-1;
		locate = findLocate(getHV(md5) , indexTable);
		if(locate == -1){
				DB.isFull = 1;
				fprintf(stderr , "DB is full (index num full)\n");
				exit(5);
		}
		memcpy(indexTable[locate].MD5 , md5 , MD5_DIGEST_LENGTH );
		indexTable[locate].fid = DB.curFid ;
		offsetTobytes(indexTable[locate].offset , offset , INDEX_OFFSET_LENGTH) ;
		indexTable[locate].size = (unsigned int )size;
		indexTable[locate].collision = 0 ;
		indexTable[locate].indexFlag = 0 ;
		indexTable[locate].indexFlag |= INDEX_EXIST ;
		saveIndex(indexTable);
		return locate;

}
off_t findLocate(off_t hv , Index *indexTable){
		off_t i;
		for(i=0 ; i<DB.BUCKETNUM ; i++,hv++){
				if(i == DB.BUCKETNUM) hv %= DB.BUCKETNUM;
				if(indexTable[hv].indexFlag & INDEX_EXIST){
						//do nothing
				}
				else{
						return hv;
				}
		}
		return -1;
}
void offsetTobytes(unsigned char *c_offset , off_t offset, int length){
		int i=0;
		for(i=length-1;i>=0;i--){
				c_offset[i] = offset & 0xff;
				offset = offset >> 8;
		}
}
