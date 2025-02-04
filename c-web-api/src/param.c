#include <ulfius.h>
#include <sqlite3.h>
#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <param/param.h>
#include <param/param_list.h>

#include "db.h"

int param_list_init() {

	const char * sql =
		"SELECT p.param_id, n.node, p.type, p.mask, p.size, p.vmem "
		"FROM param p "
		"JOIN param_node pn ON p.id = pn.param_id "
		"JOIN node n ON pn.node_id = n.id;";

	sqlite3_stmt * stmt;
	int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
		return 1;
	}

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		// Map columns to variables.
		int param_id = sqlite3_column_int(stmt, 0);
		int node_val = sqlite3_column_int(stmt, 1);
		int type = sqlite3_column_int(stmt, 2);
		int mask = sqlite3_column_int(stmt, 3);
		int size = sqlite3_column_int(stmt, 4);
		char * name = (char *)sqlite3_column_text(stmt, 5);
		int vmem = sqlite3_column_int(stmt, 6);

		param_t * param = param_list_create_remote(
			param_id,
			node_val,
			type,
			mask,
			size,
			name,
			NULL,
			NULL,
			vmem);

		if (!param) {
			fprintf(stderr, "Failed to create parameter for id: %d on node: %d\n",
					param_id, node_val);
		}
		param_list_add(param);
	}

	sqlite3_finalize(stmt);
	return 0;
}

int callback_get_params(const struct _u_request * request, struct _u_response * response, void * user_data) {
	const char * sql =
		"SELECT p.param_id, n.node, p.type, p.mask, p.size, p.vmem "
		"FROM param p "
		"JOIN param_node pn ON p.id = pn.param_id "
		"JOIN node n ON pn.node_id = n.id;";

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
