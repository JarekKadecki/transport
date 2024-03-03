// Jarosław Kadecki 332771
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <sys/select.h>
#include <unistd.h>
#include <errno.h>

#define RECEIVE_DELAY_SECONDS 1
#define RECEIVE_DELAY_MICROSECONDS 600000
#define DATA_REQUEST_SIZE 1000
#define PRINTINFO if(0)


static char* server_ip;
static uint16_t port;
static char* file_name;
static uint32_t file_size;
static uint32_t bytes_recieved = 0;
static uint32_t last_sent_size = 0;
static FILE *file = NULL;

int isDigit(char a)
{
    if(a >= 48 && a <= 57)
    return 1;
    return 0;
}

int compare_ip(char *ip1, char *ip2)
{
    if(strlen(ip1) != strlen(ip2)) return 0;
    for(int i=0; i < strlen(ip1); i++)
    {
        if(isDigit(ip1[i]) || ip1[i] == '.')
        {
            if(ip1[i] != ip2[i])
            {
                return 0;
            }
        }
        else
        {
            break;
        }
    }

    return 1;
}

int send_request(int sockfd, int offset, int size)
{
	if (sockfd < 0) {
		fprintf(stderr, "socket error: %s\n", strerror(errno)); 
		return EXIT_FAILURE;
	}

	struct sockaddr_in server_address;
	bzero (&server_address, sizeof(server_address));
	server_address.sin_family      = AF_INET;
	server_address.sin_port        = htons(port);
	inet_pton(AF_INET, server_ip, &server_address.sin_addr);

	char message[20];
    sprintf(message, "GET %d %d\n", offset, size);
	ssize_t message_len = strlen(message);
    PRINTINFO printf("Sending message %s Len: %d\n", message, message_len);
    	for(int i=0; i<4; i++)
	if (sendto(sockfd, message, message_len, 0, (struct sockaddr*) &server_address, sizeof(server_address)) != message_len) {
		fprintf(stderr, "sendto error: %s\n", strerror(errno)); 
		return EXIT_FAILURE;		
	}
    last_sent_size = size;

	return EXIT_SUCCESS;
}

int write_to_file(char* buff, int size)
{
    if(fwrite(buff, sizeof(char), size, file) == size)
    {
        bytes_recieved += size;
        return 0;
    }
    printf("File write error\n");
    return 1;
}

int recieve_response(int  sockfd)
{
    struct sockaddr_in 	sender;	
	socklen_t 			sender_len = sizeof(sender);
	u_int8_t 			buffer[IP_MAXPACKET+1];

    fd_set descriptor_set;
    struct timeval tv;
    tv.tv_sec = RECEIVE_DELAY_SECONDS;
    tv.tv_usec = RECEIVE_DELAY_MICROSECONDS;

    FD_ZERO(&descriptor_set);
    FD_SET(sockfd, &descriptor_set);
    int ready = select(sockfd+1, &descriptor_set, NULL, NULL, &tv);
    
    if(ready > 0)
    {
        ssize_t datagram_len = recvfrom (sockfd, buffer, IP_MAXPACKET, 0, (struct sockaddr*)&sender, &sender_len);
        if (datagram_len < 0) {
            fprintf(stderr, "recvfrom error: %s\n", strerror(errno)); 
        }


        char sender_ip_str[20];
        
        inet_ntop(AF_INET, &(sender.sin_addr), sender_ip_str, sizeof(sender_ip_str));
        PRINTINFO printf("Recieved from %s port = %d\n", sender_ip_str, ntohs(sender.sin_port));

        if(compare_ip(sender_ip_str, server_ip) && ntohs(sender.sin_port) == port)
        {
            int recieved_offset;
            int recieved_size;

            sscanf(buffer, "%*[^0123456789]%d%*[^0123456789]%d", &recieved_offset, &recieved_size);
            PRINTINFO printf("Recieved offset %d, recieved size %d\n", recieved_offset, recieved_size);
            if(recieved_offset == bytes_recieved && recieved_size == last_sent_size)
            {
                //Dopisywanie do pliku
                PRINTINFO printf("Dopisywanie\n\n");
                write_to_file(buffer + (datagram_len - (ssize_t)recieved_size), recieved_size);
                return 0;
            }
            else
            {
                return 5;
            }
        }
        else
        {
            return 4;
        }
    }
    else if(ready == 0)
    {
        return 1;
    }
    else if(ready < 0)
    {
        printf("Select error\n");
        return 2;
    }
    else
    {
        return 3;
    }
	
    PRINTINFO printf("No entry here\n");
    return 3;
}


int read_input(int argc, char **argv)
{
    if(argc < 5)
    {
        printf("To few arguments\n");
        return 1;
    }
    if(argc > 5)
    {
        printf("To many arguments\n");
        return 2;
    }
    
    server_ip = malloc(sizeof(char)*strlen(argv[1]));
    strcpy(server_ip, argv[1]);
    struct sockaddr_in addr;
    if(inet_pton(AF_INET, server_ip, &(addr.sin_addr)) == 0)
    {
        printf("Invalid server address\n");
        return 3;
    }
    
    port = (uint32_t)strtol(argv[2], NULL, 10);

    if(port < 1024 || port > 49151)
    {
        printf("Invalid port\n");
        return 4;
    }

    file_name = malloc(sizeof(char)*strlen(argv[3]));
    strcpy(file_name, argv[3]);
    file_size = (uint32_t)strtol(argv[4], NULL, 10);

    PRINTINFO printf("Ip: %s Port %d File %s Size %d\n", server_ip, port, file_name, file_size);

    return 0;

}

int setup_socket(int *socket)
{
    if(*socket < 0)
    {
        printf("Could not open socket\n");
        return 1;
    }
    struct sockaddr_in server_address;
	bzero (&server_address, sizeof(server_address));
	server_address.sin_family      = AF_INET;
	server_address.sin_port        = htons(port);
	server_address.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind (*socket, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
		fprintf(stderr, "bind error: %s\n", strerror(errno)); 
	}

    return 0;
}

int setup_file()
{
    file = fopen(file_name, "wb");
    if(file == NULL)
    {
        printf("Could not create file\n");
        return 1;
    }
    return 0;
}
int main(int argc, char **argv)
{
    read_input(argc, argv);
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(setup_socket(&sockfd) == 1)
    {
        exit(0);
    }   
    if(setup_file() == 1)
    {
        exit(0);
    }

    while (bytes_recieved < file_size)
    {
        do
        {
            int size = file_size - bytes_recieved > DATA_REQUEST_SIZE ? DATA_REQUEST_SIZE : file_size-bytes_recieved;
            send_request(sockfd, bytes_recieved, size);

        } while (recieve_response(sockfd));
        
        printf("Got %d%\n", bytes_recieved*100/file_size);
           
    }
    
    fclose(file);
    
    return 0;
}
