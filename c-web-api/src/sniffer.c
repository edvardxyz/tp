#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <pthread.h>

#include <param/param_server.h>
#include <param/param_queue.h>
#include <mpack/mpack.h>

#include <csp/csp.h>
#include <csp/csp_crc32.h>
#include <csp/csp_cmp.h>
#include <csp/csp_hooks.h>
#include <csp/interfaces/csp_if_zmqhub.h>
#include <csp/csp_types.h>
#include <csp/arch/csp_time.h>

#include <sqlite3.h>
#include <yder.h>
#include "csp/csp_types.h"
#include "db.h"
#include "param/param_list.h"

#include "ws.h"

int me = 0;
pthread_t param_sniffer_thread;
#define CURVE_KEYLEN 41

int param_sniffer_crc(csp_packet_t * packet) {

	/* CRC32 verified packet */
	if (packet->id.flags & CSP_FCRC32) {
		if (packet->length < 4) {
			printf("Too short packet for CRC32, %u\n", packet->length);
			return -1;
		}
		/* Verify CRC32 (does not include header for backwards compatability with csp1.x) */
		if (csp_crc32_verify(packet) != 0) {
			/* Checksum failed */
			printf("CRC32 verification error in param sniffer! Discarding packet\n");
			return -1;
		}
	}
	return 0;
}

int get_param_by_id_and_name(int param_id, const char * param_name, int * id) {
	const char * sql =
		"SELECT id "
		"FROM param "
		"WHERE param_id = ? AND name = ?;";
	sqlite3_stmt * stmt = NULL;
	int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
		return rc;
	}

	sqlite3_bind_int(stmt, 1, param_id);
	sqlite3_bind_text(stmt, 2, param_name, -1, SQLITE_TRANSIENT);

	rc = sqlite3_step(stmt);
	if (rc == SQLITE_ROW) {
		*id = sqlite3_column_int(stmt, 0);
		rc = SQLITE_OK;
	} else {
		printf("Parameter not found for param_id: %d, name: %s\n", param_id, param_name);
		rc = SQLITE_NOTFOUND;
	}
	sqlite3_finalize(stmt);
	return rc;
}

int get_param_node_id(int param_param_id, const char * param_name, int node_id, int * out_id) {
	static sqlite3_stmt * stmt = NULL;
	const char * sql =
		"SELECT pn.id "
		"FROM param_node pn "
		"JOIN param p ON p.id = pn.param_id "
		"WHERE p.param_id = ? AND p.name = ? AND pn.node_id = ?;";

	if (!stmt) {
		int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
			return rc;
		}
	}

	// Reset statement and clear previous bindings
	sqlite3_reset(stmt);
	sqlite3_clear_bindings(stmt);

	sqlite3_bind_int(stmt, 1, param_param_id);
	sqlite3_bind_text(stmt, 2, param_name, -1, SQLITE_STATIC);
	sqlite3_bind_int(stmt, 3, node_id);

	int rc = sqlite3_step(stmt);
	if (rc == SQLITE_ROW) {
		*out_id = sqlite3_column_int(stmt, 0);
		return SQLITE_OK;
	} else if (rc == SQLITE_DONE) {
		fprintf(stderr, "No matching param_node record found.\n");
		return SQLITE_NOTFOUND;
	} else {
		fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
		return rc;
	}
}
int insert_batch_begin() {
	int rc = sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Failed to begin transaction: %s\n", sqlite3_errmsg(db));
	}
	return rc;
}

int insert_batch_end() {
	int rc = sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Failed to commit transaction: %s\n", sqlite3_errmsg(db));
	}
	return rc;
}

