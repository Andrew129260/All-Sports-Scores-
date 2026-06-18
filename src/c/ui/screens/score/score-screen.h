#pragma once
#include "pebble.h"
#include "../../../data/model/models.h" // Or wherever your Game struct is defined

// The global pointer we just created in score-screen.c
extern Game *s_game;

// Function prototypes
void show_score_screen(Game *game);
void hide_score_screen(void);
