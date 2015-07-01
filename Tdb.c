#define _GNU_SOURCE
#include <openssl/md5.h>
#include <unistd.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<fcntl.h>
#include<ctype.h>
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
		long int index;
		unsigned char indexFlag;
};
struct RecordIndex{
		long int rid;
		//unsigned char MD5[MD5_DIGEST_LENGTH];
		long int rec_offset;
		long int rec_size;
		unsigned char indexFlag;
};
struct Record{
		long int recordId;
		char type[20];
		char path[50];
		int ctime;
		long int objectSize;
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
		long int MAXFILESIZE;
		int BUCKETSIZE;
		long int BUCKETNUM;
		unsigned int OBJSIZE;
		unsigned char curFid;
		long int offset;
		int isFull;
		//RECORD
		long int rec_offset;
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
size_t receiveObj(int fd,unsigned char * obj , long int bufferSize);
long int getIndex(unsigned char *md5 , Index * indexTable);
long int getIndexbyName(char *filename , FileIndex *fileIndex , int isFindex);
long int getItem( Index *indexTable , FileIndex *fileIndex,char *ridString , unsigned char *md5out ,  int byRid);
long int putItem(Index *indexTable, FileIndex *fileIndex,char *filename, int fd, long int size, int byName,RecordIndex *recIndex, long int rid , long int rec_parent , char *describe , char *name);
void saveDB();
void saveIndex(Index *indexTable);//TODO only save modify index
void saveFileIndex(FileIndex *fileIndex);
void saveRecordIndex(RecordIndex * recIndex);
/*
 * record function
 */
void parseCmdToRec(char *optarg,long int *rid,long int *parent,char *name,char *describe);
void parseToColumn(char *optarg , long int *rid , char *key , char *value  );
void putRecord(long int recordId ,char *filename , long int parent ,unsigned char *MD5 , char *describe , long int rec_size , long int obj_index);
/*after putRecord : must saveDB()*/
void writeRecord(long int recordId , char *record , long int rec_size , long int rec_offset);
/*
direct write recordString to x.rec (write to end) and update recIndex;
//TODO
*/
void rewriteRecord(long int recordId , char *record , long int rec_size);
/*
for new_rec_size > old_rec_size;
//TODO 1. mark old record _deleteFlag:1 ; 2. update recIndex ; 3. append to end;
*/
char *getRecColumn(long int rid , char* key , char * valueBuf);
char *getRecordString(long int rid );
void updateRecStringColumn(char *record , long int rec_size , char *key , char *valueBuf);
int getRecStringColumn(char *record , char *key , char *valueBuf , char **colHead);
void updateRecord( long int recordId  , char *columnName , char *value);
void deleteRecord(long int rid);
void deleteFromParrent(long int rid , long int Drid);

RecordIndex getRecIndex(long int rid);
long int updateRecIndex(long int rid , long int rec_offset , long int rec_size , unsigned char indexFlag );
long int deleteRecIndex(long int rid);
/*dirRec function*/
void createDir(long int rid , long int parent , char *filename , char *describe);
void addToDir(long int rid , long int parent);
void readDir(long int Drid);
void deleteDir(long int Drid);
char *traverseChildren(char *childrenString , char (*callback)(long int));
/*callback*/
void printChildJson(long int childRid);
int firstCharcmp(const void *a ,const void *b);
//append
/*
type:		get by filename						in the func.
parent:	receive from client
ctime:		use something like :Date().now		in the func.
tags:		default no tag, add when update
recordId:	get from node.js
*/
/*search function*/

void search(char *searchString , int startPosition , int limit);//if limit<=0 => no limit;
char *searchTraverseRecord(char must[][30] , char except[][30] , char optional[][30] , int num[] , int startPosition , int limit);
int searchRecord(char *record , char patterns[][30] , int *count);
int parseSearchString(char *searchString , char mustitems[][30] , char exceptitems[][30] , char optionalitems[][30] ,  int *num);
/*
 * tool function
*/
char *gaisRecToJson(char *gaisRec , char *buf);
int putObject(int fd , long int size , char *fileprefix , unsigned char *fid , long int *offset );
int getObject(void *start , long int size , char *fileprefix , unsigned char fid , long int offset );
int saveVariable(void *start, int size , int nnum , char *filename);
unsigned char *md5(int fd , unsigned char * out , long int length);
unsigned char *md5str( char * in , unsigned char * out , long int length);
long int readString(char **src , char * dst , int length);//src wiil move to new location
long int bytesToOffset(unsigned char *bytesNum,int size);
void offsetTobytes(unsigned char *c_offset , long int offset, int length);
long int getHV(unsigned char *md5);
long int findIndex(long int hv , Index *indexTable);
long int findLocate(long int hv , Index *indexTable);
long int updateIndex(Index *indexTable , FileIndex *fileIndex ,char *filename, unsigned char *md5 , unsigned char fid , long int offset ,long int size );
void hexToMD5(unsigned char* md5out , char *in);
char *md5ToHex(unsigned char *MD5,char *hexBuf);
unsigned char hexToChar(char c);
/*
 *main function
*/
int main(int argc , char *argv[] , char *envp[])
{
		//char *tmprecord;
		char buf[1024];

		
		int c=0;
		char dbini[100];
		char* const short_options = "b:Cd:D:f:F:G:iI:LM:n:o:p:r:R:s:T:u:U:";
		long int bytes=0,index=0,n,i ;
		long int parent=0 , rid;
		char name[512],describe[1024];
		char key[512] , value[1024];
		long int startPosition;
		char limit[50];

		unsigned char md5out[MD5_DIGEST_LENGTH];
		int fd;
		int argFlag=0;//fsnbopid ;  8bits!;  76543210 ;128  64  32  16  8  4  2  1
		Index *indexTable;
		FileIndex *fileIndex;
		RecordIndex *recIndex;
		RecordIndex tmprecIndex;
		struct option long_options[] = {
				{ "init" , 0 , NULL , 'i' },
				//{ "default" , 0 , NULL , 'd' },
				//{ "fnum" , 1 , NULL , 'u' },
				//{ "fsize" , 1 , NULL , 's' },
				//{ "bnum" , 1 , NULL , 'n' },
				//{ "bsize" , 1 , NULL , 'b' },
				//{ "maxobjsize" , 1 , NULL , 'o' },
				{ "path" , 1 , NULL , 'p' },
				//{ "filename" , 1 , NULL , 'f' },
				//{ "parent" , 1 , NULL , 'r'},
				{ "md5GET" , 1 , NULL , 'M'},
				{ "recordInfo" , 1 , NULL , 'I'},
				{ "delete" , 1 , NULL , 'd'},
				{ "file-put" , 1 , NULL , 'F'},
				{ "file-get" , 1 , NULL , 'G'},
				{ "recordUpdate" , 1 , NULL , 'u'},
				{ "list" , 0 , NULL , 'L'},
				{ "search" , 1 , NULL , 'S'},
				{ "createDir" , 1 , NULL , 'C'},
				{ "readDir" , 1 , NULL , 'R'},
				{ "updateDir" , 1 , NULL , 'U'},
				{ "deleteDir" , 1 , NULL , 'D'},
				{ "test" , 1 , NULL , 'T'},
				{ "recordGet" , 1 , NULL , 'r'},
				{ 0 , 0 , 0 , 0}
		};
		/*
		* cgi
		*/
		//getenv("CONTENT_LENGTH");

		while((c = getopt_long (argc, argv, short_options, long_options, NULL)) != -1){
				switch(c){
						/*case 'u' : 
							DB.FILENUM = atoi(optarg);
							argFlag = argFlag | ARG_DBFILENUM;
							break;
						case 's' : 
							DB.MAXFILESIZE = (long int)atol(optarg);
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
							sprintf(dbini,"db%sfn.inx",DB.PATH);
							getVariable( (void *)fileIndex, sizeof(FileIndex) , DB.BUCKETNUM , dbini);
							break;
						*/
						case 'u':
								parseToColumn(optarg , &rid , key , value  );
								updateRecord( rid , key , value );
							
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
									sprintf(dbini,"db%srec.inx",DB.PATH);
									getVariable( (void *)recIndex, sizeof(recIndex) , DB.BUCKETNUM , dbini);
									
									memset(indexTable , 0 , sizeof(Index)*DB.BUCKETNUM);
									sprintf(dbini,"%sdb.inx",DB.PATH);
									getVariable( (void *)indexTable, sizeof(Index) , DB.BUCKETNUM , dbini);
									sprintf(dbini,"db%sfn.inx",DB.PATH);
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
						case 'C' : 
							//putRecord( rid , name , rec_parent , md5out , describe , size , index);
							//parseCmdToRec(optarg,&rid,&parent,name,describe);
							createDir(rid , parent , name , describe);
							printf("{\"status\":200,\"message\":\"directory create success.\",\"rid\":%ld,\"parrent\":%ld}",rid,parent);
							
							break;
						case 'R':
							readDir(atol (optarg));
							break;
						case 'D' : 
							if(atol(optarg) == 0){
									fprintf(stderr , "can not remove root\n");
									exit(403) ;
							}
							deleteDir( atol (optarg));
							break;
/*						case 'M' : 
							hexToMD5(md5out , optarg);
							index = getItem(indexTable , fileIndex , filename , md5out ,  false );
//							fprintf(stderr,"[%ld]\n",index);
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
						case 'r' :

							//fprintf(stdout , "%s\n" ,getRecordString(atol(optarg))  );return 0;
							gaisRecToJson(getRecordString(atol(optarg) ) , buf);
							printf("%s",buf);
							return 0;
							break;
						case 'T' :
							//tmprecIndex = getRecIndex(33);
							//printf("%d",tmprecIndex.indexFlag);
							//tmprecord = getRecordString(atoi(optarg));
							//updateRecord( atol(optarg) ,  "children" , "14");
							//updateRecord( atol(optarg) ,  "children" , "14,24");
							//fprintf(stdout , "%s\n" ,getRecordString(atol(optarg))  );return 0;
							break;
						case 'G' :
							index = getItem(indexTable , fileIndex , optarg , md5out ,  true );
//							fprintf(stderr,"[%ld]\n",index);
							if(index==-1){
									fprintf(stderr , "obj not exist\n");
									exit(1);
							}
#if DEBUG
							fprintf(stderr,"[%ld]",index);
#endif
//							write(STDOUT_FILENO , buffer , indexTable[index].size );
							exit(0);
						case 'I':
							parseCmdToRec(optarg,&rid,&parent,name,describe);
							break;
						case 'U':
							parseToColumn(optarg , &rid , key , value  );//here key is curDir.rid.toString
							updateRecord( rid , "parent" , value );
							addToDir( rid , atol(value) );
							deleteFromParrent(rid , atol(key));
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
							index = putItem(indexTable , fileIndex , optarg , fd , bytes , true , recIndex , rid , parent , describe , name);
//long int putItem(Index *indexTable, FileIndex *fileIndex,char *filename, int fd, long int size, int byName,RecordIndex recIndex, long int rid , char *rec_parent , char *describe);
							close(fd);
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
											printf("{\"size\":%ld,\"filename\":\"%s\"}",(long int)indexTable[fileIndex[i].index].size,fileIndex[i].filename);
											n++;
									}
							}
							printf("]");
							exit(0);
						case 'S' :
							if(strstr(optarg,";")){
									parseToColumn(optarg , &startPosition , limit , buf  );
									search(buf , startPosition,atol(limit));
							}else{
									strcpy(buf , optarg);
									search(buf , 0 , 0);
							}
							/*printf("[");
							for(i=0,n=0;i<DB.BUCKETNUM;i++){//n is cnt for list
									if( ( fileIndex[i].indexFlag & INDEX_EXIST) && !(fileIndex[i].indexFlag & INDEX_DELETE)){
											if(!strcasestr(fileIndex[i].filename,optarg)){
											//if(strncmp(fileIndex[i].filename,optarg,strlen(optarg))){
													continue;
											}
											if(n){
													printf(",");
											}
											printf("{\"size\":%ld,\"filename\":\"%s\"}",(long int)indexTable[fileIndex[i].index].size,fileIndex[i].filename);
											n++;
									}
							}
							printf("]");
							exit(0);*/
							break;

						case 'd' : 
							//f_index = getIndexbyName( optarg , fileIndex , true);
							//index = fileIndex[f_index].index;
							getRecColumn( atol(optarg) , "type" , buf );
							if( !strcmp(buf , "dir")){
									if(atol(optarg) == 0){
											fprintf(stderr , "can not remove root\n");
											exit(403) ;
									}
									deleteDir( atol (optarg));
									return 0;
							}
							
							if( getRecColumn( atol(optarg) , "obj_index" , buf ) == NULL){
									fprintf(stderr,"object not exist(n)\n");
									exit(404);
							}
							index = atol( buf );
							//TODO add obj_offset to putRecord
							/*
							if(f_index == -1){// obj not exist
									fprintf(stderr,"object not exist(n)\n");
									exit(404);
							}
							*/
							if(indexTable[index].indexFlag & INDEX_DELETE){//obj not exist
									fprintf(stderr,"object not exist(y)\n");
									exit(404);
							}
							else{//do delete
									//fileIndex[f_index].indexFlag |= INDEX_DELETE;
									deleteRecord(atol(optarg));
									indexTable[index].ref--;
									if(indexTable[index].ref == 0){
											indexTable[index].indexFlag |= INDEX_DELETE;
									}
									saveIndex(indexTable);
									//saveFileIndex(fileIndex);
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
				saveIndex(indexTable);
				saveFileIndex(fileIndex);
				saveRecordIndex(recIndex);
				createDir( 0 , 0 , "root" , "root");
				saveDB();

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
long int getHV(unsigned char *md5){
		long int tmp=0;
		tmp = bytesToOffset(&md5[0],8) ^ bytesToOffset(&md5[8],8);
		if(tmp < 0 ) tmp = (tmp * -1);
		return tmp % DB.BUCKETNUM;
}
long int getIndexbyName(char *filename , FileIndex *fileIndex , int isFindex){
		unsigned char md5out[MD5_DIGEST_LENGTH];
		md5str(filename , md5out , strlen(filename));

		long int hv = getHV(md5out);
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
long int getIndex(unsigned char *md5 , Index * indexTable){
		long int hv = getHV(md5);

		return findIndex(hv , indexTable);
		
}
long int findIndex(long int hv , Index *indexTable){
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
size_t receiveObj(int fd,unsigned char * obj , long int bufferSize){
		long int bytes=0,cnt=0;
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
unsigned char *md5str( char *in ,unsigned char * out,long int length){
        MD5_CTX c;
        char buf[512];
        long int bytes=0;
        long int readlen=512;
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

unsigned char *md5(int fdin,unsigned char * out,long int length){
        MD5_CTX c;
        char buf[CHUNK];
        ssize_t bytes=0;
        long int readlen=CHUNK;
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
long int bytesToOffset(unsigned char *bytesNum,int size){
		int i=0;
		long int sum = 0;
		for(i=0;i<size;i++){
				sum = sum<<8;
				sum = sum | bytesNum[i];
		}
		return sum;
}
long int readString(char **src , char * dst , int length){
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
		sprintf(fileIndexFile,"db%sfn.inx",DB.PATH);
		tmp = saveVariable( (void *) fileIndex, sizeof(FileIndex) , DB.BUCKETNUM , fileIndexFile);
		if(tmp != (sizeof(FileIndex) * DB.BUCKETNUM) ){
				fprintf(stderr , "[%d] saveFileIndex() error\n",tmp);
				exit(2);
		}
}
void saveRecordIndex(RecordIndex *recIndex){
		char recIndexFile[100];
		int tmp=-2;
		sprintf(recIndexFile,"db%srec.inx",DB.PATH);
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
		close(fd);
		if(verify != (size * nnum)){
				fprintf(stderr , "get error[%s]\n",filename);
				exit(3);
				//TODO some error handle
		}
		return verify;
}
int putObject(int fdin , long int size , char *fileprefix , unsigned char *fid , long int *offset ){
		//TODO check file size
		int fileId = 0;
		char filename[50]={};
		int fd;
		long int verify=0;
		unsigned char buf[CHUNK];
		long int readlen=CHUNK,bytes=0,length=size;
		

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
        close(fd);

		if(verify != ( size )){
				fprintf(stderr , "putObj error\n");
				//TODO some error handle
		}
		*offset += verify;
		return verify;
}
int getObject(void *start , long int size , char *fileprefix , unsigned char fid , long int offset ){
		int fileId = 0;
		char filename[50]={};
		int fd;
		long int verify=-1;
		fileId = fileId | fid ;
		sprintf(filename,"%s%04d",fileprefix,fileId);
		fd = open(filename,O_RDONLY);
		//lseek;
		lseek(fd,offset,SEEK_SET);
		verify = read(fd , start,size );
		if(verify != ( size )){
				fprintf(stderr , "getObj error[%d](%s)!\nread: %ld\n{%s}\n", errno , filename ,verify, (char*)start);
				return -1;
				//TODO some error handle
		}
		close(fd);
		return verify;
}
long int getItem( Index *indexTable , FileIndex *fileIndex , char *ridString , unsigned char *md5out , int byRid){
		
		long int index=-1,bytes = 0 ;
		int fd=-1;
		int fileId = 0;
		char filename[100];
		long int rid = atol(ridString);
		char buf[CHUNK];
		if(byRid == 1){
				//index = getIndexbyName(objname , fileIndex , false);//TODO should get ft.index , not findex
				if( getRecColumn( rid , "obj_index" , buf ) == NULL){
						fprintf(stderr , "err rid in getItem");
						return -1;
				}
				index = atol(buf);
		}else{
				index = getIndex(md5out , indexTable);
		}
		if(index == -1 ) return -1;

		fileId = fileId | indexTable[index].fid ;
		sprintf(filename,"%s%04d",DB.PATH,fileId);

		fd = open(filename,O_RDONLY);
		//lseek;
		lseek(fd,bytesToOffset(indexTable[index].offset,INDEX_OFFSET_LENGTH ),SEEK_SET);
        long int readlen=CHUNK,length = indexTable[index].size;
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
        close(fd);



		
		return index;

}
void createDir(long int rid , long int parent , char *filename , char *describe){
		int fd=-1;
		char recFileName[100];
		char *record ;
		int size = sizeof(char) * DB.RECBLOCK * 4;
		char fileType[50]="" ;
		time_t now = time(NULL);
		record = (char *) malloc ( size);
		memset(record , 0 , size);


		/*give dir filetype*/
		
		strcpy(fileType , "dir");				

		sprintf(recFileName,"db%s001.rec",DB.PATH);
		fd = open( recFileName ,O_WRONLY|O_CREAT , S_IWUSR | S_IRUSR  );
		lseek(fd,DB.rec_offset,SEEK_SET);
		sprintf(record , 
		"@rid:%ld\n@_deleteFlag:%s\n@type:%s\n@name:%s\n@parent:%ld\n@ctime:%ld\n@mtime:%ld\n@desc:%s\n@children:\n@size:%d\n@_end:@\n",
		rid , "0",  fileType , filename , parent 
		, now , now , describe , 0);
		//TODO verify ()
		write(fd , record ,size );


		updateRecIndex( rid , DB.rec_offset , size , INDEX_EXIST);
		DB.rec_offset+= size;
		addToDir(rid , parent);
		/*TODO addToDir(){
				string = getRecColumn("children" , parent);
				if ( string ){//already has children
						string += ';'+rid;
				}else{
						updateColumn(parent , children , <rid>);
				}
		}
		*/
		close(fd);
		saveDB();
}
void addToDir(long int rid , long int parent){
		char *childrenBuf = (char *) malloc( sizeof(char) * DB.RECBLOCK * 4 );
		getRecColumn( parent , "children" , childrenBuf) ;
		if( childrenBuf[0] == '\0' ){//no child
				sprintf(childrenBuf , "%ld" , rid);
		}
		else{//already has some children
				sprintf(childrenBuf , "%s;%ld" , childrenBuf , rid);
		}
		updateRecord( parent , "children" , childrenBuf );
		free(childrenBuf);
}
void deleteDir(long int Drid){
		if(Drid == 0) return;
		char *childrenBuf = (char *) malloc( sizeof(char) * DB.RECBLOCK * 4 );
		childrenBuf = getRecColumn( Drid , "children" , childrenBuf) ;
		if(childrenBuf == NULL){
				fprintf(stderr ,"Not directory\n");
				return ;
		}
		if( childrenBuf[0] == '\0' ){//no child
				printf("{\"action\":\"readDir\",\"children\":[]}");
		}
		else{
				traverseChildren(childrenBuf , (void *)&deleteRecord);
		}
		deleteRecord(Drid);//delete self;
		free(childrenBuf);

}

void readDir(long int Drid){
		char *childrenBuf = (char *) malloc( sizeof(char) * DB.RECBLOCK * 4 );
		childrenBuf = getRecColumn( Drid , "children" , childrenBuf) ;
		if(childrenBuf == NULL){
				fprintf(stderr ,"Not directory\n");
				return ;
		}
		if( childrenBuf[0] == '\0' ){//no child
				printf("{\"action\":\"readDir\",\"children\":[]}");
		}
		else{
				printf("{\"action\":\"readDir\",\"children\":[");
				traverseChildren(childrenBuf , (void *)&printChildJson);
				printChildJson(-1);//clear printChildJson.isFirst
				printf("]}");
		}
		free(childrenBuf);

}
void printChildJson(long int childRid){/*not real json, just like "{childA},{childB},{childC}"*/
		static int isFirst=true;
		if(childRid == -1){
				isFirst=true;
				return ;
		}
		char *childRec = getRecordString(childRid);
		char *jsonBuf = (char *) malloc( sizeof(char) * DB.RECBLOCK * 4 );
		jsonBuf = gaisRecToJson( childRec , jsonBuf);
		if(isFirst){
				printf("%s" , jsonBuf );
				isFirst = false;
		}else{
				printf(",%s" , jsonBuf );
		}
		free (jsonBuf);
}
void deleteFromParrent(long int rid , long int Drid){
		char *childrenBuf = (char *) malloc( sizeof(char) * DB.RECBLOCK * 4 );
		char target[50],*sptr , *dptr;
		childrenBuf = getRecColumn( Drid , "children" , childrenBuf) ;
		if(childrenBuf == NULL){
				fprintf(stderr ,"Not directory\n");
				return ;
		}
		if( childrenBuf[0] == '\0' ){//something error
				fprintf(stderr,"no child!delete rid(%ld) in parent(%ld)\n" , rid ,Drid );
				exit(404);
		}
		else{
				if( sprintf(target , "%ld," , rid) && !strncmp(childrenBuf , target , strlen(target) ) ){
						sptr = childrenBuf+strlen(target);
						dptr = childrenBuf;
						while(*sptr){
								*(dptr++) = *(sptr++);
						}
						updateRecord( Drid  , "children" , childrenBuf);
				}else if( sprintf(target , "%ld" , rid) && !strcmp(childrenBuf , target ) ){
						childrenBuf[0]='\0';
						updateRecord( Drid  , "children" , childrenBuf);
				}else if( sprintf(target , ";%ld;" , rid) && (dptr=strstr(childrenBuf , target ) ) ){
						sptr = dptr+strlen(target);
						dptr++;//skip ';'
						while(*sptr){
								*(dptr++) = *(sptr++);
						}
						*dptr ='\0';
						updateRecord( Drid  , "children" , childrenBuf);
				}else if( sprintf(target , ";%ld" , rid) && ( dptr = strstr(childrenBuf , target ) ) ){
						*dptr = '\0';
						updateRecord( Drid  , "children" , childrenBuf);
				}
		}
#if DEBUG
		printf("%s",childrenBuf);
#endif
		free(childrenBuf);


}
char *traverseChildren(char *childrenString , char (*callback)(long int)){
		char *cBuf = (char *) malloc ( sizeof(char) * strlen(childrenString) + 1 ) ;
		strcpy(cBuf , childrenString );

		char *ptr = cBuf , *cptr;
		while(*ptr && !isdigit(*ptr) ){//skip to next child
				ptr++;
		}
		cptr=ptr;
		while(*ptr && *ptr != '\n'){
				callback( atol(cptr) );
				while( isdigit(*ptr) ){//skip to next child
						ptr++;
				}
				if(*ptr == ';')cptr = ++ptr;
				else break;

		}
		

		return NULL;
}
char *gaisRecToJson(char *gaisRec , char *buf){
		//TODO
		//int len = strlen (gaisRec);
		char *ptr=gaisRec;
		char *tmp;
		char *key=NULL,*value=NULL;
		int isFirst=true;
		sprintf(buf , "{");
		while(*ptr){
				while(*ptr && *ptr != '\n'){
						/*if( !col && strncmp(ptr,"@rid:" , 5 ) ){
								col = ptr;
								ptr++;
								continue;
						}
						else */
						if( !key && *ptr =='@'){
								tmp = ptr;
								while(*tmp && *tmp != ':' && *tmp != '\n' ) {
										tmp++;
								}
								if(*tmp == ':'){
										key = ++ptr;
										*tmp = '\0';
										value= ++tmp;
										while(*tmp && *tmp != '\n'){
												tmp++;
										}
										if(*tmp == '\n'){//got key & value;
												*tmp = '\0';
												ptr = tmp;
												if(!isFirst){
														sprintf(buf , "%s,\"%s\":\"%s\"" , buf , key , value);
												}else{
														isFirst=false;
														sprintf(buf , "%s\"%s\":\"%s\"" , buf , key , value);
												}
										}else{//should not 
												fprintf(stderr , "wrong gais record format:\n%s",gaisRec);
												exit(500);
										}
								}else{//should not 
										fprintf(stderr , "wrong gais record format:\n%s",gaisRec);
										exit(500);
								}
								key = value = tmp = NULL;
						}
						else{//should not 
								fprintf(stderr , "wrong gais record format:\n%s",gaisRec);
								exit(500);

						}
						ptr++;
				}
		}
		sprintf(buf , "%s}",buf);
		return buf;
}
void putRecord(long int recordId , char *filename , long int parent ,unsigned char *MD5 , char *describe , long int rec_size , long int obj_index){
		fprintf(stderr , "rid:%ld\nfilename: %s\nparent:%ld\ndescribe: %s\n",
						recordId,filename,parent,describe);
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

		sprintf(recFileName,"db%s001.rec",DB.PATH);
		fd = open( recFileName ,O_WRONLY|O_CREAT , S_IWUSR | S_IRUSR  );
		lseek(fd,DB.rec_offset,SEEK_SET);
		sprintf(record , 
		"@rid:%ld\n@_deleteFlag:%s\n@obj_index:%ld\n@type:%s\n@name:%s\n@parent:%ld\n@ctime:%ld\n@size:%ld\n@mtime:%ld\n@MD5:%s\n@desc:%s\n@_end:@\n",
		recordId , "0", obj_index , fileType , filename , parent 
		, now , rec_size , now , md5ToHex(MD5,md5Hex), describe);
		//TODO verify ()
		write(fd , record ,size );


		updateRecIndex(recordId , DB.rec_offset , size , INDEX_EXIST);
		DB.rec_offset+= size;
		addToDir(recordId , parent);
		close(fd);
		saveDB();
		
}
char *getRecordString(long int rid ){//TODO write back function ; if write back : check size isBigger than recInx.size
		char *record;
		RecordIndex recIndex=getRecIndex(rid);
		int size = sizeof(char ) * recIndex.rec_size * 2;
		record = (char *) malloc ( size );
		memset(record , 0 , size);

		char recFileName[100];
		int fd;
		sprintf(recFileName,"db%s001.rec",DB.PATH);
		fd = open( recFileName ,O_RDONLY );
		lseek(fd,recIndex.rec_offset,SEEK_SET);
		read(fd , record , recIndex.rec_size);
		record[recIndex.rec_size] = '\0';
		close(fd);
		return record;


}
void freeRecordString(char *record){
		free(record);
		return;
}
int getRecStringColumn(char *record , char *key , char *valueBuf , char **colHead){
		/*
		* return value : if 0 => fail ; if x => x = length of column (@key:value)
		*/
		char rec_key[50];
		int rec_key_length;
		char *colPtr=NULL , *valPtr=NULL , *ptr , ctmp;
		sprintf(rec_key , "@%s:" , key );
		rec_key_length = strlen(rec_key);
		//TODO
		colPtr = strstr(record , rec_key);
		if(colPtr){
				if(colHead){
						*colHead = colPtr;
				}
				valPtr = colPtr + rec_key_length;
				ptr = valPtr;

				/*find \n to get value*/
				while(*ptr != '\0' && *ptr != '\n'){
						ptr++;
				}
				ctmp = *ptr;
				*ptr = '\0'; //here must \n or \0 ; just remove \n;
				strcpy( valueBuf , valPtr);
				*ptr = ctmp;
				return ptr - colPtr;

		}
		else{
				return 0;
		}
}
void updateRecStringColumn(char *record , long int rec_size , char *key , char *valueBuf){
		char *recPtr = record , *colPtr , *endPtr; //point three part ; head , columnHead , after column
		int colLen;
		char *buf = (char *) malloc( sizeof(char) * rec_size +1);
		/*ptr graph*/
		/*v(recPtr)v(colPtr) v(endPtr)    */
		/*@xx:xxx\n@key:val\n@XX:xxxxx */
		colLen = getRecStringColumn(record , key , buf , &colPtr);
		if(colLen){
				endPtr = colPtr + colLen + 1 ;//skip \n
				strncpy(buf , recPtr , colPtr - recPtr );
				buf[colPtr-recPtr] = '\0';
				sprintf(buf , "%s@%s:%s\n%s" , buf , key , valueBuf , endPtr);
				memset(record , 0 , rec_size);
				strcpy(record , buf);

		}else{
				//error handle
				//return ;
		}

		free(buf);
		return ;
}
char *getRecColumn(long int rid , char* key , char * valueBuf){
		char *record = getRecordString(rid);
		int result;
		result = getRecStringColumn( record  , key , valueBuf , NULL);
		free(record);
		if(result == 0) return NULL;
		return valueBuf;

}
void updateRecord(long int recordId  , char *columnName , char *value){
		//TODO write back  ; if write back : check size isBigger than recInx.size//TODO
		RecordIndex recIndex=getRecIndex(recordId);
		int fd=-1;
		char recFileName[100];
		char *record ;
		int size = recIndex.rec_size;
		time_t now = time(NULL);
		int isRecSizeChange=0;
		char timeBuf[50];
		record = getRecordString(recordId);
		updateRecStringColumn(record ,  recIndex.rec_size , columnName , value);
		sprintf( timeBuf , "%ld" , now);
		updateRecStringColumn(record ,  recIndex.rec_size , "mtime" , timeBuf );

		//TODO verify (); check size
		if(!isRecSizeChange){
				sprintf(recFileName,"db%s001.rec",DB.PATH);
				fd = open( recFileName ,O_WRONLY|O_CREAT , S_IWUSR | S_IRUSR  );
				lseek(fd,recIndex.rec_offset ,SEEK_SET);
				write(fd , record ,size );
				updateRecIndex(recordId , recIndex.rec_offset , size , INDEX_EXIST);
		}
		else{
		}
		close(fd);
		


}
void writeRecord(long int recordId , char *record , long int rec_size , long int rec_offset){
		int fd;
		char recFileName[100];
		sprintf(recFileName,"db%s001.rec",DB.PATH);
		fd = open( recFileName ,O_WRONLY|O_CREAT , S_IWUSR | S_IRUSR  );
		lseek(fd,rec_offset ,SEEK_SET);
		write(fd , record , rec_size );
		updateRecIndex(recordId , rec_offset , rec_size , INDEX_EXIST);
		close(fd);
		return ;
}
int firstCharcmp(const void *a ,const void *b){
		return *((char *)a) - *((char *)b);
		//return strcmp((char*) a, (char *) b);
}
int parseSearchString(char *searchString , char mustitems[][30] , char exceptitems[][30] , char optionalitems[][30] ,  int *num){

		char *token = " ";
		char *tmpItem;


		num[0]=num[1]=num[2]=0;

		tmpItem = strtok(searchString , token);
		while(tmpItem){
				if(tmpItem[0] == '+'){
						strcpy(optionalitems[num[2]++] , (tmpItem+1));
				}
				else if(tmpItem[0] == '-'){
						strcpy(exceptitems[num[1]++] , (tmpItem+1));
				}
				else{
						strcpy(mustitems[num[0]++] , tmpItem);
				}
				tmpItem = strtok(NULL ,token );
		}

		strcpy(mustitems[num[0]] , "");
		strcpy(exceptitems[num[1]] , "");
		strcpy(optionalitems[num[2]] , "");
		


		return true;

}
void search(char *searchString , int startPosition , int limit){
		char mustitems[30][30];
		char exceptitems[30][30];
		char optionalitems[30][30];
		int num[3];
		parseSearchString(searchString , mustitems , exceptitems , optionalitems , num);
		searchTraverseRecord(mustitems , exceptitems, optionalitems, num , startPosition , limit);

}

char *searchTraverseRecord(char must[][30] , char except[][30] , char optional[][30] , int num[] , int startPosition , int limit){
		RecordIndex recIndex;
		char recIndexFile[50];
		long int recIdxOffset;
		int fd;
		int j,n;
		long int rid;
		int count[3][30];
		int result=false;
		char *recordString;
		char *childrenBuf = (char *) malloc( sizeof(char) * DB.RECBLOCK * 4 );
		int position=0;
		int isFirst=true;
		char *jsonBuf = (char *) malloc( sizeof(char) * DB.RECBLOCK * 4 );
		char *childRec;
		char *ptr;

		sprintf(recIndexFile,"db%srec.inx",DB.PATH);
		fd = open(recIndexFile , O_RDONLY);
		if(fd<0){
				//error handle
		}
		rid=0;
		childrenBuf[0] = '\0';
		printf("{\"action\":\"search\",\"children\":[");
		while(  ( n = read(fd , &recIndex , sizeof( RecordIndex ) ) ) > 0  ){
				rid++;//rid:0 not need search
				if(recIndex.indexFlag & INDEX_EXIST ){
						if(recIndex.indexFlag & INDEX_DELETE)continue;
				}else{
						continue;
				}
				for(j=0;j<30;j++){
						count[0][j]=count[1][j]=count[2][j]=0;
				}
				result=0;
				recIdxOffset = rid * sizeof(RecordIndex);
				lseek(fd , recIdxOffset ,SEEK_SET);

				recordString = getRecordString( rid );
				if( num[1]>0 &&  searchRecord(recordString , except , count[1]) ){
						free(recordString);
						continue;
				}

				if( (num[0]>0) ){
						result=searchRecord(recordString , must , count[0]);
						if(result == false){
								free(recordString);
								continue;
						}
						for(j=0;j<num[0];j++){
								if(count[0][j] == 0){
										break;
								}
						}
						if(j!=num[0])continue;
						/*TODO for loop check every count has value*/
				}
				
				if( (num[2]>0) ){
						result=searchRecord(recordString , optional , count[2]);
						if(num[0]==0 && (result==0) ){
								free(recordString);
								continue;
						}
				}else{
				}
				if(position>=startPosition){
						sprintf( childrenBuf ,"%s;%ld" , childrenBuf , rid);
						/*printChildJson*/
						childRec = getRecordString(rid);
						jsonBuf = gaisRecToJson( childRec , jsonBuf);
						ptr = &jsonBuf[strlen(jsonBuf)-1];
						sprintf(ptr , ",\"rating\":%d}",result);
						if(isFirst){
								printf("%s" , jsonBuf );
								isFirst = false;
						}else{
								printf(",%s" , jsonBuf );
						}

						/*end pcj*/
						limit--;
						if(limit==0)break;
				}
				position++;
				//free(recordString);

		}
		if( limit<0 &&n != 0){
				fprintf(stderr , "unknown error: n = %d\n",n);
				exit(500);
		}
		//traverseChildren(childrenBuf , (void *)&printChildJson);
		/*traverseChild and print json*/
		/*tapj and*/
		printChildJson(-1);//clear printChildJson.isFirst
		printf("]}");

		free (jsonBuf);
		free(childrenBuf);
		close(fd);

		return NULL;
}
int searchRecord(char *record , char patterns[][30] , int *count){//return total find ; count for each pat.

		int i=0;
		char *ptr ;
		int result=0;
		while(i<30){
				ptr = record;
				count[i] = 0;
				if(patterns[i][0] == '\0' )break;
				/*TODO 
				  check has key?
				  1.Y:strcpy(pattern, (pptr=strstr(patterns[i],":"))  +1);get key; search pattern;
				  2.N:strcpy(pattern,patterns[i]) ; search pattern;
				 */

				ptr = strcasestr(ptr,patterns[i]);
				while( ptr && *ptr ){
						/*TODO if(haskey) check key*/
						result++;
						count[i]++;
						ptr++;
						ptr = strcasestr(ptr,patterns[i]);
				}
				i++;
				
		}


		return result;

}
void deleteRecord(long int rid){
		char parentBuf[50];
		getRecColumn( rid , "parent" , parentBuf);
		updateRecord( rid  , "_deleteFlag" , "1");
		deleteRecIndex(rid);
		deleteFromParrent(rid , atol(parentBuf));
		//TODO deleteReal Record  //maybe @_deleteFlag set 1

}

long int updateRecIndex(long int rid , long int rec_offset , long int rec_size , unsigned char indexFlag ){
		char recIndexFile[50];
		long int recIdxOffset;
		RecordIndex recIndex = { rid , rec_offset , rec_size , indexFlag};
		sprintf(recIndexFile,"db%srec.inx",DB.PATH);
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
RecordIndex getRecIndex(long int rid){
		RecordIndex recIndex;
		char recIndexFile[50];
		long int recIdxOffset;
		
		sprintf(recIndexFile,"db%srec.inx",DB.PATH);
		int fd;
		recIdxOffset = rid * sizeof(RecordIndex);

		fd = open(recIndexFile , O_RDONLY);
		if(fd<0){
				//error handle
		}
		lseek(fd , recIdxOffset ,SEEK_SET);
		read(fd , &recIndex , sizeof( RecordIndex ) );
		
#if DEBUG
		fprintf(stderr , "rid: %ld;offset: %ld ;size : %ld ;flag:%d" ,recIndex.rid , recIndex.rec_offset,recIndex.rec_size,recIndex.indexFlag);
#endif
		close(fd);

		return recIndex;
}
long int deleteRecIndex(long int rid){
		updateRecIndex( rid , -1 , 0 , INDEX_DELETE);

		return 0;//TODO

}
long int putItem(Index *indexTable, FileIndex *fileIndex,char *filename, int fd, long int size, int byName,RecordIndex *recIndex, long int rid , long int rec_parent , char *describe , char *name){
		unsigned char md5out[MD5_DIGEST_LENGTH];
		long int index = -1 , startOffset , bytes;
		md5(fd , md5out , size);
		index = getIndex(md5out , indexTable);
		/*f_index = getIndexbyName(filename , fileIndex , true); //TODO should get findex ,not fT.index
		if(fileIndex[f_index].indexFlag & INDEX_EXIST && !(fileIndex[f_index].indexFlag & INDEX_DELETE)){
				return -1;
		}*/

		if(index != -1){//it found in table
				//TODO save fileIndex:save filename and point to indexTable;
				////memcpy(buffer,md5out,MD5_DIGEST_LENGTH);
				indexTable[index].indexFlag = indexTable[index].indexFlag & 0xfe; 
				indexTable[index].ref++;
				saveIndex(indexTable);
				/*put to RDB*/
				putRecord( rid , name , rec_parent , md5out , describe , size , index);
				/*end put to RDB*/

				/*
				memcpy(fileIndex[f_index].filename,filename , strlen(filename) );
				memcpy ( fileIndex[f_index].MD5  , md5out , MD5_DIGEST_LENGTH);
				fileIndex[f_index].index = index;
				fileIndex[f_index].indexFlag = INDEX_EXIST;
				saveFileIndex(fileIndex);
				*/
				return index;
		}
		startOffset = DB.offset;
		bytes = putObject( fd , size , DB.PATH , &DB.curFid , &DB.offset);
		index = updateIndex(indexTable , fileIndex , filename , md5out , DB.curFid , startOffset , size);
		/*put to RDB*/
		putRecord( rid , name , rec_parent , md5out , describe , size , index);
		/*end put to RDB*/
		if(bytes != indexTable[index].size){//TODO error handle
		}
		////memcpy(buffer,md5out,MD5_DIGEST_LENGTH);
		saveDB();
		return index;
}
long int updateIndex(Index *indexTable , FileIndex *fileIndex ,char *filename, unsigned char *md5 , unsigned char fid , long int offset , long int size ){

		long int locate =-1 , f_index;
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
long int findLocate(long int hv , Index *indexTable){
		long int i;
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
void offsetTobytes(unsigned char *c_offset , long int offset, int length){
		int i=0;
		for(i=length-1;i>=0;i--){
				c_offset[i] = offset & 0xff;
				offset = offset >> 8;
		}
}
void parseToColumn(char *optarg , long int *rid , char *key , char *value  ){
		char *rptr , *kptr , *vptr , *ptr;
		ptr = optarg;
		rptr = ptr;

		while( isdigit(*ptr) ){
				ptr++;
		}
		*(ptr++) = '\0';
		*rid = atol(rptr);

		kptr = ptr ;

		while(*ptr && *ptr != ';'){
				ptr++;
		}
		*(ptr++) = '\0';
		strcpy(key,kptr);

		vptr = ptr ;
		while(*ptr && *ptr != ';'){
				ptr++;
		}
		*(ptr++) = '\0';

		strcpy(value,vptr);
		return ; 


}
void parseCmdToRec(char *optarg,long int *rid,long int *parent,char *name,char *describe){
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

		
		*rid = (long int) atof(rptr);
		*parent = (long int) atof(pptr);
		strcpy(name,nptr);
		strcpy(describe,dptr);

		

}
