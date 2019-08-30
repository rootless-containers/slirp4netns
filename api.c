/* SPDX-License-Identifier: GPL-2.0-or-later */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/un.h>
#include <glib.h>
#include "vendor/parson/parson.h"
#include "vendor/libslirp/src/libslirp.h"
#include "api.h"
#include "slirp4netns.h"

int api_bindlisten(const char *api_socket)
{
    int fd;
    struct sockaddr_un addr;
    unlink(api_socket); /* avoid EADDRINUSE */
    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        perror("api_bindlisten: socket");
        return -1;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, api_socket, sizeof(addr.sun_path) - 1);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("api_bindlisten: bind");
        return -1;
    }
    if (listen(fd, 0) < 0) {
        perror("api_bindlisten: listen");
        return -1;
    }
    return fd;
}

struct api_hostfwd {
    int id;
    int is_udp;
    struct in_addr host_addr;
    int host_port;
    struct in_addr guest_addr;
    int guest_port;
};

struct api_ctx {
    uint8_t *buf;
    size_t buflen;
    GList *hostfwds;
    int hostfwds_nextid;
    struct slirp4netns_config *cfg;
};

struct api_ctx *api_ctx_alloc(struct slirp4netns_config *cfg)
{
    struct api_ctx *ctx = (struct api_ctx *)g_malloc0(sizeof(*ctx));
    if (ctx == NULL) {
        return NULL;
    }
    ctx->buflen = 4096;
    ctx->buf = malloc(ctx->buflen); /* FIXME: realloc */
    if (ctx->buf == NULL) {
        free(ctx);
        return NULL;
    }
    ctx->cfg = cfg;
    ctx->hostfwds = NULL;
    ctx->hostfwds_nextid = 1;
    return ctx;
}

void api_ctx_free(struct api_ctx *ctx)
{
    if (ctx != NULL) {
        if (ctx->buf != NULL) {
            free(ctx->buf);
        }
        g_list_free_full(ctx->hostfwds, g_free);
        free(ctx);
    }
}

/*
  Handler for add_hostfwd.
  e.g. {"execute": "add_hostfwd", "arguments": {"proto": "tcp", "host_addr":
  "0.0.0.0", "host_port": 8080, "guest_addr": "10.0.2.100", "guest_port": 80}}
  This function returns the return value of write(2), not the return value of
  slirp_add_hostfwd().
 */
static int api_handle_req_add_hostfwd(Slirp *slirp, int fd, struct api_ctx *ctx,
                                      JSON_Object *jo)
{
    int wrc = 0, slirprc = 0;
    char idbuf[64];
    const char *proto_s = json_object_dotget_string(jo, "arguments.proto");
    const char *host_addr_s =
        json_object_dotget_string(jo, "arguments.host_addr");
    const char *guest_addr_s =
        json_object_dotget_string(jo, "arguments.guest_addr");
    struct api_hostfwd *fwd = g_malloc0(sizeof(*fwd));
    if (fwd == NULL) {
        perror("fatal: malloc");
        exit(EXIT_FAILURE);
    }
    fwd->is_udp = -1; /* TODO: support SCTP */
    if (strcmp(proto_s, "udp") == 0) {
        fwd->is_udp = 1;
    } else if (strcmp(proto_s, "tcp") == 0) {
        fwd->is_udp = 0;
    }
    if (fwd->is_udp == -1) {
        const char *err = "{\"error\":{\"desc\":\"bad request: add_hostfwd: "
                          "bad arguments.proto\"}}";
        wrc = write(fd, err, strlen(err));
        free(fwd);
        goto finish;
    }
    if (host_addr_s == NULL || host_addr_s[0] == '\0') {
        host_addr_s = "0.0.0.0";
    }
    if (inet_pton(AF_INET, host_addr_s, &fwd->host_addr) != 1) {
        const char *err = "{\"error\":{\"desc\":\"bad request: add_hostfwd: "
                          "bad arguments.host_addr\"}}";
        wrc = write(fd, err, strlen(err));
        free(fwd);
        goto finish;
    }
    fwd->host_port = (int)json_object_dotget_number(jo, "arguments.host_port");
    if (fwd->host_port == 0) {
        const char *err = "{\"error\":{\"desc\":\"bad request: add_hostfwd: "
                          "bad arguments.host_port\"}}";
        wrc = write(fd, err, strlen(err));
        free(fwd);
        goto finish;
    }

    if (guest_addr_s == NULL || guest_addr_s[0] == '\0') {
        fwd->guest_addr = ctx->cfg->recommended_vguest;
    } else if (inet_pton(AF_INET, guest_addr_s, &fwd->guest_addr) != 1) {
        const char *err = "{\"error\":{\"desc\":\"bad request: add_hostfwd: "
                          "bad arguments.guest_addr\"}}";
        wrc = write(fd, err, strlen(err));
        free(fwd);
        goto finish;
    }
    fwd->guest_port =
        (int)json_object_dotget_number(jo, "arguments.guest_port");
    if (fwd->guest_port == 0) {
        const char *err = "{\"error\":{\"desc\":\"bad request: add_hostfwd: "
                          "bad arguments.guest_port\"}}";
        wrc = write(fd, err, strlen(err));
        free(fwd);
        goto finish;
    }
    if ((slirprc = slirp_add_hostfwd(slirp, fwd->is_udp, fwd->host_addr,
                                     fwd->host_port, fwd->guest_addr,
                                     fwd->guest_port)) < 0) {
        const char *err = "{\"error\":{\"desc\":\"bad request: add_hostfwd: "
                          "slirp_add_hostfwd failed\"}}";
        wrc = write(fd, err, strlen(err));
        free(fwd);
        goto finish;
    }
    fwd->id = ctx->hostfwds_nextid;
    ctx->hostfwds_nextid++;
    ctx->hostfwds = g_list_append(ctx->hostfwds, fwd);
    if (snprintf(idbuf, sizeof(idbuf), "{\"return\":{\"id\":%d}}", fwd->id) >
        sizeof(idbuf)) {
        fprintf(stderr, "fatal: unexpected id=%d\n", fwd->id);
        exit(EXIT_FAILURE);
    }
    wrc = write(fd, idbuf, strlen(idbuf));
finish:
    return wrc;
}

