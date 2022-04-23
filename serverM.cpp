#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>
#include <string>
#include <iostream>
#include <vector>
#include <poll.h>
#include <fstream>
#include <pthread.h>

#define localhost "127.0.0.1"

#define UDP_PORT_SERVER_A "21256"
#define UDP_PORT_SERVER_B "22256"
#define UDP_PORT_SERVER_C "23256"
#define SLAVE_SERVER_SIZE 1

#define UDP_PORT_SERVER_M "24256"
#define TCP_PORT_CLIENT_A "25256"
#define TCP_PORT_CLIENT_B "26256"
#define CLIENT_SIZE 2

#define BUF_SIZE 2048
#define FLAG 0

#define CHECK_WALLET 0
#define TX_LIST -1
#define LOGGED -2
#define STATS -3

#define INITIAL_BALANCE 1000
#define TX_LIST_FILE_PATH "./alichain.txt"
#define LIS_MAX_SIZE 100

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

vector<struct Transaction> transaction_list;

char recv_buffer[BUF_SIZE];
Transaction query;
QueryResult query_result;

Transaction operations[CLIENT_SIZE];
QueryResult operation_results[CLIENT_SIZE];

int sockfd_udp_serverM;
int tcp_listener[CLIENT_SIZE];
vector<const char *> tcp_port_list;

vector<struct addrinfo *> slave_server_info_list;
vector<const char *> udp_port_list;

int max_serial_number = 0;

// refer to https://beej.us/guide/bgnet/examples/listener.c
void start_udp_listener(const char *port) {
    struct addrinfo hints, *servinfo, *p;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd_udp_serverM = socket(p->ai_family, p->ai_socktype,
                                         p->ai_protocol)) == -1) {
            continue;
        }

        if (::bind(sockfd_udp_serverM, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd_udp_serverM);
            continue;
        }

        break;
    }

    if (p == NULL) {
        exit(2);
    }

    freeaddrinfo(servinfo);
}

// refer to https://beej.us/guide/bgnet/examples/server.c
int start_tcp_listener(const char *port) {
    tcp_port_list.push_back(port);

    int tcp_listener;
    int yes = 1;
    int rv;

    struct addrinfo hints, *ai, *p;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, port, &hints, &ai)) != 0) {
        exit(1);
    }

    for (p = ai; p != NULL; p = p->ai_next) {
        tcp_listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (tcp_listener < 0) {
            continue;
        }

        setsockopt(tcp_listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (::bind(tcp_listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(tcp_listener);
            continue;
        }

        break;
    }

    freeaddrinfo(ai);

    if ((p == NULL) || (listen(tcp_listener, 10) == -1)) {
        return -1;
    }

    return tcp_listener;
}

// refer to https://beej.us/guide/bgnet/examples/talker.c
void start_udp_talker(const char *port) {
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // set to AF_INET to use IPv4
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(localhost, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    // loop through all the results and make a socket
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("talker: socket");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "talker: failed to create socket\n");
        exit(2);
    }

    slave_server_info_list.push_back(servinfo);
    udp_port_list.push_back(port);
}

void boot_up_serverM() {
    start_udp_listener(UDP_PORT_SERVER_M);

    start_udp_talker(UDP_PORT_SERVER_A);
    start_udp_talker(UDP_PORT_SERVER_B);
    start_udp_talker(UDP_PORT_SERVER_C);

    tcp_listener[0] = start_tcp_listener(TCP_PORT_CLIENT_A);
    tcp_listener[1] = start_tcp_listener(TCP_PORT_CLIENT_B);

    cout << "The main server is up and running." << endl;
}

void talk_to_slave_server(int i) {
    sockaddr *slave_server_addr = slave_server_info_list.at(i)->ai_addr;
    socklen_t slave_server_addrlen = slave_server_info_list.at(i)->ai_addrlen;

    int sendto_result = sendto(sockfd_udp_serverM, &query, sizeof(query), FLAG, slave_server_addr,
                               slave_server_addrlen);
    if (sendto_result == -1) {
        exit(1);
    }

    cout << "The main server sent a request to server " << (char) ('A' + i) << "." << endl;
}

void listen_from_slave_server(int i) {
    memset(recv_buffer, 0, BUF_SIZE);
    struct sockaddr_storage their_addr;
    socklen_t addr_len = sizeof their_addr;
    int recv_result = recvfrom(sockfd_udp_serverM, &recv_buffer, BUF_SIZE, FLAG, (struct sockaddr *) &their_addr,
                               &addr_len);
    if (recv_result == -1) {
        perror("recvfrom error");
        exit(1);
    }

    memset(&query_result, 0, sizeof(query_result));
    memcpy(&query_result, recv_buffer, sizeof(query_result));

    cout << "The main server received the feedback from server " << (char) ('A' + i) << "using UDP over port "
         << udp_port_list.at(i) << "." << endl;
}

bool compareTransaction(Transaction t1, Transaction t2) {
    return (t1.serial_number < t2.serial_number);
}

