#define _GNU_SOURCE // For strrchr

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <pthread.h>

typedef struct {
	int fd;
} ClientBeginData;

#define BUF_SIZE 2048

const char* getMIMEType(const char* url) {
	char* ext = strrchr(url, '.');

	if (strcmp(ext, ".html") == 0) return "text/html; charset=utf-8"; // Hardcode utf8 ok?
	else if (strcmp(ext, ".png") == 0) return "image/png";
	else return "application/octet-stream";
}

void serveFile(int fd, const char* url) {
	printf("Path is %s\n", url);

	char* ext = strrchr(url, '.');
	printf("Ext is %s\n", ext);

	// TODO Check path doesn't have ../

	FILE* f = fopen(url, "r");
	if (f == NULL) {
		// 404 error
		perror("fopen");
		return;
	}

	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);

	char* data = malloc(size);
	fread(data, size, 1, f);

	char responseBuf[BUF_SIZE];
	snprintf(responseBuf, BUF_SIZE,
	"HTTP/1.1 200 OK\r\n"
	"Content-Type: %s\r\n"
	"Content-Length: %ld\r\n"
	"\r\n", getMIMEType(url), size);

	// Missing null terminator

	write(fd, responseBuf, strlen(responseBuf));
	write(fd, data, size);
}

void* clientHandler(void* arg) {
	int fd = ((ClientBeginData*)arg)->fd;
	free(arg);

	char buf[BUF_SIZE] = {0};

	int n = read(fd, buf, BUFSIZ);
	if (n == 0) {
		// EOF
	}
	else if (n == BUF_SIZE) {
		// Too big
	}

	// First line of request looks like
	// GET [URL] [HTTP version]

	// Check that this is a GET request
	char* beg = buf;
	char* end = memchr(beg, ' ', n);

	if (memcmp(beg, "GET", strlen("GET") != 0)) {
		printf("400 bad request\n"); // TODO
		return NULL;
	}

	// Extract the URL
	beg = end + 1;
	n -= end - beg;
	end = memchr(beg, ' ', n);

	char* url = malloc(end - beg + 1);
	memcpy(url, beg, end - beg);
	url[end - beg] = '\0';

	// Routing

	if (strcmp(url, "/") == 0) {
		serveFile(fd, "index.html");
	}
	else {
		serveFile(fd, url + 1);
	}
	
	free(url);

	close(fd);
}

int main() {
	int s = socket(AF_INET, SOCK_STREAM, 0);
	if (s == -1) {
		perror("socket");
		exit(EXIT_FAILURE);
	}

	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) == -1) {
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(8000);
	addr.sin_addr.s_addr = INADDR_ANY;
	if (bind(s, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) == -1) {
		perror("bind");
		exit(EXIT_FAILURE);
	}

	if (listen(s, 10) == -1) {
		perror("listen");
		exit(EXIT_FAILURE);
	}

	while (true) {
		struct sockaddr_in client_addr;
		socklen_t addrlen;
		int a = accept(s, (struct sockaddr*)&client_addr, &addrlen);
		if (a == -1) {
			perror("accept");
			exit(EXIT_FAILURE);
		}

		char* s = inet_ntoa(client_addr.sin_addr);
		printf("Inbound connection from %s\n", s);

		ClientBeginData* arg = malloc(sizeof(ClientBeginData));
		arg->fd = a;

		pthread_t clientThread;
		if (pthread_create(&clientThread, NULL, clientHandler, arg) != 0) {
			perror("pthread_create");
			exit(EXIT_FAILURE);
		}
		pthread_detach(clientThread);
	}

	return 0;
}
