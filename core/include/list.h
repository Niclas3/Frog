#ifndef __LIST_H_
#define __LIST_H_

#if defined(__GNUC__)
#define __LIST_HAVE_TYPEOF 1
#endif

#include <ostype.h>
/**
 * struct list_head - Head and node of a doubly-linked list
 * @prev: pointer to the previous node in the list
 * @next: pointer to the next node in the list
 *
 * The simple doubly-linked list consists of a head and nodes attached to
 * this head. Both node and head share the same struct type. The list_*
 * functions and macros can be used to access and modify this data structure.
 *
 * The @prev pointer of the list head points to the last list node of the
 * list and @next points to the first list node of the list. For an empty list,
 * both member variables point to the head.
 *
 * The list nodes are usually embedded in a container structure which holds the
 * actual data. Such container structure is called entry. The helper list_entry
 * can be used to calculate the structure address from the address of the node.
 */
struct list_head {
    struct list_head *prev;
    struct list_head *next;
};

/**
 * The offsetof macro takes two arguments: the type of the structure and the
 * name of the member whose offset you want to calculate. It returns the byte
 * offset of that member within the structure.
 * @type: type of member
 * @member: member name
 * */
#define offsetof(type, member) ((uint_32) &((type *) 0)->member)

/**
 * container_of() - Calculate address of structure that contains address ptr
 * @ptr: pointer to member variable
 * @type: type of the structure containing ptr
 * @member: name of the member variable in struct @type
 *
 * Return: @type pointer of structure containing ptr
 */
#ifndef container_of
#ifdef __LIST_HAVE_TYPEOF
#define container_of(ptr, type, member)                            \
    __extension__({                                                \
        const __typeof__(((type *) 0)->member) *__pmember = (ptr); \
        (type *) ((char *) __pmember - offsetof(type, member));    \
    })

#define list_entry(ptr, type, member)                              \
    __extension__({                                                \
        const __typeof__(((type *) 0)->member) *__pmember = (ptr); \
        (type *) ((char *) __pmember - offsetof(type, member));    \
    })

#else
#define container_of(ptr, type, member) \
    ((type *) ((char *) (ptr) -offsetof(type, member)))

#define list_entry(ptr, type, member) \
    ((type *) ((char *) (ptr) -offsetof(type, member)))
#endif
#endif


#define LIST_HEAD_INIT(name) { &(name), &(name) }

#define LIST_HEAD(name) \
	struct list_head name = LIST_HEAD_INIT(name)

#define INIT_LIST_HEAD(ptr) do { \
	(ptr)->next = (ptr); (ptr)->prev = (ptr); \
} while (0)

/**
 * list_for_each	-	iterate over a list
 * @pos:	the &struct list_head to use as a loop counter.
 * @head:	the head for your list.
 */
#define list_for_each(pos, head) \
	for (pos = (head)->next; pos != (head); pos = pos->next)
/**
 * list_for_each_rear  -	rear iterate over a list
 * @pos:	the &struct list_head to use as a loop counter.
 * @head:	the head for your list.
 */
#define list_for_each_rear(pos, head) \
	for (pos = (head)->prev; pos != (head); pos = pos->prev)

/**
 * INIT_LIST_HEAD() - Initialize empty list head
 * @head: pointer to list head
 *
 * This can also be used to initialize a unlinked list node.
 *
 * A node is usually linked inside a list, will be added to a list in
 * the near future or the entry containing the node will be free'd soon.
 *
 * But an unlinked node may be given to a function which uses list_del(_init)
 * before it ends up in a previously mentioned state. The list_del(_init) on an
 * initialized node is well defined and safe. But the result of a
 * list_del(_init) on an uninitialized node is undefined (unrelated memory is
 * modified, crashes, ...).
 */
// void init_list_head(struct list_head *head);

/**
 * list_find_element(struct list_head *head,)
 * @node: pointer to the new node
 * @head: pointer to the head of the list
 * return 0 == not find 
 * other find
 * */
int list_find_element(struct list_head *node , struct list_head *head);

/**
 * list_map(struct list_head *head, function func, uint_32 arg)
 * @head: pointer to the head of the list
 * @res : res of list 
 * @func: test function
 * @arg : arg for function
 *
 * @Return :!0 success ,0 error happens
 *
 * */

int list_map(struct list_head *head,
                        struct list_head *res,
                        struct list_head* (*func)(struct list_head *));
/**
 * list_filter()
 *
 * @param param write here
 * @return return Comments write here
 *****************************************************************************/
struct list_head* list_walker(struct list_head *head,
              bool func(struct list_head *cur, int value),
              int value);

/**
 * list_length() - count lenght of giving list
 * @head: pointer of head
 */
int list_length(struct list_head *head);

/**
 * list_add() - Add a list node to the beginning of the list
 * @node: pointer to the new node
 * @head: pointer to the head of the list
 */
void list_add(struct list_head *node, struct list_head *head);

