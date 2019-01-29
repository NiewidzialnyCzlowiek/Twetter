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
#include <sys/fcntl.h>
#include <signal.h>
#include <arpa/inet.h>
#include "json.hpp"

using std::string;
using json = nlohmann::json;

#define SERVER_PORT 1234
#define QUEUE_SIZE 5
#define MESSAGE_SIZE 1000
#define TAGS_SIZE 100
#define TITLE_SIZE 100
#define USERNAME_SIZE 50
#define JSON_SIZE 2048
const int MAX_CLIENTS = 10;
const char REUSE_ADDR_VAL = 1;

pthread_mutex_t mutexConnectionSocketDescriptors = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexUsers = PTHREAD_MUTEX_INITIALIZER;

//struktura zawierająca dane, które zostaną przekazane do wątku
typedef struct userData
{
    bool free = true;
    int socketDescriptor;   // -1 means offline
    char username[USERNAME_SIZE];
    bool subscribedUsers[MAX_CLIENTS]; // true in indexes corresponding to subscribed userID (userID = user index in users array).
} userData;

//struktura zawierająca dane, które zostaną przekazane do wątku
typedef struct threadData
{
    userData *users;
    int *connectionSocketDescriptors;
    int clientDescIndex;
} threadData;

//struktura wiadomosci
typedef struct message
{
    int type;
    int userID;
    char title[TITLE_SIZE];
    char username[USERNAME_SIZE];  // author
    char msg[MESSAGE_SIZE]; // content
    char tags[TAGS_SIZE];
} message;