int check_wallet(string username) {
    int balance = INITIAL_BALANCE;

    query.serial_number = CHECK_WALLET;
    query.sender = username;

    for (int i = 0; i < SLAVE_SERVER_SIZE; i++) {
        talk_to_slave_server(i);
        listen_from_slave_server(i);

        for (int k = 0; k < query_result.size; k++) {
            Transaction t = query_result.transaction_list[k];
            if (t.serial_number > max_serial_number) {
                max_serial_number = t.serial_number;
            }
            if (t.sender == username) {
                balance -= t.amount;
            }
            if (t.receiver == username) {
                balance += t.amount;
            }
        }
    }

    return balance;
}

bool tx_coins(string sender, string receiver, int amount) {
    int sender_balance = check_wallet(sender);
    if (sender_balance < amount) {
        return false;
    }
    int serial_number = max_serial_number + 1;
    query.serial_number = serial_number;
    query.sender = sender;
    query.receiver = receiver;
    query.amount = amount;

    int server_index = serial_number % 3;
    talk_to_slave_server(server_index);
    listen_from_slave_server(server_index);

    if (query_result.size == 1) {
        max_serial_number += 1;
        return true;
    }

    return false;
}

void get_and_sort_all_transactions() {
    query.serial_number = TX_LIST;
    for (int i = 0; i < SLAVE_SERVER_SIZE; i++) {
        talk_to_slave_server(i);
        listen_from_slave_server(i);
        for (int k = 0; k < query_result.size; k++) {
            Transaction t = query_result.transaction_list[k];
            if (t.serial_number > max_serial_number) {
                max_serial_number = t.serial_number;
            }
            transaction_list.push_back(t);
        }
        sort(transaction_list.begin(), transaction_list.end(), compareTransaction);
    }
}

void output_tx_list() {
    fstream output_file;
    output_file.open(TX_LIST_FILE_PATH);

    for (int i = 0; i < transaction_list.size(); i++) {
        Transaction t = transaction_list.at(i);
        output_file << t.serial_number;
        output_file << " ";
        output_file << t.sender;
        output_file << " ";
        output_file << t.receiver;
        output_file << " ";
        output_file << t.amount;
        output_file << "\n";
    }
    output_file.close();
}

void tx_list() {
    get_and_sort_all_transactions();
    output_tx_list();
}

void* handle_client_operations(void* param) {
    int client_index = *((int*)param);

    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    sin_size = sizeof their_addr;

    char buf[BUF_SIZE];
    while (true) {
        int new_fd = accept(tcp_listener[client_index], (struct sockaddr *) &their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }
        memset(&buf, 0, BUF_SIZE);
        read(new_fd, buf, BUF_SIZE);
        memset(&operations[client_index], 0, sizeof(operations[client_index]));
        memcpy(&operations[client_index], buf, sizeof(operations[client_index]));

        if (operations[client_index].serial_number == CHECK_WALLET) {
            cout << "The main server received input=\"" << operations[client_index].sender
                 << "\" from the client using TCP over port " << tcp_port_list[client_index] << "." << endl;
            int balance = check_wallet(operations[client_index].sender);
            operation_results[client_index].size = balance;

            if (send(new_fd, &operation_results[client_index], sizeof operation_results[client_index], 0) == -1)
                perror("send");
            cout << "The main server sent the current balance to client " << (char) ('A' + client_index) << "." << endl;
        }

        else if (operations[client_index].serial_number > CHECK_WALLET) {
            cout << "The main server received from \"" << operations[client_index].sender << "\" to transfer "
                 << operations[client_index].amount << " coins to \"" << operations[client_index].receiver
                 << "\" using TCP over port " << tcp_port_list[client_index] << "." << endl;
            bool is_success = tx_coins(operations[client_index].sender, operations[client_index].receiver,
                                       operations[client_index].amount);
            if (is_success) {
                operation_results[client_index].size = 1;
            } else {
                operation_results[client_index].size = 0;
            }

            if (send(new_fd, &operation_results[client_index], sizeof operation_results[client_index], 0) == -1)
                perror("send");
            cout << "The main server sent the result of the transaction to client " << (char) ('A' + client_index) << "." << endl;
        }

        else if (operations[client_index].serial_number == TX_LIST) {
            tx_list();
            operation_results[client_index].size = 1;
            if (send(new_fd, &operation_results[client_index], sizeof operation_results[client_index], 0) == -1)
                perror("send");
        }

        else if (operations[client_index].serial_number == STATS) {
            tx_list();
            operation_results[client_index].size = transaction_list.size();
            for (int i = 0; i < transaction_list.size(); i++) {
                operation_results[client_index].transaction_list[i] = transaction_list.at(i);
            }
            if (send(new_fd, &operation_results[client_index], sizeof operation_results[client_index], 0) == -1)
                perror("send");
        }
    }
}

int main() {
    boot_up_serverM();
    while(true) {
        pthread_t clientA_handler;
        pthread_t clientB_handler;
        int index_A = 0;
        pthread_create(&clientA_handler, NULL, handle_client_operations, (void *)&(index_A));
        pthread_join(clientA_handler, NULL);
        int index_B = 0;
        pthread_create(&clientB_handler, NULL, handle_client_operations, (void *)&(index_B));
        pthread_join(clientB_handler, NULL);
    }
    return 0;
}