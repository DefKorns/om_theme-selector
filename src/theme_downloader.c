/*
 *  Copyright (C) 2026 DefKorns
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/* Fetch theme catalogs and packages over HTTPS using static curl+OpenSSL.
 * Fall back from GitHub to classicmods.net automatically. */

#include <ctype.h>
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern const unsigned char _binary_cacert_pem_start[];
extern const unsigned char _binary_cacert_pem_end[];

static const char *const GITHUB_BASE = "https://github.com/DefKorns/om-theme-downloads/releases/download/themes-v1";
static const char *const CLASSICMODS_SCRIPTS = "http://classicmods.net/files/themes/themeselector-scripts/";
static const char *const CLASSICMODS_THEMES = "http://classicmods.net/files/themes/themeselector/";
static const char *const LIST_SUFFIX = "-list-v2.tar.gz";
static const char *const CA_PATH = "/tmp/.theme_downloader_ca.pem";

enum { CONNECT_TIMEOUT_S = 10, STALL_TIMEOUT_S = 30, MAX_REDIRECTS = 10 };
enum { REMOVAL_POLL_US = 200000, TARBALLS_ON_DISK = 2 };
enum exit_code { EXIT_OK = 0, EXIT_FAILED = 1, EXIT_USAGE = 2, EXIT_NO_SPACE = 3 };

static void to_upper(char *dst, const char *src, size_t dst_size) {
    size_t i = 0;
    for (; src[i] && i + 1 < dst_size; i++) {
        dst[i] = (char)toupper((unsigned char)src[i]);
    }
    dst[i] = '\0';
}

/* Extract the embedded CA bundle into /tmp once per boot. */
static void ensure_ca_bundle(void) {
    FILE *f = fopen(CA_PATH, "rb");
    if (f) {
        fclose(f);
        return;
    }
    f = fopen(CA_PATH, "wb");
    if (!f) return;
    fwrite(_binary_cacert_pem_start, 1, (size_t)(_binary_cacert_pem_end - _binary_cacert_pem_start), f);
    fclose(f);
}

static CURL *make_handle(void) {
    CURL *c = curl_easy_init();
    if (!c) return NULL;
    curl_easy_setopt(c, CURLOPT_CAINFO, CA_PATH);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_MAXREDIRS, (long)MAX_REDIRECTS);
    curl_easy_setopt(c, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(c, CURLOPT_USERAGENT, "theme_downloader/1.0");
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, (long)CONNECT_TIMEOUT_S);
    curl_easy_setopt(c, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(c, CURLOPT_LOW_SPEED_TIME, (long)STALL_TIMEOUT_S);
    return c;
}

static size_t write_to_file(void *ptr, size_t size, size_t nmemb, void *userdata) {
    return fwrite(ptr, size, nmemb, (FILE *)userdata);
}

enum fetch_result { FETCH_FAILED, FETCH_OK, FETCH_WRITE_ERROR };

static enum fetch_result download_to(CURL *c, const char *url, const char *out_path) {
    char part_path[512];
    snprintf(part_path, sizeof(part_path), "%s.part", out_path);
    FILE *f = fopen(part_path, "wb");
    if (!f) return FETCH_WRITE_ERROR;
    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, write_to_file);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, f);
    CURLcode res = curl_easy_perform(c);
    int closed = fclose(f) == 0;
    if (res == CURLE_OK && closed && rename(part_path, out_path) == 0) return FETCH_OK;
    remove(part_path);
    return (res == CURLE_WRITE_ERROR || !closed) ? FETCH_WRITE_ERROR : FETCH_FAILED;
}

static enum fetch_result fetch_with_fallback(CURL *c, const char *gh_url, const char *fallback_url, const char *out_path) {
    enum fetch_result r = download_to(c, gh_url, out_path);
    if (r == FETCH_FAILED) r = download_to(c, fallback_url, out_path);
    return r;
}

