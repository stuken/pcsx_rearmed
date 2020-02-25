/*
 * Copyright (c) 2018 naehrwert
 * Copyright (c) 2018 M4xw
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2 (or later), as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include "heap.h"

static void _heap_create(heap_t *heap, u64* start)
{
	heap->start = start;
	heap->first = NULL;
}

// Node info is before node address.
static u64 _heap_alloc(heap_t *heap, u32 size)
{
    hnode_t *node, *newNode;

    // Align to cache line size.
    size = ALIGN(size, sizeof(hnode_t));

    if (!heap->first)
    {
        node = (hnode_t *)heap->start;
        node->used = 1;
        node->size = size;
        node->prev = NULL;
        node->next = NULL;
        heap->first = node;

        return (u64)node + sizeof(hnode_t);
    }

    node = heap->first;
    while (true)
    {
        // Check if there's available unused node.
        if (!node->used && (size <= node->size))
        {
            // Size and offset of the new unused node.
            u32 new_size = node->size - size;
            newNode = (hnode_t *)((u64)node + sizeof(hnode_t) + size);

            // If there's aligned unused space from the old node,
            // create a new one and set the leftover size.
            if (new_size >= (sizeof(hnode_t) << 2))
            {
                newNode->size = new_size - sizeof(hnode_t);
                newNode->used = 0;
                newNode->next = node->next;
                // Check that we are not on first node.
                if (newNode->next)
                    newNode->next->prev = newNode;
                newNode->prev = node;
                node->next = newNode;
            }
            else // Unused node size is just enough.
                size += new_size;

            node->size = size;
            node->used = 1;

            return (u64)node + sizeof(hnode_t);
        }

        // No unused node found, try the next one.
        if (node->next)
            node = node->next;
        else
            break;
    }

    // No unused node found, create a new one.
    newNode = (hnode_t *)((u64)node + sizeof(hnode_t) + node->size);
    newNode->used = 1;
    newNode->size = size;
    newNode->prev = node;
    newNode->next = NULL;
    node->next = newNode;

    return (u64)newNode + sizeof(hnode_t);
}

static void _heap_free(heap_t *heap, void* addr)
{
	hnode_t *node = (hnode_t *)(addr - sizeof(hnode_t));
	node->used = 0;
	node = heap->first;
	while (node)
	{
		if (!node->used)
		{
			if (node->prev && !node->prev->used)
			{
				node->prev->size += node->size + sizeof(hnode_t);
				node->prev->next = node->next;
				if (node->next)
					node->next->prev = node->prev;
			}
		}
		node = node->next;
	}
}
static u32 _page_size(void* page) {
    hnode_t *node = (hnode_t *)(page - sizeof(hnode_t));
    if(node) {
        return node->size;
    } else {
        return 0;
    }
}
heap_t _heap;

void heap_init(void* base)
{
	_heap_create(&_heap, base);
}

void *hmalloc(u32 size)
{
	void *res = (void *)_heap_alloc(&_heap, size);
    memset(res, 0, size);
    return res;
}

void *hcalloc(u32 num, u32 size)
{
	void *res = (void *)_heap_alloc(&_heap, num * size);
	memset(res, 0, num * size);
	return res;
}

void hfree(void *buf)
{
	if ((u64*)buf >= _heap.start)
		_heap_free(&_heap, buf);
}

void heap_monitor(heap_monitor_t *mon, bool print_node_stats)
{
	u32 count = 0;
	memset(mon, 0, sizeof(heap_monitor_t));

	hnode_t *node = _heap.first;
	while (true)
	{
		if (node->used)
			mon->used += node->size + sizeof(hnode_t);
		else
			mon->total += node->size + sizeof(hnode_t);

		if (print_node_stats)
			printf("%3d - %d, addr: 0x%08X, size: 0x%X\n",
				count, node->used, (u64)node + sizeof(hnode_t), node->size);

		count++;

		if (node->next)
			node = node->next;
		else
			break;
	}
	mon->total += mon->used;
}

u32 page_size(void* page) {
    return _page_size(page);
}