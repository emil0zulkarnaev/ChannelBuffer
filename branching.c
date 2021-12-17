#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#define MAX_FIFO_COUNT			50
#define MAX_FIFO_NAME_LENGTH	20
#define MAX_MESSAGE_LENGTH		200

extern int errno;

char FIFO_LIST[MAX_FIFO_COUNT][MAX_FIFO_NAME_LENGTH];
uint8_t FIFO_COUNT = 0;

char PWD[PATH_MAX];
char clear[MAX_FIFO_COUNT] = {0};

void read_fifo_list() {
	FILE *file = fopen("fifo_list.txt", "r");


	FIFO_COUNT = 0;
	while (fgets(FIFO_LIST[FIFO_COUNT], MAX_FIFO_NAME_LENGTH, file) != NULL 
			&& FIFO_COUNT < MAX_FIFO_COUNT) {
		for (uint8_t i=0; i<MAX_FIFO_NAME_LENGTH; i++)
			if (FIFO_LIST[FIFO_COUNT][i] == '\n') {
				FIFO_LIST[FIFO_COUNT][i] = '\0';
				break;
			}
		FIFO_COUNT++;
	}

	fclose(file);
}

uint8_t init() {
	if (getcwd(PWD, sizeof PWD) == NULL)
		return 1;

	printf("%s\n", PWD);
	char command[200];
	sprintf(command, "mkdir -p %s/nodes", PWD); system(command);
	sprintf(command, "mkdir -p %s/nodes_buffer", PWD); system(command);
	sprintf(command, "mkdir -p %s/synchronize", PWD); system(command);
	sprintf(command, "rm -f %s/nodes/*", PWD); system(command);
	sprintf(command, "rm -f %s/nodes_buffer/*", PWD); system(command);
	//sprintf(command, "rm -f %s/synchronize/*", PWD); system(command);

	for (uint8_t i=0; i<FIFO_COUNT; i++) {
		sprintf(command, "mkfifo %s/nodes/%s", PWD, FIFO_LIST[i]); system(command);
		sprintf(command, "mkfifo %s/nodes/_%s", PWD, FIFO_LIST[i]); system(command);
		sprintf(command, "touch %s/nodes_buffer/%s.txt", PWD, FIFO_LIST[i]); system(command);
	}

	return 0;
}

pid_t PIDS[MAX_FIFO_COUNT];

void one_row_delete(char * file_name) {
	char content[MAX_MESSAGE_LENGTH][1000];
	int  rows_length[1000];
	
	int counter = 0, row_counter = 0;
	while (1) {
		int fd = open(file_name, O_RDONLY);
		if (fd == -1) continue;
		char new_chr;
		ssize_t ret;
		do {
			ret = read(fd, &new_chr, sizeof new_chr);
			if (ret <= 0) break;

			if (new_chr == '\n')
				if (counter == 0) continue;
				else {
					content[row_counter][counter] = new_chr;
					rows_length[row_counter] = counter+1;
					row_counter ++;
					counter = 0;
					continue;
				}
			content[row_counter][counter] = new_chr;
			counter ++;

		} while(ret > 0 && row_counter < MAX_MESSAGE_LENGTH);
		break;
	}
	
	/*
	for (int i=0; i<row_counter; i++) {
		printf("%s", content[i], sizeof content[i]);
	}
	*/

	while (1) {
		int fd = open(file_name, O_WRONLY | O_TRUNC);
		if (fd == -1) continue;

		int ln = 0;
		for(int i=1; i<row_counter; i++) {
			ln ++;
			write(fd, content[i], rows_length[i]);
		}
		close(fd);

		break;
	}
	/*
	FILE *file = fopen(file_name, "r");
	int row_number = 0;
	while(fgets(content[row_number], sizeof content[row_number], file) != NULL)
		row_number++;
	fclose(file);

	for (;;) {
		int fd = open(file_name, O_WRONLY);
		if (fd == -1) continue;
		
		break;
	}
	*/
}

