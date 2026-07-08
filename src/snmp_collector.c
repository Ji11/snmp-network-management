#include "collector.h"
#include "db.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>

/* ---- per-device session state (follows asynchapp.c struct session) ---- */
typedef struct {
    netsnmp_session *sess;
    const target_config_t *target;
    int current_metric;  /* index into target->metrics[] */
} device_session_t;

/* ---- global context for callback access ---- */
static MYSQL *g_db = NULL;
static int g_active_hosts = 0;

/* ---- format_value helper ---- */
static void format_value(const netsnmp_variable_list *vars, char *buffer, size_t size) {
    snprint_value(buffer, size, vars->name, vars->name_length, vars);
}

/* ---- print_result — follows asynchapp.c print_result pattern ---- */
static int print_result(int status, netsnmp_session *sp, netsnmp_pdu *pdu,
                         const char *device_name, const char *metric_name,
                         const char *oid_text) {
    char buf[1024];
    struct timeval now;
    struct timezone tz;

    gettimeofday(&now, &tz);
    struct tm *tm = localtime(&now.tv_sec);
    fprintf(stdout, "%.2d:%.2d:%.2d.%.6ld ",
            tm->tm_hour, tm->tm_min, tm->tm_sec, (long)now.tv_usec);

    switch (status) {
    case STAT_SUCCESS: {
        netsnmp_variable_list *vp = pdu->variables;
        if (pdu->errstat == SNMP_ERR_NOERROR) {
            while (vp) {
                snprint_variable(buf, sizeof(buf), vp->name, vp->name_length, vp);
                fprintf(stdout, "%s: %s\n", sp->peername, buf);

                /* store to MySQL */
                if (g_db != NULL) {
                    char value[MAX_VALUE_LEN];
                    format_value(vp, value, sizeof(value));
                    db_insert(g_db, device_name, metric_name, oid_text, value);
                }
                vp = vp->next_variable;
            }
        } else {
            int ix;
            for (ix = 1, vp = pdu->variables;
                 vp && ix != pdu->errindex;
                 vp = vp->next_variable, ix++);
            if (vp) snprint_objid(buf, sizeof(buf), vp->name, vp->name_length);
            else strcpy(buf, "(none)");
            fprintf(stdout, "%s: %s: %s\n",
                    sp->peername, buf, snmp_errstring(pdu->errstat));
        }
        return 1;
    }
    case STAT_TIMEOUT:
        fprintf(stdout, "%s: Timeout\n", sp->peername);
        return 0;
    case STAT_ERROR:
        snmp_perror(sp->peername);
        return 0;
    }
    return 0;
}

/* ---- asynch_response — follows asynchapp.c callback pattern ----
 * On success: print + store, then advance to next metric and snmp_send() it.
 * On failure/end: decrement g_active_hosts.                       ---- */
static int asynch_response(int operation, netsnmp_session *sp, int reqid,
                            netsnmp_pdu *pdu, void *magic) {
    (void)sp;
    (void)reqid;
    device_session_t *host = (device_session_t *)magic;
    const target_config_t *target = host->target;

    if (operation == NETSNMP_CALLBACK_OP_RECEIVED_MESSAGE) {
        const metric_config_t *metric = &target->metrics[host->current_metric];

        if (print_result(STAT_SUCCESS, host->sess, pdu,
                         target->name, metric->name, metric->oid_text)) {
            /* advance to next metric */
            host->current_metric++;
            if (host->current_metric < target->metric_count) {
                /* send next GET — follows asynchapp.c snmp_send() pattern */
                const metric_config_t *next = &target->metrics[host->current_metric];
                netsnmp_pdu *req = snmp_pdu_create(SNMP_MSG_GET);
                snmp_add_null_var(req, next->oid, next->oid_len);
                if (snmp_send(host->sess, req))
                    return 1;
                snmp_perror("snmp_send");
                snmp_free_pdu(req);
            }
        }
    } else {
        /* TIMEOUT / SEND_FAILED */
        const metric_config_t *metric = NULL;
        if (host->current_metric >= 0 && host->current_metric < target->metric_count)
            metric = &target->metrics[host->current_metric];
        print_result(STAT_TIMEOUT, host->sess, pdu,
                     target->name,
                     metric ? metric->name : "unknown",
                     metric ? metric->oid_text : "unknown");
    }

    /* this device is done (no more OIDs or error) */
    g_active_hosts--;
    return 1;
}

