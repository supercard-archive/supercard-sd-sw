#include <stdio.h>
#include <string.h>

static int usage(char *a0) {
	fprintf(stderr,
	"usage: %s FILE1 FILE2\ncompares FILE1 and FILE2.\n"
	"if both file's contents are identical, returns 0, else 1.\n"
	"on error, -1 (255) is returned.\n"
	, a0);
	return 1;
}

int main(int argc, char** argv) {
	if(argc != 3) return usage(argv[0]);
	FILE *f[2] = {0};
	int i, ret = 0;
	char *e = "fopen";
	for(i=0; i<2; ++i) if((f[i] = fopen(argv[i+1], "r")) == 0) {
	err:
		fprintf(stderr, "error: %s (%m)\n", e);
		ret = -1;
		goto out;
	}
	e = "fseek";
	off_t off[2];
	for(i=0; i<2; ++i) if(fseeko(f[i], 0, SEEK_END)) goto err;
	for(i=0; i<2; ++i) off[i] = ftello(f[i]);
	if(off[0] != off[1]) { ret = 1; goto out; }
	for(i=0; i<2; ++i) if(fseeko(f[i], 0, SEEK_SET)) goto err;
	unsigned char buf[2][16*1024];
	e = "fread";
	while(1) {
		size_t n[2];
		for(i=0; i<2; ++i)
			n[i] = fread(buf[i], 1, sizeof(buf[i]), f[i]);
		if(n[0] != n[1]) goto err;
		if(n[0] == 0) break;
		if(memcmp(buf[0], buf[1], n[0])) { ret = 1; break; }
	}
out:
	for(i=0; i<2; ++i) if(f[i]) fclose(f[i]);
	return ret;
}
