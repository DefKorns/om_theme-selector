# Cross-compiles theme_downloader against the static libcurl+OpenSSL built
# into the toolchain image (toolchain/Dockerfile.jessie-armhf). Run via
# ../build.sh theme_downloader (from the repo root, same Docker toolchain
# as the theme_manager build), not directly - CROSS_PREFIX must point at
# the arm-linux-gnueabihf- cross tools.
CURL_PREFIX = /opt/curl-static
CC = gcc
STRIP = strip
CFLAGS = -O2 -Wall -I$(CURL_PREFIX)/include
# NOT -static: glibc's getaddrinfo needs libnss_dns.so.2, loaded at runtime
# via dlopen - a fully static binary can't do that and fails DNS resolution
# instantly. curl/ssl/crypto/zlib link statically anyway since only .a
# versions of them exist in this toolchain; libc/libdl/libpthread/libresolv
# stay dynamic, resolved against the console's own libc at runtime (same as
# theme_manager/sprite_crop already do for SDL2/libpng).
LDLIBS = -L$(CURL_PREFIX)/lib -L/usr/lib/arm-linux-gnueabihf -lcurl -lssl -lcrypto -lz -ldl -lpthread

all: theme_downloader

# gitignored (not source we own) - fetched here so a fresh clone can still
# build instead of failing on a missing file
cacert.pem:
	curl -fL -o cacert.pem https://curl.se/ca/cacert.pem

# ld -r -b binary embeds cacert.pem verbatim as a linkable object, exposing
# _binary_cacert_pem_start/_end - avoids generating a multi-thousand-line
# .c array just to get the CA bundle into the binary.
ca_bundle.o: cacert.pem
	$(CROSS_PREFIX)ld -r -b binary -o ca_bundle.o cacert.pem

theme_downloader: theme_downloader.c ca_bundle.o
	$(CROSS_PREFIX)$(CC) $(CFLAGS) theme_downloader.c ca_bundle.o $(LDLIBS) -o theme_downloader
	$(CROSS_PREFIX)$(STRIP) theme_downloader
	upx --best theme_downloader

clean:
	rm -f theme_downloader ca_bundle.o

.PHONY: all clean
