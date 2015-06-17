#define _GNU_SOURCE
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
#include<time.h>
#define DEBUG 0
const int INDEX_OFFSET_LENGTH = 5;
/* 
 * structure declare
 */
struct Index{
		unsigned char MD5[MD5_DIGEST_LENGTH] ;
		unsigned char fid;
		unsigned char offset[5];//maxfilesize is 1T; use fixed offset;
		unsigned int size;	//object size limit : 1G
		int collision;
		unsigned char indexFlag;// see IndexFlag
		int ref;
};
struct FileIndex{
		char filename[50];
		unsigned char MD5[MD5_DIGEST_LENGTH];
		off_t index;
		unsigned char indexFlag;
};
struct RecordIndex{
		off_t rid;
		//unsigned char MD5[MD5_DIGEST_LENGTH];
		off_t rec_offset;
		off_t rec_size;
		unsigned char indexFlag;
};
struct Record{
		off_t recordId;
		char type[20];
		char path[50];
		int ctime;
		off_t objectSize;
		int mtime;
		char tag[100];
		char MD5[MD5_DIGEST_LENGTH];
		char describe[100];
};
typedef enum{
		INDEX_NULL = 0,
		INDEX_DELETE = 1,
		INDEX_COLLISION = 2,
		INDEX_EXIST = (1<<7)
}IndexFlag;
typedef enum{
		true = 1,
		false =0
}BOOLEAN;
typedef enum{
		ARG_DEFAULT = 	(1<<8),
		ARG_INIT = 		(1<<9),
		ARG_PATH = 		(1<<10),
		ARG_FILENAME =	(1<<16),
		ARG_MAXOBJSIZE =(1<<11),
		ARG_BUCKETSIZE =(1<<12),
		ARG_BUCKETNUM = (1<<13),
		ARG_DBFILESIZE =(1<<14),
		ARG_DBFILENUM = (1<<15)

		
}ArgFlag;
typedef enum{
		ACTION_FIND = 		1,
		ACTION_MD5GET = 	2,
		ACTION_FILENAMEGET=	3,
		ACTION_LIST=		4,
		ACTION_DELETE=		5
}ArgAction;

struct Config{
		char PATH[100];
		int FILENUM;
		off_t MAXFILESIZE;
		int BUCKETSIZE;
		off_t BUCKETNUM;
		unsigned int OBJSIZE;
		unsigned char curFid;
		off_t offset;
		int isFull;
		//RECORD
		off_t rec_offset;
		int RECBLOCK;

};
typedef struct Index Index;
typedef struct Config Config;
typedef struct FileIndex FileIndex;
typedef struct RecordIndex RecordIndex;
typedef struct Record Record;
/*
 * global config varible declare
 */
Config DB;
#define BUFFERSIZE 3000000
#define CHUNK 1048576
/*
 * function declare
 */
int init();
int getVariable(void *start, int size , int nnum , char *filename);
size_t receiveObj(int fd,unsigned char * obj , off_t bufferSize);
off_t getIndex(unsigned char *md5 , Index * indexTable);
off_t getIndexbyName(char *filename , FileIndex *fileIndex , int isFindex);
off_t getItem( Index *indexTable , FileIndex *fileIndex,char *objname , unsigned char *md5out ,  int byName);
off_t putItem(Index *indexTable, FileIndex *fileIndex,char *filename, int fd, off_t size, int byName,RecordIndex *recIndex, off_t rid , off_t rec_parrent , char *describe , char *name);
void saveDB();
void saveIndex(Index *indexTable);//TODO only save modify index
void saveFileIndex(FileIndex *fileIndex);
void saveRecordIndex(RecordIndex * recIndex);
/*
 * record function
 */
void parseCmdToRec(char *optarg,off_t *rid,off_t *parrent,char *name,char *describe);
void putRecord(off_t recordId , char *filename , off_t parrent ,unsigned char *MD5 , char *describe , off_t rec_size);
char *getRecord(off_t rid , char *buf);
int getRecColumn(char *record , char *key , char *valueBuf);
void updateRecord(RecordIndex recIndex , off_t recordId  , char *columnName , char *value);
void deleteRecord(off_t rid);

RecordIndex getRecIndex(off_t rid);
off_t updateRecIndex(off_t rid , off_t rec_offset , off_t rec_size , unsigned char indexFlag );
off_t deleteRecIndex(off_t rid);
//append
/*
type:		get by filename						in the func.
parrent:	receive from client
ctime:		use something like :Date().now		in the func.
tags:		default no tag, add when update
recordId:	get from node.js
*/

