#pragma once

#include <string>


typedef struct  {
    int rank;      // número: 1..7, 10..12
    const char* suit; // "Espadas", "Paus", "Ouros", "Copas"
} Card;

static const Card ordered_deck[] = {
    {1, "Espadas"}, // Ás de Espadas
    {1, "Paus"},    // Ás de Paus
    {7, "Espadas"},
    {7, "Ouros"},
    {3, "Espadas"}, {3, "Paus"}, {3, "Ouros"}, {3, "Copas"},
    {2, "Espadas"}, {2, "Paus"}, {2, "Ouros"}, {2, "Copas"},
    {1, "Ouros"}, {1, "Copas"},
    {12, "Espadas"}, {12, "Paus"}, {12, "Ouros"}, {12, "Copas"},
    {11, "Espadas"}, {11, "Paus"}, {11, "Ouros"}, {11, "Copas"},
    {10, "Espadas"}, {10, "Paus"}, {10, "Ouros"}, {10, "Copas"},
    {7, "Paus"}, {7, "Copas"},
    {6, "Espadas"}, {6, "Paus"}, {6, "Ouros"}, {6, "Copas"},
    {5, "Espadas"}, {5, "Paus"}, {5, "Ouros"}, {5, "Copas"},
    {4, "Espadas"}, {4, "Paus"}, {4, "Ouros"}, {4, "Copas"}
};

static const int DECK_SIZE = sizeof(ordered_deck) / sizeof(ordered_deck[0]);



