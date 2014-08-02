/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include "seafile-session.h"
#include "check-quota-proc.h"
#include "log.h"

#define SC_QUOTA_EXCEED "401"
#define SS_QUOTA_EXCEED "Quota Exceed"

typedef struct  {
    char        repo_id[37];
    gint64      delta;
    gboolean    success;
} SeafileCheckQuotaProcPriv;

#define GET_PRIV(o)  \
   (G_TYPE_INSTANCE_GET_PRIVATE ((o), SEAFILE_TYPE_CHECK_QUOTA_PROC, SeafileCheckQuotaProcPriv))

#define USE_PRIV \
    SeafileCheckQuotaProcPriv *priv = GET_PRIV(processor);


G_DEFINE_TYPE (SeafileCheckQuotaProc, seafile_check_quota_proc, CCNET_TYPE_PROCESSOR)

static int start (CcnetProcessor *processor, int argc, char **argv);

static void
release_resource(CcnetProcessor *processor)
{
    USE_PRIV;

    CCNET_PROCESSOR_CLASS (seafile_check_quota_proc_parent_class)->release_resource (processor);
}


static void
seafile_check_quota_proc_class_init (SeafileCheckQuotaProcClass *klass)
{
    CcnetProcessorClass *proc_class = CCNET_PROCESSOR_CLASS (klass);

    proc_class->start = start;
    proc_class->release_resource = release_resource;

    g_type_class_add_private (klass, sizeof (SeafileCheckQuotaProcPriv));
}

static void
seafile_check_quota_proc_init (SeafileCheckQuotaProc *processor)
{
}

static void *
check_quota_thread (void *vdata)
{
    CcnetProcessor *processor = vdata;
    USE_PRIV;

    if (seaf_quota_manager_check_quota_delta (seaf->quota_mgr,
                                              priv->repo_id,
                                              priv->delta) == 0)
        priv->success = TRUE;
    else
        priv->success = FALSE;

    return vdata;
}

static void
check_quota_done (void *vdata)
{
    CcnetProcessor *processor = vdata;
    USE_PRIV;

    if (priv->success)
        ccnet_processor_send_response (processor, SC_OK, SS_OK, NULL, 0);
    else
        ccnet_processor_send_response (processor, SC_QUOTA_EXCEED, SS_QUOTA_EXCEED,
                                       NULL, 0);

    ccnet_processor_done (processor, TRUE);
}

static int
start (CcnetProcessor *processor, int argc, char **argv)
{
    USE_PRIV;

    if (argc < 2 || strlen(argv[0]) != 36) {
        ccnet_processor_send_response (processor, SC_BAD_ARGS, SS_BAD_ARGS, NULL, 0);
        ccnet_processor_done (processor, FALSE);
        return -1;
    }

    memcpy (priv->repo_id, argv[0], 36);
    priv->delta = g_ascii_strtoll (argv[1], NULL, 10);

    ccnet_processor_thread_create (processor,
                                   seaf->job_mgr,
                                   check_quota_thread,
                                   check_quota_done,
                                   processor);

    return 0;
}
