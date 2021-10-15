/**
 * client.c
 *
 * @author Justin Cavalli
 *
 * USD COMP 375: Computer Networks
 * Project 1 - Reverse Engineering a Network Application
 * 
 */

#define _XOPEN_SOURCE 600

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>

#define BUFF_SIZE 1024
#define PORT_SIZE 4 

int len, sentBytes, recvBytes, sensor_fd;
int currentHum, currentTemp, currentWind;
time_t currentTime;
char sensorPort[PORT_SIZE];
char buff[BUFF_SIZE];

void mainLoop();
long prompt();
int connectToHost(char *hostname, char *port);
int sensorConnect(int server_fd);
void tempCheck(int sensor_fd);
void humidityCheck(int sensor_fd);
void windCheck(int sensor_fd);

int main() {
	printf("WELCOME TO THE COMP375 SENSOR NETWORK \n\n\n");
	mainLoop();
	return 0;
}

/**
 * Loop to keep asking user what they want to do and calling the appropriate
 * function to handle the selection.
 */
void mainLoop() {

	while (1) {
		//socket disconnects after each check, so must make new connection
		//starting port is always 7030 for university network
		int server_fd = connectToHost("hopper.sandiego.edu", "7030"); //Connect to university network first
		int sensor_fd = sensorConnect(server_fd); //Determine sensor network port

		//User decides which data to receive back
		long selection = prompt();
		switch (selection) {
			case 1:
				//Temperature check
				tempCheck(sensor_fd);
				break;
			
			case 2:
				//Humidity check
				humidityCheck(sensor_fd);
				break;

			case 3:
				//Wind speed check
				windCheck(sensor_fd);
				break;

			case 4:
				//Exit program
				printf("GOODBYE!\n");
				close(server_fd);
				exit(0);
				break;

			default:
				fprintf(stderr, "ERROR: Invalid selection\n");
				break;
		}
	}
}

/** 
 * Print command prompt to user and obtain user input.
 *
 * @return The user's desired selection, or -1 if invalid selection.
 */
long prompt() {

	// User menu options
	printf("Which sensor would you like to read: \n\n");
	printf("\t(1) Air temperature \n");
	printf("\t(2) Relative humidity \n");
	printf("\t(3) Wind speed \n");
	printf("\t(4) Quit Program \n\n");
	printf("Selection: ");

	// Read in a value from standard input
	char input[10];
	memset(input, 0, 10); // set all characters in input to '\0' (i.e. nul)
	char *read_str = fgets(input, 10, stdin);

	// Check if EOF or an error, exiting the program in both cases.
	if (read_str == NULL) {
		if (feof(stdin)) {
			exit(0);
		}
		else if (ferror(stdin)) {
			perror("fgets");
			exit(1);
		}
	}

	// get rid of newline, if there is one
	char *new_line = strchr(input, '\n');
	if (new_line != NULL) new_line[0] = '\0';

	// convert string to a long int
	char *end;
	long selection = strtol(input, &end, 10);

	if (end == input || *end != '\0') {
		selection = -1;
	}

	return selection;
}

/**
 * Socket implementation of connecting to a host at a specific port.
 *
 * @param hostname The name of the host to connect to (e.g. "foo.sandiego.edu")
 * @param port The port number to connect to
 * @return File descriptor of new socket to use.
 */
