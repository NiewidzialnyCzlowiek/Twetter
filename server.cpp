#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>
#include <iostream>
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <sys/fcntl.h>
#include <signal.h>
#include <arpa/inet.h>
//#include <sys/wait.h>
//#include <netinet/in.h>
//#include <time.h>

using std::string;

#define SERVER_PORT 1234
#define QUEUE_SIZE 5
#define MESSAGE_SIZE 1000
#define USERNAME_SIZE 50
const int MAX_CLIENTS = 10;
const char REUSE_ADDR_VAL = 1;

//struktura zawierająca dane, które zostaną przekazane do wątku
typedef struct userData
{
    bool free = true;
    int socketDescriptor;   // -1 means offline
    char username[USERNAME_SIZE];
    bool subscribedUsers[MAX_CLIENTS]; // indexes (in: userData users) of subscribed clients
} userData;

//struktura zawierająca dane, które zostaną przekazane do wątku
typedef struct threadData
{
    userData *users;
    int *connectionSocketDescriptors;
    int clientDescIndex;
    //int currentUserID;  // index (in: userData users) of current user
    //bool run = true;
} threadData;

//struktura wiadomosci
typedef struct message
{
    int type;   /* type of message (user message, system message)
    0 - user authentication initialization from client
    1 - user authentication response to client (success, send userdID)
    TODO 2 - initial data (subscriptions) for client
    TODO  - client disconnect request
    10 - user text message
    20 - request subscribe
    21 - response subscribe success
    22 - response subscribe fail
    30 - request unsubscribe
    31 - response unsubscribe success
    32 - response unsubscribe fail
    */

    int userID;
    char username[USERNAME_SIZE];  // ensure non empty string in client
    char msg[MESSAGE_SIZE];
} message;

//associate user with socket, returns index of user associated with calling socket thread
int authenticateUser(int socketDescriptor, userData *users, char username[USERNAME_SIZE]) {
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(!(users[i].free) && strcmp(users[i].username, username) == 0) { 
            users[i].socketDescriptor = socketDescriptor;
            return i;
        }
    }
    //create new user
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(users[i].free) {
            users[i].free = false;
            strcpy(users[i].username, username);
            users[i].socketDescriptor = socketDescriptor;
            return i;
        }
    }
    return -1;
}

int subscribeUser(userData *users, int requestingUserID, char subscribedUsername[USERNAME_SIZE]) {
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(!(users[i].free) && strcmp(users[i].username, subscribedUsername) == 0) { 
            if(users[requestingUserID].subscribedUsers[i] == false) {
                users[requestingUserID].subscribedUsers[i] = true;
                return 0;//success
            }
            else {
                return 2;//user already subscribed
            }
        }
    }
    return 1; //user not found
}

int unsubscribeUser(userData *users, int requestingUserID, char unsubscribedUsername[USERNAME_SIZE]) {
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(!(users[i].free) && strcmp(users[i].username, unsubscribedUsername) == 0) { 
            if(users[requestingUserID].subscribedUsers[i] == true) {
                users[requestingUserID].subscribedUsers[i] = false;
                return 0;//success
            }
            else {
                return 2;//user was not subscribed
            }
        }
    }
    return 1; //user not found
}


