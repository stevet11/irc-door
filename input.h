#ifndef INPUT_H
#define INPUT_H

#include "door.h"
#include "irc.h"

extern bool allow_part;
extern bool allow_join;
extern int ms_input_delay;

void clear_input(door::Door &d);
void restore_input(door::Door &d);
void parse_input(door::Door &door, ircClient &irc);

bool check_for_input(door::Door &d, ircClient &irc);

#endif