/**
 * list_add_tail() - Add a list node to the end of the list
 * @node: pointer to the new node
 * @head: pointer to the head of the list
 */
void list_add_tail(struct list_head *node, struct list_head *head);

/**
 * list_pop() - pop element from a list node the head of the list
 * @head: pointer to the head of the list
 */
struct list_head* list_pop(struct list_head *head);

/**
 * list_del() - Remove a list node from the list
 * @node: pointer to the node
 *
 * The node is only removed from the list. Neither the memory of the removed
 * node nor the memory of the entry containing the node is free'd. The node
 * has to be handled like an uninitialized node. Accessing the next or prev
 * pointer of the node is not safe.
 *
 * Unlinked, initialized nodes are also uninitialized after list_del.
 *
 * LIST_POISONING can be enabled during build-time to provoke an invalid memory
 * access when the memory behind the next/prev pointer is used after a list_del.
 * This only works on systems which prohibit access to the predefined memory
 * addresses.
 */
void list_del(struct list_head *node);

/**
 * list_del_init() - Remove a list node from the list and reinitialize it
 * @node: pointer to the node
 *
 * The removed node will not end up in an uninitialized state like when using
 * list_del. Instead the node is initialized again to the unlinked state.
 */
void list_del_init(struct list_head *node);

/**
 * list_destory()
 * Remove all node from a list
 *
 * @param head list head
 * @return !0 success/ 0 failed
 *****************************************************************************/
int list_destory(struct list_head* head);

/**
 * list_empty() - Check if list head has no nodes attached
 * @head: pointer to the head of the list
 *
 * Return: 0 false - list is not empty 
 *        !0  true - list is empty
 *
 */
int list_is_empty(const struct list_head *head);

/**
 * list_is_singular() - Check if list head has exactly one node attached
 * @head: pointer to the head of the list
 *
 * Return: 0 - list is not singular 
 *        !0 - list has exactly one entry
 */
int list_is_singular(const struct list_head *head);

/**
 * list_append() - Add list nodes from a list to beginning of another list
 * @list: pointer to the head of the list with the node entries
 * @head: pointer to the head of the list
 *
 * All nodes from @list are added to the beginning of the list of @head.
 * It is similar to list_add but for multiple nodes. The @list head is not
 * modified and has to be initialized to be used as a valid list head/node
 * again.
 */
void list_append(struct list_head *list, struct list_head *head);

/**
 * list_append_tail() - Add list nodes from a list to end of another list
 * @list: pointer to the head of the list with the node entries
 * @head: pointer to the head of the list
 *
 * All nodes from @list are added to to the end of the list of @head.
 * It is similar to list_add_tail but for multiple nodes. The @list head is not
 * modified and has to be initialized to be used as a valid list head/node
 * again.
 */
void list_append_tail(struct list_head *list, struct list_head *head);

/**
 * list_append_init() - Move list nodes from a list to beginning of another list
 * @list: pointer to the head of the list with the node entries
 * @head: pointer to the head of the list
 *
 * All nodes from @list are added to to the beginning of the list of @head.
 * It is similar to list_add but for multiple nodes.
 *
 * The @list head will not end up in an uninitialized state like when using
 * list_splice. Instead the @list is initialized again to the an empty
 * list/unlinked state.
 */
void list_append_init(struct list_head *list, struct list_head *head);

/**
 * list_append_tail_init() - Move list nodes from a list to end of another list
 * @list: pointer to the head of the list with the node entries
 * @head: pointer to the head of the list
 *
 * All nodes from @list are added to to the end of the list of @head.
 * It is similar to list_add_tail but for multiple nodes.
 *
 * The @list head will not end up in an uninitialized state like when using
 * list_splice. Instead the @list is initialized again to the an empty
 * list/unlinked state.
 */
void list_append_tail_init(struct list_head *list,
                                  struct list_head *head);

/**
 * list_cut_position() - Move beginning of a list to another list
 * @head_to: pointer to the head of the list which receives nodes
 * @head_from: pointer to the head of the list
 * @node: pointer to the node in which defines the cutting point
 *
 * All entries from the beginning of the list @head_from to (including) the
 * @node is moved to @head_to.
 *
 * @head_to is replaced when @head_from is not empty. @node must be a real
 * list node from @head_from or the behavior is undefined.
 */
void list_cut_position(struct list_head *head_to,
                              struct list_head *head_from,
                              struct list_head *node);

/**
 * list_move() - Move a list node to the beginning of the list
 * @node: pointer to the node
 * @head: pointer to the head of the list
 *
 * The @node is removed from its old position/node and add to the beginning of
 * @head
 */
void list_move(struct list_head *node, struct list_head *head);

/**
 * list_move_tail() - Move a list node to the end of the list
 * @node: pointer to the node
 * @head: pointer to the head of the list
 *
 * The @node is removed from its old position/node and add to the end of @head
 */
void list_move_tail(struct list_head *node, struct list_head *head);


#endif