//funkcja opisującą zachowanie wątku - musi przyjmować argument typu (void *) i zwracać (void *)
void *ThreadBehavior(void *voidThreadData)
{
    std::cout<< pthread_self() << ": Hi, I'm new thread\n";
    pthread_detach(pthread_self());
    threadData *threadData = (struct threadData*)voidThreadData;
    message msg;
    int clientSocket = threadData->connectionSocketDescriptors[threadData->clientDescIndex];
    int userID = -1;
    bool run = true;
    while(run)
    {
        memset(&msg, 0, sizeof(struct message));
        std::cout<< pthread_self() << ": waiting for data (read)\n";
        int received = read(clientSocket, &msg, sizeof(struct message));
        printf("%lu: received type: %d, size: %d Bytes\n", pthread_self(), msg.type, received);  
        if(received == 0) {
            std::cout<< pthread_self() << ": received 0B, client dead, stopping thread...\n";
            run = false;
            continue;
        }      
        if(userID == -1) {
            // authentication
            if(msg.type == 0) {
                userID = authenticateUser(clientSocket, threadData->users, msg.username);
            }
            if(userID == -1) {
                std::cout <<  pthread_self() << ": Authentication failed.\n";
            }
            else {
                std::cout << pthread_self() << ": Authentication success, userID: " << userID << ", username: " << msg.username << std::endl;
                memset(&msg, 0, sizeof(struct message));
                msg.type = 1;
                msg.userID = userID;
                write(clientSocket, &msg, sizeof(struct message));
            }
        }
        else if (msg.type == 10) {
            // text message, only client thread sends message further
            if(msg.userID == userID) {
                for(int i = 0; i < MAX_CLIENTS; i++)
                {
                    if(threadData->users[i].free == true) {
                        continue;
                    }
                    int destinationSocket = threadData->users[i].socketDescriptor;
                    if(destinationSocket > 2 && destinationSocket != clientSocket) {
                        if(threadData->users[i].subscribedUsers[userID] == true) {
                            int sent = write(destinationSocket, &msg, sizeof(struct message));
                            printf("%lu: wyslano type: %d, %d Bajtow\n", pthread_self(), msg.type, sent);
                        }
                        else {
                            //Sent to unsubscribed
                            message msg2;
                            msg2.type = 11;
                            msg2.userID = msg.userID;
                            strcpy(msg2.username, msg.username);
                            strcpy(msg2.msg, msg.msg);
                            int sent = write(destinationSocket, &msg2, sizeof(struct message));
                            printf("%lu: wyslano type: %d, %d Bajtow\n", pthread_self(), msg.type, sent);
                        }
                        
                    }
                }
            }
        }
        else if (msg.type == 20) {
            // subscribe
            int status = subscribeUser(threadData->users, userID, msg.msg);
            if(status == 0) {
                printf("%lu: user %s subscribed user %s\n", pthread_self(), msg.username, msg.msg);
                msg.type = 21;
                write(clientSocket, &msg, sizeof(struct message));
            }
            else {
                printf("%lu: user %s failed to subscribe user %s, error code: %d\n", pthread_self(), msg.username, msg.msg, status);
                msg.type = 22;
                write(clientSocket, &msg, sizeof(struct message));
            }
        }
        else if (msg.type == 30) {
            // unsubscribe
            int status = unsubscribeUser(threadData->users, userID, msg.msg);
            if(status == 0) {
                printf("%lu: user %s unsubscribed user %s\n", pthread_self(), msg.username, msg.msg);
                msg.type = 31;
                write(clientSocket, &msg, sizeof(struct message));
            }
            else {
                printf("%lu: user %s failed to unsubscribe user %s, error code: %d\n", pthread_self(), msg.username, msg.msg, status);
                msg.type = 32;
                write(clientSocket, &msg, sizeof(struct message));
            }
        }
        else {
            std::cout<< pthread_self() << ": Received unexpected type: , skipping...\n" << msg.type << std::endl;
        }
    }
    close(clientSocket);
    threadData->connectionSocketDescriptors[threadData->clientDescIndex] = -1;
    threadData->users[userID].socketDescriptor = -1;
    std::cout<< pthread_self() << ": thread stopped.\n";
    pthread_exit(NULL);
}

//funkcja obsługująca połączenie z nowym klientem
void handleConnection(int *connectionSocketDescriptors, int clientDescIndex, userData *users) {

    //dane, które zostaną przekazane do wątku
    threadData *threadData = new struct threadData;
    threadData->connectionSocketDescriptors = connectionSocketDescriptors;
    threadData->clientDescIndex = clientDescIndex;
    threadData->users = users;
    pthread_t thread;
    int createResult = pthread_create(&thread, NULL, ThreadBehavior, (void *)threadData);
    if (createResult){
        printf("Error %d encountered creating thread.\n", createResult);
    }
}

int main(int argc, char* argv[]) {  
    //initialize server socket
    int serverSocketDescriptor;
    serverSocketDescriptor = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocketDescriptor < 0) {
        fprintf(stderr, "%s: Error encountered initializing server socket\n", argv[0]);
        exit(1);
    }
    setsockopt(serverSocketDescriptor, SOL_SOCKET, SO_REUSEADDR, (char*)&REUSE_ADDR_VAL, sizeof(REUSE_ADDR_VAL));
    /* // set socket to be non-blocking
    int flags = fcntl(serverSocketDescriptor, F_GETFL, 0);
    fcntl(serverSocketDescriptor, F_SETFL, flags | O_NONBLOCK);*/
    //bind socket
    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(struct sockaddr));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    std::cout << "port: " << atoi(argv[1]) << '\n';    
    server_address.sin_port = htons(atoi(argv[1]));
    if (bind(serverSocketDescriptor, (struct sockaddr*)&server_address, sizeof(struct sockaddr)) < 0) {
        fprintf(stderr, "%s: Error encountered binding address and port to socket\n", argv[0]);
        exit(1);
    }
    //set listener
    if (listen(serverSocketDescriptor, QUEUE_SIZE) < 0) {
        fprintf(stderr, "%s: Error encountered setting listener\n", argv[0]);
        exit(1);
    }
    std::cout << "server socket initialized: file descriptor = " << serverSocketDescriptor << '\n';        
    std::cout << "Initializing services and database..." << '\n';
