#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define STACK_SIZE 256
#define WORD_NAME_SIZE 32

#define ASSERT(x)                                                              \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "%s:%d: %s: assertion \"%s\" failed\n", __FILE_NAME__,   \
              __LINE__, "???"/*__func__*/, #x);                                \
      abort();                                                                 \
    }                                                                          \
  } while (0)

typedef long cell;
typedef void (*word_fn)(void);

typedef struct Word {
	word_fn primitive; /* non-null if word is primitive */
	struct Word *body; /* non-null if word is composite */
	int body_len;      /* composite word len */
} Word;

typedef struct {
	Word *body;        /* body of current function */
	int pos;           /* instruction pointer */
	int len;           /* length of function */
} Frame;

typedef struct Entry { /* Dictionary of words */
    char name[WORD_NAME_SIZE]; /* identifier of word */
    Word word;                 /* word info */
    struct Entry *prev;        /* previous word */
} Entry;
Entry *dict_head = NULL;

/* Stack */
cell dstack[STACK_SIZE], *dsp = dstack + STACK_SIZE;
Frame rstack[STACK_SIZE]; int rsp = 0;

void push(cell v) {
	ASSERT(dsp > dstack); /* Overflow */
	*--dsp = v;
}
cell pop(void) {
	ASSERT(dsp < dstack + STACK_SIZE); /* Underflow */
	return *dsp++;
}

static void push_frame(Word *body, int pos, int len) {
	Frame tmp;

	tmp.body = body;
	tmp.pos  = pos;
	tmp.len  = len;
	rstack[rsp++] = tmp;
}

/* Dictionary utils */
Entry *dict_find(const char *name) {
	Entry *entry;
	for(entry = dict_head; entry; entry = entry->prev)
		if(strcmp(entry->name, name) == 0)
			return entry;
	return NULL;
}

void dict_push(const char *name, Word *word) {
	Entry *ent = malloc(sizeof(Entry));
	memset(ent, 0, sizeof(Entry));

	strncpy(ent->name, name, WORD_NAME_SIZE);
	ent->word = *word;
	if(dict_head)
		ent->prev = dict_head;
	dict_head = ent;
}

/* Tokenizer */
char *next_token(char **input) {
	char *tmp = *input, *tok, c;

	while((c = *tmp) != '\0' && (c == ' ' || c == '\n' || c == '\t')) tmp++;
	if(c == '\0') return NULL;

	tok = tmp;
	while((c = *tmp) != '\0' && c != ' ' && c != '\n' && c != '\t') tmp++;
	if(*tmp != '\0') *tmp++ = '\0';

	*input = tmp;
	return tok;
}

/* Primitives */
void prim_dup(void)  { cell a = pop(); push(a); push(a); }
void prim_plus(void) { cell b = pop(), a = pop(); push(a + b); }
void prim_mul(void)  { cell b = pop(), a = pop(); push(a * b); }
void prim_dot(void)  { printf("%ld ", (long)pop()); }
void prim_cr(void)   { printf("\n"); }

Word wprim_dup  = {prim_dup,  NULL, -1};
Word wprim_plus = {prim_plus, NULL, -1};
Word wprim_mul  = {prim_mul,  NULL, -1};
Word wprim_dot  = {prim_dot,  NULL, -1};
Word wprim_cr   = {prim_cr,   NULL, -1};


void run(Word *body, int len) {
	Frame *frame;
	Word *word;

	ASSERT(rsp == 0);

	/* Push first frame */
	push_frame(body, 0, len);

	while(rsp > 0) {
		/* Set current frame and word pointer */
		frame = &rstack[rsp - 1];
		word = &frame->body[frame->pos++];

		/* Execute current word if primitive */
		if(word->primitive) {
			word->primitive();
		}
		/* Push frame with user defined word */
		else if(word->body) {
			push_frame(word->body, 0, word->body_len);
			continue; /* Skip position checking for current frame */
		}

		/* Pop frame */
		if(frame->pos >= frame->len && rsp > 0 && --rsp) (void)0;
	}
}

void execute(Word *body, int len) {
	if(body->primitive)
		run(body, 1);
	else if(body->body)
		run(body->body, len);
}

void repl(void) {
	char new_word_name[WORD_NAME_SIZE];
	Word new_word_buf[128], *new_word;
	int new_word_status = 0, new_word_ip = 0, new_word_size = 0;

	char *token, *tmp, *endptr;
	char input[1024];
	long number = 0;
	int running = 1;
	Entry *ent;

	for(;running;) {
		tmp = input;
		if(!fgets(input, sizeof(input), stdin)) {
			return;
		}

		while((token = next_token(&tmp)) != NULL) {
			if(new_word_status == 0) {
				/* Switch to word define mode */
				if(strcmp(":", token) == 0) {
					new_word_status = 2;
					continue;
				}

				/* This is number */
				number = strtol(token, &endptr, 10);
				if(token != endptr) {
					if(*endptr == '\0')
						push(number);
					else
						printf("%s ? Unexpected %s in number token\n",
							   token, endptr);
					continue;
				}

				/* This is word */
				if((ent = dict_find(token))) {
					execute(&ent->word, ent->word.body_len);
					continue;
				}

				/* Exit */
				if(strcmp("BYE", token) == 0) {
					running = 0;
					break;
				}
			} else {
				if(new_word_status == 2) {
					if(strlen(token) >= WORD_NAME_SIZE) {
						printf("%s ? Too long word identifier (>%d chars)\n",
							   token, WORD_NAME_SIZE);
						new_word_status = 0;
						/* Ignore all next tokens */
						break;
					}
					strncpy(new_word_name, token, WORD_NAME_SIZE);
					new_word_status = 1;
					continue;
				}

				/* This is word */
				if((ent = dict_find(token))) {
					if(new_word_ip > 128) {
						printf("%s ? Too long word definition (>%d words)\n",
							   new_word_name, 128);
						new_word_ip = 0;
						new_word_status = 0;
						/* Ignore all next tokens */
						break;
					}
					new_word_buf[new_word_ip++] = ent->word;
					continue;
				}

				if(strcmp(";", token) == 0) {
					if(new_word_ip == 0) {
						printf("%s ? Empty word definition\n", new_word_name);
						new_word_status = 0;
					}
					/* Allocate memory for Word body and info */
					new_word_size = sizeof(Word) * new_word_ip;
					new_word = malloc(new_word_size + sizeof(Word));

					/* Push new word */
					memcpy(new_word + 1, new_word_buf, new_word_size);
					new_word->primitive = NULL;
					new_word->body      = new_word + 1;
					new_word->body_len  = new_word_ip;
					dict_push(new_word_name, new_word);

					/* Clean status */
					new_word_ip = 0;
					new_word_status = 0;
					continue;
				}
			}

			/* Error */
			printf("%s ? Unrecognized word\n", token);
		}
	}
}

int main(void) {
	dict_push("DUP", &wprim_dup);
	dict_push("+",   &wprim_plus);
	dict_push("*",   &wprim_mul);
	dict_push(".",   &wprim_dot);
	dict_push("CR",  &wprim_cr);

	repl();

	return 0;
}
