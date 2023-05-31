	//High-level (easy) picture loading function:
void kpzload (const char *, intptr_t *, int *, int *, int *);
	//Low-level PNG/JPG functions:
int kpgetdim (const char *buf, int leng, int *xsiz, int *ysiz);
int kprender (const char *buf, int leng, intptr_t frameptr, int bpl,
					int xdim, int ydim, int xoff, int yoff);
	//ZIP functions:
int kzaddstack (const char *);
void kzuninit ();
int kzopen (const char *);
int kzread (void *, int);
int kzfilelength ();
int kzseek (int, int);
int kztell ();
int kzgetc ();
int kzeof ();
void kzclose ();

void kzfindfilestart (const char *); //pass wildcard string
int kzfindfile (char *); //you alloc buf, returns 1:found,0:~found