int param_sniffer_insert(void * queue, param_t * param, int offset, void * reader, unsigned long timestamp) {
	if (offset < 0)
		offset = 0;

	int count = 1;
	mpack_tag_t tag = mpack_peek_tag(reader);
	if (tag.type == mpack_type_array) {
		count = mpack_expect_array(reader);
	}

	uint64_t time_sec;
	if (timestamp > 0) {
		time_sec = timestamp;
	} else {
		struct timeval tv;
		gettimeofday(&tv, NULL);
		time_sec = (uint64_t)tv.tv_sec;
	}

	int param_node_pk;
	int rc = get_param_node_id(param->id, param->name, *param->node, &param_node_pk);
	if (rc != SQLITE_OK) {
		return rc;
	}

	int i = offset;
	while (i < offset + count) {
		switch (param->type) {
			case PARAM_TYPE_UINT8:
			case PARAM_TYPE_XINT8:
			case PARAM_TYPE_UINT16:
			case PARAM_TYPE_XINT16:
			case PARAM_TYPE_UINT32:
			case PARAM_TYPE_XINT32:
			case PARAM_TYPE_UINT64:
			case PARAM_TYPE_XINT64:
			case PARAM_TYPE_INT8:
			case PARAM_TYPE_INT16:
			case PARAM_TYPE_INT32:
			case PARAM_TYPE_INT64: {
				sqlite3_int64 value;
				if (param->type == PARAM_TYPE_UINT8 ||
					param->type == PARAM_TYPE_XINT8 ||
					param->type == PARAM_TYPE_UINT16 ||
					param->type == PARAM_TYPE_XINT16 ||
					param->type == PARAM_TYPE_UINT32 ||
					param->type == PARAM_TYPE_XINT32 ||
					param->type == PARAM_TYPE_UINT64 ||
					param->type == PARAM_TYPE_XINT64) {
					value = (sqlite3_int64)mpack_expect_u64(reader);
				} else {
					value = (sqlite3_int64)mpack_expect_i64(reader);
				}

				// Reuse a static prepared statement for integer inserts
				static sqlite3_stmt * stmt_int = NULL;
				const char * sql_int =
					"INSERT INTO param_int (value, idx, param_node_id, time_sec) VALUES (?, ?, ?, ?);";
				if (!stmt_int) {
					rc = sqlite3_prepare_v2(db, sql_int, -1, &stmt_int, NULL);
					if (rc != SQLITE_OK) {
						fprintf(stderr, "Failed to prepare param_int statement: %s\n", sqlite3_errmsg(db));
						return rc;
					}
				}
				sqlite3_reset(stmt_int);
				sqlite3_clear_bindings(stmt_int);

				sqlite3_bind_int64(stmt_int, 1, value);
				sqlite3_bind_int(stmt_int, 2, i);
				sqlite3_bind_int(stmt_int, 3, param_node_pk);
				sqlite3_bind_int64(stmt_int, 4, time_sec);

				rc = sqlite3_step(stmt_int);
				if (rc != SQLITE_DONE) {
					fprintf(stderr, "Failed to execute param_int insert: %s\n", sqlite3_errmsg(db));
				}
				break;
			}
			// For floating point types
			case PARAM_TYPE_DOUBLE:
			case PARAM_TYPE_FLOAT: {
				double value = mpack_expect_double(reader);  // mpack handles casting for floats

				static sqlite3_stmt * stmt_real = NULL;
				const char * sql_real =
					"INSERT INTO param_real (value, idx, param_node_id, time_sec) VALUES (?, ?, ?, ?);";
				if (!stmt_real) {
					rc = sqlite3_prepare_v2(db, sql_real, -1, &stmt_real, NULL);
					if (rc != SQLITE_OK) {
						fprintf(stderr, "Failed to prepare param_real statement: %s\n", sqlite3_errmsg(db));
						return rc;
					}
				}
				sqlite3_reset(stmt_real);
				sqlite3_clear_bindings(stmt_real);

				sqlite3_bind_double(stmt_real, 1, value);
				sqlite3_bind_int(stmt_real, 2, i);
				sqlite3_bind_int(stmt_real, 3, param_node_pk);
				sqlite3_bind_int64(stmt_real, 4, time_sec);

				rc = sqlite3_step(stmt_real);
				if (rc != SQLITE_DONE) {
					fprintf(stderr, "Failed to execute param_real insert: %s\n", sqlite3_errmsg(db));
				}
				break;
			}
			case PARAM_TYPE_STRING:
			case PARAM_TYPE_DATA:
			default:
				mpack_discard(reader);
				break;
		}

		if (mpack_reader_error(reader) != mpack_ok) {
			fprintf(stderr, "mpack reader error: %d\n", mpack_reader_error(reader));
			break;
		}
		i++;
	}

	return 0;
}

