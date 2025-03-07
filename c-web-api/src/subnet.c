#include <ulfius.h>
#include <sqlite3.h>
#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "db.h"

int callback_post_subnets(const struct _u_request * request, struct _u_response * response, void * user_data) {
	json_error_t err;
	json_t * json_body = ulfius_get_json_body_request(request, &err);
	if (!json_body) {
		printf("error: %s", err.text);
		ulfius_set_string_body_response(response, 400, "Missing JSON body");
		return U_CALLBACK_CONTINUE;
	}

	json_t * json_name = json_object_get(json_body, "name");
	json_t * json_node = json_object_get(json_body, "node");
	json_t * json_mask = json_object_get(json_body, "mask");

	if (!json_name || !json_is_string(json_name) ||
		!json_node || !json_is_integer(json_node) ||
		!json_mask || !json_is_integer(json_mask)) {
		ulfius_set_string_body_response(response, 400, "Invalid or missing parameters");
		json_decref(json_body);
		return U_CALLBACK_CONTINUE;
	}

	const char * name = json_string_value(json_name);
	int node = json_integer_value(json_node);
	int mask = json_integer_value(json_mask);

	const char * sql = "INSERT INTO subnet (name, node, mask) VALUES (?, ?, ?);";
	sqlite3_stmt * stmt = NULL;
	int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Failed to prepare insert statement: %s\n", sqlite3_errmsg(db));
		ulfius_set_string_body_response(response, 500, "Database error");
		json_decref(json_body);
		return U_CALLBACK_CONTINUE;
	}

	sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
	sqlite3_bind_int(stmt, 2, node);
	sqlite3_bind_int(stmt, 3, mask);

	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE) {
		fprintf(stderr, "Failed to execute insert: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		ulfius_set_string_body_response(response, 500, "Database error");
		json_decref(json_body);
		return U_CALLBACK_CONTINUE;
	}

	sqlite3_finalize(stmt);
	json_decref(json_body);

	ulfius_set_string_body_response(response, 200, "Subnet inserted successfully");

	return U_CALLBACK_CONTINUE;
}

int callback_get_subnets(const struct _u_request * request, struct _u_response * response, void * user_data) {
	const char * sql = "SELECT id, name, node, mask, created FROM subnet;";

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
