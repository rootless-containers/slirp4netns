CFLAGS := -Ird235_libslirp/include -Ird235_libslirp/src -Iqemu/include -Iqemu/slirp $(CFLAGS)

slirp4netns: slirp.o main.o $(subst .c,.o,$(wildcard qemu/slirp/*.c) $(wildcard rd235_libslirp/src/*.c))
	$(CC) -o $@ $^

clean:
	$(RM) slirp4netns slirp.o main.o $(subst .c,.o,$(wildcard qemu/slirp/*.c) $(wildcard rd235_libslirp/src/*.c))