int node_exists(int node, int * exists) {
	const char * sql =
		"SELECT 1 "
		"FROM node n "
		"WHERE n.node = ? LIMIT 1;";

	static sqlite3_stmt * stmt = NULL;
	int rc = SQLITE_OK;
	if (!stmt) {
		rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
			return rc;
		}
	}
	sqlite3_reset(stmt);
	sqlite3_clear_bindings(stmt);

	sqlite3_bind_int(stmt, 1, node);
	sqlite3_changes(db);

	rc = sqlite3_step(stmt);
	if (rc == SQLITE_ROW) {
		*exists = 1;
	} else {
		*exists = 0;
	}

	return SQLITE_OK;
}

int insert_node(int node, char * hostname) {
	const char * sql = "INSERT INTO node (node, name) VALUES (?, ?);";

	sqlite3_stmt * stmt = NULL;
	int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Failed to prepare node insert: %s\n", sqlite3_errmsg(db));
		return rc;
	}

	sqlite3_bind_int(stmt, 1, node);
	sqlite3_bind_text(stmt, 2, hostname, strlen(hostname), NULL);

	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE) {
		fprintf(stderr, "Failed to execute node insert: %s\n", sqlite3_errmsg(db));
	}

	sqlite3_finalize(stmt);
	return rc;
}

int insert_param_node(int param_pk, int node_id) {
	const char * sql = "INSERT OR IGNORE INTO param_node (param_id, node_id) VALUES (?, ?);";

	sqlite3_stmt * stmt = NULL;
	int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Failed to prepare param_node insert: %s\n", sqlite3_errmsg(db));
		return rc;
	}

	sqlite3_bind_int(stmt, 1, param_pk);
	sqlite3_bind_int(stmt, 2, node_id);

	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE) {
		fprintf(stderr, "Failed to execute param_node insert: %s\n", sqlite3_errmsg(db));
	}

	sqlite3_finalize(stmt);
	return rc;
}

int param_list_save_db(int node) {
	param_t * param = NULL;
	param_list_iterator iter = {};
	int rc;
	int param_pk;
	sqlite3_stmt * stmt = NULL;

	while ((param = param_list_iterate(&iter)) != NULL) {
		if (*param->node == 0) {
			continue;
		}

		// does param already exists based on name and param_id?
		rc = get_param_by_id_and_name(param->id, param->name, &param_pk);
		if (rc == SQLITE_OK) {
			// link this node to the found param, ignored if already present
			rc = insert_param_node(param_pk, *param->node);
			continue;
		}

		// create param
		const char * sql_param =
			"INSERT INTO param (param_id, name, size, mask, vmem, type) "
			"VALUES (?, ?, ?, ?, ?, ?);";
		rc = sqlite3_prepare_v2(db, sql_param, -1, &stmt, NULL);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "Failed to prepare param insert: %s\n", sqlite3_errmsg(db));
			continue;
		}

		sqlite3_bind_int(stmt, 1, param->id);
		sqlite3_bind_text(stmt, 2, param->name, -1, SQLITE_STATIC);
		sqlite3_bind_int(stmt, 3, param->array_size);
		sqlite3_bind_int(stmt, 4, param->mask);
		sqlite3_bind_int(stmt, 5, param->vmem->type);
		sqlite3_bind_int(stmt, 6, param->type);

		rc = sqlite3_step(stmt);
		if (rc != SQLITE_DONE) {
			fprintf(stderr, "Failed to insert param: %s\n", sqlite3_errmsg(db));
			sqlite3_finalize(stmt);
			continue;
		}
		sqlite3_finalize(stmt);
		stmt = NULL;

		// retrieve the auto-generated primary key from the param table.
		sqlite3_int64 inserted_param_pk = sqlite3_last_insert_rowid(db);

		rc = insert_param_node(inserted_param_pk, *param->node);
	}

	return 0;
}

