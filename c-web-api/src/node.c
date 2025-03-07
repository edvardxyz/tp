#include <ulfius.h>
#include <sqlite3.h>
#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "db.h"

int callback_get_nodes(const struct _u_request * request, struct _u_response * response, void * user_data) {

	const char * sql = NULL;
	const char * node = u_map_get(request->map_url, "node");
	json_t * json_array = NULL;

	if (node) {
		sql = "SELECT name, node FROM node WHERE node = ?;";
		json_array = fetch_query_results(sql, "%s", node);
	} else {
		sql = "SELECT name, node FROM node;";
		json_array = fetch_query_results(sql, "");
	}

	if (!json_array) {
		ulfius_set_json_body_response(response, 500, json_pack("{s:s}", "error", "Failed to fetch nodes"));
		return U_CALLBACK_CONTINUE;
	}

	// Convert the JSON array to a string for the response body
	char * json_str = json_dumps(json_array, JSON_INDENT(2));

	// Set a 200 OK status and send the JSON response
	ulfius_set_string_body_response(response, 200, json_str);

	// Clean up
	free(json_str);
	json_decref(json_array);

	return U_CALLBACK_CONTINUE;
}
