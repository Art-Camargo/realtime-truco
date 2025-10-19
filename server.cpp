#include <arpa/inet.h>
#include <iostream>
#include <cstring>
#include <pthread.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <semaphore.h>
#include <vector>
#include "cards.h"  

#define PORT 8080
#define TOTAL_PER_ROOM 2
#define MAX_ROOMS 2
#define NOT_AVAILABLE_MESSAGE "No rooms available. Try again later.\n"

using namespace std;

typedef struct {
  pthread_t id;
  sem_t start_round;
  vector<int> players;
  int total_players;
} Room;

typedef struct {
  int socket_fd;
  int points;
  vector<Card> hand;
  vector<int> already_played; 
} Player;

typedef struct {
  vector<Card> cards_on_table;
} Table;

int server_fd, new_socket;
struct sockaddr_in address;
int addrlen = sizeof(address);

int init_main_socket() {
  const int opt = 1;
  int server_fd;
  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("Socket failed");
    exit(EXIT_FAILURE);
  }

  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
    perror("Setsockopt failed");
    exit(EXIT_FAILURE);
  }

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(PORT);

  if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
    perror("Bind failed");
    exit(EXIT_FAILURE);
  }

  if (listen(server_fd, 4) < 0) {
    perror("Listen failed");
    exit(EXIT_FAILURE);
  }

  cout << "Server listening on port " << PORT << endl;
  return server_fd;
}

bool is_room_full(Room* room) {
  return room->total_players >= TOTAL_PER_ROOM;
}

void create_room(Room* room, pthread_t tid) {
  room->id = tid;
  room->total_players = 0;
  sem_init(&room->start_round, 0, 0);
  room->players.clear();
}

vector<Card> deal_hand() {
  vector<Card> hand;
  srand(time(nullptr));
  
  while (hand.size() < (TOTAL_PER_ROOM * 3)) {
    int index = rand() % DECK_SIZE;
    Card card = ordered_deck[index];
    bool already_in_hand = false;
    for (const Card& c : hand) {
      if (c.rank == card.rank && strcmp(c.suit, card.suit) == 0) {
        already_in_hand = true;
        break;
      }
    }
    if (!already_in_hand) {
      hand.push_back(card);
    }
  }

  return hand;
}

void send_hand_to_player(int socket_fd, vector<Card>& hand, vector<int>& already_played, Player players[], int total_players, vector<Card>& table) {
  string clear_screen = "\033[2J\033[H";
  send(socket_fd, clear_screen.c_str(), clear_screen.length(), 0);

  string scoreboard = "=== PLACAR ===\n";
  for (int i = 0; i < total_players; i++) {
    scoreboard += "Jogador " + to_string(i) + " - " + to_string(players[i].points) + " pontos\n";
  }
  scoreboard += "\n";
  send(socket_fd, scoreboard.c_str(), scoreboard.length(), 0);
  
  string table_msg = "=== MESA ===\n";
  if (table.size() > 0) {
    for (int i = 0; i < table.size(); i++) {
      table_msg += "Jogador " + to_string(i) + ": " + to_string(table[i].rank) + " de " + string(table[i].suit) + "\n";
    }
  } else {
    table_msg += "(vazia)\n";
  }
  table_msg += "\n";
  send(socket_fd, table_msg.c_str(), table_msg.length(), 0);
  
  string message = "HAND:\n";
  
  for (int i = 0; i < hand.size(); i++) {
    bool was_played = false;
    for (int played_idx : already_played) {
      if (played_idx == i) {
        was_played = true;
        break;
      }
    }
    
    if (!was_played) {
      message += to_string(i + 1) + " - " + to_string(hand[i].rank) + " de " + string(hand[i].suit) + "\n";
    }
  }
  
  message += "\n";
  send(socket_fd, message.c_str(), message.length(), 0);
}

void send_table_to_all(Room* room, vector<Card>& table, int who_played) {
  string message = "TABLE:\n";
  
  for (int i = 0; i < table.size(); i++) {
    message += "Jogador " + to_string(i) + ": " + to_string(table[i].rank) + " de " + string(table[i].suit) + "\n";
  }
  
  message += "Jogou: Jogador " + to_string(who_played) + "\n";
  
  for (int i = 0; i < TOTAL_PER_ROOM; i++) {
    send(room->players[i], message.c_str(), message.length(), 0);
  }
}

void close_player_connection(int socket_fd) {
  close(socket_fd);
}

int get_card_strength(Card& card) {
  for (int i = 0; i < DECK_SIZE; i++) {
    if (ordered_deck[i].rank == card.rank && strcmp(ordered_deck[i].suit, card.suit) == 0) {
      return i;
    }
  }
  return DECK_SIZE; 
}

int get_round_winner(vector<Card>& table) {
  if (table.size() != 2) {
    return -1; 
  }
  
  int strength_0 = get_card_strength(table[0]);
  int strength_1 = get_card_strength(table[1]);

  if (strength_0 < strength_1) {
    return 0;
  } else if (strength_1 < strength_0) {
    return 1; 
  } else {
    return -1; 
  }
}

