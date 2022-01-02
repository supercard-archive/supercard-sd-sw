/* wrapper around srampatch.exe and gep.exe to see when and how they are
   called from Supercardsd.exe. compile with PellesC 8.0 to an exe, then rename
   to either srampatch.exe, gep.exe, or both (backup originals, ofc).
   output will end up in supercardsd's temp directory.
   if a file "glitch.txt" exists in that directory, this wrapper will generate
   an empty output file, which is identical to what srampatch.exe does due to
   a bug when patching a rom with EEPROM v1[01]* type saver, so it can be
   observed how supercardsd.exe then calls gep.exe when this happens. */

#define WORKMODE_COPY

#undef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <time.h>
#include <stddef.h>

size_t timestamp(char* buffer, size_t bufsize) {
        time_t secs = time(NULL);
        struct tm *tim = localtime(&secs);
        return snprintf(buffer, bufsize, "%.4d-%.2d-%.2d %.2d:%.2d:%.2d",
			1900 + tim->tm_year, tim->tm_mon + 1, tim->tm_mday, tim->tm_hour,
			tim->tm_min, tim->tm_sec);
}


#define WRAPPER_LOG "wrapper.log"

static void logit(int argc, char **argv) {
	char ts[256];
	timestamp(ts, sizeof(ts));
	char args[2048], *p = args;
	int i;
	for(i=0; i<argc; ++i)
		p += sprintf(p, "%s ", argv[i]);
	FILE *f = fopen(WRAPPER_LOG, "a");
	fprintf(f, "[%s] %s\n", ts, args);
	fclose(f);
}

int srampatch = 0;

static void copy_file(char* fn[2]) {
	FILE *f[2];
	int glitch = 0;
	if(srampatch && (f[0] = fopen("glitch.txt", "r"))) {
		fclose(f[0]);
		++glitch;
	}
	int i;
	for(i = 0; i < 2; ++i)
		f[i] = fopen(fn[i], i == 0 ? "rb" : "wb");
	unsigned char buf[16384];
	if(!glitch) while(1) {
		size_t n = fread(buf, 1, sizeof buf, f[0]);
		if(n == 0) break;
		fwrite(buf, 1, n, f[1]);
	}
	for(i = 0; i < 2; ++i)
		fclose(f[i]);
}

int main(int argc, char **argv) {
	logit(argc, argv);
#ifdef WORKMODE_COPY
	int startarg = 1;
	if(argc == 4) {
		++startarg; // srampatch mode
		++srampatch;
	}
	char *fn[2] = {argv[startarg], argv[startarg+1]};
	copy_file(fn);
	return 0;
#else
	// todo: call real executable with args
	#error "not implemented"
#endif
}
