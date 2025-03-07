#include <ulfius.h>
#include <sqlite3.h>
#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <param/param.h>
#include <param/param_list.h>
#include <string.h>

#include "db.h"

/**
 * decode a u_map into a string
 */
char * print_map(const struct _u_map * map) {
	char *line, *to_return = NULL;
	const char **keys, *value;
	int len, i;
	if (map != NULL) {
		keys = u_map_enum_keys(map);
		for (i = 0; keys[i] != NULL; i++) {
			value = u_map_get(map, keys[i]);
			len = snprintf(NULL, 0, "key is %s, value is %s", keys[i], value);
			line = o_malloc((size_t)(len + 1));
			snprintf(line, (size_t)(len + 1), "key is %s, value is %s", keys[i], value);
			if (to_return != NULL) {
				len = (int)(o_strlen(to_return) + o_strlen(line) + 1);
				to_return = o_realloc(to_return, (size_t)(len + 1));
				if (o_strlen(to_return) > 0) {
					strcat(to_return, "\n");
				}
			} else {
				to_return = o_malloc((o_strlen(line) + 1));
				to_return[0] = 0;
			}
			strcat(to_return, line);
			o_free(line);
		}
		return to_return;
	} else {
		return NULL;
	}
}

int param_list_init() {

	const char * sql =
		"SELECT p.param_id, n.node, p.type, p.mask, p.size, p.vmem "
		"FROM param p "
		"JOIN param_node pn ON p.id = pn.param_id "
		"JOIN node n ON pn.node_id = n.node;";

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
			continue;
		}
		param_list_add(param);
	}

	sqlite3_finalize(stmt);
	return 0;
}