void *room_thread(void* arg) {
  Room* room = (Room*)arg;

  while (true) {
    sem_wait(&room->start_round);
    
    cout << "Iniciando partida na sala " << room->id << endl;
    int client_to_play = 0;

    Player players[TOTAL_PER_ROOM];
    for (int i = 0; i < TOTAL_PER_ROOM; i++) {
      players[i].socket_fd = room->players[i];
      players[i].points = 0;
    }
    
    while(true) {
      vector<Card> unique_cards = deal_hand();
      vector<Card> table;

      for (int i = 0; i < TOTAL_PER_ROOM; i++) {
        players[i].hand = vector<Card>(unique_cards.begin() + i * 3, unique_cards.begin() + (i + 1) * 3);
        players[i].already_played.clear();
      
        send_hand_to_player(players[i].socket_fd, players[i].hand, players[i].already_played, players, TOTAL_PER_ROOM, table);
      }

      int rounds_won[TOTAL_PER_ROOM] = {0, 0};
      int first_player = client_to_play;
      int pending_tie_winner = -1; 
      
      for (int round = 0; round < 3; round++) {
        table.clear();

        for (int turn = 0; turn < TOTAL_PER_ROOM; turn++) {
          int current_player = client_to_play;

          send_hand_to_player(players[current_player].socket_fd, players[current_player].hand, players[current_player].already_played, players, TOTAL_PER_ROOM, table);
          send(players[current_player].socket_fd, "YOUR_TURN\n", 10, 0);
          
          char buffer[32];
          int bytes = recv(players[current_player].socket_fd, buffer, sizeof(buffer), 0);
          buffer[bytes] = '\0';
          
          int choice = atoi(buffer) - 1; 

          bool already_played = false;
          for (int played_idx : players[current_player].already_played) {
            if (played_idx == choice) {
              already_played = true;
              break;
            }
          }
          
          if (choice >= 0 && choice < players[current_player].hand.size() && !already_played) {
            table.push_back(players[current_player].hand[choice]);
            players[current_player].already_played.push_back(choice);
            
            send_table_to_all(room, table, current_player);
          }

          client_to_play = (current_player == 0) ? 1 : 0;
        }

        int round_winner = get_round_winner(table);
        
        if (round_winner != -1) {
          if (pending_tie_winner != -1) {
            rounds_won[round_winner]++;
            pending_tie_winner = -1;
          } else {
            rounds_won[round_winner]++;
          }
          
          client_to_play = round_winner;

          if (rounds_won[round_winner] == 2) {
            cout << "Jogador " << round_winner << " venceu a mão!" << endl;
            players[round_winner].points++;
            break;
          }
        } else {
          if (pending_tie_winner == -1) {
            pending_tie_winner = -2; 
          }
        }
      }

      if (rounds_won[0] == rounds_won[1]) {
        cout << "Empate total! Jogador " << first_player << " (que começou) venceu a mão!" << endl;
        players[first_player].points++;
      }
      
      int has_winner = 0;
      for (int i = 0; i < TOTAL_PER_ROOM; i++) {
        if (players[i].points >= 10) {
          has_winner = 1;
          cout << "Jogador " << i << " venceu a partida na sala " << room->id << "!" << endl;
          
          for (int j = 0; j < TOTAL_PER_ROOM; j++) {
            string win_message = "WINNER:\nJogador " + to_string(i) + " venceu a partida!\n";
            send(players[j].socket_fd, win_message.c_str(), win_message.length(), 0);
          }
          break;
        }
      }

      if (has_winner) break;
    }

    cout << "Partida finalizada na sala " << room->id << endl;
    for (int i = 0; i < TOTAL_PER_ROOM; i++) {
      close_player_connection(room->players[i]);
    }
    room->total_players = 0;
    room->players.clear();
  }
  return nullptr;
}

void run_server() {
  server_fd = init_main_socket();
  Room rooms[MAX_ROOMS];

  for (int i = 0; i < MAX_ROOMS; i++) {
    pthread_t tid;
    
    create_room(&rooms[i], tid); 
    
    if (pthread_create(&tid, nullptr, room_thread, &rooms[i]) != 0) {
      perror("Failed to create room thread");
      exit(EXIT_FAILURE);
    }

    pthread_detach(tid);
  }

  while (true) {
    if ((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
      perror("Accept failed");
      exit(EXIT_FAILURE);
    }
    
    int has_any_room = 0;
    int first_room_available = -1;

    for(int j = 0; j < MAX_ROOMS; j++) {
      if (!is_room_full(&rooms[j])) {
        has_any_room = 1;
        first_room_available = j;
        break;
      }
    }

    if (!has_any_room) {
      send(new_socket, NOT_AVAILABLE_MESSAGE, strlen(NOT_AVAILABLE_MESSAGE), 0);
      close(new_socket);
      continue;
    }
    
    Room* room = &rooms[first_room_available];
    room->players.push_back(new_socket);
    room->total_players++;
    send(new_socket, "Joined room successfully.\n", 28, 0);
    if (is_room_full(room)) {
      sem_post(&room->start_round);
    }
  }
}

int main() {
  run_server();
  return 0;
}