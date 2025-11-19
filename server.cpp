#include <arpa/inet.h>
#include <iostream>
#include <cstring>
#include <pthread.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <semaphore.h>
#include <vector>
#include <map>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include "types.h"  

#define PORT 8080
#define TOTAL_PER_ROOM 2
#define MAX_ROOMS 2
#define NOT_AVAILABLE_MESSAGE "No rooms available. Try again later.\n"
#define WIN_SCORE 12

using namespace std;

typedef struct {
  vector<string> cards;
  vector<int> cards_already_played;
} Hand;

typedef struct {
  int socket_player;
  Hand hand;
  int points;
  int envido_points;
  bool has_flor;
} Player;

typedef struct {
  pthread_t id;
  sem_t start_round;
  vector<Player> players;
  int total_players;
} Room;

typedef struct {
  int hand_value;      
  bool truco_called;
  bool retruco_called;
  bool vale4_called;
  int last_raiser;     
} TrucoState;

typedef struct {
  bool envido_called;
  bool real_envido_called;
  bool falta_envido_called;
  int envido_value;    
  int last_caller;    
} EnvidoState;

int server_fd, new_socket;
struct sockaddr_in address;
int addrlen = sizeof(address);


void* room_thread(void* arg);
void send_message(int socket_fd, MessageType type, const string& text);

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

void build_rooms(Room rooms[MAX_ROOMS]) {
  for (int i = 0; i < MAX_ROOMS; i++) {
    pthread_t tid;
    
    create_room(&rooms[i], tid); 
    
    if (pthread_create(&tid, nullptr, room_thread, &rooms[i]) != 0) {
      perror("Failed to create room thread");
      exit(EXIT_FAILURE);
    }

    pthread_detach(tid);
  }
}

void get_free_room(Room rooms[MAX_ROOMS], int* room_index) {
  for (int i = 0; i < MAX_ROOMS; i++) {
    if (!is_room_full(&rooms[i])) {
      *room_index = i;
      return;
    }
  }
  *room_index = -1;
}

void send_message(int socket_fd, MessageType type, const string& text) {
  Message msg;
  msg.type = type;
  strncpy(msg.text, text.c_str(), MESSAGE_SIZE - 1);
  msg.text[MESSAGE_SIZE - 1] = '\0';
  send(socket_fd, &msg, sizeof(Message), 0);
}

void join_room(Room* room, int socket_player) {
  Player player;
  player.socket_player = socket_player;
  player.points = 0;
  player.envido_points = 0;
  player.has_flor = false;
  
  room->players.push_back(player);
  room->total_players++;
  
  send_message(socket_player, MSG_ROOM_JOIN, "Você entrou na sala!\n");
}

void start_room_round(Room* room) {
  sem_post(&room->start_round);
}

string get_random_card(vector<string>& used_cards) {
  const string suits[] = {"C", "E", "P", "O"}; 
  const string values[] = {"4", "5", "6", "7", "J", "Q", "K", "A", "2", "3"}; 
  
  string card;
  do {
    string value = values[rand() % 10];
    string suit = suits[rand() % 4];
    card = value + suit;
  } while (find(used_cards.begin(), used_cards.end(), card) != used_cards.end());
  
  used_cards.push_back(card);
  return card;
}

void deal_cards(Room* room) {
  vector<string> used_cards;
  
  for (int i = 0; i < TOTAL_PER_ROOM; i++) {
    room->players[i].hand.cards.clear();
    room->players[i].hand.cards_already_played.clear();
    
    for (int j = 0; j < 3; j++) {
      string card = get_random_card(used_cards);
      room->players[i].hand.cards.push_back(card);
    }
  }
}

