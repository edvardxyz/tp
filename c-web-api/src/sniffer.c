#include <stdio.h>
#include <sys/time.h>
#include <pthread.h>
#include <param/param_server.h>
#include <param/param_queue.h>
#include <mpack/mpack.h>
#include <csp/csp.h>
#include <csp/csp_crc32.h>
#include <csp/interfaces/csp_if_zmqhub.h>
#include <stdio.h>
#include <sqlite3.h>
#include "db.h"
#include "param/param_list.h"

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

int get_param_node_id(int external_param_id, int node_id, int * param_node_id) {
	const char * sql =
		"SELECT pn.id "
		"FROM param_node pn "
		"JOIN param p ON p.id = pn.param_id "
		"WHERE p.param_id = ? AND pn.node_id = ?;";

	sqlite3_stmt * stmt = NULL;
	int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
		return rc;
	}

	sqlite3_bind_int(stmt, 1, external_param_id);
	sqlite3_bind_int(stmt, 2, node_id);

	rc = sqlite3_step(stmt);
	if (rc == SQLITE_ROW) {
		*param_node_id = sqlite3_column_int(stmt, 0);
		rc = SQLITE_OK;
	} else {
		fprintf(stderr, "No matching param_node found.\n");
		rc = SQLITE_NOTFOUND;
	}

	sqlite3_finalize(stmt);
	return rc;
}

int param_sniffer_insert(int param_node_id, void * queue, param_t * param, int offset, void * reader, unsigned long timestamp) {

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

	for (int i = offset; i < offset + count; i++) {
		int rc;
		sqlite3_stmt * stmt = NULL;

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

				const char * sql = "INSERT INTO param_int (value, idx, param_node_id, time_sec) VALUES (?, ?, ?, ?);";
				rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
				if (rc != SQLITE_OK) {
					fprintf(stderr, "Failed to prepare param_int statement: %s\n", sqlite3_errmsg(db));
					return rc;
				}

				sqlite3_bind_int64(stmt, 1, value);
				sqlite3_bind_int(stmt, 2, i);
				sqlite3_bind_int(stmt, 3, param_node_id);
				sqlite3_bind_int64(stmt, 4, time_sec);

				rc = sqlite3_step(stmt);
				if (rc != SQLITE_DONE) {
					fprintf(stderr, "Failed to execute param_int insert: %s\n", sqlite3_errmsg(db));
				}
				sqlite3_finalize(stmt);
				break;
			}

			case PARAM_TYPE_FLOAT: {
				float fvalue = mpack_expect_float(reader);
				double value = (double)fvalue;

				const char * sql = "INSERT INTO param_real (value, idx, param_node_id, time_sec) VALUES (?, ?, ?, ?);";
				rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
				if (rc != SQLITE_OK) {
					fprintf(stderr, "Failed to prepare param_real statement: %s\n", sqlite3_errmsg(db));
					return rc;
				}

				sqlite3_bind_double(stmt, 1, value);
				sqlite3_bind_int(stmt, 2, i);
				sqlite3_bind_int(stmt, 3, param_node_id);
				sqlite3_bind_int64(stmt, 4, time_sec);

				rc = sqlite3_step(stmt);
				if (rc != SQLITE_DONE) {
					fprintf(stderr, "Failed to execute param_real insert: %s\n", sqlite3_errmsg(db));
				}
				sqlite3_finalize(stmt);
				break;
			}
			case PARAM_TYPE_DOUBLE: {
				double value = mpack_expect_double(reader);

				const char * sql = "INSERT INTO param_real (value, idx, param_node_id, time_sec) VALUES (?, ?, ?, ?);";
				rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
				if (rc != SQLITE_OK) {
					fprintf(stderr, "Failed to prepare param_real statement: %s\n", sqlite3_errmsg(db));
					return rc;
				}

				sqlite3_bind_double(stmt, 1, value);
				sqlite3_bind_int(stmt, 2, i);
				sqlite3_bind_int(stmt, 3, param_node_id);
				sqlite3_bind_int64(stmt, 4, time_sec);

				rc = sqlite3_step(stmt);
				if (rc != SQLITE_DONE) {
					fprintf(stderr, "Failed to execute param_real insert: %s\n", sqlite3_errmsg(db));
				}
				sqlite3_finalize(stmt);
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
	}

	return 0;
}

