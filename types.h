#ifndef TYPES_H
#define TYPES_H

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

inline int get_card_rank(const string& card) {
  static map<string, int> ranking = {
    {"AE", 14}, {"AP", 13},
    {"7E", 12}, {"7O", 11},
    {"3C", 10}, {"3E", 10}, {"3P", 10}, {"3O", 10},
    {"2C", 9},  {"2E", 9},  {"2P", 9},  {"2O", 9},
    {"AC", 8},  {"AO", 8},
    {"KC", 7},  {"KE", 7},  {"KP", 7},  {"KO", 7},
    {"QC", 6},  {"QE", 6},  {"QP", 6},  {"QO", 6},
    {"JC", 5},  {"JE", 5},  {"JP", 5},  {"JO", 5},
    {"7C", 4},  {"7P", 4},
    {"6C", 3},  {"6E", 3},  {"6P", 3},  {"6O", 3},
    {"5C", 2},  {"5E", 2},  {"5P", 2},  {"5O", 2},
    {"4C", 1},  {"4E", 1},  {"4P", 1},  {"4O", 1}
  };
  
  if (ranking.find(card) != ranking.end()) {
    return ranking[card];
  }
  return 0; 
}

#endif
