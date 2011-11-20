#ifndef LIST_H
#define LIST_H 1

/* This is a implementation of type-safe lists in C.  The biggest advantage
   (apart from the type safety) is that only one malloc block is used per node,
   versus a typical generic linked-list which might use two.  */

#include <stdlib.h>
#include <stdio.h>

typedef struct generic_list {
  struct generic_list *next;
  struct generic_list *prev;
  char data[0];
} generic_list;

#define MAKE_LIST_OF(X)			\
  typedef struct X##_list_node {	\
    struct X##_list_node *next;		\
    struct X##_list_node *prev;		\
    X data;				\
  } X##_list_node;			\
  typedef X##_list_node * X##_list;	\
  extern void list_free_##X (X *);

#define FREE_NODE(T, N)			\
  void list_free_##T (T *N)

#define LIST(X) X##_list
#define EMPTY(X) ((X##_list) 0)

#define DIE die_at (__FILE__, __LINE__)

#define list_cons(D, L)							\
  ({									\
    typeof (D) _tmp = (D);						\
    (typeof (L)) __builtin_choose_expr					\
      (__builtin_types_compatible_p (typeof ((L)->data), typeof (_tmp)),\
       list_cons_internal ((void *) &_tmp, (L),	sizeof (*L)),		\
       DIE);								\
  })

void* list_cons_internal (void*, void*, size_t);

void* die_at (const char *, int);

#define list_iter(D, L)							\
  for (typeof (L) _iter = __builtin_choose_expr				\
         (__builtin_types_compatible_p (typeof ((L)->data), typeof (D)),\
          (L),								\
          DIE);								\
       (_iter && ((D) = _iter->data, 1)), _iter;			\
       _iter = _iter->prev)

#define list_iter_ptr(D, L)						\
  for (typeof (L) _iter = __builtin_choose_expr				\
         (__builtin_types_compatible_p (typeof ((L)->data), typeof (*D)),\
          (L),								\
          DIE);								\
       (_iter && ((D) = &_iter->data, 1)), _iter;			\
       _iter = _iter->prev)

#define list_fold_left(ACC, I, F, A, L)					\
  ({									\
    typeof (A) ACC = (A);						\
    list_iter ((I), (L))						\
      ACC = (F);							\
    ACC;								\
  })

#define list_maprev(RT, I, F, L)					\
  ({									\
    LIST (RT) _reslist = EMPTY (RT);					\
    list_fold_left (_acc, (I),						\
      ({ RT _cell = (F); list_cons (_cell, _acc); }),			\
      _reslist, (L));							\
  })

#define list_rev(L)							\
  do { (L) = list_rev_internal ((L)); } while (0)

void *list_rev_internal (void *);

#define list_map(RT, I, F, L)						\
  ({									\
    LIST (RT) _reslist = list_maprev (RT, I, F, L);			\
    list_rev (_reslist);						\
    _reslist;								\
  })

#define list_free(T, L)							\
  while ((L)) {								\
    typeof (L) _prev = (L)->prev;					\
    __builtin_choose_expr						\
      (__builtin_types_compatible_p (typeof ((L)->data), T),		\
       list_free_##T (&(L)->data),					\
       DIE);								\
    free ((L));								\
    (L) = _prev;							\
  }

#endif
