/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#ifndef SEAFILE_CHECK_QUOTA_PROC_H
#define SEAFILE_CHECK_QUOTA_PROC_H

#include <glib-object.h>
#include <ccnet/processor.h>

#include "transfer-mgr.h"

#define SEAFILE_TYPE_CHECK_QUOTA_PROC                  (seafile_check_quota_proc_get_type ())
#define SEAFILE_CHECK_QUOTA_PROC(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAFILE_TYPE_CHECK_QUOTA_PROC, SeafileCheckQuotaProc))
#define SEAFILE_IS_CHECK_QUOTA_PROC(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAFILE_TYPE_CHECK_QUOTA_PROC))
#define SEAFILE_CHECK_QUOTA_PROC_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), SEAFILE_TYPE_CHECK_QUOTA_PROC, SeafileCheckQuotaProcClass))
#define IS_SEAFILE_CHECK_QUOTA_PROC_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAFILE_TYPE_CHECK_QUOTA_PROC))
#define SEAFILE_CHECK_QUOTA_PROC_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAFILE_TYPE_CHECK_QUOTA_PROC, SeafileCheckQuotaProcClass))

typedef struct _SeafileCheckQuotaProc SeafileCheckQuotaProc;
typedef struct _SeafileCheckQuotaProcClass SeafileCheckQuotaProcClass;

struct _SeafileCheckQuotaProc {
    CcnetProcessor parent_instance;

    TransferTask *task;
    gboolean passed;
};

struct _SeafileCheckQuotaProcClass {
    CcnetProcessorClass parent_class;
};

GType seafile_check_quota_proc_get_type ();

#endif

