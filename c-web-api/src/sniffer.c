#include <stdio.h>
#include <sys/time.h>
#include <pthread.h>
#include <param/param_server.h>
#include <param/param_queue.h>
#include <mpack/mpack.h>
#include <csp/csp.h>
#include <csp/csp_crc32.h>
#include <csp/interfaces/csp_if_zmqhub.h>

pthread_t param_sniffer_thread;
#define CURVE_KEYLEN 41
/*
int param_sniffer_log(void * ctx, param_queue_t *queue, param_t *param, int offset, void *reader, long unsigned int timestamp) {

	char tmp[1000] = {};

	if (offset < 0)
		offset = 0;

	int count = 1;

	//Inspect for array
	mpack_tag_t tag = mpack_peek_tag(reader);
	if (tag.type == mpack_type_array) {
		count = mpack_expect_array(reader);
	}

	double vts_arr[4];
	int vts = check_vts(*(param->node), param->id);

	uint64_t time_ms;
	if (timestamp > 0) {
		time_ms = timestamp * 1000;
	} else {
		struct timeval tv;
		gettimeofday(&tv, NULL);
		time_ms = ((uint64_t) tv.tv_sec * 1000000 + tv.tv_usec) / 1000;
	}

	for (int i = offset; i < offset + count; i++) {

		switch (param->type) {
			case PARAM_TYPE_UINT8:
			case PARAM_TYPE_XINT8:
			case PARAM_TYPE_UINT16:
			case PARAM_TYPE_XINT16:
			case PARAM_TYPE_UINT32:
			case PARAM_TYPE_XINT32:
				sprintf(tmp, "%s{node=\"%u\", idx=\"%u\"} %u %"PRIu64"\n", param->name, *(param->node), i, mpack_expect_uint(reader), time_ms);
				break;
			case PARAM_TYPE_UINT64:
			case PARAM_TYPE_XINT64:
				sprintf(tmp, "%s{node=\"%u\", idx=\"%u\"} %"PRIu64" %"PRIu64"\n", param->name, *(param->node), i, mpack_expect_u64(reader), time_ms);
				break;
			case PARAM_TYPE_INT8:
			case PARAM_TYPE_INT16:
			case PARAM_TYPE_INT32:
				sprintf(tmp, "%s{node=\"%u\", idx=\"%u\"} %d %"PRIu64"\n", param->name, *(param->node), i, mpack_expect_int(reader), time_ms);
				break;
			case PARAM_TYPE_INT64:
				sprintf(tmp, "%s{node=\"%u\", idx=\"%u\"} %"PRIi64" %"PRIu64"\n", param->name, *(param->node), i, mpack_expect_i64(reader), time_ms);
				break;
			case PARAM_TYPE_FLOAT:
				sprintf(tmp, "%s{node=\"%u\", idx=\"%u\"} %e %"PRIu64"\n", param->name, *(param->node), i, mpack_expect_float(reader), time_ms);
				break;
			case PARAM_TYPE_DOUBLE: {
				double tmp_dbl = mpack_expect_double(reader);
				sprintf(tmp, "%s{node=\"%u\", idx=\"%u\"} %.12e %"PRIu64"\n", param->name, *(param->node), i, tmp_dbl, time_ms);
				if(vts){
					vts_arr[i] = tmp_dbl;
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
			break;
		}

		if(vm_running){
			vm_add(tmp);
		}

		if(prometheus_started){
			prometheus_add(tmp);
		}

		if (logfile) {
			fprintf(logfile, "%s", tmp);
			fflush(logfile);
		}
	}

	if(vts){
		vts_add(vts_arr, param->id, count, time_ms);
	}

	return 0;
}
*/

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
			/* If parameter timestamp is not inside the header, and the lower layer found a timestamp*/
			if ((timestamp == 0) && (packet->timestamp_rx != 0)) {
				timestamp = packet->timestamp_rx;
			}
			param_t * param = param_list_find_id(node, id);
			if (param) {
				// param_sniffer_log(NULL, &queue, param, offset, &reader, timestamp);
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
	while(1) {
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