/*
    //initialize epoll
    struct epoll_event event, events[MAX_CLIENTS+1];
    int epoll = epoll_create1(0);   // epoll file descriptor
    if(epoll == -1) {
        fprintf(stderr, "Failed to create epoll file descriptor\n");
        exit(1);
    }
    //add listening server socket to epoll
    event.events = EPOLLIN;
    event.data.fd = serverSocketDescriptor;
    if(epoll_ctl(epoll, EPOLL_CTL_ADD, serverSocketDescriptor, &event) == -1) {
        fprintf(stderr, "Failed to add file descriptor to epoll\n");
        close(epoll);
        exit(1);
    }
*/
    int clientsOnlineCount = 0;
    int connectionSocketDescriptors[MAX_CLIENTS] = { -1 };   // client sockets, -1 menas free slot
    for(int i = 0;i < MAX_CLIENTS;i++) { connectionSocketDescriptors[i] = -1; }
    userData users[MAX_CLIENTS];
    for(int i = 0;i < MAX_CLIENTS;i++) {
        users[i].socketDescriptor = -1; 
        for(int j = 0;j < MAX_CLIENTS;j++) { users[i].subscribedUsers[j] = false; } }
    int run = true;
    std::cout << "Server successfully started." << '\n';  
    while(run) {
        //while(clientsOnlineCount >= 10) {}
        struct sockaddr_in newClient;  //connected client data
        socklen_t newClientSize = sizeof(newClient);
        std::cout<<"Server: Waiting for connection request...\n";
        int connectionSocketDescriptor = accept(serverSocketDescriptor, (struct sockaddr *) &newClient, &newClientSize);
        if (connectionSocketDescriptor < 0) {
            fprintf(stderr, "Server: Error encountered accepting connection\n");
            continue;
        }
        int clientDescIndex;
        for(int i = 0;i < MAX_CLIENTS;i++) {
            if(connectionSocketDescriptors[i] == -1) {
                connectionSocketDescriptors[i] = connectionSocketDescriptor;
                clientDescIndex = i;
                break;
            }
        }
        std::cout << "Server: Accepted new client: clientDescIndex: "<< clientDescIndex << ", socket: " << connectionSocketDescriptor;
        clientsOnlineCount++;   // needed?
        handleConnection(connectionSocketDescriptors, clientDescIndex, users);

        /* epoll
        int eventCount = epoll_wait(epoll, events, MAX_CLIENTS+1, 3*60*1000);   // time out 3 minutes
        if ( eventCount == -1 ) { std::cout << "error when epolling" << '\n'; }
        else if ( eventCount == 0 ) {
            std::cout << "Server timed out. Stoping serivces..." << '\n';

            // TODO eventual stopping actions

            break;
        }
        else {
            for(int i = 0; i < eventCount; i++)
            {
                int eventSocketDesc = events[i].data.fd;
                if(eventSocketDesc == serverSocketDescriptor) { // server socket (accept client)
                    struct sockaddr_in newClient;  //connected client data
                    socklen_t newClientSize = sizeof(newClient);
                    int connectionSocketDescriptor = accept(serverSocketDescriptor, (struct sockaddr *) &newClient, &newClientSize);
                    if (connectionSocketDescriptor < 0) {
                        fprintf(stderr, "%s: Error encountered accepting connection\n", argv[0]);
                    }
                    connectionSocketDescriptors[clientsOnlineCount] = connectionSocketDescriptor;
                    event.events = EPOLLIN;
                    event.data.fd = connectionSocketDescriptor;
                    if(epoll_ctl(epoll, EPOLL_CTL_ADD, serverSocketDescriptor, &event) == -1) {
                        fprintf(stderr, "Failed to add new client (socket descriptor) to epoll: IP: %d, port: %hu \n", newClient.sin_addr.s_addr, newClient.sin_port);
                    }
                    else {
                        std::cout << "Accepted new client: IP: " << newClient.sin_addr.s_addr << ", port: " << newClient.sin_port << '\n';
                        clientsOnlineCount++;
                        handleConnection(connectionSocketDescriptor);
                    }
                }
                else {
                    // deal with client 
                    printf("Reading file descriptor '%d' -- ", eventSocketDesc);
                    char readBuffer[MESSAGE_SIZE+1];
                    int bytesRead = read(eventSocketDesc, readBuffer, MESSAGE_SIZE);
                    printf("%d bytes read.\n", bytesRead);
                    readBuffer[bytesRead] = '\0';
                    printf("Read '%s'\n", readBuffer);
                }
            }
        }
        */
    }

    // TODO wątek na serwer, by w terminalu można było ładnie zakończyć działanie i zwolnić zasoby

    //close(epoll);
    close(serverSocketDescriptor);
    std::cout << "Server is shut down." << '\n';
    return(0);
}

/* POLL init
        struct pollfd fds[MAX_CLIENTS+1];   // socket descriptors, [0] - listening socket, [1+] - clients' sockets
        memset(fds, 0 , sizeof(fds));
        //set server socket for poll
        fds[0].fd = serverSocketDescriptor;
        fds[0].events = POLLIN;

        // poll
        int ret = poll( fds, 2, 3*60*1000 ); 

        if ( ret == -1 ) { std::cout << "error when polling" << '\n'; }
        else if ( ret == 0 ) {
            std::cout << "Server timed out. Stoping serivces..." << '\n';

            // TODO eventual stopping actions

            break;
        }
        else {
            if ( pfd[0].revents & POLLIN )
                pfd[0].revents = 0;
                // input event on sock1
            if ( pfd[1].revents & POLLOUT )
                pfd[1].revents = 0;
                // output event on sock2
        }*/