static void api_handle_req_list_hostfwd_foreach(gpointer data,
                                                gpointer user_data)
{
    struct api_hostfwd *fwd = data;
    JSON_Array *entries_array = (JSON_Array *)user_data;
    JSON_Value *entry_value = json_value_init_object();
    JSON_Object *entry_object = json_value_get_object(entry_value);
    char host_addr[INET_ADDRSTRLEN], guest_addr[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &fwd->host_addr, host_addr, sizeof(host_addr)) ==
        NULL) {
        perror("fatal: inet_ntop");
        exit(EXIT_FAILURE);
    }
    if (inet_ntop(AF_INET, &fwd->guest_addr, guest_addr, sizeof(guest_addr)) ==
        NULL) {
        perror("fatal: inet_ntop");
        exit(EXIT_FAILURE);
    }
    json_object_set_number(entry_object, "id", fwd->id);
    json_object_set_string(entry_object, "proto", fwd->is_udp ? "udp" : "tcp");
    json_object_set_string(entry_object, "host_addr", host_addr);
    json_object_set_number(entry_object, "host_port", fwd->host_port);
    json_object_set_string(entry_object, "guest_addr", guest_addr);
    json_object_set_number(entry_object, "guest_port", fwd->guest_port);
    /* json_array_append_value does not copy passed value */
    if (json_array_append_value(entries_array, entry_value) != JSONSuccess) {
        fprintf(stderr, "fatal: json_array_append_value\n");
        exit(EXIT_FAILURE);
    }
}

/*
  Handler for list_hostfwd.
  e.g. {"execute": "list_hostfwd"}
*/
static int api_handle_req_list_hostfwd(Slirp *slirp, int fd,
                                       struct api_ctx *ctx, JSON_Object *jo)
{
    int wrc = 0;
    JSON_Value *root_value = json_value_init_object(),
               *entries_value = json_value_init_array();
    JSON_Object *root_object = json_value_get_object(root_value);
    JSON_Array *entries_array = json_array(entries_value);
    char *serialized_string = NULL;
    g_list_foreach(ctx->hostfwds, api_handle_req_list_hostfwd_foreach,
                   entries_array);
    json_object_set_value(root_object, "entries", entries_value);
    serialized_string = json_serialize_to_string(root_value);
    wrc = write(fd, serialized_string, strlen(serialized_string));
    json_free_serialized_string(serialized_string);
    json_value_free(root_value);
    return wrc;
}

static int api_handle_remove_list_hostfwd_find(gconstpointer gcp_fwd,
                                               gconstpointer gcp_id)
{
    struct api_hostfwd *fwd = (struct api_hostfwd *)gcp_fwd;
    int id = *(int *)gcp_id;
    return id == fwd->id ? 0 : 1;
}

