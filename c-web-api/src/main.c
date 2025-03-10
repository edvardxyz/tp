#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jwt.h>
#include <inttypes.h>
#include <ulfius.h>

#define PORT 8888
#include "auth.h"
#include "subnet.h"
#include "node.h"
#include "param.h"
#include "db.h"
#include "sniffer.h"
#include "contact.h"
#include "ws.h"

static char * read_file(const char * filename) {
	char * buffer = NULL;
	long length;
	FILE * f;
	if (filename != NULL) {
		f = fopen(filename, "rb");
		if (f) {
			fseek(f, 0, SEEK_END);
			length = ftell(f);
			fseek(f, 0, SEEK_SET);
			buffer = o_malloc((size_t)(length + 1));
			if (buffer != NULL) {
				fread(buffer, 1, (size_t)length, f);
				buffer[length] = '\0';
			}
			fclose(f);
		}
		return buffer;
	} else {
		return NULL;
	}
}

int callback_default(const struct _u_request * request, struct _u_response * response, void * user_data) {
	(void)(request);
	(void)(user_data);
	ulfius_set_string_body_response(response, 404, "Page not found");
	return U_CALLBACK_CONTINUE;
}

// create fucntion to add endpoint with a auth endpint with higher priority
int ulfius_add_endpoint_by_val_auth(struct _u_instance * u_instance,
									const char * http_method,
									const char * url_prefix,
									const char * url_format,
									unsigned int priority,
									int (*callback_function)(const struct _u_request * request,  // Input parameters (set by the framework)
															 struct _u_response * response,      // Output parameters (set by the user)
															 void * user_data),
									void * user_data) {

	if (priority == 0) {
		y_log_message(Y_LOG_LEVEL_ERROR, "Priority 0 is reserved for default endpoint");
		exit(1);
	}
	ulfius_add_endpoint_by_val(u_instance, http_method, url_prefix, url_format, priority, callback_function, user_data);
	ulfius_add_endpoint_by_val(u_instance, http_method, url_prefix, url_format, 0, &callback_auth, user_data);

	return 0;
}

int callback_options(const struct _u_request * request, struct _u_response * response, void * user_data) {
	u_map_put(response->map_header, "Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
	u_map_put(response->map_header, "Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept, Bearer, Authorization");
	u_map_put(response->map_header, "Access-Control-Max-Age", "1800");
	return U_CALLBACK_COMPLETE;
}

int main(int argc, char ** argv) {
	int ret;

	// Set the framework port number
	struct _u_instance instance;

	y_init_logs("C-API", Y_LOG_MODE_CONSOLE, Y_LOG_LEVEL_DEBUG, NULL, "Starting C WEB API");

	if (ulfius_init_instance(&instance, PORT, NULL, NULL) != U_OK) {
		y_log_message(Y_LOG_LEVEL_ERROR, "Error ulfius_init_instance, abort");
		return (1);
	}

	u_map_put(instance.default_headers, "Access-Control-Allow-Origin", "*");
	ulfius_add_endpoint_by_val(&instance, "OPTIONS", NULL, "*", 0, &callback_options, NULL);

	ulfius_add_endpoint_by_val(&instance, "POST", NULL, "/login", 1, &callback_login, NULL);
	ulfius_add_endpoint_by_val(&instance, "POST", NULL, "/register", 1, &callback_create_user, NULL);
	ulfius_add_endpoint_by_val(&instance, "POST", NULL, "/refresh", 1, &callback_refresh, NULL);

	// Auth required endpoints below
	ulfius_add_endpoint_by_val_auth(&instance, "GET", NULL, "/subnet", 1, &callback_get_subnets, NULL);
	ulfius_add_endpoint_by_val_auth(&instance, "POST", NULL, "/subnet", 1, &callback_post_subnets, NULL);
	ulfius_add_endpoint_by_val_auth(&instance, "GET", NULL, "/contact", 1, &callback_get_contacts, NULL);
	ulfius_add_endpoint_by_val_auth(&instance, "GET", NULL, "/contact/:node", 1, &callback_get_contacts, NULL);
	ulfius_add_endpoint_by_val_auth(&instance, "GET", NULL, "/wsprint", 1, &callback_websocket, NULL);

	ulfius_add_endpoint_by_val_auth(&instance, "GET", NULL, "/node", 1, &callback_get_nodes, NULL);
	ulfius_add_endpoint_by_val_auth(&instance, "GET", NULL, "/node/:node", 1, &callback_get_nodes, NULL);
	ulfius_add_endpoint_by_val_auth(&instance, "GET", NULL, "/node/:node/param", 1, &callback_get_params, NULL);
	ulfius_add_endpoint_by_val_auth(&instance, "GET", NULL, "/node/:node/param/:paramid", 1, &callback_get_params, NULL);
	ulfius_add_endpoint_by_val_auth(&instance, "GET", NULL, "/node/:node/param/:paramid/value/:limit", 1, &callback_get_values, NULL);
	ulfius_add_endpoint_by_val_auth(&instance, "GET", NULL, "/node/:node/param/:paramid/value/:limit/:from/:to", 1, &callback_get_values, NULL);

	ulfius_set_default_endpoint(&instance, &callback_default, NULL);

	init_thread_safe_node_queue();

	ret = init_db();
	if (ret) {
		y_log_message(Y_LOG_LEVEL_ERROR, "Error opening db");
		return 1;
	}

	ret = param_list_init();
	if (ret) {
		y_log_message(Y_LOG_LEVEL_ERROR, "Error loading param list");
		return 1;
	}

	ret = param_sniffer_init(60, "localhost", "zmq.key");
	if (ret) {
		y_log_message(Y_LOG_LEVEL_ERROR, "Error opening starting sniffer");
		return 1;
	}

	// Start the framework
	if (argc == 4 && o_strcmp("-secure", argv[1]) == 0) {
		// If command-line options are -secure <key_file> <cert_file>, then open an https connection
		char *key_pem = read_file(argv[2]), *cert_pem = read_file(argv[3]);
		ret = ulfius_start_secure_framework(&instance, key_pem, cert_pem);
		printf("HTTPS secure started\n");
		o_free(key_pem);
		o_free(cert_pem);
	} else {
		// Open an http connection
		ret = ulfius_start_framework(&instance);
	}

	if (ret == U_OK) {
		y_log_message(Y_LOG_LEVEL_DEBUG, "Start %sframework on port %d", ((argc == 4 && o_strcmp("-secure", argv[1]) == 0) ? "secure " : ""), instance.port);

		// Wait for the user to press <enter> on the console to quit the application
		getchar();
	} else {
		y_log_message(Y_LOG_LEVEL_DEBUG, "Error starting framework");
	}
	y_log_message(Y_LOG_LEVEL_DEBUG, "End framework");

	y_close_logs();

	ulfius_stop_framework(&instance);
	ulfius_clean_instance(&instance);

	return 0;
}
