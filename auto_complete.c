#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
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
        /*printf("Prefix [%d:%s]\n", len, word);*/
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
    printf("Adding word [%s]\n", s);
    add_node(s, 0, &node->root);
    return 0;
}

static void split_words(struct auto_complete_node *node, const char *line, const char *token)
{
    char *line_copy, *s = NULL;
    char *tok;
    assert(node && token);
    s = line_copy = strdup(line);
    while ( (tok = strtok(s, token) ) )
    {
        const char *w;
        if(strlen(tok))
        {
            if(!add_word(node, (w = strdup(tok))))
                link_word(w, &node->list, 0);
        }
        if(s) s = NULL;
    }
    free(line_copy);
}

int read_words_file(struct auto_complete_node *node, const char *f, 
                    const char *tokens, 
                    int (*line_callback)(const char *line, char **words, int *num_words, int *free_word))
{
    FILE *fptr = NULL;
    char buf[0xff+1];
    assert(node && f);
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
                    if(!add_word(node, w))
                        link_word(w, &node->list, 0);

                }
            }
            free(words);
        }
        else
            split_words(node, buf, tokens);
    }
    fclose(fptr);
    return 0;
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
        printf("No prefix node found for word [%s]\n", word);
        return -1;
    }
    words = gather_words(strdup(word), loc->center, completion_list);
    assert(words == completion_list->nodes);
    if(!words)
    {
        printf("No completion found for the word [%s]\n", word);
    }
    else printf("[%d] Completions found for the word [%s]\n", words, word);
    return 0;
}

int init_auto_complete(struct auto_complete_node *node)
{
    assert(node);
    node->root = NULL;
    list_init(&node->list);
    node->initialized = 1;
    return 0;
}

int compute_levenshtein(const char *s, const char *d)
{
#undef MIN
#define __MIN(x,y) ( (x) < (y) ? (x) : (y) )
#define MIN(a,b,c) __MIN(a, __MIN(b, c))
    int *distance_matrix = NULL;
    int min_distance = 0;
    register int i, j;
    int n = strlen(s);
    int m = strlen(d);
    if(!n) return m;
    if(!m) return n;

    distance_matrix = calloc((n+1) * (m+1), sizeof(*distance_matrix));
    assert(distance_matrix);
    for(i = 0; i <= n; ++i)
        distance_matrix[i*(m+1)] = i;

    for(j = 0; j <= m; ++j)
        distance_matrix[j] = j;

    for(i = 1; i <= n; ++i)
    {
        char s1 = s[i-1];
        for(j = 1; j <= m ; ++j)
        {
            char d1 = d[j-1];
            int cost = 0;
            if(s1 != d1) cost = 1;
            distance_matrix[i*(m+1) + j] = 
                MIN(1 + distance_matrix[(i-1)*(m+1) + j],
                    1 + distance_matrix[i*(m+1) + (j-1)],
                    cost + distance_matrix[(i-1)*(m+1) + (j-1)]);
        }
    }
    min_distance = distance_matrix[n*(m+1) + m];
    free(distance_matrix);
    return min_distance;
#undef MIN
}
