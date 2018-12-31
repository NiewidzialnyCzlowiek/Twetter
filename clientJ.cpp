#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <iostream>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include "json.hpp"

#define BUF_SIZE 1024
#define NUM_THREADS 5
#define TAGS_SIZE 100
#define TITLE_SIZE 100
#define MESSAGE_SIZE 1000
#define USERNAME_SIZE 50
#define JSON_SIZE 2048

using std::string;
using json = nlohmann::json;

//struktura zawierająca dane, które zostaną przekazane do wątku
struct thread_data_t
{
    int socket_desc;
    bool run;
};

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

int userID = -1;
char username[USERNAME_SIZE];
bool hasUsername = false;

//wskaźnik na funkcję opisującą zachowanie wątku
void *ThreadBehavior(void *t_data)
{
    // SEND
    
    struct thread_data_t *th_data = (struct thread_data_t*)t_data;
    //dostęp do pól struktury: (*th_data).pole
    //SEND
    
    message msg;
    bool run = true;
    char input[50];
    while(run)
    {
        memset(&msg, 0, sizeof(struct message));
        printf("Podaj type:\n");
        fgets(input, sizeof(input), stdin);
        msg.type = atoi(input);
        msg.userID = userID;
        if(!hasUsername) {
            printf("Podaj username:\n");
            fgets(msg.username, sizeof(msg.username), stdin);
            strcpy(username, msg.username);
            hasUsername = true;
        } else { strcpy(msg.username, username); }   
        printf("Podaj message:\n");
        fgets(msg.msg, sizeof(msg.msg), stdin);
        if(strcmp(msg.msg, "exit") == 0)
        {
            (*th_data).run = false;
            run = false;
        }
        else
        {
            json j = convertMsgToJson(&msg);
            std::string strToSend = j.dump();
            char toSend[JSON_SIZE];
            memset(toSend, 0, JSON_SIZE);                
            strcpy(toSend, strToSend.c_str());
            int sent = write((*th_data).socket_desc, toSend, JSON_SIZE);
            printf("wyslano %d Bajtow\n", sent);
        }
    }

    pthread_exit(NULL);
}


//funkcja obsługująca połączenie z serwerem
void handleConnection(int sd) {
    //wynik funkcji tworzącej wątek
    int create_result = 0;

    //uchwyt na wątek
    pthread_t thread1;

    //dane, które zostaną przekazane do wątku
    struct thread_data_t t_data;
    t_data.socket_desc = sd;
    t_data.run = true;
    bool run=true;
    
    create_result = pthread_create(&thread1, NULL, ThreadBehavior, (void *)&t_data);
    if (create_result){
       printf("Błąd przy próbie utworzenia wątku, kod błędu: %d\n", create_result);
       exit(-1);
    }

    //RECEIVE

    message msg;
    json j;
    while(t_data.run)
    {
        //memset(&j, 0, sizeof(json));   
        char receivedChar[JSON_SIZE]; 
        int received = read(sd, receivedChar, JSON_SIZE);
        std::string receivedStr(receivedChar);              
        j = json::parse(receivedStr);
        memset(&msg, 0, sizeof(struct message));
        convertJsonToMsg(j, &msg);
        printf("odebrano type: %d, rozmair: %d Bajtow\n", msg.type, received);
        if(userID == -1) {
            if(msg.type == 1) {
                userID = msg.userID;
                continue;
            }
            else if (msg.type == 2) {
                printf("user %s is already logged in.\n", msg.username);
            }
            else {
                printf("Oczekiwano type 1 (autoryzacja), otrzymano %d\n", msg.type);
            }
        }
        else if (msg.type == 10) {
            printf("user %s: %s\n", msg.username, msg.msg);
        }
        else if (msg.type == 11) {
            printf("user %s (UNSUBSCRIBED): %s\n", msg.username, msg.msg);
        }
        
    }
}


int main (int argc, char *argv[])
{
   int connection_socket_descriptor;
   int connect_result;
   struct sockaddr_in server_address;
   struct hostent* server_host_entity;

   if (argc != 3)
   {
     fprintf(stderr, "Sposób użycia: %s server_name port_number\n", argv[0]);
     exit(1);
   }

   server_host_entity = gethostbyname(argv[1]);
   if (! server_host_entity)
   {
      fprintf(stderr, "%s: Nie można uzyskać adresu IP serwera.\n", argv[0]);
      exit(1);
   }

   connection_socket_descriptor = socket(PF_INET, SOCK_STREAM, 0);
   if (connection_socket_descriptor < 0)
   {
      fprintf(stderr, "%s: Błąd przy probie utworzenia gniazda.\n", argv[0]);
      exit(1);
   }

   memset(&server_address, 0, sizeof(struct sockaddr));
   server_address.sin_family = AF_INET;
   memcpy(&server_address.sin_addr.s_addr, server_host_entity->h_addr, server_host_entity->h_length);
   server_address.sin_port = htons(atoi(argv[2]));

   connect_result = connect(connection_socket_descriptor, (struct sockaddr*)&server_address, sizeof(struct sockaddr));
   if (connect_result < 0)
   {
      fprintf(stderr, "%s: Błąd przy próbie połączenia z serwerem (%s:%i).\n", argv[0], argv[1], atoi(argv[2]));
      exit(1);
   }

   handleConnection(connection_socket_descriptor);

   close(connection_socket_descriptor);
   return 0;

}