int calculate_envido(vector<string>& cards) {
  map<char, vector<int>> suits_map;
  
  for (const string& card : cards) {
    char suit = card[card.length() - 1];
    int value = 0;
    
    if (card[0] == 'A') value = 1;
    else if (card[0] >= '2' && card[0] <= '7') value = card[0] - '0';
    else value = 0;
    
    suits_map[suit].push_back(value);
  }
  
  int max_envido = 0;
  for (auto& pair : suits_map) {
    if (pair.second.size() >= 2) {
      sort(pair.second.rbegin(), pair.second.rend());
      int envido = 20 + pair.second[0] + pair.second[1];
      max_envido = max(max_envido, envido);
    } else if (pair.second.size() == 1) {
      max_envido = max(max_envido, pair.second[0]);
    }
  }
  
  return max_envido;
}

bool check_flor(vector<string>& cards) {
  if (cards.size() < 3) return false;
  
  char first_suit = cards[0][cards[0].length() - 1];
  for (size_t i = 1; i < cards.size(); i++) {
    if (cards[i][cards[i].length() - 1] != first_suit) {
      return false;
    }
  }
  return true;
}

void send_scoreboard(Room* room) {
  string scoreboard = "\n=== PLACAR ===\n";
  for (int i = 0; i < TOTAL_PER_ROOM; i++) {
    scoreboard += "Jogador " + to_string(i) + ": " + to_string(room->players[i].points) + " pontos\n";
  }
  scoreboard += "\n";
  
  for (int i = 0; i < TOTAL_PER_ROOM; i++) {
    send_message(room->players[i].socket_player, MSG_SCOREBOARD, scoreboard);
  }
}

void send_hand(Player* player, const string played_cards[], int cards_played) {
  string full_msg = "\n=== MESA ===\n";
  if (cards_played == 0) {
    full_msg += "(vazia)\n";
  } else {
    for (int i = 0; i < cards_played; i++) {
      full_msg += "Jogador " + to_string(i) + ": " + played_cards[i] + "\n";
    }
  }
  full_msg += "\n=== SUAS CARTAS ===\n";
  
  for (size_t i = 0; i < player->hand.cards.size(); i++) {
    bool played = find(player->hand.cards_already_played.begin(), 
                      player->hand.cards_already_played.end(), i) != 
                  player->hand.cards_already_played.end();
    
    if (!played) {
      full_msg += to_string(i + 1) + ". " + player->hand.cards[i] + "\n";
    }
  }
  full_msg += "\n";

  send_message(player->socket_player, MSG_HAND, full_msg);
}

int compare_cards(const string& card1, const string& card2) {
  int rank1 = get_card_rank(card1);
  int rank2 = get_card_rank(card2);
  
  if (rank1 > rank2) return 0;
  if (rank2 > rank1) return 1;
  return -1;
}

