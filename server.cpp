#include <arpa/inet.h>
#include <iostream>
#include <cstring>
#include <pthread.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <semaphore.h>
#include <vector>
#include "types.h"  

#define PORT 8080
#define TOTAL_PER_ROOM 2
#define MAX_ROOMS 2
#define NOT_AVAILABLE_MESSAGE "No rooms available. Try again later.\n"

using namespace std;

typedef struct {
  vector<string> cards;
  vector<int> cards_already_played; // (para evitar que a mesma carta seja jogada mais de uma vez e para retornar na mesa para os jogadores da sala verem as ja jogadas)
} Hand;

typedef struct {
  int socket_player;
  Hand hand;
} Player;

typedef struct {
  pthread_t id;
  sem_t start_round;
  vector<Player> players;
  int total_players;
} Room;

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

void join_room(Room* room, int socket_player) {
  Player player;
  player.socket_player = socket_player;
  room->players.push_back(player);
  room->total_players++;
  send(socket_player, "Joined room successfully.\n", 28, 0);
}

void start_room_round(Room* room) {
  sem_post(&room->start_round);
}

void *room_thread(void* arg) {
  Room* room = (Room*)arg;

  while (1) {
    sem_wait(&room->start_round);
    cout << "Starting round in room with " << room->total_players << " players." << endl;
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