static int ident(int node, char * hostname) {

	csp_conn_t * conn = csp_connect(CSP_PRIO_NORM, node, CSP_CMP, 1000, CSP_O_CRC32);
	if (conn == NULL) {
		return -1;
	}

	csp_packet_t * packet = csp_buffer_get(0);
	if (packet == NULL) {
		csp_close(conn);
		return -1;
	}

	struct csp_cmp_message * msg = (struct csp_cmp_message *)packet->data;
	int size = sizeof(msg->type) + sizeof(msg->code) + sizeof(msg->ident);
	msg->type = CSP_CMP_REQUEST;
	msg->code = CSP_CMP_IDENT;
	packet->length = size;

	csp_send(conn, packet);
	int found = 0;

	while ((packet = csp_read(conn, 1500)) != NULL) {
		msg = (struct csp_cmp_message *)packet->data;
		if (msg->code == CSP_CMP_IDENT) {
			strncpy(hostname, msg->ident.hostname, CSP_HOSTNAME_LEN - 1);
			found = 1;
			csp_buffer_free(packet);
			break;
		}
		csp_buffer_free(packet);
	}
	csp_close(conn);
	if (found) {
		return 0;
	} else {
		printf("ident timeout\n");
		return -2;
	}
}

// This function upserts a contact row.
// If a row for the given (node_to, node_from) exists, it updates time_sec.
// Otherwise, it inserts a new row.
int upsert_contact(int node_to, int node_from, uint64_t time_sec) {
	static sqlite3_stmt * stmt_sel = NULL;
	int rc;
	int contact_id = -1;

	// First, check if a contact row already exists for this node pair.
	const char * select_sql =
		"SELECT id FROM contact WHERE node_to_id = ? AND node_from_id = ?;";
	if (!stmt_sel) {
		rc = sqlite3_prepare_v2(db, select_sql, -1, &stmt_sel, NULL);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "Failed to prepare SELECT statement: %s\n", sqlite3_errmsg(db));
			return rc;
		}
	}

	sqlite3_reset(stmt_sel);
	sqlite3_clear_bindings(stmt_sel);

	sqlite3_bind_int(stmt_sel, 1, node_to);
	sqlite3_bind_int(stmt_sel, 2, node_from);

	rc = sqlite3_step(stmt_sel);
	if (rc == SQLITE_ROW) {
		// Found an existing row.
		contact_id = sqlite3_column_int(stmt_sel, 0);
	}

	if (contact_id != -1) {
		// Update the time_sec field in the existing row.
		static sqlite3_stmt * stmt_up = NULL;
		const char * update_sql =
			"UPDATE contact SET time_sec = ? WHERE id = ?;";
		if (!stmt_up) {
			rc = sqlite3_prepare_v2(db, update_sql, -1, &stmt_up, NULL);
			if (rc != SQLITE_OK) {
				fprintf(stderr, "Failed to prepare UPDATE statement: %s\n", sqlite3_errmsg(db));
				return rc;
			}
		}

		sqlite3_reset(stmt_up);
		sqlite3_clear_bindings(stmt_up);

		sqlite3_bind_int(stmt_up, 1, time_sec);
		sqlite3_bind_int(stmt_up, 2, contact_id);

		rc = sqlite3_step(stmt_up);
		if (rc != SQLITE_DONE) {
			fprintf(stderr, "Failed to update contact: %s\n", sqlite3_errmsg(db));
			return rc;
		}
	} else {
		// Insert a new row for this node pair.
		const char * insert_sql =
			"INSERT INTO contact (node_to_id, node_from_id, time_sec) VALUES (?, ?, ?);";
		static sqlite3_stmt * stmt = NULL;
		if (!stmt) {
			rc = sqlite3_prepare_v2(db, insert_sql, -1, &stmt, NULL);
			if (rc != SQLITE_OK) {
				fprintf(stderr, "Failed to prepare INSERT statement: %s\n", sqlite3_errmsg(db));
				return rc;
			}
		}

		sqlite3_reset(stmt);
		sqlite3_clear_bindings(stmt);

		sqlite3_bind_int(stmt, 1, node_to);
		sqlite3_bind_int(stmt, 2, node_from);
		sqlite3_bind_int(stmt, 3, time_sec);

		rc = sqlite3_step(stmt);
		if (rc != SQLITE_DONE) {
			fprintf(stderr, "Failed to insert contact: %s\n", sqlite3_errmsg(db));
			return rc;
		}
	}

	return SQLITE_OK;
}

