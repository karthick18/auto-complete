#ifndef _AUTO_COMPLETE_H_
#define _AUTO_COMPLETE_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "list.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WORD_TOKENS " ;/,\\|=()}{"

struct node;
struct auto_complete_node
{
    struct node *root;
    struct list_head list;
    int initialized;
};

struct word_list
{
    struct list list;
    char *word;
};

extern int init_auto_complete(struct auto_complete_node *node);
extern int add_word(struct auto_complete_node*, const char *w);
extern int lookup_word(struct auto_complete_node*, const char *w, int *is_word);
extern int complete_word(struct auto_complete_node*, const char *w, struct list_head *completion);
extern void link_word(const char *w, struct list_head *list, int tail);
extern int read_words_file(struct auto_complete_node *node, const char *f, const char *tokens,
                           int (*line_callback)(const char *line, char **words, int *num_words, int *free_words));

extern int compute_levenshtein(const char *source, const char *target);

#ifdef __cplusplus
}
#endif

#endif
