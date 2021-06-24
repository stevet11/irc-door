#ifndef RENDER_H
#define RENDER_H

#include "door.h"
#include "irc.h"
#include <string>
#include <vector>

extern std::string timestamp_format;

void render(message_stamp &irc_msg, door::Door &door, ircClient &irc);
void stamp(std::time_t &stamp, door::Door &door);

#endif