void *room_thread(void* arg) {
  Room* room = (Room*)arg;
  srand(time(nullptr));

  while (1) {
    sem_wait(&room->start_round);
    cout << "Starting game in room with " << room->total_players << " players." << endl;

    for (int i = 0; i < TOTAL_PER_ROOM; i++) {
      room->players[i].points = 0;
    }
    
    int first_player = 0; 
    while (true) {
      deal_cards(room);

      for (int i = 0; i < TOTAL_PER_ROOM; i++) {
        room->players[i].envido_points = calculate_envido(room->players[i].hand.cards);
        room->players[i].has_flor = check_flor(room->players[i].hand.cards);
      }
      
      send_scoreboard(room);

      string empty_table[TOTAL_PER_ROOM];
      for (int i = 0; i < TOTAL_PER_ROOM; i++) {
        send_hand(&room->players[i], empty_table, 0);
        if (room->players[i].has_flor) {
          send_message(room->players[i].socket_player, MSG_TEXT, "VOCÊ TEM FLOR!\n");
        }
      }
      
      TrucoState truco_state = {1, false, false, false, -1};
      EnvidoState envido_state = {false, false, false, 0, -1};
      
      int rounds_won[TOTAL_PER_ROOM] = {0, 0};
      int current_player = first_player;
      bool first_round = true;
      bool hand_finished = false;

      for (int round = 0; round < 3 && !hand_finished; round++) {
        string played_cards[TOTAL_PER_ROOM];
        int card_indices[TOTAL_PER_ROOM];
        int cards_played = 0;
        
        for (int turn = 0; turn < TOTAL_PER_ROOM && !hand_finished; turn++) {
          send_hand(&room->players[current_player], played_cards, cards_played);
          
          send_message(room->players[current_player].socket_player, MSG_YOUR_TURN, 
                      "Sua vez! Digite:\n- Número da carta (1-3)\n- 'T' para Truco\n- 'E' para Envido (só 1ª rodada)\n");
          
          Message response;
          recv(room->players[current_player].socket_player, &response, sizeof(Message), 0);
          
          string input = response.text;

          if (first_round && (input == "E" || input == "e")) {
            int opponent = (current_player == 0) ? 1 : 0;
            int new_envido_value = 2;
            string envido_name = "ENVIDO";
            
            if (!envido_state.envido_called) {
              envido_state.envido_called = true;
              new_envido_value = 2;
            } else if (!envido_state.real_envido_called) {
              envido_state.real_envido_called = true;
              new_envido_value = envido_state.envido_value + 3; 
              envido_name = "REAL ENVIDO";
            } else if (!envido_state.falta_envido_called) {
              envido_state.falta_envido_called = true;
              new_envido_value = WIN_SCORE - room->players[opponent].points;
              envido_name = "FALTA ENVIDO";
            } else {
              send_message(room->players[current_player].socket_player, MSG_TEXT, 
                          "Envido já está no máximo! Jogue uma carta.\n");
              turn--;
              continue;
            }

            string envido_msg = "💎 Jogador " + to_string(current_player) + " cantou " + envido_name + "!\n";
            for (int i = 0; i < TOTAL_PER_ROOM; i++) {
              send_message(room->players[i].socket_player, MSG_TEXT, envido_msg);
            }

            send_message(room->players[opponent].socket_player, MSG_YOUR_TURN, 
                        "Aceitar? Digite:\n- 'S' para aceitar\n- 'N' para correr\n- 'E' para aumentar\n");
            
            Message envido_response;
            recv(room->players[opponent].socket_player, &envido_response, sizeof(Message), 0);
            string resp = envido_response.text;
            
            if (resp == "N" || resp == "n") {
              room->players[current_player].points += 1;
              
              string run_msg = "Jogador " + to_string(opponent) + " não quis! Jogador " + 
                             to_string(current_player) + " ganha 1 ponto.\n";
              for (int i = 0; i < TOTAL_PER_ROOM; i++) {
                send_message(room->players[i].socket_player, MSG_TEXT, run_msg);
              }
              
              send_scoreboard(room);
            } else if (resp == "E" || resp == "e") {
              send_message(room->players[opponent].socket_player, MSG_TEXT, 
                          "Você aumentou o Envido! Voltando...\n");
              turn--;
              current_player = opponent;
              continue;
            } else {
              envido_state.envido_value = new_envido_value;
              
              int envido_p0 = room->players[0].envido_points;
              int envido_p1 = room->players[1].envido_points;
              
              string compare_msg = "🔍 Comparando Envido:\n";
              compare_msg += "Jogador 0: " + to_string(envido_p0) + " pontos\n";
              compare_msg += "Jogador 1: " + to_string(envido_p1) + " pontos\n";
              
              int envido_winner;
              if (envido_p0 > envido_p1) {
                envido_winner = 0;
              } else if (envido_p1 > envido_p0) {
                envido_winner = 1;
              } else {
                envido_winner = first_player;
                compare_msg += "(Empate - mão ganha)\n";
              }
              
              room->players[envido_winner].points += envido_state.envido_value;
              compare_msg += "Jogador " + to_string(envido_winner) + " venceu e ganhou " + 
                           to_string(envido_state.envido_value) + " pontos!\n";
              
              for (int i = 0; i < TOTAL_PER_ROOM; i++) {
                send_message(room->players[i].socket_player, MSG_TEXT, compare_msg);
              }
              
              send_scoreboard(room);

              if (room->players[envido_winner].points >= WIN_SCORE) {
                string winner_msg = "\n🏆 JOGADOR " + to_string(envido_winner) + " VENCEU O JOGO COM ENVIDO! 🏆\n";
                for (int j = 0; j < TOTAL_PER_ROOM; j++) {
                  send_message(room->players[j].socket_player, MSG_WINNER, winner_msg);
                }
                
                for (int j = 0; j < TOTAL_PER_ROOM; j++) {
                  close(room->players[j].socket_player);
                }
                
                room->total_players = 0;
                room->players.clear();
                goto game_over;
              }
            }
            
            turn--;
            continue;
          }

          if (input == "T" || input == "t") {
            int opponent = (current_player == 0) ? 1 : 0;
            int new_value = truco_state.hand_value;
            string raise_name;

            if (!truco_state.truco_called) {
              new_value = 2;
              raise_name = "TRUCO";
              truco_state.truco_called = true;
            } else if (!truco_state.retruco_called) {
              new_value = 3;
              raise_name = "RETRUCO";
              truco_state.retruco_called = true;
            } else if (!truco_state.vale4_called) {
              new_value = 4;
              raise_name = "VALE 4";
              truco_state.vale4_called = true;
            } else {
              send_message(room->players[current_player].socket_player, MSG_TEXT, 
                          "Já está no máximo (Vale 4)! Jogue uma carta.\n");
              turn--;
              continue;
            }

            string truco_msg = "🔥 Jogador " + to_string(current_player) + " pediu " + raise_name + 
                             " (vale " + to_string(new_value) + " pontos)!\n";
            for (int i = 0; i < TOTAL_PER_ROOM; i++) {
              send_message(room->players[i].socket_player, MSG_TEXT, truco_msg);
            }
            
            send_message(room->players[opponent].socket_player, MSG_YOUR_TURN, 
                        "Aceitar? Digite:\n- 'S' para aceitar\n- 'N' para correr\n- 'T' para aumentar\n");
            
            Message truco_response;
            recv(room->players[opponent].socket_player, &truco_response, sizeof(Message), 0);
            string resp = truco_response.text;
            
            if (resp == "N" || resp == "n") {
              room->players[current_player].points += truco_state.hand_value;
              
              string run_msg = "Jogador " + to_string(opponent) + " correu! Jogador " + 
                             to_string(current_player) + " ganha " + 
                             to_string(truco_state.hand_value) + " ponto(s)!\n";
              for (int i = 0; i < TOTAL_PER_ROOM; i++) {
                send_message(room->players[i].socket_player, MSG_TEXT, run_msg);
              }
              
              hand_finished = true;
              break;
            } else if (resp == "T" || resp == "t") {
              send_message(room->players[opponent].socket_player, MSG_TEXT, 
                          "Você aumentou! Voltando para o adversário...\n");
              turn--;
              current_player = opponent;
              continue;
            } else {
              truco_state.hand_value = new_value;
              truco_state.last_raiser = current_player;
              
              string accept_msg = "Jogador " + to_string(opponent) + " aceitou! Mão vale " + 
                                to_string(new_value) + " ponto(s).\n";
              for (int i = 0; i < TOTAL_PER_ROOM; i++) {
                send_message(room->players[i].socket_player, MSG_TEXT, accept_msg);
              }
            }
            
            turn--;
            continue;
          }

          int card_choice = atoi(input.c_str()) - 1;
          
          if (card_choice < 0 || card_choice >= 3) {
            send_message(room->players[current_player].socket_player, MSG_TEXT, 
                        "Carta inválida! Escolha 1, 2 ou 3.\n");
            turn--;
            continue;
          }

          if (find(room->players[current_player].hand.cards_already_played.begin(),
                  room->players[current_player].hand.cards_already_played.end(), 
                  card_choice) != room->players[current_player].hand.cards_already_played.end()) {
            send_message(room->players[current_player].socket_player, MSG_TEXT, 
                        "Você já jogou essa carta!\n");
            turn--;
            continue;
          }
          
          room->players[current_player].hand.cards_already_played.push_back(card_choice);
          played_cards[current_player] = room->players[current_player].hand.cards[card_choice];
          card_indices[current_player] = card_choice;
          cards_played++;

          for (int i = 0; i < TOTAL_PER_ROOM; i++) {
            send_hand(&room->players[i], played_cards, cards_played);
          }
          
          current_player = (current_player == 0) ? 1 : 0;
        }
        
        first_round = false;

        int round_winner = compare_cards(played_cards[0], played_cards[1]);
        
        if (round_winner != -1) {
          rounds_won[round_winner]++;
          current_player = round_winner;
          
          string msg = "Jogador " + to_string(round_winner) + " venceu a rodada!\n";
          for (int i = 0; i < TOTAL_PER_ROOM; i++) {
            send_message(room->players[i].socket_player, MSG_TEXT, msg);
          }

          if (rounds_won[round_winner] == 2) {
            room->players[round_winner].points += truco_state.hand_value;
            
            string winner_msg = "Jogador " + to_string(round_winner) + 
                              " venceu a mão e ganhou " + to_string(truco_state.hand_value) + " ponto(s)!\n";
            for (int i = 0; i < TOTAL_PER_ROOM; i++) {
              send_message(room->players[i].socket_player, MSG_TEXT, winner_msg);
            }
            break;
          }
        } else {
          string msg = "Rodada empatada!\n";
          for (int i = 0; i < TOTAL_PER_ROOM; i++) {
            send_message(room->players[i].socket_player, MSG_TEXT, msg);
          }
        }
      }

      if (rounds_won[0] == rounds_won[1]) {
        room->players[first_player].points += truco_state.hand_value;
        
        string msg = "Empate total! Jogador " + to_string(first_player) + 
                   " (que começou) venceu a mão!\n";
        for (int i = 0; i < TOTAL_PER_ROOM; i++) {
          send_message(room->players[i].socket_player, MSG_TEXT, msg);
        }
      }
      
      send_scoreboard(room);
      for (int i = 0; i < TOTAL_PER_ROOM; i++) {
        if (room->players[i].points >= WIN_SCORE) {
          string winner_msg = "\n🏆 JOGADOR " + to_string(i) + " VENCEU O JOGO! 🏆\n";
          for (int j = 0; j < TOTAL_PER_ROOM; j++) {
            send_message(room->players[j].socket_player, MSG_WINNER, winner_msg);
          }

          for (int j = 0; j < TOTAL_PER_ROOM; j++) {
            close(room->players[j].socket_player);
          }
          
          room->total_players = 0;
          room->players.clear();
          goto game_over;
        }
      }

      first_player = (first_player == 0) ? 1 : 0;
    }
    
    game_over:
    cout << "Game finished in room." << endl;
  }
  
  return nullptr;
}

void run_server() {
  server_fd = init_main_socket();
  Room rooms[MAX_ROOMS];

  build_rooms(rooms);

  while (true) {
    if ((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
      perror("Accept failed");
      exit(EXIT_FAILURE);
    }
  
    int first_room_available = -1;
    get_free_room(rooms, &first_room_available);

    if (first_room_available == -1) {
      send(new_socket, NOT_AVAILABLE_MESSAGE, strlen(NOT_AVAILABLE_MESSAGE), 0);
      close(new_socket);
      continue;
    }
    
    Room* room = &rooms[first_room_available];
    join_room(room, new_socket);

    if (is_room_full(room)) {
      start_room_round(room);
    }
  }

  close(server_fd);
}

int main() {
  run_server();
  return 0;
}