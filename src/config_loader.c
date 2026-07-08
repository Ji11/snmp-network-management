#include "collector.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int get_string_setting(const config_setting_t *setting, const char *name, char *buffer, size_t size) {
    const char *value = NULL;
    if (!config_setting_lookup_string(setting, name, &value) || value == NULL) {
        return -1;
    }

    if (strlen(value) >= size) {
        return -1;
    }

    memcpy(buffer, value, strlen(value) + 1);
    return 0;
}

static int load_device_metrics(config_setting_t *device_entry, target_config_t *target) {
    config_setting_t *metrics = config_setting_get_member(device_entry, "metrics");
    if (metrics == NULL || !config_setting_is_list(metrics)) {
        fprintf(stderr, "config error: device '%s' metrics must be a list\n", target->name);
        return -1;
    }

    int count = config_setting_length(metrics);
    if (count <= 0 || count > MAX_METRICS_PER_DEVICE) {
        fprintf(stderr, "config error: device '%s' metrics count must be between 1 and %d\n",
                target->name, MAX_METRICS_PER_DEVICE);
        return -1;
    }

    target->metric_count = count;
    for (int i = 0; i < count; ++i) {
        config_setting_t *item = config_setting_get_elem(metrics, i);
        metric_config_t *metric = &target->metrics[i];

        if (get_string_setting(item, "name", metric->name, sizeof(metric->name)) != 0 ||
            get_string_setting(item, "oid", metric->oid_text, sizeof(metric->oid_text)) != 0) {
            fprintf(stderr, "config error: invalid metric at index %d for device '%s'\n", i, target->name);
            return -1;
        }

        memset(metric->oid, 0, sizeof(metric->oid));
        metric->oid_len = MAX_OID_LEN;
        if (!snmp_parse_oid(metric->oid_text, metric->oid, &metric->oid_len)) {
            snmp_perror(metric->oid_text);
            return -1;
        }
    }

    return 0;
}

static int load_devices(config_t *cfg, collector_config_t *config) {
    config_setting_t *devices = config_lookup(cfg, "devices");
    if (devices == NULL || !config_setting_is_list(devices)) {
        fprintf(stderr, "config error: devices must be a list\n");
        return -1;
    }

    int count = config_setting_length(devices);
    if (count <= 0 || count > MAX_TARGETS) {
        fprintf(stderr, "config error: devices count must be between 1 and %d\n", MAX_TARGETS);
        return -1;
    }

    config->target_count = count;
    for (int i = 0; i < count; ++i) {
        config_setting_t *item = config_setting_get_elem(devices, i);
        target_config_t *target = &config->targets[i];

        if (get_string_setting(item, "name", target->name, sizeof(target->name)) != 0 ||
            get_string_setting(item, "peer", target->peer, sizeof(target->peer)) != 0 ||
            get_string_setting(item, "community", target->community, sizeof(target->community)) != 0 ||
            get_string_setting(item, "version", target->version, sizeof(target->version)) != 0) {
            fprintf(stderr, "config error: invalid device at index %d\n", i);
            return -1;
        }

        target->retries = 1;
        target->timeout_ms = 1000;
        config_setting_lookup_int(item, "retries", &target->retries);
        config_setting_lookup_int(item, "timeout_ms", &target->timeout_ms);

        if (load_device_metrics(item, target) != 0) {
            return -1;
        }
    }

    return 0;
}

int load_collector_config(const char *path, collector_config_t *config) {
    config_t cfg;

    if (path == NULL || config == NULL) {
        return -1;
    }

    memset(config, 0, sizeof(*config));
    strcpy(config->mode, "sync");
    config->poll_interval_sec = 5;

    config_init(&cfg);
    if (!config_read_file(&cfg, path)) {
        fprintf(stderr, "config read error: %s:%d - %s\n",
                config_error_file(&cfg), config_error_line(&cfg), config_error_text(&cfg));
        config_destroy(&cfg);
        return -1;
    }

    const char *mode = NULL;
    if (config_lookup_string(&cfg, "mode", &mode) && mode != NULL) {
        if (strlen(mode) >= sizeof(config->mode)) {
            fprintf(stderr, "config error: mode is too long\n");
            config_destroy(&cfg);
            return -1;
        }
        memcpy(config->mode, mode, strlen(mode) + 1);
    }

    config_lookup_int(&cfg, "poll_interval_sec", &config->poll_interval_sec);

    /* database section */
    config_setting_t *db = config_lookup(&cfg, "database");
    if (db != NULL) {
        const char *db_host = NULL, *db_user = NULL;
        const char *db_password = NULL, *db_name = NULL;
        config_setting_lookup_string(db, "host", &db_host);
        config_setting_lookup_string(db, "user", &db_user);
        config_setting_lookup_string(db, "password", &db_password);
        config_setting_lookup_string(db, "name", &db_name);
        if (db_host != NULL)     memcpy(config->db.host, db_host, strlen(db_host) + 1);
        if (db_user != NULL)     memcpy(config->db.user, db_user, strlen(db_user) + 1);
        if (db_password != NULL) memcpy(config->db.password, db_password, strlen(db_password) + 1);
        if (db_name != NULL)     memcpy(config->db.name, db_name, strlen(db_name) + 1);
    }

    if (load_devices(&cfg, config) != 0) {
        config_destroy(&cfg);
        return -1;
    }

    config_destroy(&cfg);
    return 0;
}
