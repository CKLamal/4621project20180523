#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <time.h>
#include <assert.h>
#include "gzip.c"

#define MAXLINE 1024
#define SERVER_PORT 2333
#define LISTENNQ 5
#define MAXTHREAD 20
#define CHUNKSIZE 128


int threads_count = 0;
void* request_func(void* args);
char* check_file(const char* string);
int main(int argc, char* argv[])
{
	int listenfd, connfd;
	pthread_t threads[MAXTHREAD];
	struct sockaddr_in serv_addr, cli_addr;
	socklen_t len = sizeof(struct sockaddr_in);
	char ip_str[INET_ADDRSTRLEN], wrt_buff[MAXLINE], rcv_buff[MAXLINE];


	/*server socket*/
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	if (listenfd<0)
	{
		printf("Error: init socket\n");
		return 0;
	}
	printf("Init socket\n");

	/* initialize server address (IP:port)*/
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr =INADDR_ANY;//ip address
	serv_addr.sin_port = htons(SERVER_PORT);//port number

	/*bind socket to server address*/
	if(bind(listenfd,(struct sockaddr*)&serv_addr, sizeof(struct sockaddr))<0)
	{
		printf("Error: bind\n");
		return 0;
	}
	printf("Bind\n");

	if(listen(listenfd, LISTENNQ)<0)
	{
		printf("Error: listen\n");
		return 0;
	}
	printf("Listen\n");

	/*keep processing incoming requests*/
	while(1)
	{
		/*accept an incoming connection from the remote side*/
		connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &len);
		if(connfd<0)
		{
			printf("Error: accept\n");
			return 0;
		}
		printf("Accept\n");
		memset(&ip_str,0, sizeof(ip_str));
		// memset(&wrt_buff, 0, sizeof(wrt_buff));
		// memset(&rcv_buff, 0, sizeof(rcv_buff));

		/*print client (remote) address IP:Port*/
		inet_ntop(AF_INET, &(cli_addr.sin_addr), ip_str, INET_ADDRSTRLEN);
		printf("Incoming connection from %s : %hu\n", ip_str, ntohs(cli_addr.sin_port));

		if (pthread_create(&threads[threads_count], NULL, request_func, (void *)connfd) != 0) {
			printf("Error when creating thread %d\n", threads_count);
			return 0;
		}
		printf("Creat thread %d\n", threads_count);
		if (++threads_count >= MAXTHREAD) {
			break;
		}

	}
	printf("Thread is in maximum number.\n");

	int i;
	for (i = 0; i < MAXTHREAD; i++)
	{
		pthread_join(threads[i],NULL);
	}
	return 0;

}
char* check_file(const char* string)
{
	//printf("check file start\n");
	assert(string != NULL);
	char* ext = strrchr(string, '.');

	if(ext == NULL)
	{
		//printf("ext = null\n");
		return (char*) string + strlen(string);
	}
	//printf("ext != null\n");
	for(char* iter = ext + 1; *iter != '\0'; iter++)
	{
		if (!isalnum((unsigned char)*iter))
				return (char*) string + strlen(string);
	}
	//printf("for loop ends\n");
	return ext;
}
void* request_func(void* args)
{
		int connfd = (int)args;
		int bytes_rcv, bytes_wrt, total_bytes_wrt, size, count;
		char wrt_buff[MAXLINE], rcv_buff[MAXLINE];
		char chunk_buff[CHUNKSIZE];

		FILE* file = NULL;
		char file_name[128];
		char* file_type,* ext;
		char method[16];

		int index = 0;

		int CHUNK = 1;
		int GZIP = 0;

		memset(&wrt_buff, 0, sizeof(wrt_buff));
		memset(&rcv_buff, 0, sizeof(rcv_buff));

		//printf("Complete initialize\n");
		/* read response*/
		bytes_rcv = 0;
		while(1)
		{
			bytes_rcv += recv(connfd, rcv_buff + bytes_rcv, sizeof(rcv_buff) - bytes_rcv -1, 0);
			if(bytes_rcv && rcv_buff[bytes_rcv - 1] == '\n')break;
		}

		sscanf(rcv_buff, "%s %s\n", method, file_name);

		if(strcmp(file_name,"/")==0)//default
		{
			strcpy(file_name,"/index.html");
		}
		//printf("Not empty\n");
		strcpy(file_name,file_name+1);
		ext = check_file(file_name);
		//printf("file exist");
		printf("%s file request\n",ext);
		if(strcmp(ext,".html")==0)
		{
			file_type = "text/html";
		}
		else if (strcmp(ext,".jpg")==0)
		{
			file_type = "image/jpg";
		}
		else if(strcmp(ext,".pdf")==0)
		{
			file_type = "application/pdf";
		}
		else if(strcmp(ext,".pptx")==0)
		{
			file_type = "application/vnd.openxmlformats-officedocument.presentationml.presentation";
		}
		else if(strcmp(ext,".css")==0)
		{
			file_type = "text/css";
		}
		else if (strcmp(ext,".gz")==0)
		{
			GZIP = 1;
			file_type = "";
		}
		else
		{
			file_type = "";
			printf("Type error\n");
		}
		printf("Reading file %s\n",file_name);
		if(GZIP)
		{zipg(file_name);
		file = fopen("test.gz","r");}
		else
		file = fopen(file_name,"r");
		if(!file)
		{
			printf("404\n");
			snprintf(wrt_buff, sizeof(wrt_buff) - 1,
			"HTTP/1.1 404 Not Found\r\n\
			Server: COMP4621_Project\r\n\
			Content-Type: text/html\r\n\r\n\
			<!DOCTYPE html>\
			<html>\
			<head>\
			<title>404 Not Found</title>\
			</head>\
			<body><div>\
			<h1 style='font-weight: bold'>404 Not Found</h1>\
			<img src=\"/error.jpg\">\
			</div></body>\
			</html>");
			write(connfd, wrt_buff, strlen(wrt_buff));
		}
		else
		{
			printf("200\n");
			fseek(file, 0L, SEEK_END);
			size = ftell(file);
			rewind(file);

			snprintf(wrt_buff, sizeof(wrt_buff)-1,"HTTP/1.1 200 OK\r\n");
			write(connfd,wrt_buff, strlen(wrt_buff));

			snprintf(wrt_buff, sizeof(wrt_buff)-1, "Server: cklamal\r\n");
			write(connfd,wrt_buff, strlen(wrt_buff));

			if(CHUNK)
			{
				snprintf(wrt_buff, sizeof(wrt_buff)-1, "Transfer-Encoding: chunked\r\n");
				write(connfd,wrt_buff, strlen(wrt_buff));
			}
			else
			{
				snprintf(wrt_buff, sizeof(wrt_buff)-1, "Content-Length: %d\r\n", size);
				write(connfd,wrt_buff, strlen(wrt_buff));
			}
			if(file_type != "")
			{
				snprintf(wrt_buff, sizeof(wrt_buff)-1, "Content-Type: %s\r\n",file_type);
				write(connfd,wrt_buff, strlen(wrt_buff));
 			}
			if(GZIP)
			{
				snprintf(wrt_buff, sizeof(wrt_buff)-1, "Content-Encoding: gzip\r\n");
				write(connfd,wrt_buff, strlen(wrt_buff));
			}
			snprintf(wrt_buff, sizeof(wrt_buff)-1, "Keep-Alive: timeout=5, max=100\r\nConnection: Keep-Alive\n\n");
			write(connfd,wrt_buff,strlen(wrt_buff));

			if(CHUNK&&!GZIP)
			{

				count = fread(chunk_buff, 1, CHUNKSIZE, file);
				while(count == CHUNKSIZE)
				{
					snprintf(wrt_buff, sizeof(wrt_buff) - 1, "%x\r\n", count);
					write(connfd, wrt_buff, strlen(wrt_buff));
					write(connfd, chunk_buff, count);
					write(connfd, "\r\n\r\n", 2);
					count = fread(chunk_buff, 1, CHUNKSIZE, file);
				}
				if(count != 0)//last
				{
					snprintf(wrt_buff, sizeof(wrt_buff) - 1, "%x\r\n", count);
					write(connfd, wrt_buff, strlen(wrt_buff));
					write(connfd, chunk_buff, count);
					write(connfd, "\r\n\r\n", 2);
				}
				write(connfd, "0\r\n\r\n", 5);
			}
			else
			{

				index = 0;
				while((wrt_buff[index] = fgetc(file)) != EOF)
				{
					index++;
				}
				bytes_wrt = 0;
				total_bytes_wrt = strlen(wrt_buff);
				while (bytes_wrt < total_bytes_wrt)
				{
					bytes_wrt += write(connfd, wrt_buff + bytes_wrt, total_bytes_wrt - bytes_wrt);
				}
			}
			fclose(file);
		}
		close(connfd);
		threads_count--;
}