/*
  Handler for remove_hostfwd.
  e.g. {"execute": "remove_hostfwd", "arguments": {"id": 42}}
*/
static int api_handle_req_remove_hostfwd(Slirp *slirp, int fd,
                                         struct api_ctx *ctx, JSON_Object *jo)
{
    int wrc = 0;
    int id = (int)json_object_dotget_number(jo, "arguments.id");
    GList *found = g_list_find_custom(ctx->hostfwds, &id,
                                      api_handle_remove_list_hostfwd_find);
    if (found == NULL) {
        const char *err = "{\"error\":{\"desc\":\"bad request: remove_hostfwd: "
                          "bad arguments.id\"}}";
        wrc = write(fd, err, strlen(err));
    } else {
        struct api_hostfwd *fwd = found->data;
        const char *api_ok = "{\"return\":{}}";
        if (slirp_remove_hostfwd(slirp, fwd->is_udp, fwd->host_addr,
                                 fwd->host_port) < 0) {
            const char *err = "{\"error\":{\"desc\":\"bad request: "
                              "remove_hostfwd: slirp_remove_hostfwd failed\"}}";
            wrc = write(fd, err, strlen(err));
        } else {
            ctx->hostfwds = g_list_remove(ctx->hostfwds, fwd);
            g_free(fwd);
            wrc = write(fd, api_ok, strlen(api_ok));
        }
    }
    return wrc;
}

static int api_handle_req(Slirp *slirp, int fd, struct api_ctx *ctx)
{
    JSON_Value *jv = NULL;
    JSON_Object *jo = NULL;
    const char *execute = NULL;
    int wrc = 0;
    if ((jv = json_parse_string((const char *)ctx->buf)) == NULL) {
        const char *err =
            "{\"error\":{\"desc\":\"bad request: cannot parse JSON\"}}";
        wrc = write(fd, err, strlen(err));
        goto finish;
    }
    if ((jo = json_object(jv)) == NULL) {
        const char *err =
            "{\"error\":{\"desc\":\"bad request: json_object() failed\"}}";
        wrc = write(fd, err, strlen(err));
        goto finish;
    }
    /* TODO: json_validate */
    if ((execute = json_object_get_string(jo, "execute")) == NULL) {
        const char *err =
            "{\"error\":{\"desc\":\"bad request: no execute found\"}}";
        wrc = write(fd, err, strlen(err));
        goto finish;
    }
    if ((strcmp(execute, "add_hostfwd")) == 0) {
        wrc = api_handle_req_add_hostfwd(slirp, fd, ctx, jo);
    } else if ((strcmp(execute, "list_hostfwd")) == 0) {
        wrc = api_handle_req_list_hostfwd(slirp, fd, ctx, jo);
    } else if ((strcmp(execute, "remove_hostfwd")) == 0) {
        wrc = api_handle_req_remove_hostfwd(slirp, fd, ctx, jo);
    } else {
        const char *err =
            "{\"error\":{\"desc\":\"bad request: unknown execute\"}}";
        wrc = write(fd, err, strlen(err));
        goto finish;
    }
finish:
    if (jv != NULL) {
        json_value_free(jv);
    }
    return wrc;
}

/*
  API handler.
  This function returns the return value of either read(2) or write(2).
 */
int api_handler(Slirp *slirp, int listenfd, struct api_ctx *ctx)
{
    struct sockaddr_un addr;
    socklen_t addrlen = sizeof(struct sockaddr_un);
    int fd;
    int rc = 0, wrc = 0;
    ssize_t len;
    memset(&addr, 0, sizeof(addr));
    if ((fd = accept(listenfd, (struct sockaddr *)&addr, &addrlen)) < 0) {
        perror("api_handler: accept");
        return -1;
    }
    if ((len = read(fd, ctx->buf, ctx->buflen)) < 0) {
        perror("api_handler: read");
        rc = len;
        goto finish;
    }
    if (len == ctx->buflen) {
        const char *err =
            "{\"error\":{\"desc\":\"bad request: too large message\"}}";
        fprintf(stderr, "api_handler: too large message (>= %ld bytes)\n", len);
        wrc = write(fd, err, strlen(err));
        rc = -1;
        goto finish;
    }
    ctx->buf[len] = 0;
    fprintf(stderr, "api_handler: got request: %s\n", ctx->buf);
    wrc = api_handle_req(slirp, fd, ctx);
finish:
    shutdown(fd, SHUT_RDWR);
    if (rc == 0 && wrc != 0) {
        rc = wrc;
    }
    close(fd);
    return rc;
}