static void * param_sniffer(void * param) {
	csp_promisc_enable(0);
	while (1) {
		csp_packet_t * packet = csp_promisc_read(CSP_MAX_DELAY);

		/* TODO implement house keeping
		if(hk_param_sniffer(packet)){
			csp_buffer_free(packet);
			continue;
		}
		*/
		if (param_sniffer_crc(packet) < 0) {
			csp_buffer_free(packet);
			continue;
		}

		int dst = packet->id.dst;
		int src = packet->id.src;

		pthread_mutex_lock(&tsq.mutex);
		struct node_item * item = NULL;
		TAILQ_FOREACH(item, &tsq.queue, entries) {
			if (item->node == src || item->node == dst) {
				printf("Found matching node: %d\n", item->node);
				y_log_message(Y_LOG_LEVEL_DEBUG, "Enqueueing data to %p", item->char_queue);
				char buf[256] = {0};
				csp_id_t * idout = &packet->id;

				if (src == item->node) {
					snprintf(buf, sizeof(buf) - 1,
							 "OUT: S %u, D %u, Dp %u, Sp %u, Pr %u, Fl 0x%02X, Sz %u, Tms %u\n",
							 idout->src, idout->dst, idout->dport, idout->sport, idout->pri, idout->flags, packet->length, csp_get_ms());
				} else {
					snprintf(buf, sizeof(buf) - 1,
							 "IN: S %u, D %u, Dp %u, Sp %u, Pr %u, Fl 0x%02X, Sz %u, Tms %u\n",
							 idout->src, idout->dst, idout->dport, idout->sport, idout->pri, idout->flags, packet->length, csp_get_ms());
				}
				enqueue_char(item->char_queue, buf);  // enqueue data to websocket it will strdup the data
			}
		}
		pthread_mutex_unlock(&tsq.mutex);

		if (src == me || dst == me) {
			csp_buffer_free(packet);
			continue;
		}

		int exists = 0;
		node_exists(packet->id.src, &exists);
		if (!exists) {
			// ident
			char hostname[CSP_HOSTNAME_LEN];
			if (ident(src, hostname) == 0) {
				insert_node(src, hostname);
			}
		}

		struct timeval tv;
		gettimeofday(&tv, NULL);
		long time_sec = tv.tv_sec;
		upsert_contact(dst, src, time_sec);

		if (packet->id.sport != PARAM_PORT_SERVER) {
			csp_buffer_free(packet);
			continue;
		}

		uint8_t type = packet->data[0];
		if ((type != PARAM_PULL_RESPONSE) && (type != PARAM_PULL_RESPONSE_V2)) {
			csp_buffer_free(packet);
			continue;
		}

		int queue_version;
		if (type == PARAM_PULL_RESPONSE) {
			queue_version = 1;
		} else {
			queue_version = 2;
		}

		param_queue_t queue;
		param_queue_init(&queue, &packet->data[2], packet->length - 2, packet->length - 2, PARAM_QUEUE_TYPE_SET, queue_version);
		queue.last_node = packet->id.src;

		mpack_reader_t reader;
		mpack_reader_init_data(&reader, queue.buffer, queue.used);

		insert_batch_begin();
		while (reader.data < reader.end) {
			int id, node, offset = -1;
			long unsigned int timestamp = 0;
			param_t * param = NULL;
			param_deserialize_id(&reader, &id, &node, &timestamp, &offset, &queue);
			if (node == 0) {
				node = packet->id.src;
			}
			/* If parameter timestamp is not inside the header, and the lower layer found a timestamp*/
			if ((timestamp == 0) && (packet->timestamp_rx != 0)) {
				timestamp = packet->timestamp_rx;
			}

			int retries = 0;
			const int max_retries = 1;

			while (retries <= max_retries) {
				param = param_list_find_id(node, id);
				if (param) {
					param_sniffer_insert(&queue, param, offset, &reader, timestamp);
					break;
				} else {
					if (retries == 0) {
						printf("Found unknown param node %d id %d\n", node, id);
						param_list_download(node, 2000, 3, 0);
						param_list_save_db(node);
					}
					retries++;
					if (retries > max_retries) {
						mpack_discard(&reader);
						break;
					}
				}
			}
		}
		insert_batch_end();
		csp_buffer_free(packet);
	}
	return NULL;
}

