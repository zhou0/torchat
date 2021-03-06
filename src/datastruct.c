#include "lib/datastructs.h" // struct list
#include "include/mem.h"
#include <string.h> // strdup
#include <stdlib.h> // MALLOC
#include "lib/util.h" // get_date
#include <stdbool.h>
#include "lib/socks_helper.h" // send_message_to_socket
#include "include/uthash.h"

static struct peer *head = NULL;
// a pointer to the head of peer list

static struct peer *
new_peer (const char *id)
{
	// allocate a new peer list
	struct peer *new = CALLOC (1, sizeof (struct peer));
	if (new == NULL) {
		exit_error ("CALLOC");
	}
	strncpy (new->id, id, strlen (id));
	new->msg = NULL;
	return new;
}

bool
insert_peer (const char *onion)
{
	// first allocate a new peer node
	// then push it to the existing list
	// returns the head
	struct peer *new = new_peer (onion);
	HASH_ADD_STR (head, id, new);
// uthash modifies *p, so it can't be used again in other functions
	return true;
}

static struct peer *
get_peer (const char *id)
{
	struct peer *p;
	HASH_FIND_STR (head, id, p); // returs null if doesn't exist
	return p;
}

bool
peer_exist (const char *id)
{
	if (get_peer (id) == NULL) {
		return false;
	} else {
		return true;
	}
}

static struct message *
new_message (const char *content, char *date, enum command cmd)
{
	// first allocate a new message node
	// then insert content and date
	struct message *new = MALLOC (sizeof (struct message));

	if (new == NULL) {
		exit_error ("MALLOC");
	}
	new->next = NULL;
	new->prev = NULL;
	new->date = STRDUP (date);
	new->content = STRDUP (content);
	new->cmd = cmd;
	return new;
}

static struct message *
get_tail (struct message *h)
{
	// get to most recent message, the tail
	while (h->next != NULL) {
		h = h->next;
	}
	return h;
}

bool
insert_new_message  (const char *peerId, const char *content, enum command cmd)
{
	// insert a new message to a message list
	// most recent at tail 
	// returns oldest message as head
	//
	// does not check that peer exist
	struct peer *p = get_peer (peerId);
	char *date = get_short_date ();
	struct message *newMsg = new_message (content, date, cmd);
	FREE (date);
	if(p->msg == NULL){
		p->msg = newMsg;
	} else {
		struct message *tmp = get_tail (p->msg);
		tmp->next = newMsg;
		newMsg->prev = tmp;
	}
	return true;
}

static struct message *
delete_message (struct message *msg)
{
	// FREEs the message and deletes its content
	// delete all messages
	struct message *tmp;
	FREE(msg->content);
	FREE(msg->date);
	tmp = msg;
	msg = msg->next;
	FREE(tmp);
	return msg;
}

static void
delete_peer(struct peer *currentPeer)
{
	// FREEs the peer given and deletes its id
	// note: here we suppose that the msg list
	// of the peer has already been FREEd (see delete_message)
	HASH_DEL (head, currentPeer);
	if (currentPeer->msg != NULL) {
		FREE (currentPeer->msg);
	}
}

struct message *
get_unread_message(const char *peerId)
{
	// for the given peer
	// check if there are messages that need to be read
	// if there are any, return the oldest one
	// otherwise, NULL is returned
	struct peer *currentPeer = get_peer(peerId);
	if(currentPeer == NULL){
		return NULL;
	}
	struct message *msg = currentPeer->msg;
	if(msg == NULL){
		return NULL;
	}
	/*int len = strlen(msg->content)+strlen(msg->date)+3;*/
	struct message *retMsg = new_message (msg->content, msg->date, msg->cmd); 
	currentPeer->msg = delete_message (currentPeer->msg);
	if (currentPeer->msg == NULL) {
		// if we read every message of the peer, remove peer from hash table
		delete_peer (currentPeer);
	}
	
	return retMsg;
}

struct peer *
get_list_head()
{
	return head;
}

char *
get_peer_list ()
{
	// get a list of the peers that send the server a message the client still didn't read
	// this should be parsed as a json after
	struct peer *ptr, *tmp; // used to iterate
	size_t size = 0; // size = sum of strlen of all peer id
	if (head == NULL) return NULL;

	// else
	HASH_ITER (hh, head, ptr, tmp) {
		size += strlen (ptr->id) + 1; // +1 is for the comma
	}
	char *peerList = CALLOC (size + 1, sizeof (char));
	HASH_ITER (hh, head, ptr, tmp) {
		// iterate again and concatenate the char*
		strncat (peerList, ptr->id, strlen (ptr->id));
		strncat (peerList, ",", sizeof (char));
	}
	peerList[strlen (peerList) - 1] = '\0';
	return peerList; // already in heap
}

void
clear_datastructs ()
{
	struct peer *ptr, *tmp; // used to iterate
	HASH_ITER (hh, head, ptr, tmp) {
		delete_peer (ptr);
	}
}