//associate user with socket, returns index of user associated with calling socket thread
int authenticateUser(int socketDescriptor, userData *users, char username[USERNAME_SIZE]) {
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(!(users[i].free) && strcmp(users[i].username, username) == 0) {
            if(users[i].socketDescriptor == -1) {
                users[i].socketDescriptor = socketDescriptor;
                return i;
            }
            return -2;  //user already logged in
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

void getSubscribedUsers(char* usersList, userData *users, int userID) {
    usersList[0] = '\0';                
    for(int i = 0;i < MAX_CLIENTS;i++) {
        if(users[userID].subscribedUsers[i] == true) {
            strcat(usersList, users[i].username);
        }
    }
    printf("Users subscribed by %d: (new line)\n%s end of list\n", userID, usersList);
}

json convertMsgToJson(message *msg) {
    json j;
    j["type"] = msg->type;
    j["userID"] = msg->userID;
    j["title"] = std::string(msg->title);
    j["author"] = std::string(msg->username);
    j["content"] = std::string(msg->msg);
    j["tags"] = std::string(msg->tags);
    return j;
}

void convertJsonToMsg(json j, message *msg) {
    j.at("type").get_to(msg->type);
    j.at("userID").get_to(msg->userID);
    std::string str;
    const char *cstr;
    j.at("title").get_to(str);
    cstr = str.c_str();
    strcpy(msg->title, cstr);
    j.at("author").get_to(str);
    cstr = str.c_str();
    strcpy(msg->username, cstr);
    j.at("content").get_to(str);
    cstr = str.c_str();
    strcpy(msg->msg, cstr);  
    j.at("tags").get_to(str);
    cstr = str.c_str();
    strcpy(msg->tags, cstr);  
}

// from input message generates JSON formatted string to be sent to client
char *prepareAnswer(message *msg, char toSend[JSON_SIZE]) {
    json jResponse = convertMsgToJson(msg);
    std::string strToSend = jResponse.dump();
    memset(toSend, 0, JSON_SIZE);                
    strcpy(toSend, strToSend.c_str());
    return toSend;
}

bool writeMessageLength(int socketDesc, int length) {
    return true;
}

bool writeMessage(int socketDesc, char* message) {
    int msgSize = sizeof(message);
    bool statusOk = true;
    if (msgSize > 0) {
        write(socketDesc, message, JSON_SIZE);
    } else {
        statusOk = false;
    }
    return statusOk;
}

bool readMessageLength(int socketDesc, int& length) {
    length = JSON_SIZE;
    return true;
}

bool readMessage(int socketDesc, char* buffer) {
    int maxLen = sizeof(buffer);
    int msgLen;
    bool statusOk = true;
    if (readMessageLength(socketDesc, msgLen)) {
        int red = read(socketDesc, buffer, maxLen);
    } else {
        statusOk = false;
    }
    return statusOk;
}

//funkcja opisującą zachowanie wątku - musi przyjmować argument typu (void *) i zwracać (void *)
void *ThreadBehavior(void *voidThreadData)
{
    std::cout<< pthread_self() << ": Hi, I'm new thread\n";
    pthread_detach(pthread_self());
    threadData *threadData = (struct threadData*)voidThreadData;
    json jmsg;
    message msg;   
    pthread_mutex_lock(&mutexConnectionSocketDescriptors);    
    int clientSocket = threadData->connectionSocketDescriptors[threadData->clientDescIndex];
    pthread_mutex_unlock(&mutexConnectionSocketDescriptors);
    int userID = -1;
    char username[USERNAME_SIZE];
    bool run = true;
    char toSend[JSON_SIZE];
    while(run)
    {
        std::cout<< pthread_self() << ": waiting for data (read)\n";
        //memset(&jmsg, 0, sizeof(json));  
        char receivedChar[JSON_SIZE];
        memset(receivedChar, 0, JSON_SIZE);    
        int received = read(clientSocket, receivedChar, JSON_SIZE);
        if(received == 0) {
            std::cout<< pthread_self() << ": client disconnected, stopping thread...\n";
            run = false;
            continue;
        }  
        std::string receivedStr(receivedChar);                    
        jmsg = json::parse(receivedStr);            
        memset(&msg, 0, sizeof(struct message)); 
        convertJsonToMsg(jmsg, &msg);
        printf("%lu: received type: %d, size: %d Bytes\n", pthread_self(), msg.type, received);  
        if(msg.type == 100) {
            std::cout<< pthread_self() << ": client disconnected, stopping thread...\n";
            run = false;
            continue;
        }      
        if(userID == -1) {
            // authentication
            if(msg.type == 0) {
                pthread_mutex_lock(&mutexUsers);
                userID = authenticateUser(clientSocket, threadData->users, msg.username);
                pthread_mutex_unlock(&mutexUsers);
                if(userID == -1) {
                    std::cout <<  pthread_self() << ": Authentication failed.\n";
                }
                else if (userID == -2) {
                    userID = -1;
                    std::cout <<  pthread_self() << ": user " << msg.username << " already logged in.\n";
                    msg.type = 2;
                    write(clientSocket, prepareAnswer(&msg, toSend), JSON_SIZE);
                }
                else {
                    std::cout << pthread_self() << ": Authentication success, userID: " << userID << ", username: " << msg.username << std::endl;
                    strcpy(username, msg.username);
                    memset(&msg, 0, sizeof(struct message));
                    msg.type = 1;
                    msg.userID = userID;
                    pthread_mutex_lock(&mutexUsers);
                    getSubscribedUsers(msg.msg, threadData->users, userID);
                    pthread_mutex_unlock(&mutexUsers);
                    write(clientSocket, prepareAnswer(&msg, toSend), JSON_SIZE);
                }
            }
            else {
                std::cout<< pthread_self() << ": Received unexpected type. Waiting for authentication request (type 0) but received: " << msg.type << ". Skipping..." << std::endl;
            }
        }
        else if (msg.type == 10) {
            // text message, only client thread sends message further
            if(msg.userID == userID) {
                pthread_mutex_lock(&mutexUsers);
                for(int i = 0; i < MAX_CLIENTS; i++) {
                    if(threadData->users[i].free == true) {
                        continue;
                    }
                    int destinationSocket = threadData->users[i].socketDescriptor;
                    if(destinationSocket > 2 && destinationSocket != clientSocket) {
                        if(threadData->users[i].subscribedUsers[userID] == true) {
                            int sent = write(destinationSocket, prepareAnswer(&msg, toSend), JSON_SIZE);
                            printf("%lu: wyslano type: %d, %d Bajtow\n", pthread_self(), msg.type, sent);
                        }
                    }
                }
                pthread_mutex_unlock(&mutexUsers);
            }
        }
        else if (msg.type == 20) {
            // subscribe
            if(strcmp(msg.msg, username) == 0) {
                printf("%lu: user %s tried to subscribe himself, deny.\n", pthread_self(), msg.username);
                msg.type = 23;
                write(clientSocket, prepareAnswer(&msg, toSend), JSON_SIZE);
            }
            else {
                pthread_mutex_lock(&mutexUsers);
                int status = subscribeUser(threadData->users, userID, msg.msg);
                pthread_mutex_unlock(&mutexUsers);
                if(status == 0) {
                    printf("%lu: user %s subscribed user %s\n", pthread_self(), msg.username, msg.msg);
                    msg.type = 21;
                    write(clientSocket, prepareAnswer(&msg, toSend), JSON_SIZE);
                }
                else {
                    printf("%lu: user %s failed to subscribe user %s, error code: %d\n", pthread_self(), msg.username, msg.msg, status);
                    msg.type = 22;
                    write(clientSocket, prepareAnswer(&msg, toSend), JSON_SIZE);
                }
            }
        }
        else if (msg.type == 30) {
            // unsubscribe
            if(strcmp(msg.msg, username) == 0) {
                printf("%lu: user %s tried to unsubscribe himself, deny.\n", pthread_self(), msg.username);
                msg.type = 33;
                write(clientSocket, prepareAnswer(&msg, toSend), JSON_SIZE);
            }
            else {
                pthread_mutex_lock(&mutexUsers);
                int status = unsubscribeUser(threadData->users, userID, msg.msg);
                pthread_mutex_unlock(&mutexUsers);
                if(status == 0) {
                    printf("%lu: user %s unsubscribed user %s\n", pthread_self(), msg.username, msg.msg);
                    msg.type = 31;
                    write(clientSocket, prepareAnswer(&msg, toSend), JSON_SIZE);
                }
                else {
                    printf("%lu: user %s failed to unsubscribe user %s, error code: %d\n", pthread_self(), msg.username, msg.msg, status);
                    msg.type = 32;
                    write(clientSocket, prepareAnswer(&msg, toSend), JSON_SIZE);
                }
            }
        }
        else {
            std::cout<< pthread_self() << ": Received unexpected type: " << msg.type << ", skipping..." << std::endl;
        }
    }
    close(clientSocket);
    pthread_mutex_lock(&mutexConnectionSocketDescriptors);
    threadData->connectionSocketDescriptors[threadData->clientDescIndex] = -1;
    pthread_mutex_unlock(&mutexConnectionSocketDescriptors);
    pthread_mutex_lock(&mutexUsers);
    threadData->users[userID].socketDescriptor = -1;
    pthread_mutex_unlock(&mutexUsers);
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
    //bind socket
    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(struct sockaddr));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY); 
    if(argv[1] != nullptr) {
        std::cout << "port: " << atoi(argv[1]) << '\n';    
        server_address.sin_port = htons(atoi(argv[1]));
    } else {
        std::cout << "port: " << SERVER_PORT << '\n';    
        server_address.sin_port = htons(SERVER_PORT);
    }
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
    //initialize variables and database
    int connectionSocketDescriptors[MAX_CLIENTS] = { -1 };   // client sockets, -1 menas free slot
    for(int i = 0;i < MAX_CLIENTS;i++) {
        connectionSocketDescriptors[i] = -1;
    }
    userData users[MAX_CLIENTS];
    for(int i = 0;i < MAX_CLIENTS;i++) {
        users[i].socketDescriptor = -1; 
        for(int j = 0;j < MAX_CLIENTS;j++) {
            users[i].subscribedUsers[j] = false;
        }
    }
    int run = true;
    std::cout << "Server successfully started." << '\n';
    //START    
    while(run) {
        std::cout<<"Server: Waiting for connection request...\n";
        int connectionSocketDescriptor = accept(serverSocketDescriptor, NULL, NULL);        
        if (connectionSocketDescriptor < 0) {
            fprintf(stderr, "Server: Error encountered accepting connection\n");
            continue;
        }
        int clientDescIndex = -1;
        pthread_mutex_lock(&mutexConnectionSocketDescriptors);
        for(int i = 0;i < MAX_CLIENTS;i++) {
            if(connectionSocketDescriptors[i] == -1) {
                connectionSocketDescriptors[i] = connectionSocketDescriptor;
                clientDescIndex = i;
                break;
            }
        }
        pthread_mutex_unlock(&mutexConnectionSocketDescriptors);        
        if(clientDescIndex == -1) {
            close(connectionSocketDescriptor);
            std::cout << "Server: No free resources for another client, closing connection.";
            continue;            
        }
        std::cout << "Server: Accepted new client: clientDescIndex: "<< clientDescIndex << ", socket: " << connectionSocketDescriptor;
        handleConnection(connectionSocketDescriptors, clientDescIndex, users);
    }

    close(serverSocketDescriptor);
    std::cout << "Server is shut down." << '\n';
    return(0);
}