static enum fetch_result fetch_theme(CURL *c, const char *sftype, const char *theme_name, const char *out_path) {
    char system_name[32];
    char gh_url[512];
    char fallback_url[512];
    to_upper(system_name, sftype, sizeof(system_name));
    snprintf(gh_url, sizeof(gh_url), "%s/%s.%s.tar.gz", GITHUB_BASE, system_name, theme_name);
    snprintf(fallback_url, sizeof(fallback_url), "%s%s.%s.tar.gz", CLASSICMODS_THEMES, system_name, theme_name);
    return fetch_with_fallback(c, gh_url, fallback_url, out_path);
}

static enum exit_code result_code(enum fetch_result r) {
    if (r == FETCH_WRITE_ERROR) return EXIT_NO_SPACE;
    return r == FETCH_OK ? EXIT_OK : EXIT_FAILED;
}

static void wait_until_removed(const char *path) {
    while (path[0] && access(path, F_OK) == 0) usleep(REMOVAL_POLL_US);
}

static enum exit_code fetch_theme_list(CURL *c, const char *sftype, const char *list_path) {
    FILE *list = fopen(list_path, "r");
    if (!list) return EXIT_FAILED;
    char system_name[32];
    to_upper(system_name, sftype, sizeof(system_name));
    char name[256];
    char out_path[TARBALLS_ON_DISK][512] = {{0}};
    int slot = 0;
    enum exit_code code = EXIT_OK;
    while (fgets(name, sizeof(name), list)) {
        name[strcspn(name, "\r\n")] = '\0';
        if (!name[0]) continue;
        wait_until_removed(out_path[slot]);
        snprintf(out_path[slot], sizeof(out_path[slot]), "%s.%s.tar.gz", system_name, name);
        code = result_code(fetch_theme(c, sftype, name, out_path[slot]));
        if (code != EXIT_OK) out_path[slot][0] = '\0';
        if (printf("%d %s\n", code, name) < 0 || fflush(stdout) != 0) break;
        if (code == EXIT_NO_SPACE) break;
        slot = (slot + 1) % TARBALLS_ON_DISK;
    }
    fclose(list);
    return code == EXIT_NO_SPACE ? EXIT_NO_SPACE : EXIT_OK;
}

static void usage(const char *prog) {
    fprintf(stderr,
            "usage:\n"
            "  %s catalog <sftype> <out_path>\n"
            "  %s theme <sftype> <theme_name> <out_path>\n"
            "  %s themes <sftype> <list_path>\n"
            "  %s all-manifest <sftype> <out_path>\n",
            prog, prog, prog, prog);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage(argv[0]);
        return EXIT_USAGE;
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);
    ensure_ca_bundle();
    CURL *c = make_handle();
    if (!c) {
        curl_global_cleanup();
        return EXIT_FAILED;
    }

    char gh_url[512];
    char fallback_url[512];
    enum exit_code code = EXIT_USAGE;

    if (strcmp(argv[1], "catalog") == 0 && argc == 4) {
        const char *sftype = argv[2];
        snprintf(gh_url, sizeof(gh_url), "%s/%s%s", GITHUB_BASE, sftype, LIST_SUFFIX);
        snprintf(fallback_url, sizeof(fallback_url), "%s%s%s", CLASSICMODS_SCRIPTS, sftype, LIST_SUFFIX);
        code = result_code(fetch_with_fallback(c, gh_url, fallback_url, argv[3]));
    } else if (strcmp(argv[1], "theme") == 0 && argc == 5) {
        code = result_code(fetch_theme(c, argv[2], argv[3], argv[4]));
    } else if (strcmp(argv[1], "themes") == 0 && argc == 4) {
        code = fetch_theme_list(c, argv[2], argv[3]);
    } else if (strcmp(argv[1], "all-manifest") == 0 && argc == 4) {
        const char *sftype = argv[2];
        snprintf(gh_url, sizeof(gh_url), "%s/%s-all", GITHUB_BASE, sftype);
        snprintf(fallback_url, sizeof(fallback_url), "%s%s-all", CLASSICMODS_SCRIPTS, sftype);
        code = result_code(fetch_with_fallback(c, gh_url, fallback_url, argv[3]));
    } else {
        usage(argv[0]);
    }

    curl_easy_cleanup(c);
    curl_global_cleanup();
    return code;
}
