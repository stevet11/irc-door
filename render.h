#ifndef RENDER_H
#define RENDER_H

#include "door.h"
#include "irc.h"
#include <string>
#include <vector>

void render(message_stamp &irc_msg, door::Door &door, ircClient &irc);

#endif