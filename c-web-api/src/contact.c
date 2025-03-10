#include <ulfius.h>
#include <sqlite3.h>
#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "db.h"

int callback_get_contacts(const struct _u_request * request, struct _u_response * response, void * user_data) {

	char * sql = NULL;
	json_t * json_array = NULL;
	// get node arg from u map
	const char * node = u_map_get(request->map_url, "node");

	if (node) {
		sql =
			"SELECT n1.node, n1.name AS node_to, n2.name AS node_from, c.time_sec FROM contact c "
			"JOIN node n1 ON c.node_to_id = n1.node "
			"JOIN node n2 ON c.node_from_id = n2.node "
			"WHERE c.time_sec = (SELECT MAX(c2.time_sec) "
			"FROM contact c2 WHERE c2.node_to_id = c.node_to_id) "
			"AND n1.node = ?;";  // todo make sort by node
	} else {
		sql =
			"SELECT n1.node, n1.name AS node_to, n2.name AS node_from, c.time_sec FROM contact c "
			"JOIN node n1 ON c.node_to_id = n1.node "
			"JOIN node n2 ON c.node_from_id = n2.node "
			"WHERE c.time_sec = (SELECT MAX(c2.time_sec) "
			"FROM contact c2 WHERE c2.node_to_id = c.node_to_id);";
	}

	json_array = fetch_query_results(sql, "%s", node);

	if (!json_array) {
		ulfius_set_json_body_response(response, 500, json_pack("{s:s}", "error", "Failed to fetch contacts"));
		return U_CALLBACK_CONTINUE;
	}

	ulfius_set_json_body_response(response, 200, json_array);

	return U_CALLBACK_CONTINUE;
}
