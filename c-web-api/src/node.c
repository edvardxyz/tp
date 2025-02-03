#include <ulfius.h>
#include <sqlite3.h>
#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "db.h"

int callback_get_nodes(const struct _u_request * request, struct _u_response * response, void * user_data) {
	const char * sql = "SELECT id, name, node, FROM node;";

	json_t * json_array = fetch_query_results(sql, "");

	// Convert the JSON array to a string for the response body
	char * json_str = json_dumps(json_array, JSON_INDENT(2));

	// Set a 200 OK status and send the JSON response
	ulfius_set_string_body_response(response, 200, json_str);

	// Clean up
	free(json_str);
	json_decref(json_array);

	return U_CALLBACK_CONTINUE;
}
