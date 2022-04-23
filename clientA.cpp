#include <string>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define localhost "127.0.0.1"
#define TCP_PORT "25256"

#define BUF_SIZE 2048
#define FLAG 0
#define LIS_MAX_SIZE 100

#define CHECK_WALLET 0
#define TX_COINS 1
#define TX_LIST -1
#define STATS -3

using namespace std;

struct Transaction {
    int serial_number;
    string sender;
    string receiver;
    int amount;
};

struct QueryResult {
    int size;
    Transaction transaction_list[LIS_MAX_SIZE];
};

Transaction operation;
QueryResult operation_result;

int sockfd;
char buf[BUF_SIZE];

void start_tcp() {
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(localhost, TCP_PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    }

    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
    }

    freeaddrinfo(servinfo); // all done with this structure
}

// refer to: https://beej.us/guide/bgnet/examples/client.c
void send() {
    if (send(sockfd, &operation, sizeof operation, 0) == -1)
        perror("send");
}

// refer to: https://beej.us/guide/bgnet/examples/client.c
void receive() {
    memset(&buf, 0, sizeof BUF_SIZE);
    if (recv(sockfd, buf, BUF_SIZE, 0) == -1) {
        perror("recv");
        exit(1);
    }
    memset(&operation_result, 0, sizeof operation_result);
    memcpy(&operation_result, buf, sizeof operation_result);
}

void check_wallet(string username) {
    operation.serial_number = CHECK_WALLET;
    operation.sender = username;
    send();
    cout << "\"" << username << "\" sent a balance enquiry request to the main server." << endl;
    receive();
    int balance = operation_result.size;
    cout << "The current balance of \"" << username << "\" is : " << balance << " alicoins." << endl;
}

void tx_coins(string sender, string receiver, int amount) {
    check_wallet(sender);

    operation.serial_number = TX_COINS;
    operation.sender = sender;
    operation.receiver = receiver;
    operation.amount = amount;
    send();
    cout << "\"" << sender << "\" has requested to transfer " << amount << " coins to s\"" << receiver << "\"." << endl;
    receive();
    if (operation_result.size == 0) {
        cout << "\"" << sender << "\" was unable to transfer " << amount << " alicoins to \"" << receiver << "\" because of insufficient balance." << endl;
    } else {
        cout << "\"" << sender << "\" successfully transferred " << amount << " alicoins to \"" << receiver << "\"." << endl;
    }

    check_wallet(sender);
}

void get_and_sort_all_transactions() {
    operation.serial_number = TX_LIST;
    send();
    cout << "\"clientA\" sent a sorted list request to the main server." << endl;
    receive();
}


void stats(string sender) {
    operation.serial_number = STATS;
    send();
    cout << "\"" << sender << "\" sent a statistics enquiry request to the main server." << endl;
    receive();
    cout << "\"" << sender << "\" statistics are the following.:" << endl;
    cout << "Rank--Username--NumofTransacions--Total" << endl;
    for (int i = 0; i < operation_result.size; i++) {
        Transaction t = operation_result.transaction_list[i];
        cout << t.serial_number << "--" << t.sender << "--" << operation_result.size << endl;
    }
}


// refer to https://beej.us/guide/bgnet/examples/client.c
int main(int argc, char *argv[])
{
    start_tcp();
    cout << "The client is up and running." << endl;

    while (true) {
        if (argc == 2 && strcmp(argv[1], "TXLIST") == 0) {
            get_and_sort_all_transactions();
            close(sockfd);
            return 0;
        }

        else if (argc == 2) {
            check_wallet(argv[1]);
            close(sockfd);
            return 0;
        }

        else if (argc == 3 && strcmp(argv[2], "stats") == 0) {
            stats(argv[1]);
            close(sockfd);
            return 0;
        }

        else if (argc == 4) {
            tx_coins(argv[1], argv[2], atoi(argv[3]));
            close(sockfd);
            return 0;
        }
    }
    return 0;
}