int callback_get_params(const struct _u_request * request, struct _u_response * response, void * user_data) {

	const char * sql = NULL;
	const char * node = u_map_get(request->map_url, "node");
	const char * paramid = u_map_get(request->map_url, "paramid");
	json_t * json_array = NULL;

	if (node) {
		if (paramid) {
			sql =
				"SELECT p.param_id, n.node, p.type, p.mask, p.size, p.vmem, p.name "
				"FROM param p "
				"JOIN param_node pn ON p.id = pn.param_id "
				"JOIN node n ON pn.node_id = n.node "
				"WHERE n.node = ? AND p.param_id = ?;";
			json_array = fetch_query_results(sql, "%s%s", node, paramid);
		} else {
			sql =
				"SELECT p.param_id, n.node, p.type, p.mask, p.size, p.vmem, p.name "
				"FROM param p "
				"JOIN param_node pn ON p.id = pn.param_id "
				"JOIN node n ON pn.node_id = n.node "
				"WHERE n.node = ?;";
			json_array = fetch_query_results(sql, "%s", node);
		}
	} else {
		ulfius_set_json_body_response(response, 400, json_pack("{s:s}", "error", "Node not specified"));
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

// returns 0 for integer types, 1 for float types, -1 for unsupported types
int param_type_to_db_type(int type) {
	switch (type) {
		case PARAM_TYPE_UINT8:
		case PARAM_TYPE_UINT16:
		case PARAM_TYPE_UINT32:
		case PARAM_TYPE_UINT64:
		case PARAM_TYPE_INT8:
		case PARAM_TYPE_INT16:
		case PARAM_TYPE_INT32:
		case PARAM_TYPE_INT64:
		case PARAM_TYPE_XINT8:
		case PARAM_TYPE_XINT16:
		case PARAM_TYPE_XINT32:
		case PARAM_TYPE_XINT64:
			return 0;
		case PARAM_TYPE_FLOAT:
		case PARAM_TYPE_DOUBLE:
			return 1;
		default:
			return -1;
	}
}

/*
CREATE TABLE param (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	param_id INTEGER NOT NULL,
	name TEXT NOT NULL,
	size INTEGER NOT NULL,
	mask INTEGER,
	vmem INTEGER,
	type INTEGER NOT NULL,
	created INTEGER NOT NULL DEFAULT (strftime('%s','now')),
	UNIQUE (param_id, name)
);

CREATE TABLE param_node (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	param_id INTEGER NOT NULL,
	node_id INTEGER NOT NULL,
	created INTEGER NOT NULL DEFAULT (strftime('%s','now')),
	FOREIGN KEY(param_id) REFERENCES param(id),
	FOREIGN KEY(node_id) REFERENCES node(node),
	UNIQUE (param_id, node_id)
);

CREATE TABLE param_int (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	value INTEGER NOT NULL,
	idx INTEGER NOT NULL,
	param_node_id INTEGER NOT NULL,
	time_sec INTEGER NOT NULL,
	created INTEGER NOT NULL DEFAULT (strftime('%s','now')),
	FOREIGN KEY(param_node_id) REFERENCES param_node(id)
);

CREATE TABLE param_real (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	value REAL NOT NULL,
	idx INTEGER NOT NULL,
	param_node_id INTEGER NOT NULL,
	time_sec INTEGER NOT NULL,
	created INTEGER NOT NULL DEFAULT (strftime('%s','now')),
	FOREIGN KEY(param_node_id) REFERENCES param_node(id)
);
*/

int callback_get_values(const struct _u_request * request, struct _u_response * response, void * user_data) {

	const char * sql = NULL;
	const char * node = u_map_get(request->map_url, "node");         // required
	const char * param_id = u_map_get(request->map_url, "paramid");  // required
	const char * limit = u_map_get(request->map_url, "limit");       // required
	const char * from = u_map_get(request->map_url, "from");         // optional
	const char * to = u_map_get(request->map_url, "to");             // optional

	json_t * json_array = NULL;

	if (!node || !param_id || !limit) {
		ulfius_set_json_body_response(response, 400, json_pack("{s:s}", "error", "Missing required parameters"));
		return U_CALLBACK_CONTINUE;
	}

	const char * sql_type =
		"SELECT type "
		"FROM param p JOIN param_node pn ON p.id = pn.param_id "
		"WHERE pn.node_id = ? AND p.param_id = ?;";

	sqlite3_stmt * stmt_type = NULL;
	int param_type = -1;

	if (sqlite3_prepare_v2(db, sql_type, -1, &stmt_type, NULL) == SQLITE_OK) {
		sqlite3_bind_text(stmt_type, 1, node, -1, SQLITE_STATIC);
		sqlite3_bind_text(stmt_type, 2, param_id, -1, SQLITE_STATIC);
		if (sqlite3_step(stmt_type) == SQLITE_ROW) {
			param_type = param_type_to_db_type(sqlite3_column_int(stmt_type, 0));
		}
	}
	sqlite3_finalize(stmt_type);

	if (param_type < 0) {
		// parameter not found or error occurred
		ulfius_set_json_body_response(response, 404, json_pack("{s:s}", "error", "Parameter not found"));
		return U_CALLBACK_CONTINUE;
	}

	if (param_type == 0) {
		if (from && to) {
			sql =
				"SELECT value, idx, time_sec FROM param_int pi "
				"JOIN param_node pn ON pi.param_node_id = pn.id "
				"JOIN param p ON pn.param_id = p.id "
				"WHERE pn.node_id = ? AND p.param_id = ? AND time_sec >= ? AND time_sec <= ? ORDER BY time_sec DESC LIMIT ?;";
		} else {
			sql =
				"SELECT value, idx, time_sec FROM param_int pi "
				"JOIN param_node pn ON pi.param_node_id = pn.id "
				"JOIN param p ON pn.param_id = p.id "
				"WHERE pn.node_id = ? AND p.param_id = ? ORDER BY time_sec DESC LIMIT ?;";
		}
	} else if (param_type == 1) {
		if (from && to) {
			sql =
				"SELECT value, idx, time_sec FROM param_real pr "
				"JOIN param_node pn ON pr.param_node_id = pn.id "
				"JOIN param p ON pn.param_id = p.id "
				"WHERE pn.node_id = ? AND p.param_id = ? AND time_sec >= ? AND time_sec <= ? ORDER BY time_sec DESC LIMIT ?;";
		} else {
			sql =
				"SELECT value, idx, time_sec FROM param_real pr "
				"JOIN param_node pn ON pr.param_node_id = pn.id "
				"JOIN param p ON pn.param_id = p.id "
				"WHERE pn.node_id = ? AND p.param_id = ? ORDER BY time_sec DESC LIMIT ?;";
		}
	} else {
		ulfius_set_json_body_response(response, 400, json_pack("{s:s}", "error", "Unsupported parameter type"));
		return U_CALLBACK_CONTINUE;
	}

	if (from && to) {
		json_array = fetch_query_results(sql, "%s%s%s%s%s", node, param_id, from, to, limit);
	} else {
		json_array = fetch_query_results(sql, "%s%s%s", node, param_id, limit);
	}

	ulfius_set_json_body_response(response, 200, json_array);

	return U_CALLBACK_CONTINUE;
}
