#include <arpa/inet.h>
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include "types.h"

#define PORT 8080

using namespace std;

int sock = 0;
bool game_running = true;


void* receive_messages(void* arg) {
    Message msg;
    
    while (game_running) {
        int bytes = recv(sock, &msg, sizeof(Message), 0);
        
        if (bytes <= 0) {
            cout << "\n❌ Conexão perdida com o servidor.\n";
            game_running = false;
            break;
        }
  
        switch (msg.type) {
            case MSG_WINNER:
                cout << "\n" << msg.text << "\n";
                game_running = false;
                break;
                
            case MSG_SCOREBOARD:
                cout << msg.text;
                break;
                
            case MSG_HAND:
                cout << msg.text;
                break;
                
            case MSG_TABLE:
                cout << msg.text;
                break;
                
            case MSG_YOUR_TURN:
                cout << "\n" << msg.text;
                break;
                
            case MSG_TEXT:
                cout << msg.text;
                break;
                
            case MSG_ROOM_JOIN:
                cout << "✅ " << msg.text;
                break;
                
            default:
                cout << msg.text;
                break;
        }
        
        fflush(stdout);
    }
    
    return nullptr;
}


void* send_commands(void* arg) {
    string input;
    
    while (game_running) {
        getline(cin, input);
        
        if (!game_running) break;
        
        if (input.empty()) continue;

        Message msg;
        msg.type = MSG_GAME_ACTION;
        strncpy(msg.text, input.c_str(), MESSAGE_SIZE - 1);
        msg.text[MESSAGE_SIZE - 1] = '\0';
 
        if (send(sock, &msg, sizeof(Message), 0) < 0) {
            cout << "❌ Erro ao enviar mensagem.\n";
            game_running = false;
            break;
        }
    }
    
    return nullptr;
}

int main(int argc, char const* argv[]) {
    struct sockaddr_in serv_addr;
    
    cout << "🃏 TRUCO GAUDÉRIO - Cliente\n";
    cout << "============================\n\n";

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        cout << "❌ Erro ao criar socket\n";
        return -1;
    }
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        cout << "❌ Endereço inválido ou não suportado\n";
        return -1;
    }

    cout << "🔄 Conectando ao servidor...\n";
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        cout << "❌ Falha na conexão\n";
        return -1;
    }
    
    cout << "✅ Conectado ao servidor!\n";
    cout << "⏳ Aguardando outro jogador...\n\n";

    pthread_t recv_thread, send_thread;
    
    pthread_create(&recv_thread, nullptr, receive_messages, nullptr);
    pthread_create(&send_thread, nullptr, send_commands, nullptr);

    pthread_join(recv_thread, nullptr);
    pthread_join(send_thread, nullptr);
    
    close(sock);
    cout << "\n👋 Desconectado do servidor.\n";
    
    return 0;
}
