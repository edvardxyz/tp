#include <orcania.h>
#include <ulfius.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <sys/queue.h>
#include "ws.h"

thread_safe_node_queue tsq;

void init_thread_safe_char_queue(thread_safe_char_queue * tsq) {
	TAILQ_INIT(&tsq->queue);
	pthread_mutex_init(&tsq->mutex, NULL);
	pthread_cond_init(&tsq->cond, NULL);
}

void init_thread_safe_node_queue() {
	TAILQ_INIT(&tsq.queue);
	pthread_mutex_init(&tsq.mutex, NULL);
	pthread_cond_init(&tsq.cond, NULL);
}

void enqueue_char(thread_safe_char_queue * tsq, const char * data) {
	struct char_item * item = malloc(sizeof(struct char_item));
	if (!item) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	if (data)
		item->data = strdup(data);
	else
		item->data = NULL;  // close signal
	pthread_mutex_lock(&tsq->mutex);
	TAILQ_INSERT_TAIL(&tsq->queue, item, entries);
	pthread_cond_signal(&tsq->cond);
	pthread_mutex_unlock(&tsq->mutex);
}

char * dequeue_char(thread_safe_char_queue * tsq) {
	pthread_mutex_lock(&tsq->mutex);
	while (TAILQ_EMPTY(&tsq->queue)) {
		pthread_cond_wait(&tsq->cond, &tsq->mutex);
	}
	struct char_item * item = TAILQ_FIRST(&tsq->queue);
	TAILQ_REMOVE(&tsq->queue, item, entries);
	pthread_mutex_unlock(&tsq->mutex);
	char * data = item->data;
	free(item);
	return data;
}

void enqueue_node(thread_safe_node_queue * tsq, int node, struct thread_safe_char_queue * char_queue_ptr) {
	struct node_item * item = malloc(sizeof(struct node_item));
	if (!item) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	item->node = node;
	item->char_queue = char_queue_ptr;
	pthread_mutex_lock(&tsq->mutex);
	TAILQ_INSERT_TAIL(&tsq->queue, item, entries);
	pthread_cond_signal(&tsq->cond);
	pthread_mutex_unlock(&tsq->mutex);
}

void remove_node(thread_safe_node_queue * tsq, struct thread_safe_char_queue * char_queue_ptr) {
	pthread_mutex_lock(&tsq->mutex);
	struct node_item * item;
	TAILQ_FOREACH(item, &tsq->queue, entries) {
		if (item->char_queue == char_queue_ptr) {
			TAILQ_REMOVE(&tsq->queue, item, entries);
			free(item);
			break;
		}
	}
	pthread_mutex_unlock(&tsq->mutex);
}

struct node_item * dequeue_node() {
	pthread_mutex_lock(&tsq.mutex);
	while (TAILQ_EMPTY(&tsq.queue)) {
		pthread_cond_wait(&tsq.cond, &tsq.mutex);
	}
	struct node_item * item = TAILQ_FIRST(&tsq.queue);
	TAILQ_REMOVE(&tsq.queue, item, entries);
	pthread_mutex_unlock(&tsq.mutex);
	return item;
}

void websocket_manager_callback(const struct _u_request * request,
								struct _websocket_manager * websocket_manager,
								void * websocket_manager_user_data) {

	thread_safe_char_queue * char_queue = (thread_safe_char_queue *)websocket_manager_user_data;
	y_log_message(Y_LOG_LEVEL_DEBUG, "Starting websocket_manager_callback");

	while (ulfius_websocket_wait_close(websocket_manager, 1) == U_WEBSOCKET_STATUS_OPEN) {

		y_log_message(Y_LOG_LEVEL_DEBUG, "Dequeu from %p ", char_queue);
		char * data = dequeue_char(char_queue);
		y_log_message(Y_LOG_LEVEL_DEBUG, "Sending message: %s", data);
		if (data == NULL) {
			break;
		}
		if (ulfius_websocket_send_message(websocket_manager, U_WEBSOCKET_OPCODE_TEXT, strlen(data), data) != U_OK) {
			y_log_message(Y_LOG_LEVEL_ERROR, "Error send message");
			free(data);
			break;
		}
		free(data);
	}
	y_log_message(Y_LOG_LEVEL_DEBUG, "Closing websocket_manager_callback");
}

void websocket_onclose_callback(const struct _u_request * request,
								struct _websocket_manager * websocket_manager,
								void * websocket_onclose_user_data) {

	y_log_message(Y_LOG_LEVEL_DEBUG, "Websocket closed");
	thread_safe_char_queue * char_queue = (thread_safe_char_queue *)websocket_onclose_user_data;
	remove_node(&tsq, char_queue);
	enqueue_char(char_queue, NULL);
	usleep(1000);

	pthread_mutex_lock(&char_queue->mutex);
	while (!TAILQ_EMPTY(&char_queue->queue)) {
		struct char_item * item = TAILQ_FIRST(&char_queue->queue);
		TAILQ_REMOVE(&char_queue->queue, item, entries);
		free(item->data);
		free(item);
	}
	pthread_mutex_unlock(&char_queue->mutex);

	pthread_mutex_destroy(&char_queue->mutex);
	pthread_cond_destroy(&char_queue->cond);
	free(char_queue);
}

int callback_websocket(const struct _u_request * request, struct _u_response * response, void * user_data) {
	int ret;
	(void)(request);
	(void)(user_data);
	y_log_message(Y_LOG_LEVEL_DEBUG, "Websocket connected");
	

	const char * node_str = u_map_get(request->map_url, "node");
	if (!node_str) {
		ulfius_set_json_body_response(response, 400, json_pack("{s:s}", "error", "Missing node"));
		return U_CALLBACK_COMPLETE;
	}
	int node = atoi(node_str);
	thread_safe_char_queue * char_queue = malloc(sizeof(thread_safe_char_queue));
	init_thread_safe_char_queue(char_queue);
	enqueue_node(&tsq, node, char_queue);

	if ((ret = ulfius_set_websocket_response(
			 response, NULL, NULL, &websocket_manager_callback,
			 char_queue, NULL, NULL,
			 &websocket_onclose_callback, char_queue)) == U_OK) {
		ulfius_add_websocket_deflate_extension(response);
		return U_CALLBACK_CONTINUE;
	} else {
		return U_CALLBACK_ERROR;
	}
}
