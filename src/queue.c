/*
 * BSD 2-Clause License
 * Copyright (c) 2024, Yazeed Alharthi
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *	  1. Redistributions of source code must retain the above copyright
 *	  notice, this list of conditions and the following disclaimer.
 *
 *	  2. Redistributions in binary form must reproduce the above copyright
 *	  notice, this list of conditions and the following disclaimer in the
 *	  documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "queue.h"
#include "type.h"
#include <stdio.h>
#include <stdlib.h>
#include <xcb/xcb_icccm.h>

queue_t *
create_queue(void)
{
	queue_t *q = (queue_t *)malloc(sizeof(queue_t));
	if (!q)
		return NULL;
	q->front = q->rear = NULL;
	q->size = 0;
	return q;
}

void
enqueue(queue_t *q, node_t *n)
{
	queue_node_t *nnode = (queue_node_t *)malloc(sizeof(queue_node_t));
	if (!nnode)
		return;
	nnode->tree_node = n;
	nnode->next		 = NULL;
	nnode->prev      = q->rear;
	if (!q->rear) {
		q->front = q->rear = nnode;
	} else {
		q->rear->next = nnode;
		q->rear		  = nnode;
	}
	q->size++;
}

void
enqueue_front(queue_t *q, node_t *n)
{
	queue_node_t *nnode = (queue_node_t *)malloc(sizeof(queue_node_t));
	if (!nnode)
		return;
	nnode->tree_node = n;
	nnode->next		 = q->front;
	nnode->prev      = NULL;
	if (!q->front) {
		q->front = q->rear = nnode;
	} else {
		q->front->prev = nnode;
		q->front = nnode;
	}
	q->size++;
}

node_t *
dequeue(queue_t *q)
{
	if (!q->front)
		return NULL;
	queue_node_t *temp = q->front;
	node_t		 *node = temp->tree_node;
	q->front		   = q->front->next;
	if (!q->front)
		q->rear = NULL;
	else
		q->front->prev = NULL;
	free(temp);
	q->size--;
	return node;
}

node_t *
dequeue_rear(queue_t *q)
{
	if (!q->rear)
		return NULL;
	queue_node_t *temp = q->rear;
	node_t		 *node = temp->tree_node;
	q->rear		       = q->rear->prev;
	if (!q->rear)
		q->front = NULL;
	else
		q->rear->next = NULL;
	free(temp);
	q->size--;
	return node;
}

node_t *
peek_front(queue_t *q)
{
	if (!q->front) return NULL;
	return q->front->tree_node;
}

node_t *
peek_rear(queue_t *q)
{
	if (!q->rear) return NULL;
	return q->rear->tree_node;
}

bool
remove_node(queue_t *q, node_t *n)
{
	queue_node_t *curr = q->front;
	while (curr) {
		if (curr->tree_node == n) {
			if (curr->prev)
				curr->prev->next = curr->next;
			else
				q->front = curr->next;
			
			if (curr->next)
				curr->next->prev = curr->prev;
			else
				q->rear = curr->prev;

			free(curr);
			q->size--;
			return true;
		}
		curr = curr->next;
	}
	return false;
}

bool
is_queue_empty(queue_t *q)
{
	return q->front == NULL;
}

size_t
get_queue_size(queue_t *q)
{
	return q->size;
}

void
free_queue(queue_t *q)
{
	while (q->front) {
		queue_node_t *temp = q->front;
		q->front		   = q->front->next;
		free(temp);
	}
	free(q);
}
