#include <map>
#include <string>
#define MESSAGE_SIZE 256

using namespace std;

typedef enum {
  MSG_TEXT,           
  MSG_HAND,           
  MSG_TABLE,         
  MSG_SCOREBOARD,     
  MSG_YOUR_TURN,      
  MSG_WINNER,         
  MSG_ROOM_JOIN,      
  MSG_GAME_ACTION     
} MessageType;

typedef struct {
  MessageType type;
  char text[MESSAGE_SIZE];
} Message;

map<string, int> ranking = {
  {"AE", 14}, {"AP", 13}, // as de espada e paus
  {"7E", 12}, {"7O", 11}, // sete de espada e ouro
  {"3C", 10}, {"3E", 10}, {"3P", 10}, {"3O", 10}, // todos os três
  {"2C", 9},  {"2E", 9},  {"2P", 9},  {"2O", 9}, // todos os dois
  {"AC", 8},  {"AO", 8}, // as de copas e ouros
  {"KC", 7},  {"KE", 7},  {"KP", 7},  {"KO", 7}, // todos os numeros 12
  {"QC", 6},  {"QE", 6},  {"QP", 6},  {"QO", 6}, // todas os numeros 11
  {"JC", 5},  {"JE", 5},  {"JP", 5},  {"JO", 5}, // todas os numeros 10
  {"7C", 4},  {"7P", 4}, // sete de copas e paus
  {"6C", 3},  {"6E", 3},  {"6P", 3},  {"6O", 3}, // todos os seis
  {"5C", 2},  {"5E", 2},  {"5P", 2},  {"5O", 2}, // todos os cinco
  {"4C", 1},  {"4E", 1},  {"4P", 1},  {"4O", 1} // todos os quatro
};

int get_card_rank(const string& card) {
  if (ranking.find(card) != ranking.end()) {
    return ranking[card];
  }
  return 0; 
}

string get_randomic_card() {
  // para gerar cartas aleatórias
  srand(time(nullptr));
  const string suits[] = {"C", "E", "P", "O"}; 
  const string values[] = {"4", "5", "6", "7", "J", "Q", "K", "A", "2", "3"}; 

  string suit = suits[rand() % 4];
  string value = values[rand() % 10];

  return value + suit;
}