int connectToHost(char *hostname, char *port) {
	// Step 1: fill in the address info in preparation for setting 
	//   up the socket
	int status;
	struct addrinfo hints;
	struct addrinfo *servinfo;  // will point to the results

	memset(&hints, 0, sizeof hints); // make sure the struct is empty
	hints.ai_family = AF_INET;       // Use IPv4
	hints.ai_socktype = SOCK_STREAM; // TCP stream sockets

	// get ready to connect
	if ((status = getaddrinfo(hostname, port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
		exit(1);
	}

	// Step 2: Make a call to socket
	int fd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
	if (fd == -1) {
		perror("socket");
		exit(1);
	}

	// Step 3: connect!
	if (connect(fd, servinfo->ai_addr, servinfo->ai_addrlen) != 0) {
		perror("connect");
		exit(1);
	}

	freeaddrinfo(servinfo); // free's the memory allocated by getaddrinfo

	return fd;
}

/** 
 * Connects to a sensor network hosted on a university server. Each connection will
 * randomly assign a new port, so the function will parse and check
 * the given port.
 *
 * @param server_fd Socket file descriptor for communicating with the server
 * @return The assigned network port, or 1 if connection error.
 */

int sensorConnect(int server_fd) {

	//Connecting to university/sensor networks with password authentication
	memset(buff, 0, BUFF_SIZE);
	strcpy(buff, "AUTH password123\n"); //example network used 'password123', can change for different servers
	len = strlen(buff);
	sentBytes = send(server_fd, buff, len, 0);

	//Must check each send/recv for errors, especially for -1
	if (sentBytes == -1)
	{
		printf("AUTH send() error\n");
		return 1;
	}

	memset(buff, 0, BUFF_SIZE); //Each memset empties the shared buff
	recvBytes = recv(server_fd, buff, BUFF_SIZE, 0);

	if (recvBytes == -1)
	{
		printf("recv() error\n");
		return 1;
	}
	else if (recvBytes == 0)
	{
		printf("Connection closed\n"); //premature connection closure
		return 1; 
	}

	sscanf(buff, "%*s %*s %s %*s", sensorPort); //parsing for next port (server randomly chooses different ports for sensor data)
	int sensor_fd = connectToHost("weatherstation.sandiego.edu", sensorPort); //sensor network hostname, change for different server

	memset(buff, 0, BUFF_SIZE);
	strcpy(buff, "AUTH sensorpass321\n"); //reauthentication for sensor network
	len = strlen(buff);
	sentBytes = send(sensor_fd, buff, len, 0);

	if (sentBytes == -1)
	{
		printf("AUTH send() error\n");
		return 1;
	}

	memset(buff, 0, BUFF_SIZE);
	recvBytes = recv(sensor_fd, buff, BUFF_SIZE, 0);
	if (recvBytes == -1)
	{
		printf("recv() error\n");
		return 1;
	}
	else if (recvBytes == 0)
	{
		printf("Connection closed\n");
		return 1;
	}

	//Check server response, should be "SUCCESS"
	const char success[10] = "SUCCESS\n";
	char *responseCheck = strstr(buff, success);

	if (responseCheck == NULL)
	{
		printf("AUTH failed, bad response\n"); //Server should send "SUCCESS" back
		return 1;
	}

	memset(buff, 0, BUFF_SIZE);
	return sensor_fd;
}

/** 
 * Checks and prints the current temperature, as well as converting
 * Unix epoch time. 
 *
 * @param sensor_fd Socket file descriptor for communicating with the server
 */

void tempCheck(int sensor_fd) {

	//Sending command and receiving temp data
	//Send/recv returns # of bits sent/returned, must check for error each time
	memset(buff, 0, BUFF_SIZE);
	strcpy(buff, "AIR TEMPERATURE\n");
	len = strlen(buff);
	sentBytes = send(sensor_fd, buff, len, 0);

	if (sentBytes == -1)
	{
		printf("AUTH send() error 1\n");
		return;
	}

	memset(buff, 0, BUFF_SIZE);
	recvBytes = recv(sensor_fd, buff, BUFF_SIZE, 0);
	if (recvBytes == -1)
	{
		printf("recv() error\n");
		return;
	}
	else if (recvBytes == 0)
	{
		printf("Connection closed\n");
		return;
	}

	//Parsing buff for epoch time and current air temperature
	//Print data to stdout, converting epoch to human-readable time
	printf("\n");
	sscanf(buff, "%ld %d %*s", &currentTime, &currentTemp);
	printf("The last AIR TEMPERATURE reading was %d F, taken at %s \n", currentTemp, ctime(&currentTime));

	//Send 'CLOSE' command to server network, stopping connection
	memset(buff, 0, BUFF_SIZE);
	strcpy(buff, "CLOSE\n");
	len = strlen(buff);
	sentBytes = send(sensor_fd, buff, len, 0);

	if (sentBytes == -1)
	{
		printf("CLOSE send() error\n");
		return;
	}

	memset(buff, 0, BUFF_SIZE);
	recvBytes = recv(sensor_fd, buff, BUFF_SIZE, 0);
	if (recvBytes == -1)
	{
		printf("recv() error\n");
		return;
	}

	//Making sure sensor network replies back
	const char bye[5] = "BYE\n";
	char *responseCheck = strstr(buff, bye);

	if (responseCheck == NULL)
	{
		printf("AUTH failed, bad response\n"); //Server should send "BYE" back
		return;
	}

	memset(buff, 0, BUFF_SIZE); //Empty out buffer
}

/** 
 * Checks and prints the current humidity, as well as converting
 * Unix epoch time. 
 *
 * @param sensor_fd Socket file descriptor for communicating with the server
 */

void humidityCheck(int sensor_fd) {

	//Sending command and receiving humidity data
	//Send/recv returns # of bits sent/returned, must check for error each time
	memset(buff, 0, BUFF_SIZE);
	strcpy(buff, "RELATIVE HUMIDITY\n");
	len = strlen(buff);
	sentBytes = send(sensor_fd, buff, len, 0);

	if (sentBytes == -1)
	{
		printf("AUTH send() error 2\n");
		return;
	}

	memset(buff, 0, BUFF_SIZE);
	recvBytes = recv(sensor_fd, buff, BUFF_SIZE, 0);
	if (recvBytes == -1)
	{
		printf("recv() error\n");
		return;
	}
	else if (recvBytes == 0)
	{
		printf("Connection closed\n");
		return;
	}

	//Parsing buff for epoch time and current relative humidity
	//Print data to stdout, converting epoch to human-readable time
	printf("\n");
	sscanf(buff, "%ld %d %%", &currentTime, &currentHum);
	printf("The last RELATIVE HUMIDITY reading was %d %%, taken at %s \n", currentHum, ctime(&currentTime));

	//Send 'CLOSE' command to server network, stopping connection
	memset(buff, 0, BUFF_SIZE);
	strcpy(buff, "CLOSE\n");
	len = strlen(buff);
	sentBytes = send(sensor_fd, buff, len, 0);

	if (sentBytes == -1)
	{
		printf("CLOSE send() error\n");
		return;
	}

	memset(buff, 0, BUFF_SIZE);
	recvBytes = recv(sensor_fd, buff, BUFF_SIZE, 0);
	if (recvBytes == -1)
	{
		printf("recv() error\n");
		return;
	}

	//Making sure sensor network replies back
	const char bye[5] = "BYE\n";
	char *responseCheck = strstr(buff, bye);

	if (responseCheck == NULL)
	{
		printf("AUTH failed, bad response\n"); //Server should send "BYE" back
		return;
	}

	memset(buff, 0, BUFF_SIZE); //Empty out buffer
}

/** 
 * Checks and prints the current wind speed, as well as converting
 * Unix epoch time. 
 *
 * @param sensor_fd Socket file descriptor for communicating with the server
 */

void windCheck(int sensor_fd) {

	//Sending command and receiving wind data
	//Send/recv returns # of bits sent/returned, must check for error each time
	memset(buff, 0, BUFF_SIZE);
	strcpy(buff, "WIND SPEED\n");
	len = strlen(buff);
	sentBytes = send(sensor_fd, buff, len, 0);

	if (sentBytes == -1)
	{
		printf("AUTH send() error 3\n");
		return;
	}

	memset(buff, 0, BUFF_SIZE);
	recvBytes = recv(sensor_fd, buff, BUFF_SIZE, 0);
	if (recvBytes == -1)
	{
		printf("recv() error\n");
		return;
	}
	else if (recvBytes == 0)
	{
		printf("Connection closed\n"); //if 0, network is most likely down
		return;
	}

	//Parsing buff for epoch time and current wind speed in MPH
	//Print data to stdout, converting epoch to human-readable time
	printf("\n");
	sscanf(buff, "%ld %d %*s", &currentTime, &currentWind);
	printf("The last WIND SPEED reading was %d MPH, taken at %s \n", currentWind, ctime(&currentTime));

	//Send 'CLOSE' command to server network, stopping connection
	memset(buff, 0, BUFF_SIZE);
	strcpy(buff, "CLOSE\n");
	len = strlen(buff);
	sentBytes = send(sensor_fd, buff, len, 0);

	if (sentBytes == -1)
	{
		printf("CLOSE send() error\n");
		return;
	}

	memset(buff, 0, BUFF_SIZE);
	recvBytes = recv(sensor_fd, buff, BUFF_SIZE, 0);
	if (recvBytes == -1)
	{
		printf("recv() error\n");
		return;
	}

	//Making sure sensor network replies back
	const char bye[5] = "BYE\n";
	char *responseCheck = strstr(buff, bye);

	if (responseCheck == NULL)
	{
		printf("AUTH failed, bad response\n"); //Server should send "BYE" back
		return;
	}

	memset(buff, 0, BUFF_SIZE); //Empty out buffer
}