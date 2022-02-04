#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "queue.h"

struct node {
    void* data;
    struct node* nextnode;
};

typedef struct node* node_t;

struct queue {
    /* TODO */
    node_t head;
    node_t tail;
};

queue_t queue_create(void)
{
	/* TODO */
    queue_t queue = (queue_t)malloc(sizeof(struct queue));
    queue->head = NULL;
    queue->tail = NULL;
	return queue;
}

int queue_destroy(queue_t queue)
{
	/* TODO */
    if (queue == NULL) {   // check whether queue is null
        return -1;
    } else if (queue->head != NULL) {  // check queue is empty
        return -1;
    }
    free(queue);
	return 0;
}

int queue_enqueue(queue_t queue, void *data)
{
	/* TODO */
    node_t newnode = (node_t)malloc(sizeof(node));
    newnode->data = data;

    if (queue->head == NULL) {
        // queue is empty
        queue->head = newnode;
    }else if (queue->tail == NULL) {
        // tail is empty
        queue->head->nextnode = newnode;
        queue->tail = newnode;
    }else {
        // both head and tail are not empty
        queue->tail->nextnode = newnode;
        queue->tail = newnode;
    }
	return 0;
}

int queue_dequeue(queue_t queue, void **data)
{
	/* TODO */
    if (queue == NULL) {
        return -1;
    }else if ((queue->head) == NULL) {
        return -1;
    }else if (data == NULL) {
        return -1;
    }
    // normal
    *data = queue->head->data;
    // free the space
    node* temp;
    temp = queue->head;
    //check whether there is other node inside the queue
    if (queue->head->nextnode != NULL) {
        queue->head = queue->head->nextnode;
    }else {
        queue->head = NULL;
    }
    free(temp);
	return 0;
}

int queue_delete(queue_t queue, void *data)
{
	/* TODO */
	return -1;
}

int queue_iterate(queue_t queue, queue_func_t func, void *arg, void **data)
{
	/* TODO */
	return -1;
}

int queue_length(queue_t queue)
{
	/* TODO */
	return -1;
}