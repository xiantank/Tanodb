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
#define DEBUG 1
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
typedef enum{
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
};
typedef struct Index Index;
typedef struct Config Config;
typedef struct FileIndex FileIndex;
/*
 * global config varible declare
 */
Config DB;
#define BUFFERSIZE 3000000
unsigned char buffer[BUFFERSIZE];
/*
 * function declare
 */
int init();
int getVariable(void *start, int size , int nnum , char *filename);
size_t receiveObj(int fd,unsigned char * obj , off_t bufferSize);
off_t getIndex(unsigned char *md5 , Index * indexTable);
off_t getIndexbyName(char *filename , FileIndex *fileIndex , int isFindex);
off_t getItem( Index *indexTable , FileIndex *fileIndex,char *filename , unsigned char *md5out , unsigned char* buffer , int byName);
off_t putItem(Index * indexTable , FileIndex *fileIndex , char *filename ,unsigned char *buffer , off_t size , int byName );
void saveDB();
void saveIndex(Index *indexTable);//TODO only save modify index
void saveFileIndex(FileIndex *fileIndex);
/*
 * tool function
*/
int putObject(void *start , off_t size , char *fileprefix , unsigned char *fid , off_t *offset );
int getObject(void *start , off_t size , char *fileprefix , unsigned char fid , off_t offset );
int saveVariable(void *start, int size , int nnum , char *filename);
unsigned char *md5(char *in , unsigned char * out , off_t length);
int readString(char **src , char * dst , int length);//src wiil move to new location
off_t bytesToOffset(unsigned char *bytesNum,int size);
void offsetTobytes(unsigned char *c_offset , off_t offset, int length);
off_t getHV(unsigned char *md5);
off_t findIndex(off_t hv , Index *indexTable);
off_t findLocate(off_t hv , Index *indexTable);
off_t updateIndex(Index *indexTable , FileIndex *fileIndex ,char *filename, unsigned char *md5 , unsigned char fid , off_t offset ,off_t size );
void hexToMD5(unsigned char* md5out , char *in);
unsigned char hexToChar(char c);
/*
 *main function
*/
int main(int argc , char *argv[] , char *envp[])
{
		memset(buffer , 0 , BUFFERSIZE);

		int c=0;
		char dbini[100];
		char filename[100]="";
		char* const short_options = "b:dD:f:F:G:iI:LM:n:o:p:Ps:u:";
		off_t bytes=0,index=0,n,i , f_index;
		unsigned char md5out[MD5_DIGEST_LENGTH];
		int fd;
		int argFlag=0;//fsnbopid ;  8bits!;  76543210 ;128  64  32  16  8  4  2  1
		Index *indexTable;//TODO use malloc and check and memset(indexTable , 0)
		FileIndex *fileIndex;//TODO use malloc and check and memset(indexTable , 0)
		//char obj[DB.OBJSIZE];//TODO use malloc and check
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

				{ "md5GET" , 1 , NULL , 'M'},
				{ "nameGET" , 1 , NULL , 'G'},
				{ "idGET" , 1 , NULL , 'I'},
				{ "PUT" , 0 , NULL , 'P'},
				{ "file-put" , 1 , NULL , 'F'},
				{ "list" , 0 , NULL , 'L'},
				{ "delete" , 1 , NULL , 'D'},
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
									memset(indexTable , 0 , sizeof(Index)*DB.BUCKETNUM);
									sprintf(dbini,"%sdb.inx",DB.PATH);
									getVariable( (void *)indexTable, sizeof(Index) , DB.BUCKETNUM , dbini);
									sprintf(dbini,"%sfn.inx",DB.PATH);
									getVariable( (void *)fileIndex, sizeof(FileIndex) , DB.BUCKETNUM , dbini);
									strcpy(DB.PATH,optarg);
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
							memset(indexTable , 0 , sizeof(Index)*DB.BUCKETNUM);
							memset(fileIndex , 0 , sizeof(FileIndex)*DB.BUCKETNUM);
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
						case 'M' : 
							hexToMD5(md5out , optarg);
							index = getItem(indexTable , fileIndex , filename , md5out , buffer , false );
//							fprintf(stderr,"[%zd]\n",index);
							if(index==-1){
									fprintf(stderr , "obj not exist\n");
									exit(1);
							}
							write(STDOUT_FILENO , buffer , indexTable[index].size );
/*							for(n=0 ; n < MD5_DIGEST_LENGTH ; n++){
									printf("%02x",md5out[n]);
							}*/
							exit(0);
							return 0;

							break;
						case 'G' : 
							index = getItem(indexTable , fileIndex , optarg , md5out , buffer , true );
//							fprintf(stderr,"[%zd]\n",index);
							if(index==-1){
									fprintf(stderr , "obj not exist\n");
									exit(1);
							}
#if DEBUG
							fprintf(stderr,"[%zd]",index);
#endif
							write(STDOUT_FILENO , buffer , indexTable[index].size );
							exit(0);
						case 'I' : 

							break;
						case 'F' : 
							fd = open( optarg , O_RDONLY);
							if(fd<0){
									fprintf(stderr, "file open fail\n" );
									exit(1);
							}
							bytes = receiveObj(fd,buffer , BUFFERSIZE);
							if(bytes == -1){
									fprintf(stderr , "file size limit is %d" , BUFFERSIZE);
									exit(5);
							}
							index = putItem(indexTable , fileIndex , optarg , buffer , bytes , true);
							if(index == -1){
									fprintf(stderr , "filename exist\n");
									exit(2);
							}
							for(n=0 ; n < MD5_DIGEST_LENGTH ; n++){
									printf("%02x",buffer[n]);
							}
							exit(0);
						case 'P' : 
							bytes = receiveObj(STDIN_FILENO,buffer , BUFFERSIZE);
							if(bytes == -1){
									fprintf(stderr , "file size limit is %d" , BUFFERSIZE);
									exit(5);
							}
							index = putItem(indexTable , fileIndex , filename , buffer , bytes , false);
							if(index == -1){
									fprintf(stderr , "filename exist\n");
									exit(2);
							}
							for(n=0 ; n < MD5_DIGEST_LENGTH ; n++){
									fprintf(stdout,"%02x",buffer[n]);
							}
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
off_t getIndexbyName(char *filename , FileIndex *fileIndex , int isFindex){
		unsigned char md5out[MD5_DIGEST_LENGTH];
		md5(filename , md5out , strlen(filename));

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
		unsigned char buf[512];
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
unsigned char *md5(char *in,unsigned char * out,off_t length){
        MD5_CTX c;
        char buf[512];
        ssize_t bytes=0;
        off_t readlen=512;
        //unsigned char out[MD5_DIGEST_LENGTH];
        memset(&c , 0 , sizeof(c));

        MD5_Init(&c);
        bytes=readString(&in , buf, readlen);
        length -= bytes;
        //bytes=read(STDIN_FILENO, buf, 512);
        while(bytes > 0 && readlen > 0)
        {
                MD5_Update(&c, buf, bytes);
                if(length<readlen){
                		readlen = length;
				}
				bytes=readString(&in , buf, readlen);
				length -= bytes;
                //bytes=read(STDIN_FILENO, buf, 512);
        }

        MD5_Final(out, &c);

        //printf("%d\n",MD5_DIGEST_LENGTH);

        return out;

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
int readString(char **src , char * dst , int length){
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
off_t getItem( Index *indexTable , FileIndex *fileIndex , char *filename , unsigned char *md5out , unsigned char* buffer , int byName){
		//TODO maybe use filename to get md5out
		off_t index=-1,bytes = 0 ;
		if(byName == 1){
				index = getIndexbyName(filename , fileIndex , false);//TODO should get ft.index , not findex
		}else{
				index = getIndex(md5out , indexTable);
		}
		if(index == -1 ) return -1;
		bytes = getObject(buffer , indexTable[index].size , DB.PATH , indexTable[index].fid 
							,(off_t)bytesToOffset(indexTable[index].offset,INDEX_OFFSET_LENGTH ));
		if(bytes != indexTable[index].size){//TODO error handle
		}
		return index;


}
off_t putItem(Index * indexTable , FileIndex *fileIndex,char *filename  ,unsigned char *buffer , off_t size , int byName ){
		unsigned char md5out[MD5_DIGEST_LENGTH];
		off_t index = -1 , f_index = -1 , startOffset , bytes;
		md5(buffer , md5out , size);
		index = getIndex(md5out , indexTable);
		f_index = getIndexbyName(filename , fileIndex , true); //TODO should get findex ,not fT.index
		if(fileIndex[f_index].indexFlag & INDEX_EXIST && !(fileIndex[f_index].indexFlag & INDEX_DELETE)){
				return -1;
		}
		if(index != -1){//it found in table
				//TODO save fileIndex:save filename and point to indexTable;
				memcpy(buffer,md5out,MD5_DIGEST_LENGTH);
				indexTable[index].indexFlag = indexTable[index].indexFlag & 0xfe; 
				indexTable[index].ref++;
				saveIndex(indexTable);

				memcpy(fileIndex[f_index].filename,filename , strlen(filename) );
				memcpy ( fileIndex[f_index].MD5  , md5out , MD5_DIGEST_LENGTH);
				fileIndex[f_index].index = index;
				fileIndex[f_index].indexFlag = INDEX_EXIST;
				saveFileIndex(fileIndex);
				return index;
		}
		startOffset = DB.offset;
		bytes = putObject( buffer , size , DB.PATH , &DB.curFid , &DB.offset);
		index = updateIndex(indexTable , fileIndex , filename , md5out , DB.curFid , startOffset , size);
		if(bytes != indexTable[index].size){//TODO error handle
		}
		memcpy(buffer,md5out,MD5_DIGEST_LENGTH);
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
