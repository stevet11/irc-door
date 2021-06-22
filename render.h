#ifndef RENDER_H
#define RENDER_H

#include "door.h"
#include "irc.h"
#include <string>
#include <vector>

void render(std::vector<std::string> irc_msg, door::Door &door, ircClient &irc);

#endif