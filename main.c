/*
 * Copyright (c) 2026 horoni (https://github.com/horoni)
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define STACK_SIZE 256
#define WORD_NAME_SIZE 32

#define ASSERT(x)                                                              \
  do{                                                                          \
    if (!(x)) {                                                                \
      fprintf(stderr, "%s:%d: %s: assertion \"%s\" failed\n", __FILE_NAME__,   \
              __LINE__, "???"/*__func__*/, #x);                                \
      abort();                                                                 \
    }                                                                          \
  } while(0)

typedef long cell;
typedef void (*word_fn)(void);

enum WORD_TYPE {
	WORD_PRIMITIVE,
	WORD_COMPOSITE,
	WORD_LITERAL
};

typedef struct Word {
	enum WORD_TYPE type;   /* type of word */
	union {
		word_fn primitive; /* non-null if word is primitive */
		struct Word *body; /* non-null if word is composite */
		cell literal;      /* number literal */
	} v;
	int body_len;          /* composite word len */
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
cell dstack[STACK_SIZE], *dsp = dstack;
Frame rstack[STACK_SIZE]; int rsp = 0;

void push(cell v) {
	ASSERT(dsp < dstack + STACK_SIZE); /* Overflow */
	*dsp++ = v;
}
cell pop(void) {
	ASSERT(dsp > dstack); /* Underflow */
	return *--dsp;
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
#define FORTH_TRUE  (~((cell)0))
#define FORTH_FALSE   ((cell)0)

void prim_swap(void) { cell a = pop(), b = pop(); push(a); push(b); }
void prim_dup(void)  { cell a = pop(); push(a); push(a); }
void prim_over(void) { cell a = pop(), b = pop(); push(b); push(a); push(b); }
void prim_rot(void)  { cell a = pop(), b = pop(), c = pop(); push(b); push(a); push(c); }
void prim_drop(void) { pop(); }
void prim_eq(void)   { cell b = pop(), a = pop(); push(a == b ? FORTH_TRUE : FORTH_FALSE); }
void prim_ne(void)   { cell b = pop(), a = pop(); push(a != b ? FORTH_TRUE : FORTH_FALSE); }
void prim_lt(void)   { cell b = pop(), a = pop(); push(a <  b ? FORTH_TRUE : FORTH_FALSE); }
void prim_le(void)   { cell b = pop(), a = pop(); push(a <= b ? FORTH_TRUE : FORTH_FALSE); }
void prim_gt(void)   { cell b = pop(), a = pop(); push(a >  b ? FORTH_TRUE : FORTH_FALSE); }
void prim_ge(void)   { cell b = pop(), a = pop(); push(a >= b ? FORTH_TRUE : FORTH_FALSE); }
void prim_plus(void) { cell b = pop(), a = pop(); push(a + b); }
void prim_mul(void)  { cell b = pop(), a = pop(); push(a * b); }
void prim_dot(void)  { printf("%ld ", (long)pop()); }
void prim_cr(void)   { printf("\n"); }

Word wprim_swap = {WORD_PRIMITIVE, {prim_swap}, -1};
Word wprim_dup  = {WORD_PRIMITIVE, {prim_dup }, -1};
Word wprim_over = {WORD_PRIMITIVE, {prim_over}, -1};
Word wprim_rot  = {WORD_PRIMITIVE, {prim_rot }, -1};
Word wprim_drop = {WORD_PRIMITIVE, {prim_drop}, -1};
Word wprim_eq   = {WORD_PRIMITIVE, {prim_eq  }, -1};
Word wprim_ne   = {WORD_PRIMITIVE, {prim_ne  }, -1};
Word wprim_lt   = {WORD_PRIMITIVE, {prim_lt  }, -1};
Word wprim_le   = {WORD_PRIMITIVE, {prim_le  }, -1};
Word wprim_gt   = {WORD_PRIMITIVE, {prim_gt  }, -1};
Word wprim_ge   = {WORD_PRIMITIVE, {prim_ge  }, -1};
Word wprim_plus = {WORD_PRIMITIVE, {prim_plus}, -1};
Word wprim_mul  = {WORD_PRIMITIVE, {prim_mul }, -1};
Word wprim_dot  = {WORD_PRIMITIVE, {prim_dot }, -1};
Word wprim_cr   = {WORD_PRIMITIVE, {prim_cr  }, -1};

void prim__register(void) {
	dict_push("SWAP", &wprim_swap);
	dict_push("DUP",  &wprim_dup);
	dict_push("OVER", &wprim_over);
	dict_push("ROT",  &wprim_rot);
	dict_push("DROP", &wprim_drop);
	dict_push("=",    &wprim_eq);
	dict_push("<>",   &wprim_ne);
	dict_push("<",    &wprim_lt);
	dict_push("<=",   &wprim_le);
	dict_push(">",    &wprim_gt);
	dict_push(">=",   &wprim_ge);
	dict_push("+",    &wprim_plus);
	dict_push("*",    &wprim_mul);
	dict_push(".",    &wprim_dot);
	dict_push("CR",   &wprim_cr);
}


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
		if(word->type == WORD_PRIMITIVE) {
			word->v.primitive();
		}
		/* Push frame with user defined word */
		else if(word->type == WORD_COMPOSITE) {
			push_frame(word->v.body, 0, word->body_len);
			continue; /* Skip position checking for current frame */
		}
		/* Push literal on top of stack */
		else if(word->type == WORD_LITERAL) {
			push(word->v.literal);
		}

		/* Pop frame */
		if(frame->pos >= frame->len && rsp > 0 && --rsp) (void)0;
	}
}

void execute(Word *body, int len) {
	if(body->type == WORD_PRIMITIVE)
		run(body, 1);
	else if(body->type == WORD_COMPOSITE)
		run(body->v.body, len);
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

				/* This is word */
				if((ent = dict_find(token))) {
					execute(&ent->word, ent->word.body_len);
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
					if(new_word_ip >= 128) {
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

				/* This is number */
				number = strtol(token, &endptr, 10);
				if(token != endptr) {
					if(*endptr == '\0') {
						if(new_word_ip >= 128) {
							printf("%s ? Too long word definition (>%d words)\n",
								   new_word_name, 128);
							new_word_ip = 0;
							new_word_status = 0;
							/* Ignore all next tokens */
							break;
						}

						new_word_buf[new_word_ip].type      = WORD_LITERAL;
						new_word_buf[new_word_ip].v.literal = number;
						new_word_buf[new_word_ip].body_len  = -1;
						new_word_ip++;
					} else {
						printf("%s ? Unexpected %s in number literal\n",
							   token, endptr);
						new_word_ip = 0;
						new_word_status = 0;
						/* Ignore all next tokens */
						break;
					}
					continue;
				}

				/* End of word definition */
				if(strcmp(";", token) == 0) {
					if(new_word_ip == 0) {
						printf("%s ? Empty word definition\n", new_word_name);
						new_word_status = 0;
						break;
					}
					/* Allocate memory for Word body and info */
					new_word_size = sizeof(Word) * new_word_ip;
					new_word = malloc(new_word_size + sizeof(Word));

					/* Push new word */
					memcpy(new_word + 1, new_word_buf, new_word_size);
					new_word->type     = WORD_COMPOSITE;
					new_word->v.body   = new_word + 1;
					new_word->body_len = new_word_ip;
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
	prim__register();

	repl();

	return 0;
}
