#include <stdio.h>
#include <stdlib.h>

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

typedef struct { Word *body; int pos; int len; } Frame;

#define STACK_SIZE 256
cell dstack[STACK_SIZE], *dsp = dstack + STACK_SIZE;
/* cell rstack[STACK_SIZE], *rsp = rstack + STACK_SIZE; */
Frame rstack[64]; int rsp = 0;

void push(cell v) {
	ASSERT(dsp > dstack); /* Overflow */
	*--dsp = v;
}
cell pop(void) {
	ASSERT(dsp < dstack + STACK_SIZE); /* Underflow */
	return *dsp++;
}

/* Primitives */
void prim_dup(void)  { cell a = pop(); push(a); push(a); }
void prim_plus(void) { cell b = pop(), a = pop(); push(a + b); }
void prim_mul(void)  { cell b = pop(), a = pop(); push(a * b); }
void prim_dot(void)  { printf("%ld ", (long)pop()); }
void prim_cr(void)   { printf("\n"); }


Word print_body[] =  {{prim_dot, NULL, -1},
					  {prim_cr,  NULL, -1}};
Word square_body[] = {{prim_dup, NULL, -1},
					  {prim_mul, NULL, -1}};
Word cube_body[] =   {{prim_dup, NULL,       -1},
					  {NULL,     square_body, 2},
					  {prim_mul, NULL,       -1}};


static void push_frame(Word *body, int pos, int len) {
	Frame tmp;

	tmp.body = body;
	tmp.pos  = pos;
	tmp.len  = len;
	rstack[rsp++] = tmp;
}

void run(Word *body, int len) {
	Frame *frame;
	Word *word;

	ASSERT(rsp == 0);

	/* Push first frame */
	push_frame(body, 0, len);

	while (rsp > 0) {
		/* Set current frame and word pointer */
		frame = &rstack[rsp - 1];
		word = &frame->body[frame->pos++];

		/* Execute current word if primitive */
		if (word->primitive) {
			word->primitive();
		} else if (word->body) {
			push_frame(word->body, 0, word->body_len);
			continue;
		}

		/* Jump to parent frame */
		if (frame->pos >= frame->len && rsp > 0 && --rsp) (void)0;
	}
}

int main(void) {
	push(5);
	run(square_body, 2);
	run(print_body, 2);

	push(5);
	run(cube_body, 2);
	run(print_body, 2);

	return 0;
}
