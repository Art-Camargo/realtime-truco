#include <arpa/inet.h>
#include <iostream>
#include <cstring>
#include <pthread.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <semaphore.h>
#include <vector>

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

void *room_thread(void* arg) {
  Room* room = (Room*)arg;
  while (true) {
    sem_wait(&room->start_round);
    // Game logic would go here
    // For simplicity, we just reset the room
    cout << "Teste room " << room->id << " starting a new round with players: " << endl;
    break;
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
    if (is_room_full(room)) {
      sem_post(&room->start_round);
    }

  }

}

int main() {
  run_server();
  return 0;
}