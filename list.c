#include <string.h>

#include "list.h"

void *
list_cons_internal (void *data, void *prevheadv, size_t nodesize)
{
  generic_list *prevhead = (generic_list *) prevheadv;
  generic_list *newnode = malloc (nodesize);
  size_t dsize = nodesize - sizeof (generic_list);
  
  memcpy (&newnode->data[0], data, dsize);
  newnode->prev = prevhead;
  newnode->next = 0;
  if (prevhead)
    prevhead->next = newnode;
  
  return newnode;
}

void *
list_rev_internal (void *inv)
{
  generic_list *in = (generic_list *) inv;
  generic_list *walk;
  generic_list *new_end = 0;
  
  /* Reverse of empty list is empty list.  */
  if (!in)
    return 0;
  
  for (walk = in; walk;)
    {
      generic_list *prev = walk->prev;
      generic_list *next = walk->next;
      walk->prev = next;
      walk->next = prev;
      /* Before overwriting walk with end-of-list marker, remember the last
         element.  */
      if (!prev)
        new_end = walk;

      walk = prev;
    }
  
  return new_end;
}

void *
die_at (const char *file, int line)
{
  fprintf (stderr, "Type mismatch at %s:%d\n", file, line);
  abort ();
  return 0;
}
