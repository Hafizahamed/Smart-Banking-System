#ifndef ROUTES_H
#define ROUTES_H

#include <mongoose.h>

void routes_init(void);
void routes_handle_http(struct mg_connection *c, int ev, void *ev_data);

#endif // ROUTES_H
