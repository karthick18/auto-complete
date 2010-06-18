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

#define likely(expr) __builtin_expect(!!(expr), 1)

#define unlikely(expr) __builtin_expect(!!(expr), 0)

#define do_log(fmt, arg...) do { if(unlikely(auto_complete_opts.verbose)) fprintf(stderr, fmt, ##arg); }while(0)

struct node;
struct auto_complete_node
{
    struct node *root;
    struct list_head list;
    int initialized;
};

struct auto_complete_opts
{
    unsigned int verbose: 1; /*for logging*/
};

struct word_list
{
    struct list list;
    char *word;
};

struct spell_check_word
{
    char *word; /*to be freed*/
    int weight; /*levenshtein value: lesser the better*/
};

extern struct auto_complete_opts auto_complete_opts;
extern int init_auto_complete(struct auto_complete_node *node, const char *wordfile);
extern int load_auto_complete(struct auto_complete_node *node, const char *wordfile); 
extern int add_word(struct auto_complete_node*, const char *w);
extern int lookup_word(struct auto_complete_node*, const char *w, int *is_word);
extern int complete_word(struct auto_complete_node*, const char *w, struct list_head *completion);
extern void link_word(const char *w, struct list_head *list, int tail);
extern int read_words(struct auto_complete_node *node, struct list_head *list, 
                      const char *wordfile, const char *tokens,
                      int (*line_callback)(const char *line, char **words, int *num_words, int *free_words));
extern int compute_levenshtein(const char *source, const char *target);
extern int spell_check(struct auto_complete_node *auto_complete, const char *w, struct spell_check_word *wordlist, int *num_words);
extern void spell_check_cache_init(void);

#ifdef __cplusplus
}
#endif

#endif