/*
 * tool function
*/
int putObject(int fd , off_t size , char *fileprefix , unsigned char *fid , off_t *offset );
int getObject(void *start , off_t size , char *fileprefix , unsigned char fid , off_t offset );
int saveVariable(void *start, int size , int nnum , char *filename);
unsigned char *md5(int fd , unsigned char * out , off_t length);
unsigned char *md5str( char * in , unsigned char * out , off_t length);
off_t readString(char **src , char * dst , int length);//src wiil move to new location
off_t bytesToOffset(unsigned char *bytesNum,int size);
void offsetTobytes(unsigned char *c_offset , off_t offset, int length);
off_t getHV(unsigned char *md5);
off_t findIndex(off_t hv , Index *indexTable);
off_t findLocate(off_t hv , Index *indexTable);
off_t updateIndex(Index *indexTable , FileIndex *fileIndex ,char *filename, unsigned char *md5 , unsigned char fid , off_t offset ,off_t size );
void hexToMD5(unsigned char* md5out , char *in);
char *md5ToHex(unsigned char *MD5,char *hexBuf);
unsigned char hexToChar(char c);
/*
 *main function
*/
int main(int argc , char *argv[] , char *envp[])
{

		int c=0;
		char dbini[100];
		char* const short_options = "b:dD:f:F:G:iI:LM:n:o:p:PR:s:u:";
		off_t bytes=0,index=0,n,i , f_index;
		off_t parrent=0 , rid;
		char name[512],describe[1024];
		unsigned char md5out[MD5_DIGEST_LENGTH];
		int fd;
		int argFlag=0;//fsnbopid ;  8bits!;  76543210 ;128  64  32  16  8  4  2  1
		Index *indexTable;
		FileIndex *fileIndex;
		RecordIndex *recIndex;
		struct option long_options[] = {
				{ "init" , 0 , NULL , 'i' },
				{ "default" , 0 , NULL , 'd' },
				{ "fnum" , 1 , NULL , 'u' },
				{ "fsize" , 1 , NULL , 's' },
				{ "bnum" , 1 , NULL , 'n' },
				{ "bsize" , 1 , NULL , 'b' },
				{ "maxobjsize" , 1 , NULL , 'o' },
				{ "path" , 1 , NULL , 'p' },
				{ "filename" , 1 , NULL , 'f' },

//				{ "parrent" , 1 , NULL , 'r'},
				{ "md5GET" , 1 , NULL , 'M'},
				{ "nameGET" , 1 , NULL , 'G'},
				{ "idGET" , 1 , NULL , 'I'},
				{ "PUT" , 0 , NULL , 'P'},
				{ "file-put" , 1 , NULL , 'F'},
				{ "list" , 0 , NULL , 'L'},
				{ "search" , 1 , NULL , 'S'},
				{ "delete" , 1 , NULL , 'D'},
				{ "recordPut" , 1 , NULL , 'R'},
				{ 0 , 0 , 0 , 0}
		};
		/*
		* cgi
		*/
		//getenv("CONTENT_LENGTH");

		while((c = getopt_long (argc, argv, short_options, long_options, NULL)) != -1){
				switch(c){
						case 'u' : 
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
						case 'f' :
							argFlag = argFlag | ARG_FILENAME;
							sprintf(dbini,"%sfn.inx",DB.PATH);
							getVariable( (void *)fileIndex, sizeof(FileIndex) , DB.BUCKETNUM , dbini);
							break;
						case 'p' : 
							strcpy(DB.PATH,optarg);
							argFlag = argFlag | ARG_PATH;
							if( !(argFlag & ARG_INIT) ){
									sprintf(dbini,"%sdb.ini",DB.PATH);
									getVariable( (void *)&DB, sizeof(DB) , 1 , dbini);
									indexTable = (Index *) malloc ( sizeof(Index) * DB.BUCKETNUM);
									fileIndex = (FileIndex *) malloc ( sizeof(FileIndex) * DB.BUCKETNUM);
									
									recIndex = (RecordIndex *) malloc ( sizeof(RecordIndex) * DB.BUCKETNUM);
									sprintf(dbini,"%srec.inx",DB.PATH);
									getVariable( (void *)recIndex, sizeof(recIndex) , DB.BUCKETNUM , dbini);
									
									memset(indexTable , 0 , sizeof(Index)*DB.BUCKETNUM);
									sprintf(dbini,"%sdb.inx",DB.PATH);
									getVariable( (void *)indexTable, sizeof(Index) , DB.BUCKETNUM , dbini);
									sprintf(dbini,"%sfn.inx",DB.PATH);
									getVariable( (void *)fileIndex, sizeof(FileIndex) , DB.BUCKETNUM , dbini);
									strcpy(DB.PATH,optarg);

									//buffer = (unsigned char *) malloc ( sizeof(unsigned char) * DB.OBJSIZE);
									//memset(buffer , 0 , DB.OBJSIZE);
							}
							break;
						case 'i' : 
							if(argFlag!= 0){
									fprintf(stderr,
										"argument init must placed at the top\n%s --init [options]\n",argv[0]);
									exit(1);
							}
							init();	//init DB setting 
							indexTable = (Index *) malloc ( sizeof(Index) * DB.BUCKETNUM);
							fileIndex = (FileIndex *) malloc ( sizeof(FileIndex) * DB.BUCKETNUM);
							recIndex = (RecordIndex *) malloc ( sizeof(RecordIndex) * DB.BUCKETNUM);
							memset(indexTable , 0 , sizeof(Index)*DB.BUCKETNUM);
							memset(fileIndex , 0 , sizeof(FileIndex)*DB.BUCKETNUM);
							memset(recIndex , 0 , sizeof(RecordIndex)*DB.BUCKETNUM);
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
/*						case 'M' : 
							hexToMD5(md5out , optarg);
							index = getItem(indexTable , fileIndex , filename , md5out ,  false );
//							fprintf(stderr,"[%zd]\n",index);
							if(index==-1){
									fprintf(stderr , "obj not exist\n");
									exit(1);
							}
							write(STDOUT_FILENO , buffer , indexTable[index].size );
							for(n=0 ; n < MD5_DIGEST_LENGTH ; n++){
									printf("%02x",md5out[n]);
							}
							exit(0);
							return 0;

							break;*/
						case 'G' : 
							index = getItem(indexTable , fileIndex , optarg , md5out ,  true );
//							fprintf(stderr,"[%zd]\n",index);
							if(index==-1){
									fprintf(stderr , "obj not exist\n");
									exit(1);
							}
#if DEBUG
							fprintf(stderr,"[%zd]",index);
#endif
//							write(STDOUT_FILENO , buffer , indexTable[index].size );
							exit(0);
						case 'I' : 

							break;
						case 'R':
							parseCmdToRec(optarg,&rid,&parrent,name,describe);
							break;
						case 'F' : 
							fd = open( optarg , O_RDONLY);
							if(fd<0){
									fprintf(stderr, "file open fail\n" );
									exit(1);
							}
							bytes = lseek(fd,0,SEEK_END);
							lseek(fd,0,SEEK_SET);
							/*bytes = receiveObj(fd,buffer , DB.OBJSIZE);
							if(bytes == -1){
									fprintf(stderr , "file size limit is %d" , DB.OBJSIZE);
									exit(5);
							}*/
							index = putItem(indexTable , fileIndex , optarg , fd , bytes , true , recIndex , rid , parrent , describe , name);
//off_t putItem(Index *indexTable, FileIndex *fileIndex,char *filename, int fd, off_t size, int byName,RecordIndex recIndex, off_t rid , char *rec_parrent , char *describe);
							if(index == -1){
									fprintf(stderr , "filename exist\n");
									exit(2);
							}
							/*for(n=0 ; n < MD5_DIGEST_LENGTH ; n++){
									printf("%02x",indexTable[index].MD5[n]);
							}
							printf("\n");*/
							//printf("[%s]",optarg);
							exit(0);
						case 'L' :
#if DEBUG
							for(i=0;i<DB.BUCKETNUM;i++){
									if( ( indexTable[i].indexFlag & INDEX_EXIST)  && !(indexTable[i].indexFlag & INDEX_DELETE)  ){
											for(n=0 ; n < MD5_DIGEST_LENGTH ; n++){
													printf("%02x",indexTable[i].MD5[n]);
											}
											printf("\n");
									}
							}
							printf("\nreal:\n\n");
#endif
							printf("[");
							for(i=0,n=0;i<DB.BUCKETNUM;i++){//n is cnt for list
									if( ( fileIndex[i].indexFlag & INDEX_EXIST) && !(fileIndex[i].indexFlag & INDEX_DELETE)){
											if(n){
													printf(",");
											}
											printf("{\"size\":%zd,\"filename\":\"%s\"}",(off_t)indexTable[fileIndex[i].index].size,fileIndex[i].filename);
											n++;
									}
							}
							printf("]");
							exit(0);
						case 'S' :
							printf("[");
							for(i=0,n=0;i<DB.BUCKETNUM;i++){//n is cnt for list
									if( ( fileIndex[i].indexFlag & INDEX_EXIST) && !(fileIndex[i].indexFlag & INDEX_DELETE)){
											if(!strcasestr(fileIndex[i].filename,optarg)){
											//if(strncmp(fileIndex[i].filename,optarg,strlen(optarg))){
													continue;
											}
											if(n){
													printf(",");
											}
											printf("{\"size\":%zd,\"filename\":\"%s\"}",(off_t)indexTable[fileIndex[i].index].size,fileIndex[i].filename);
											n++;
									}
							}
							printf("]");
							exit(0);

						case 'D' : 
							f_index = getIndexbyName( optarg , fileIndex , true);
							index = fileIndex[f_index].index;
							if(f_index == -1){// obj not exist
									fprintf(stderr,"object not exist(n)\n");
									exit(404);
							}
							if(indexTable[index].indexFlag & INDEX_DELETE){//obj not exist
									fprintf(stderr,"object not exist(y)\n");
									exit(404);
							}
							else{//do delete
									fileIndex[f_index].indexFlag |= INDEX_DELETE;
									indexTable[index].ref--;
									if(indexTable[index].ref == 0){
											indexTable[index].indexFlag |= INDEX_DELETE;
									}
									saveIndex(indexTable);
									saveFileIndex(fileIndex);
									saveRecordIndex(recIndex);
									fprintf(stdout,"[%s] deleted!\n",optarg);
							}
							exit(0);
				}
		}
		if( !(argFlag & ARG_PATH) ){
				//usage();
				fprintf(stderr , "arg must have --path\n");
				exit(1);
		}
		if(argFlag & ARG_INIT){//that is must init
				saveDB();
				saveIndex(indexTable);
				saveFileIndex(fileIndex);
				saveRecordIndex(recIndex);
				exit(0);

		}else if(argFlag == (ARG_PATH)){//only arg:path
		}else {
				fprintf(stderr,"arg error!%x",argFlag);
				exit(1);
				//TODO usage();
		}

		/* test */



		
		return 0;
}
/* function implement*/
int init(){
		strcpy (DB.PATH ,"./");
		DB.FILENUM = 5;
		DB.MAXFILESIZE = 1 * 1024 * 1024 * 1024;
		DB.BUCKETSIZE = 0;
		DB.BUCKETNUM = 100000;
		DB.OBJSIZE = 512 * 1024 * 1024;
		DB.curFid = 0;
		DB.offset = 0;
		DB.rec_offset = 0;
		DB.RECBLOCK = 4 * 1024;

		return 1;
}
off_t getHV(unsigned char *md5){
		off_t tmp=0;
		tmp = bytesToOffset(&md5[0],8) ^ bytesToOffset(&md5[8],8);
		if(tmp < 0 ) tmp = (tmp * -1);
		return tmp % DB.BUCKETNUM;
}
off_t getIndexbyName(char *filename , FileIndex *fileIndex , int isFindex){
		unsigned char md5out[MD5_DIGEST_LENGTH];
		md5str(filename , md5out , strlen(filename));

		off_t hv = getHV(md5out);
		if(isFindex == true){
				return hv;
		}
		if(fileIndex[hv].indexFlag & INDEX_EXIST){
				return fileIndex[hv].index;
		}
		else{
				return -1;
		}
}
off_t getIndex(unsigned char *md5 , Index * indexTable){
		off_t hv = getHV(md5);

		return findIndex(hv , indexTable);
		
}
off_t findIndex(off_t hv , Index *indexTable){
		if( indexTable[hv].indexFlag & INDEX_EXIST){// check is INDEX_EXIST //TODO set flag
				if(indexTable[hv].indexFlag & INDEX_DELETE){// INDEX_DELETE){ //TODO set flag
						return -1;
				}
				else if(memcmp(indexTable[hv].MD5 , md5 , 16)){
						return hv;
				}
				else if(indexTable[hv].indexFlag & INDEX_COLLISION){
						return findIndex(indexTable[hv].collision , indexTable);
				}
				return -1;
		}
		else {
				return -1;
		}
}
size_t receiveObj(int fd,unsigned char * obj , off_t bufferSize){
		off_t bytes=0,cnt=0;
		unsigned char *ptr = obj;
		unsigned char buf[CHUNK];
		//bytes=read(STDIN_FILENO, buf, 512);
		bytes=read( fd , buf, 512);
		while(bytes > 0)
		{
				cnt+=bytes;
				if(cnt >= bufferSize){
						return -1;
				}
				memcpy(ptr,buf,bytes);
				ptr+=bytes;
				bytes=read(fd, buf, 512);
		}
		*ptr = '\0';
		return (ptr-obj);

}
unsigned char *md5str( char *in ,unsigned char * out,off_t length){
        MD5_CTX c;
        char buf[512];
        off_t bytes=0;
        off_t readlen=512;
        //unsigned char out[MD5_DIGEST_LENGTH];
        memset(&c , 0 , sizeof(c));

        MD5_Init(&c);
		if(length<readlen){
				readlen = length;
		}
        bytes=readString((char **)&in , buf, readlen);
        //bytes=read(fdin , buf, 512);
        while(bytes > 0 && readlen > 0)
        {
                MD5_Update(&c, buf, bytes);
				length -= bytes;
                if(length<readlen){
                		readlen = length;
				}
				bytes=readString((char **)&in , buf, readlen);
                //bytes=read(STDIN_FILENO, buf, 512);
        }

        MD5_Final(out, &c);

        //printf("%d\n",MD5_DIGEST_LENGTH);

        return out;

}