/* ---- version_from_text ---- */
static long version_from_text(const char *version) {
    if (strcmp(version, "1") == 0 || strcmp(version, "v1") == 0)
        return SNMP_VERSION_1;
    if (strcmp(version, "2c") == 0 || strcmp(version, "v2c") == 0)
        return SNMP_VERSION_2c;
    return SNMP_VERSION_2c;
}

/* ---- run_async_collection — follows asynchapp.c asynchronous() pattern ----
 * Phase 1: init sessions, send first GET per device
 * Phase 2: select() event loop
 * Phase 3: cleanup                                        ---- */
int run_async_collection(const collector_config_t *config) {
    if (config == NULL) return -1;

    /* connect database */
    g_db = db_connect(&config->db);
    if (g_db == NULL)
        fprintf(stderr, "[async] warning: database not available\n");

    int n = config->target_count;
    device_session_t *sessions = calloc(n, sizeof(device_session_t));
    if (sessions == NULL) {
        perror("calloc");
        if (g_db) db_close(g_db);
        return -1;
    }

    g_active_hosts = 0;

    /* Phase 1: open sessions and send first GET for each device
     * (follows asynchapp.c asynchronous() startup loop) */
    for (int i = 0; i < n; i++) {
        const target_config_t *target = &config->targets[i];
        device_session_t *hs = &sessions[i];

        netsnmp_session sess;
        snmp_sess_init(&sess);
        sess.peername = strdup(target->peer);
        sess.community = (u_char *)strdup(target->community);
        sess.community_len = strlen(target->community);
        sess.version = version_from_text(target->version);
        sess.retries = target->retries;
        sess.timeout = target->timeout_ms * 1000L;
        sess.callback = asynch_response;
        sess.callback_magic = hs;

        hs->target = target;
        hs->current_metric = 0;

        if (!(hs->sess = snmp_open(&sess))) {
            snmp_perror("snmp_open");
            free(sess.peername);
            free(sess.community);
            continue;
        }
        free(sess.peername);
        free(sess.community);

        /* send first GET — follows asynchapp.c snmp_send() pattern */
        const metric_config_t *metric = &target->metrics[0];
        netsnmp_pdu *req = snmp_pdu_create(SNMP_MSG_GET);
        snmp_add_null_var(req, metric->oid, metric->oid_len);
        if (snmp_send(hs->sess, req))
            g_active_hosts++;
        else {
            snmp_perror("snmp_send");
            snmp_free_pdu(req);
        }
    }

    if (g_active_hosts == 0) {
        fprintf(stderr, "[async] no active hosts, exiting\n");
        free(sessions);
        if (g_db) db_close(g_db);
        return 0;
    }

    /* Phase 2: event loop (canonical asynchapp.c select pattern) */
    while (g_active_hosts > 0) {
        int fds = 0, block = 1;
        fd_set fdset;
        struct timeval timeout;

        FD_ZERO(&fdset);
        snmp_select_info(&fds, &fdset, &timeout, &block);
        fds = select(fds, &fdset, NULL, NULL, block ? NULL : &timeout);
        if (fds < 0) {
            perror("select failed");
            break;
        }
        if (fds)
            snmp_read(&fdset);
        else
            snmp_timeout();
    }

    /* Phase 3: cleanup (follows asynchapp.c cleanup pattern) */
    for (int i = 0; i < n; i++) {
        if (sessions[i].sess)
            snmp_close(sessions[i].sess);
    }
    free(sessions);

    if (g_db) db_close(g_db);
    return 0;
}
