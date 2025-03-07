#pragma once
#include <ulfius.h>

int callback_auth(const struct _u_request * request, struct _u_response * response, void * user_data);
int callback_login(const struct _u_request * request, struct _u_response * response, void * user_data);
int callback_create_user(const struct _u_request * request, struct _u_response * response, void * user_data);
int callback_refresh(const struct _u_request * request, struct _u_response * response, void * user_data);