void resend_from(uint8_t i) {
	char node_pwd[PATH_MAX], node_buff_pwd[PATH_MAX], message[MAX_MESSAGE_LENGTH],
		 buffer_dec[PATH_MAX], command[PATH_MAX], node_synchronize[PATH_MAX];
	sprintf(node_pwd,	   "%s/nodes/_%s",			 PWD, FIFO_LIST[i]);
	sprintf(node_buff_pwd, "%s/nodes_buffer/%s.txt", PWD, FIFO_LIST[i]);
	//sprintf(buffer_dec, "echo \"$(tail -n +2 %s)\" > %s", node_buff_pwd, node_buff_pwd);
	sprintf(node_synchronize, "%s/synchronize/%d.txt", PWD, i);
	sprintf(command,	   "rm %s/synchronize/%d.txt", PWD, i);

	//printf("buffer name: %s\n", node_buff_pwd);

	for (;;) {
		/*
		int fd_to_clear = open(node_synchronize, O_WRONLY | O_NONBLOCK);
		if (fd_to_clear != -1) {
			close(fd_to_clear);
			system(command);
			one_row_delete(node_buff_pwd);
		}
		*/
		
		int fd = open(node_pwd, O_RDONLY);
		if (fd == -1) {
			printf("FIFO: _%s OUT [fifo doesn't exist]\n", FIFO_LIST[i]);
			break;
		}

		read(fd, message, sizeof message);
		close(fd);

		char next_circle = 0;
		int  message_length = 0;
		for (int j=0; j<MAX_MESSAGE_LENGTH-1; j++) {
			if (message[j] == '\0') {
				if (j == 0) next_circle = 1;
				else if (message[j-1] == '\n' && j-1 == 0)
					next_circle = 1;
				else if (message[j-1] != '\n') {
					message[j] = '\n';
					message[j+1] = '\0';
					message_length ++;
				}

				break;
			}
			message_length ++;
		}

		if (next_circle == 1) continue;

		printf("Message from %s: %s", FIFO_LIST[i], message);

		int fd_out = open(node_buff_pwd, O_WRONLY | O_APPEND);
		if (fd_out == -1) {
			printf("FIFO: _%s OUT [buffer doesn't exist]\n", FIFO_LIST[i]);
			break;
		}
		write(fd_out, message, message_length);
		close(fd_out);

		for (int j=0; j<message_length; j++) {
			message[j] = '\0';
		}
		if (message_length < MAX_MESSAGE_LENGTH) message[message_length+1] = '\0';
	}

	exit(0);
}

void resend_to(uint8_t i) {
	printf("resend_to %d\n", i);
	char node_pwd[PATH_MAX], node_buff_pwd[PATH_MAX], message[MAX_MESSAGE_LENGTH],
		 command[PATH_MAX], node_synchronize[PATH_MAX];
	sprintf(node_pwd,	   "%s/nodes/%s",			 PWD, FIFO_LIST[i]);
	sprintf(node_buff_pwd, "%s/nodes_buffer/%s.txt", PWD, FIFO_LIST[i]);
	sprintf(node_synchronize, "%s/synchronize/%d.txt", PWD, i);
	sprintf(command,	   "touch %s/synchronize/%d.txt", PWD, i);

	for (;;) {
		int fd_in = open(node_buff_pwd, O_RDONLY),
			counter = 0;
		if (fd_in == -1) continue;

		char new_chr = '1';
		ssize_t ret;
		do {
			ret = read(fd_in, &new_chr, sizeof new_chr);
			if (ret <= 0) break;

			if (new_chr == '\n')
				if (counter == 0) continue;
				else {
					message[counter] = new_chr;
					counter ++;
					break;
				}
			message[counter] = new_chr;
			counter ++;

		} while(ret > 0 && new_chr != '\n');

		/*
		while((ret = read(fd_in, &next_sm, sizeof next_sm)) > 0 && next_sm != '\n') {
			if (next_sm == '\0')
				if (counter == 0) continue;
				else break;
			read(fd_in, &next_sm, sizeof next_sm);
			message[counter] = next_sm;
			counter ++;
		}
		*/
		close(fd_in);

		if (counter == 0) continue;
		//counter ++;
		//printf("counter: %d\n", counter);
		one_row_delete(node_buff_pwd);

		int fd = open(node_pwd, O_WRONLY);
		if (fd == -1) {
			printf("FIFO: %s OUT [fifo doesn't exist]\n", FIFO_LIST[i]);
			break;
		}
		write(fd, message, counter);
		close(fd);

		printf("Message to %s: %s", FIFO_LIST[i], message);

		for (int j=0; j<counter; j++) {
			message[j] = '\0';
		}
		if (counter < MAX_MESSAGE_LENGTH) message[counter+1] = '\0';
	}
	printf("out");

	exit(0);
}

void start(uint8_t i) {
	pid_t pid = fork();
	if (pid) {
		resend_from(i);
	}
	resend_to(i);
}

int main()
{
	//one_row_delete("output.txt");
	read_fifo_list();
	init();

	for (uint8_t i=0; i<FIFO_COUNT; i++) {
		PIDS[i] = fork();

		if (PIDS[i]) {
			start(i);
		}
	}
	
	return 0;
}
