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

extern const unsigned char _binary_cacert_pem_start[];
extern const unsigned char _binary_cacert_pem_end[];

static const char *GITHUB_BASE = "https://github.com/DefKorns/om-theme-downloads/releases/download/themes-v1";
static const char *CLASSICMODS_SCRIPTS = "http://classicmods.net/files/themes/themeselector-scripts/";
static const char *CLASSICMODS_THEMES = "http://classicmods.net/files/themes/themeselector/";
static const char *LIST_SUFFIX = "-list-v2.tar.gz";
static const char *CA_PATH = "/tmp/.theme_downloader_ca.pem";

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

static CURL *make_handle(const char *url, long connect_timeout, long total_timeout) {
    CURL *c = curl_easy_init();
    if (!c) return NULL;
    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_CAINFO, CA_PATH);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(c, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(c, CURLOPT_USERAGENT, "theme_downloader/1.0");
    if (connect_timeout > 0) curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, connect_timeout);
    if (total_timeout > 0) curl_easy_setopt(c, CURLOPT_TIMEOUT, total_timeout);
    return c;
}

static int url_reachable(const char *url) {
    CURL *c = make_handle(url, 5L, 5L);
    if (!c) return 0;
    curl_easy_setopt(c, CURLOPT_NOBODY, 1L);
    CURLcode res = curl_easy_perform(c);
    curl_easy_cleanup(c);
    return res == CURLE_OK;
}

static size_t write_to_file(void *ptr, size_t size, size_t nmemb, void *userdata) {
    return fwrite(ptr, size, nmemb, (FILE *)userdata);
}

static int download_to(const char *url, const char *out_path) {
    FILE *f = fopen(out_path, "wb");
    if (!f) return 0;
    CURL *c = make_handle(url, 10L, 0L);
    if (!c) {
        fclose(f);
        return 0;
    }
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, write_to_file);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, f);
    CURLcode res = curl_easy_perform(c);
    curl_easy_cleanup(c);
    fclose(f);
    if (res != CURLE_OK) {
        remove(out_path);
        return 0;
    }
    return 1;
}

static int fetch_with_fallback(const char *gh_url, const char *fallback_url, const char *out_path) {
    if (url_reachable(gh_url) && download_to(gh_url, out_path)) return 1;
    if (url_reachable(fallback_url) && download_to(fallback_url, out_path)) return 1;
    return 0;
}

static void usage(const char *prog) {
    fprintf(stderr,
            "usage:\n"
            "  %s catalog <sftype> <out_path>\n"
            "  %s theme <sftype> <theme_name> <out_path>\n"
            "  %s all-manifest <sftype> <out_path>\n",
            prog, prog, prog);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage(argv[0]);
        return 2;
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);
    ensure_ca_bundle();

    char gh_url[512];
    char fallback_url[512];
    int ok = 0;

    if (strcmp(argv[1], "catalog") == 0 && argc == 4) {
        const char *sftype = argv[2];
        const char *out_path = argv[3];
        snprintf(gh_url, sizeof(gh_url), "%s/%s%s", GITHUB_BASE, sftype, LIST_SUFFIX);
        snprintf(fallback_url, sizeof(fallback_url), "%s%s%s", CLASSICMODS_SCRIPTS, sftype, LIST_SUFFIX);
        ok = fetch_with_fallback(gh_url, fallback_url, out_path);
    } else if (strcmp(argv[1], "theme") == 0 && argc == 5) {
        const char *sftype = argv[2];
        const char *theme_name = argv[3];
        const char *out_path = argv[4];
        char system_name[32];
        to_upper(system_name, sftype, sizeof(system_name));
        snprintf(gh_url, sizeof(gh_url), "%s/%s.%s.tar.gz", GITHUB_BASE, system_name, theme_name);
        snprintf(fallback_url, sizeof(fallback_url), "%s%s.%s.tar.gz", CLASSICMODS_THEMES, system_name, theme_name);
        ok = fetch_with_fallback(gh_url, fallback_url, out_path);
    } else if (strcmp(argv[1], "all-manifest") == 0 && argc == 4) {
        const char *sftype = argv[2];
        const char *out_path = argv[3];
        snprintf(gh_url, sizeof(gh_url), "%s/%s-all", GITHUB_BASE, sftype);
        snprintf(fallback_url, sizeof(fallback_url), "%s%s-all", CLASSICMODS_SCRIPTS, sftype);
        ok = fetch_with_fallback(gh_url, fallback_url, out_path);
    } else {
        usage(argv[0]);
        curl_global_cleanup();
        return 2;
    }

    curl_global_cleanup();
    return ok ? 0 : 1;
}