static int csp_ifadd_zmq(int node, char * host, char * key_file) {

	static int ifidx = 0;

	char name[10];
	sprintf(name, "ZMQ%u", ifidx++);

	int promisc = 1;
	int mask = 8;
	int dfl = 1;
	char * sec_key = NULL;
	unsigned int subport = 0;
	unsigned int pubport = 0;
	char * server = strdup(host);
	unsigned int addr = node;
	me = node;

	if (subport == 0) {
		subport = CSP_ZMQPROXY_SUBSCRIBE_PORT + ((key_file == NULL) ? 0 : 1);
	}

	if (pubport == 0) {
		pubport = CSP_ZMQPROXY_PUBLISH_PORT + ((key_file == NULL) ? 0 : 1);
	}

	if (key_file) {

		char key_file_local[256];
		if (key_file[0] == '~') {
			strcpy(key_file_local, getenv("HOME"));
			strcpy(&key_file_local[strlen(key_file_local)], &key_file[1]);
		} else {
			strcpy(key_file_local, key_file);
		}

		FILE * file = fopen(key_file_local, "r");
		if (file == NULL) {
			printf("Could not open config %s\n", key_file_local);
			return 1;
		}

		sec_key = malloc(CURVE_KEYLEN * sizeof(char));
		if (sec_key == NULL) {
			printf("Failed to allocate memory for secret key.\n");
			fclose(file);
			return 1;
		}

		if (fgets(sec_key, CURVE_KEYLEN, file) == NULL) {
			printf("Failed to read secret key from file.\n");
			free(sec_key);
			fclose(file);
			return 1;
		}
		/* We are most often saved from newlines, by only reading out CURVE_KEYLEN.
			But we still attempt to strip them, in case someone decides to use a short key. */
		char * const newline = strchr(sec_key, '\n');
		if (newline) {
			*newline = '\0';
		}
		fclose(file);
	}

	csp_iface_t * iface;
	int error = csp_zmqhub_init_filter2((const char *)name, server, addr, mask, promisc, &iface, sec_key, subport, pubport);
	if (error != CSP_ERR_NONE) {
		csp_print("Failed to add zmq interface [%s], error: %d\n", server, error);
		return 1;
	}
	iface->is_default = dfl;
	iface->addr = addr;
	iface->netmask = mask;

	if (sec_key != NULL) {
		free(sec_key);
	}
	return 0;
}

void * router_task(void * param) {
	while (1) {
		csp_route_work();
	}
}

int param_sniffer_init(int node, char * host, char * key_file) {

	csp_conf.hostname = "SqliteSniffer";
	csp_conf.model = "";
	csp_conf.revision = "";
	csp_conf.version = 2;
	csp_conf.dedup = 0;
	csp_init();

	csp_bind_callback(csp_service_handler, CSP_ANY);
	csp_bind_callback(param_serve, PARAM_PORT_SERVER);

	static pthread_t router_handle;
	pthread_create(&router_handle, NULL, &router_task, NULL);

	// TODO take args
	int res = csp_ifadd_zmq(node, host, key_file);

	pthread_create(&param_sniffer_thread, NULL, &param_sniffer, NULL);

	return res;
}
