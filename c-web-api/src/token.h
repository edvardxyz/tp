#pragma once

char * create_jwt_hs256(const char * username);
int authenticate(http_s * request);
