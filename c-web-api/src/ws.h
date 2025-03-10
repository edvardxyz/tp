#pragma once

#include <sys/queue.h>
#include <pthread.h>
#include <ulfius.h>

struct char_item {
	char * data;
	TAILQ_ENTRY(char_item)
	entries;
};

TAILQ_HEAD(char_queue, char_item);

// Thread-safe wrapper for the char queue.
typedef struct thread_safe_char_queue {
	struct char_queue queue;  // head
	pthread_mutex_t mutex;
	pthread_cond_t cond;
} thread_safe_char_queue;

// Each node holds an integer and a pointer to a char_queue.
struct node_item {
	int node;
	thread_safe_char_queue * char_queue;
	TAILQ_ENTRY(node_item)
	entries;
};

TAILQ_HEAD(node_queue, node_item);

typedef struct thread_safe_node_queue {
	struct node_queue queue;  // The actual queue head.
	pthread_mutex_t mutex;
	pthread_cond_t cond;
} thread_safe_node_queue;

extern thread_safe_node_queue tsq;

void init_thread_safe_node_queue();
void enqueue_node(thread_safe_node_queue * tsq, int node, struct thread_safe_char_queue * char_queue_ptr);
void enqueue_char(thread_safe_char_queue * tsq, const char * data);
void init_thread_safe_char_queue(thread_safe_char_queue * tsq);
int callback_websocket(const struct _u_request * request, struct _u_response * response, void * user_data);