unsigned char *md5(int fdin,unsigned char * out,off_t length){
        MD5_CTX c;
        char buf[CHUNK];
        ssize_t bytes=0;
        off_t readlen=CHUNK;
        //unsigned char out[MD5_DIGEST_LENGTH];
        memset(&c , 0 , sizeof(c));

        MD5_Init(&c);
        //bytes=readString(&in , buf, readlen);
		if(length<readlen){
				readlen = length;
		}
        bytes=read(fdin , buf, readlen);
        while(bytes > 0 && readlen > 0)
        {
                MD5_Update(&c, buf, bytes);
				length -= bytes;
                if(length<readlen){
                		readlen = length;
				}
				//bytes=readString(&in , buf, readlen);
                bytes=read(fdin , buf, readlen);
        }

        MD5_Final(out, &c);

        //printf("%d\n",MD5_DIGEST_LENGTH);
		lseek(fdin,0,SEEK_SET);

        return out;

}
char *md5ToHex(unsigned char *MD5,char *hexBuf){
		char *ptr = hexBuf;
		int n;
		for(n=0 ; n < MD5_DIGEST_LENGTH ; n++,ptr+=2){
				        sprintf(ptr,"%02x",MD5[n]);
		}
		return hexBuf;
}
void hexToMD5(unsigned char* md5out , char *in){
		int i=0,j;
		for(i=0,j=0;j<16;i+=2,j++){
				md5out[j] = ( (hexToChar(in[i]) <<4) | hexToChar(in[i+1])  ) ;
		}
}
unsigned char hexToChar(char c){
		switch(c){
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':return (unsigned char) (c-48 );
				case 'a':
				case 'b':
				case 'c':
				case 'd':
				case 'e':
				case 'f':return (unsigned char) (c-87 );
				case 'A':
				case 'B':
				case 'C':
				case 'D':
				case 'E':
				case 'F':return (unsigned char) (c-55 );
				default : 
						 fprintf(stderr,"must [0-1a-f]\n");
						 exit(4);
		}
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
off_t readString(char **src , char * dst , int length){
		char *cur = *src;
		char *head = *src;
		while( ( (cur-head) < length )  && ( *cur != EOF )  ){
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
				exit(3);
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
void saveFileIndex(FileIndex *fileIndex){
		char fileIndexFile[100];
		int tmp=-2;
		sprintf(fileIndexFile,"%sfn.inx",DB.PATH);
		tmp = saveVariable( (void *) fileIndex, sizeof(FileIndex) , DB.BUCKETNUM , fileIndexFile);
		if(tmp != (sizeof(FileIndex) * DB.BUCKETNUM) ){
				fprintf(stderr , "[%d] saveFileIndex() error\n",tmp);
				exit(2);
		}
}
void saveRecordIndex(RecordIndex *recIndex){
		char recIndexFile[100];
		int tmp=-2;
		sprintf(recIndexFile,"%srec.inx",DB.PATH);
		tmp = saveVariable( (void *) recIndex , sizeof(RecordIndex) , DB.BUCKETNUM , recIndexFile);
		if(tmp != (sizeof(RecordIndex) * DB.BUCKETNUM) ){
				fprintf(stderr , "[%d] saveRecordIndex() error\n",tmp);
				exit(2);
		}
}
void saveIndex(Index *indexTable){
		char indexFile[100];
		int tmp=-2;
		sprintf(indexFile,"%sdb.inx",DB.PATH);
		tmp = saveVariable( (void *) indexTable, sizeof(Index) , DB.BUCKETNUM , indexFile);
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
				fprintf(stderr , "get error[%s]\n",filename);
				exit(3);
				//TODO some error handle
		}
		return verify;
}
int putObject(int fdin , off_t size , char *fileprefix , unsigned char *fid , off_t *offset ){
		//TODO check file size
		int fileId = 0;
		char filename[50]={};
		int fd;
		off_t verify=0;
		unsigned char buf[CHUNK];
		off_t readlen=CHUNK,bytes=0,length=size;
		

		lseek(fdin,0,SEEK_SET);

		fileId = fileId | *fid ;
		sprintf(filename,"%s%04d",fileprefix,fileId);
		fd = open(filename,O_WRONLY|O_CREAT , S_IWUSR | S_IRUSR  );
		//lseek;
		lseek(fd,*offset,SEEK_SET);
		//read(fdin , ) TODO TODO use loop to write
		if(length<readlen){
				readlen = length;
		}

		bytes=read(fdin , buf, readlen);
        while(bytes > 0 && readlen > 0)
        {
				verify += write(fd , buf,size );
				length -= bytes;
                if(length<readlen){
                		readlen = length;
				}
                bytes=read(fdin , buf, readlen);
        }

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
				fprintf(stderr , "getObj error[%d](%s)!\nread: %zd\n{%s}\n", errno , filename ,verify, (char*)start);
				return -1;
				//TODO some error handle
		}
		return verify;
}
off_t getItem( Index *indexTable , FileIndex *fileIndex , char *objname , unsigned char *md5out , int byName){
		
		off_t index=-1,bytes = 0 ;
		int fd=-1;
		int fileId = 0;
		char filename[100];
		if(byName == 1){
				index = getIndexbyName(objname , fileIndex , false);//TODO should get ft.index , not findex
		}else{
				index = getIndex(md5out , indexTable);
		}
		if(index == -1 ) return -1;

		fileId = fileId | indexTable[index].fid ;
		sprintf(filename,"%s%04d",DB.PATH,fileId);

		fd = open(filename,O_RDONLY);
		//lseek;
		lseek(fd,bytesToOffset(indexTable[index].offset,INDEX_OFFSET_LENGTH ),SEEK_SET);
		char buf[CHUNK];
        off_t readlen=CHUNK,length = indexTable[index].size;
        //unsigned char out[MD5_DIGEST_LENGTH];

        //bytes=readString(&in , buf, readlen);
		if(length<readlen){
				readlen = length;
		}
        bytes=read(fd , buf, readlen);
        while(bytes > 0 && readlen > 0)
        {
				bytes = write(STDOUT_FILENO , buf,bytes );
				length -= bytes;
                if(length<readlen){
                		readlen = length;
				}
				//bytes=readString(&in , buf, readlen);
                bytes=read(fd , buf, readlen);
        }



		
		return index;

}
void putRecord(off_t recordId , char *filename , off_t parrent ,unsigned char *MD5 , char *describe , off_t rec_size){
		fprintf(stderr , "rid:%zd\nfilename: %s\nparrent:%zd\ndescribe: %s\n",
						recordId,filename,parrent,describe);
		int fd=-1;
		char recFileName[100];
		char *record , *ptr;
		int size = sizeof(char) * DB.RECBLOCK;
		int len;
		char fileType[50]="" , md5Hex[33];
		time_t now = time(NULL);
		record = (char *) malloc ( size);
		memset(record , 0 , size);


		/*find filetype*/
		
		for(len = strlen(filename)-1;len>=0;len--){
				if(filename[len] == '.'){
						ptr = &filename[len];
						strcpy(fileType , ++ptr);
						break;
				}
		}
		if(len <= 0){ //first character is '.'  that is not data type , is hidden!
				strcpy(fileType , "none");				
		}
		/*end find filetype*/

		sprintf(recFileName,"%s001.rec",DB.PATH);
		fd = open( recFileName ,O_WRONLY|O_CREAT , S_IWUSR | S_IRUSR  );
		lseek(fd,DB.rec_offset,SEEK_SET);
		sprintf(record , 
		"@rid:%zd\n@_deleteFlag:%s@type:%s\n@name:%s\n@parrent:%zd\n@ctime:%ld\n@size:%zd\n@mtime:%ld\n@MD5:%s\n@desc:%s\n@_end:@\n",
		recordId , "0",fileType , filename , parrent , now , rec_size , now , md5ToHex(MD5,md5Hex), describe);
		//TODO verify ()
		write(fd , record ,size );


		updateRecIndex(recordId , DB.rec_offset , rec_size , INDEX_EXIST);
		DB.rec_offset+= size;
		close(fd);
		
}
char *getRecord(off_t rid , char *record){
		RecordIndex recIndex=getRecIndex(rid);
		char recFileName[100];
		int fd;
		sprintf(recFileName,"%s001.rec",DB.PATH);
		fd = open( recFileName ,O_RDONLY );
		lseek(fd,recIndex.rec_offset,SEEK_SET);
		read(fd , record , recIndex.rec_size);
		record[recIndex.rec_size] = '\0';
		return record;


}
int getRecColumn(char *record , char *key , char *valueBuf){
		/*
		* return value : if 0 => fail ; if x => x = length of column (@key:value)
		*/
		char rec_key[50];
		int rec_key_length;
		char *recPtr=NULL , *valPtr=NULL , *ptr;
		sprintf(rec_key , "@%s:" , key );
		rec_key_length = strlen(rec_key);
		//TODO
		recPtr = strstr(record , rec_key);
		if(recPtr){
				valPtr = recPtr + rec_key_length;
				ptr = valPtr;

				/*find \n to get value*/
				while(*ptr != '\0' || *ptr != '\n'){
						ptr++;
				}
				*ptr = '\0'; //here must \n or \0 ; just remove \n;
				strcpy( valueBuf , valPtr);
				return ptr - recPtr;

		}
		else{
				return 0;
		}
}
void updateRecord(RecordIndex recIndex , off_t recordId  , char *columnName , char *value){
		//TODO

}
void deleteRecord(off_t rid){
		deleteRecIndex(rid);
		//TODO deleteReal Record  //maybe @_deleteFlag set 1

}

off_t updateRecIndex(off_t rid , off_t rec_offset , off_t rec_size , unsigned char indexFlag ){
		char recIndexFile[50];
		off_t recIdxOffset;
		RecordIndex recIndex = { rid , rec_offset , rec_size , indexFlag};
		sprintf(recIndexFile,"%srec.inx",DB.PATH);
		int fd;
		recIdxOffset = recIndex.rid * sizeof(RecordIndex);

		fd = open( recIndexFile ,O_WRONLY|O_CREAT , S_IWUSR | S_IRUSR  );
		lseek(fd , recIdxOffset ,SEEK_SET);
		write(fd , &recIndex ,sizeof(RecordIndex) );
		close (fd);
#if DEBUG
		getRecIndex(recIndex.rid);
#endif

		return 0;//TODO
}
RecordIndex getRecIndex(off_t rid){
		RecordIndex recIndex;
		char recIndexFile[50];
		off_t recIdxOffset;
		
		sprintf(recIndexFile,"%srec.inx",DB.PATH);
		int fd;
		recIdxOffset = rid * sizeof(RecordIndex);

		fd = open(recIndexFile , O_RDONLY);
		if(fd<0){
				//error handle
		}
		lseek(fd , recIdxOffset ,SEEK_SET);
		read(fd , &recIndex , sizeof( RecordIndex ) );
		
#if DEBUG
		fprintf(stderr , "rid: %zd;offset: %zd ;size : %zd ;flag:%d" ,recIndex.rid , recIndex.rec_offset,recIndex.rec_size,recIndex.indexFlag);
#endif
		close(fd);

		return recIndex;
}
off_t deleteRecIndex(off_t rid){
		updateRecIndex( rid , -1 , 0 , INDEX_DELETE);

		return 0;//TODO

}
off_t putItem(Index *indexTable, FileIndex *fileIndex,char *filename, int fd, off_t size, int byName,RecordIndex *recIndex, off_t rid , off_t rec_parrent , char *describe , char *name){
		unsigned char md5out[MD5_DIGEST_LENGTH];
		off_t index = -1 , f_index = -1 , startOffset , bytes;
		md5(fd , md5out , size);
		index = getIndex(md5out , indexTable);
		f_index = getIndexbyName(filename , fileIndex , true); //TODO should get findex ,not fT.index
		if(fileIndex[f_index].indexFlag & INDEX_EXIST && !(fileIndex[f_index].indexFlag & INDEX_DELETE)){
				return -1;
		}
		/*put to RDB*/
		putRecord( rid , name , rec_parrent , md5out , describe , size );




		/*end put to RDB*/

		if(index != -1){//it found in table
				//TODO save fileIndex:save filename and point to indexTable;
				////memcpy(buffer,md5out,MD5_DIGEST_LENGTH);
				indexTable[index].indexFlag = indexTable[index].indexFlag & 0xfe; 
				indexTable[index].ref++;
				saveIndex(indexTable);

				memcpy(fileIndex[f_index].filename,filename , strlen(filename) );
				memcpy ( fileIndex[f_index].MD5  , md5out , MD5_DIGEST_LENGTH);
				fileIndex[f_index].index = index;
				fileIndex[f_index].indexFlag = INDEX_EXIST;
				saveFileIndex(fileIndex);
				//TODO saveRecordIndex();
				return index;
		}
		startOffset = DB.offset;
		bytes = putObject( fd , size , DB.PATH , &DB.curFid , &DB.offset);
		index = updateIndex(indexTable , fileIndex , filename , md5out , DB.curFid , startOffset , size);
		if(bytes != indexTable[index].size){//TODO error handle
		}
		////memcpy(buffer,md5out,MD5_DIGEST_LENGTH);
		saveDB();
		return index;
}
off_t updateIndex(Index *indexTable , FileIndex *fileIndex ,char *filename, unsigned char *md5 , unsigned char fid , off_t offset , off_t size ){

		off_t locate =-1 , f_index;
		locate = findLocate(getHV(md5) , indexTable);
		if(locate == -1){
				DB.isFull = 1;
				fprintf(stderr , "DB is full (index num full)\n");
				exit(5);
		}
		f_index = getIndexbyName(filename , fileIndex , true);
		memcpy(fileIndex[f_index].filename,filename , strlen(filename) );
		memcpy ( fileIndex[f_index].MD5  , md5 , MD5_DIGEST_LENGTH);
		fileIndex[f_index].index = locate;
		fileIndex[f_index].indexFlag = INDEX_EXIST;
		saveFileIndex(fileIndex);
		//TODO saveRecordIndex();




		memcpy(indexTable[locate].MD5 , md5 , MD5_DIGEST_LENGTH );
		indexTable[locate].fid = DB.curFid ;
		offsetTobytes(indexTable[locate].offset , offset , INDEX_OFFSET_LENGTH) ;
		indexTable[locate].size = (unsigned int )size;
		indexTable[locate].collision = 0 ;
		indexTable[locate].indexFlag = 0 ;
		indexTable[locate].indexFlag |= INDEX_EXIST ;
		indexTable[locate].ref = 1;
		saveIndex(indexTable);
		return locate;

}
off_t findLocate(off_t hv , Index *indexTable){
		off_t i;
		for(i=0 ; i<DB.BUCKETNUM ; i++,hv++){
				if(i == DB.BUCKETNUM) hv %= DB.BUCKETNUM;
				if(indexTable[hv].indexFlag & INDEX_EXIST && !(indexTable[hv].indexFlag & INDEX_DELETE)){
						indexTable[hv].indexFlag = indexTable[hv].indexFlag | INDEX_COLLISION;
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
void parseCmdToRec(char *optarg,off_t *rid,off_t *parrent,char *name,char *describe){
		char *rptr,*pptr,*nptr,*dptr,*ptr;
		
		ptr = optarg;
		rptr=ptr;


		while(*ptr && *ptr != ';'){
				ptr++;
		}
		*ptr = '\0';
		ptr++;
		pptr = ptr;


		while(*ptr && *ptr != ';'){
				ptr++;
		}
		*ptr = '\0';
		ptr++;
		nptr = ptr;
		while(*ptr && *ptr != ';'){
				ptr++;
		}
		*ptr = '\0';
		ptr++;
		dptr = ptr;

		
		*rid = (off_t) atof(rptr);
		*parrent = (off_t) atof(pptr);
		strcpy(name,nptr);
		strcpy(describe,dptr);

		

}