int get_param_by_id_and_name(int param_id, const char *param_name, int * id) {
    const char *sql =
        "SELECT id"
        "FROM param "
        "WHERE param_id = ? AND name = ?;";
    sqlite3_stmt *stmt = NULL;
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

int param_list_save_db(int node) {

	param_t * param = NULL;
	param_list_iterator i = {};
	int rc;
	sqlite3_stmt * stmt = NULL;
	while ((param = param_list_iterate(&i)) != NULL) {
		if (*param->node == 0 || *param->node == node) {
			continue;
		}
	}
	const char * sql = "INSERT INTO param_node () VALUES (?, ?, ?, ?);";
	const char * sql = "INSERT INTO param () VALUES (?, ?, ?, ?);";
	rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Failed to prepare param_int statement: %s\n", sqlite3_errmsg(db));
		return rc;
	}

	/*
	sqlite3_bind_int64(stmt, 1, value);
	sqlite3_bind_int(stmt, 2, i);
	sqlite3_bind_int(stmt, 3, param_node_id);
	sqlite3_bind_int64(stmt, 4, time_sec);
	*/

	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE) {
		fprintf(stderr, "Failed to execute param_int insert: %s\n", sqlite3_errmsg(db));
	}
	sqlite3_finalize(stmt);

	return 0;
}

static void * param_sniffer(void * param) {
	csp_promisc_enable(0);
	while (1) {
		csp_packet_t * packet = csp_promisc_read(CSP_MAX_DELAY);

		/*
		if(hk_param_sniffer(packet)){
			csp_buffer_free(packet);
			continue;
		}
		*/

		if (packet->id.sport != PARAM_PORT_SERVER) {
			csp_buffer_free(packet);
			continue;
		}

		if (param_sniffer_crc(packet) < 0) {
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

		while (reader.data < reader.end) {
			int id, node, offset = -1;
			long unsigned int timestamp = 0;
			param_deserialize_id(&reader, &id, &node, &timestamp, &offset, &queue);
			if (node == 0) {
				node = packet->id.src;
			}
			int param_node_id;
			int res = get_param_node_id(id, node, &param_node_id);  // TODO is this slow?
			if (res == SQLITE_NOTFOUND) {
				printf("DB failed param_node_id lookup node %d id %d\n", node, id);
				param_list_download(node, 3000, 3, 0);
				// TODO list save into db
				continue;
			}
			/* If parameter timestamp is not inside the header, and the lower layer found a timestamp*/
			if ((timestamp == 0) && (packet->timestamp_rx != 0)) {
				timestamp = packet->timestamp_rx;
			}
			param_t * param = param_list_find_id(node, id);
			if (param) {
				param_sniffer_insert(param_node_id, &queue, param, offset, &reader, timestamp);
			} else {
				printf("Found unknown param node %d id %d\n", node, id);
				break;
			}
		}
		csp_buffer_free(packet);
	}
	return NULL;
}

static int csp_ifadd_zmq() {

	static int ifidx = 0;

	char name[10];
	sprintf(name, "ZMQ%u", ifidx++);

	int promisc = 1;
	int mask = 8;
	int dfl = 1;
	char * key_file = NULL;
	char * sec_key = NULL;
	unsigned int subport = 6000;
	unsigned int pubport = 7000;
	char * server = "localhost";
	unsigned int addr = 1;

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

int param_sniffer_init() {

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
	int res = csp_ifadd_zmq();

	pthread_create(&param_sniffer_thread, NULL, &param_sniffer, NULL);

	return res;
}
