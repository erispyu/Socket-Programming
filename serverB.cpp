#include <string>
#include <fstream>
#include <map>
#include <vector>
#include <iostream>
#include <netdb.h>
#include <unistd.h>

#define localhost "127.0.0.1"
#define UDP_PORT "22256"
#define BLOCK_FILE_PATH "./block2.txt"

#define BUF_SIZE 2048
#define FLAG 0

#define CHECK_WALLET 0
#define TX_LIST -1
#define LOGGED -2

#define LIS_MAX_SIZE 100

using namespace std;

int sockfd;
struct addrinfo *serverM;

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

char recv_buf[BUF_SIZE];
Transaction query;
QueryResult query_result;

void start_upd_listener() {
    struct addrinfo hints, *servinfo, *p;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // set to AF_INET to use IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, UDP_PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            continue;
        }

        if (::bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            continue;
        }

        break;
    }

    if (p == NULL) {
        exit(2);
    }

    freeaddrinfo(servinfo);
}

void start_udp_talker() {
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(localhost, UDP_PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    // loop through all the results and make a socket
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            continue;
        }

        break;
    }

    if (p == NULL) {
        exit(2);
    }

    serverM = servinfo;
}

void parse_block_file() {
    ifstream input_file;
    input_file.open(BLOCK_FILE_PATH);

    while (!input_file.eof()) {
        Transaction t;
        string serial_number_str;
        string amount_str;

        input_file >> serial_number_str;
        t.serial_number = stoi(serial_number_str);
        input_file >> t.sender;
        input_file >> t.receiver;
        input_file >> amount_str;
        t.amount = stoi(amount_str);

        transaction_list.push_back(t);
    }

    input_file.close();
}

void log_new_transaction(Transaction t) {
    transaction_list.push_back(t);

    fstream output_file;
    output_file.open(BLOCK_FILE_PATH, ios_base::app | ios_base::in | ios::skipws);

    output_file << t.serial_number;
    output_file << " ";
    output_file << t.sender;
    output_file << " ";
    output_file << t.receiver;
    output_file << " ";
    output_file << t.amount;
    output_file << "\n";

    output_file.close();
}

vector<struct Transaction> find_related_transactions(string username) {
    vector<struct Transaction> related_transactions;

    for (int i = 0; i < transaction_list.size(); i++) {
        Transaction t = transaction_list.at(i);
        if (t.sender == username || t.receiver == username) {
            related_transactions.push_back(t);
        }
    }

    return related_transactions;
}

void listen_from_serverM() {
    struct sockaddr_storage their_addr;
    socklen_t addr_len = sizeof their_addr;

    memset(recv_buf, 0, BUF_SIZE);
    int recvfrom_result = recvfrom(sockfd, &recv_buf, BUF_SIZE, FLAG, (struct sockaddr *)&their_addr, &addr_len);
    if (recvfrom_result == -1) {
        perror("recvfrom error");
        exit(1);
    }

    memset(&query, 0, sizeof(query));
    memcpy(&query, recv_buf, sizeof(query));

    cout << "The ServerB received a request from the Main Server." << endl;
}

void talk_to_serverM() {
    if (sendto(sockfd, &query_result, sizeof(query_result), 0, serverM->ai_addr, serverM->ai_addrlen) == -1) {
        perror("talker: sendto");
        exit(1);
    }
    cout << "The ServerB finished sending the response to the Main Server." << endl;
}


void handle_query() {
    memset(&query_result, 0, sizeof query_result);

    cout << query.serial_number << ":" << query.sender << ":" << query.receiver << ":" << query.amount << endl;

    if (query.serial_number == CHECK_WALLET) {
        vector<struct Transaction> related_transactions = find_related_transactions(query.sender);
        query_result.size = related_transactions.size();
        for (int i = 0; i < query_result.size; i++) {
            query_result.transaction_list[i] = related_transactions.at(i);
        }
    }

    else if (query.serial_number == TX_LIST) {
        query_result.size = transaction_list.size();
        for (int i = 0; i < query_result.size; i++) {
            query_result.transaction_list[i] = transaction_list.at(i);
        }
    }

    else if (query.serial_number > 0){
        log_new_transaction(query);
        query_result.size = LOGGED;
    }

    memset(&query, 0, sizeof query);
}


int main() {
    start_upd_listener();
    start_udp_talker();
    parse_block_file();
    cout << "The ServerB is up and running using UDP on port " << UDP_PORT << "." << endl;

    listen_from_serverM();
    handle_query();
    talk_to_serverM();

    return 0;
}

