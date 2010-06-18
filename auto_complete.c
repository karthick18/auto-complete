#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <time.h>
#include "auto_complete.h"

struct node
{
    struct node *left;
    struct node *right;
    struct node *center;
    char c;
    unsigned int word:1;
};

struct auto_complete_opts auto_complete_opts;

void link_word(const char *w, struct list_head *list, int tail)
{
    struct word_list *l = calloc(1, sizeof(*l));
    assert(l);
    l->word = (char*)w;
    if(tail)
        list_add_tail(&l->list, list);
    else
        list_add(&l->list, list);
} 

static struct node *make_node(char c, struct node **p)
{
    struct node *n ;
    assert(p);
    n = calloc(1, sizeof(*n));
    assert(n);
    n->c = c;
    n->word = 0;
    *p = n;
    return n;
}

static void add_node(const char *s, int pos, struct node **node)
{
    struct node *p = NULL;
    assert(node);
    if(!(p = *node))
    {
        p = make_node(s[pos], node);
    }
    /*
     * add the characters in the word in the ternary tree.
     */
    if(s[pos] < p->c )
        add_node(s, pos, &p->left);
    else if(s[pos] > p->c)
        add_node(s, pos, &p->right);
    else 
    {
        if(pos+1 == strlen(s)) p->word = 1;
        else 
            add_node(s, pos+1, &p->center);
    }
}

static int gather_words(char *word, struct node *node, struct list_head *head)
{
    struct node *iter;
    char *prefix = NULL;
    int words = 0;
    for(iter = node; iter; iter = iter->center)
    {
        int turns = 0;
        int len = strlen(word);
        prefix = strdup(word);
        /*do_log("Prefix [%d:%s]\n", len, word);*/
        word = realloc(word, len+2);
        assert(word);
        word[len] = iter->c;
        word[len+1] = 0;
        if(iter->word)  /*end of a word*/
        {
            ++words;
            link_word(strdup(word), head, 1);
        }
        if(iter->left)
        {
            char *prefix2 = NULL;
            if(iter->right)
            {
                prefix2 = strdup(prefix);
                assert(prefix2);
            }
            words += gather_words(prefix, iter->left, head);
            turns = 1;
            if(prefix2) prefix = prefix2;
        }
        if(iter->right)
        {
            words += gather_words(prefix, iter->right, head);
            turns = 1;
        }
        if(!turns) free(prefix);
    }
    if(word) free(word);
    return words;
}

static int find_word(struct node *root, const char *s, int *is_word, 
                     struct node **ret_node)
{
    struct node *node = root;
    int pos = 0;
    int cmp;
    assert(s);
    if(strlen(s) == 0) return -1;
    while(node)
    {
        if( (cmp = s[pos] - node->c ) < 0)
            node = node->left;
        else if(cmp > 0)
            node = node->right;
        else
        {
            if(++pos == strlen(s))
            {
                if(is_word)
                    *is_word = node->word;
                if(ret_node)
                    *ret_node = node;
                return 0;
            }
            node = node->center;
        }
    }
    return -1;
}

int lookup_word(struct auto_complete_node *node, const char *s, int *is_word)
{
    assert(node && s);
    assert(node->initialized == 1);
    return find_word(node->root, s, is_word, NULL);
}

int add_word(struct auto_complete_node *node, const char *s)
{
    assert(node && s );
    assert(node->initialized == 1);
    do_log("Adding word [%s]\n", s);
    add_node(s, 0, &node->root);
    return 0;
}

static void split_words(struct auto_complete_node *node, struct list_head *list, const char *line, const char *token)
{
    char *line_copy, *s = NULL;
    char *tok;
    if(!token) return;
    s = line_copy = strdup(line);
    while ( (tok = strtok(s, token) ) )
    {
        if(strlen(tok))
        {
            const char *w = strdup(tok);
            if(node)
                add_word(node, w);
            if(list)
                link_word(w, list, 1);
        }
        if(s) s = NULL;
    }
    free(line_copy);
}

int read_words(struct auto_complete_node *node, struct list_head *list,
               const char *f,  const char *tokens,
               int (*line_callback)(const char *line, char **words, int *num_words, int *free_word))
{
    FILE *fptr = NULL;
    char buf[0xff+1];
    if(!f) return -1;
    if(node)
        assert(node->initialized == 1);
    if(!tokens) tokens = WORD_TOKENS;
    fptr = fopen(f, "r");
    if(!fptr) return -1;
    while(fgets(buf, sizeof(buf), fptr))
    {
        register char *s = buf;
        int l = strlen(buf);
        if(buf[l-1] == '\n')
            buf[l-1] = 0;
        while(*s && isspace(*s)) ++s;
        if(!*s || *s == '#') /*skip comments*/
            continue;
        /*
         * split the file into words
         */
        if(line_callback)
        {
            /*
             * Allocate capacity for as many pointers as the line len.
             */
            int num_words = 0;
            int free_words = 1;
            char **words = calloc(l, sizeof(*words));
            assert(words != NULL);
            if(!line_callback(buf, words, &num_words, &free_words))
            {
                register int i;
                for(i = 0; i < num_words; ++i)
                {
                    char *w = words[i];
                    if(!w || !*w) continue;
                    if(!free_words)
                    {
                        w = strdup(words[i]);
                        assert(w);
                    }
                    if(node)
                        add_word(node, w);
                    if(list)
                        link_word(w, list, 1);

                }
            }
            free(words);
        }
        else
            split_words(node, list, buf, tokens);
    }

    fclose(fptr);
    return 0;
}

int load_auto_complete(struct auto_complete_node *node, const char *f)
{
    if(!node || !f) return -1;
    assert(node->initialized == 1);
    return read_words(node, NULL, f, NULL, NULL); /*load wordlists into the auto complete db*/
}

int complete_word(struct auto_complete_node *node, const char *word, struct list_head *completion_list)
{
    int status = 0;
    int words = 0;
    struct node *loc = NULL;
    if(!node || !word || !completion_list) return -1;
    assert(node->initialized == 1);
    find_word(node->root, word, &status, &loc);
    if(!loc)
    {
        do_log("No prefix node found for word [%s]\n", word);
        return -1;
    }
    words = gather_words(strdup(word), loc->center, completion_list);
    assert(words == completion_list->nodes);
    if(!words)
    {
        do_log("No completion found for the word [%s]\n", word);
    }
    else do_log("[%d] Completions found for the word [%s]\n", words, word);
    return 0;
}

int init_auto_complete(struct auto_complete_node *node, const char *wordfile)
{
    int err = 0;
    assert(node);
    node->root = NULL;
    list_init(&node->list);
    spell_check_cache_init();
    node->initialized = 1;
    if(wordfile)
        err = load_auto_complete(node, wordfile); /*suck in the dictionary*/
    return err;
}

