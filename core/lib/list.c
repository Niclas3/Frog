#include <list.h>
#include <const.h>
#include <debug.h>
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
inline void init_list_head(struct list_head *head)
{
    head->next = head;
    head->prev = head;
}

/**
 * list_find_element(struct list_head *head,)
 * @node: pointer to the new node
 * @head: pointer to the head of the list
 * return 0 == not find
 * other find
 * */
inline int list_find_element(struct list_head *node , struct list_head *head){
    struct list_head *next = head->next;
    for(;next != node && next != head->prev;){
        next = next->next;
    }
   return (next != head->prev);
}

/**
 * map_list(struct list_head *head, function func, uint_32 arg)
 * @head: pointer to the head of the list
 * @func: test function
 * @arg : arg for function
 *
 * */
inline struct list_head* map_list(struct list_head *head, int func(struct list_head *,int), int arg){
    if(list_is_empty(head)){
        return NULL;
    }
    struct list_head *next = head->next;
    while(next != head->prev){
        if(func(next, arg)){
            return next;
        }
        next = next->next;
    }
    return NULL;
}

/**
 * list_length() - count lenght of giving list
 * @head: pointer of head
 */
inline int list_length(struct list_head *head)
{
    if((head->next == head->prev) && head->next == head) return 0;
    ASSERT(head->next != 0 && head->prev != 0);
    int length = 1;
    struct list_head *iter = head->next;
    for(;iter != head->prev;){
        length++;
        iter = iter->next;
    }
    return length;
}

/**
 * list_add() - Add a list node to the beginning of the list
 * @node: pointer to the new node
 * @head: pointer to the head of the list
 */
inline void list_add(struct list_head *node, struct list_head *head){
    struct list_head *first_node = head->next;
    first_node->prev = node;
    node->next = first_node;
    node->prev = head;
    head->next = node;
}

/**
 * list_add_tail() - Add a list node to the end of the list
 * @node: pointer to the new node
 * @head: pointer to the head of the list
 */
inline void list_add_tail(struct list_head *node, struct list_head *head){
    struct list_head *last_node = head->prev;
    last_node->next = node;
    node->next = head;
    node->prev = last_node;
    head->prev = node;
}

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
 */
inline void list_del(struct list_head *node){
    struct list_head *next = node->next;
    struct list_head *prev = node->prev;
    next->prev = prev;
    prev->next = next;
}

/**
 * list_del_init() - Remove a list node from the list and reinitialize it
 * @node: pointer to the node
 *
 * The removed node will not end up in an uninitialized state like when using
 * list_del. Instead the node is initialized again to the unlinked state.
 */
inline void list_del_init(struct list_head *node){
    list_del(node);
    init_list_head(node);
}

/**
 * list_empty() - Check if list head has no nodes attached
 * @head: pointer to the head of the list
 *
 * Return: 0 - list is not empty 
 *        !0 - list is empty
 */
inline int list_is_empty(const struct list_head *head){
    return (head->next == head);
}

/**
 * list_is_singular() - Check if list head has exactly one node attached
 * @head: pointer to the head of the list
 *
 * Return: 0 - list is not singular !0 -list has exactly one entry
 */
inline int list_is_singular(const struct list_head *head){
    return list_length(head) == 1 ? 1 : 0;
}

/**
 * list_splice() - Add list nodes from a list to beginning of another list
 * @list: pointer to the head of the list with the node entries
 * @head: pointer to the head of the list
 *
 * All nodes from @list are added to the beginning of the list of @head.
 * It is similar to list_add but for multiple nodes. The @list head is not
 * modified and has to be initialized to be used as a valid list head/node
 * again.
 */
inline void list_splice(struct list_head *list, struct list_head *head) {
   struct list_head *h_next = head->next;
   struct list_head *l_next = list->next;
   struct list_head *l_prev = list->prev;

   head->next = l_next;
   h_next->prev = l_prev;

   l_next->prev = head;
   l_prev->next = h_next;
}

/**
 * list_splice_tail() - Add list nodes from a list to end of another list
 * @list: pointer to the head of the list with the node entries
 * @head: pointer to the head of the list
 *
 * All nodes from @list are added to to the end of the list of @head.
 * It is similar to list_add_tail but for multiple nodes. The @list head is not
 * modified and has to be initialized to be used as a valid list head/node
 * again.
 */
inline void list_splice_tail(struct list_head *list, struct list_head *head) {
   struct list_head *h_prev = head->prev;
   struct list_head *l_next = list->next;
   struct list_head *l_prev = list->prev;
   head->prev = l_prev;
   h_prev->next = l_next;
   l_next->prev = h_prev;
   l_prev->next = head;
}

/**
 * list_splice_init() - Move list nodes from a list to beginning of another list
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
inline void list_splice_init(struct list_head *list, struct list_head *head) {
    list_splice(list, head);
    init_list_head(list);
}

/**
 * list_splice_tail_init() - Move list nodes from a list to end of another list
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
inline void list_splice_tail_init(struct list_head *list,
                                  struct list_head *head)
{
    list_splice_tail(list, head);
    init_list_head(list);
}

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
inline void list_cut_position(struct list_head *head_to,
                              struct list_head *head_from,
                              struct list_head *node)
{


}

/**
 * list_move() - Move a list node to the beginning of the list
 * @node: pointer to the node
 * @head: pointer to the head of the list
 *
 * The @node is removed from its old position/node and add to the beginning of
 * @head
 */
inline void list_move(struct list_head *node, struct list_head *head) {
    list_del(node);
    list_add(node, head);
}

/**
 * list_move_tail() - Move a list node to the end of the list
 * @node: pointer to the node
 * @head: pointer to the head of the list
 *
 * The @node is removed from its old position/node and add to the end of @head
 */
inline void list_move_tail(struct list_head *node, struct list_head *head) {
    list_del(node);
    list_add_tail(node, head);
}
