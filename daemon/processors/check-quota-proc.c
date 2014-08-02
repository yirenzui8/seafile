/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include "check-quota-proc.h"
#define DEBUG_FLAG SEAFILE_DEBUG_TRANSFER
#include "log.h"

#define SC_QUOTA_EXCEED "401"
#define SS_QUOTA_EXCEED "Quota Exceed"

typedef struct  {
    gboolean success;
    gint64 delta;
} SeafileCheckQuotaProcPriv;

#define GET_PRIV(o)  \
   (G_TYPE_INSTANCE_GET_PRIVATE ((o), SEAFILE_TYPE_CHECK_QUOTA_PROC, SeafileCheckQuotaProcPriv))

#define USE_PRIV \
    SeafileCheckQuotaProcPriv *priv = GET_PRIV(processor);


G_DEFINE_TYPE (SeafileCheckQuotaProc, seafile_check_quota_proc, CCNET_TYPE_PROCESSOR)

static int start (CcnetProcessor *processor, int argc, char **argv);
static void handle_response (CcnetProcessor *processor,
                             char *code, char *code_msg,
                             char *content, int clen);

static void
release_resource(CcnetProcessor *processor)
{
    /* FILL IT */

    CCNET_PROCESSOR_CLASS (seafile_check_quota_proc_parent_class)->release_resource (processor);
}


static void
seafile_check_quota_proc_class_init (SeafileCheckQuotaProcClass *klass)
{
    CcnetProcessorClass *proc_class = CCNET_PROCESSOR_CLASS (klass);

    proc_class->start = start;
    proc_class->handle_response = handle_response;
    proc_class->release_resource = release_resource;

    g_type_class_add_private (klass, sizeof (SeafileCheckQuotaProcPriv));
}

static void
seafile_check_quota_proc_init (SeafileCheckQuotaProc *processor)
{
}

static int
diff_files (int n, const char *basedir, SeafDirent *files[], void *vdata)
{
    CcnetProcessor *processor = vdata;
    SeafileCheckQuotaProc *proc = (SeafileCheckQuotaProc *)processor;
    TransferTask *task = proc->task;
    USE_PRIV;
    SeafDirent *file1 = files[0];
    SeafDirent *file2 = files[1];
    gint64 size1, size2;

    if (file1 && file2) {
        if (file1->version > 0) {
            size1 = file1->size;
            size2 = file2->size;
        } else {
            size1 = seaf_fs_manager_get_file_size (seaf->fs_mgr,
                                                   task->repo_id, task->repo_version,
                                                   file1->id);
            if (size1 < 0) {
                seaf_warning ("Failed to get size of file %s.\n", file1->id);
                return -1;
            }
            size2 = seaf_fs_manager_get_file_size (seaf->fs_mgr,
                                                   task->repo_id, task->repo_version,
                                                   file2->id);
            if (size2 < 0) {
                seaf_warning ("Failed to get size of file %s.\n", file2->id);
                return -1;
            }
        }

        priv->delta += (size1 - size2);
    } else if (file1 && !file2) {
        if (file1->version > 0) {
            size1 = file1->size;
        } else {
            size1 = seaf_fs_manager_get_file_size (seaf->fs_mgr,
                                                   task->repo_id, task->repo_version,
                                                   file1->id);
            if (size1 < 0) {
                seaf_warning ("Failed to get size of file %s.\n", file1->id);
                return -1;
            }
        }
        priv->delta += size1;
    } else if (!file1 && file2) {
        if (file2->version > 0) {
            size2 = file2->size;
        } else {
            size2 = seaf_fs_manager_get_file_size (seaf->fs_mgr,
                                                   task->repo_id, task->repo_version,
                                                   file2->id);
            if (size2 < 0) {
                seaf_warning ("Failed to get size of file %s.\n", file2->id);
                return -1;
            }
        }
        priv->delta -= size2;
    }

    return 0;
}

static int
diff_dirs (int n, const char *basedir, SeafDirent *dirs[], void *data,
           gboolean *recurse)
{
    /* Do nothing */
    return 0;
}

static void *
calculate_upload_size_delta (void *vdata)
{
    CcnetProcessor *processor = vdata;
    SeafileCheckQuotaProc *proc = (SeafileCheckQuotaProc *)processor;
    TransferTask *task = proc->task;
    USE_PRIV;

    SeafBranch *local = NULL, *master = NULL;
    local = seaf_branch_manager_get_branch (seaf->branch_mgr, task->repo_id, "local");
    if (!local) {
        seaf_warning ("Branch local not found for repo %.8s.\n", task->repo_id);
        priv->success = FALSE;
        goto out;
    }
    master = seaf_branch_manager_get_branch (seaf->branch_mgr, task->repo_id, "master");
    if (!master) {
        seaf_warning ("Branch master not found for repo %.8s.\n", task->repo_id);
        priv->success = FALSE;
        goto out;
    }

    SeafCommit *local_head = NULL, *master_head = NULL;
    local_head = seaf_commit_manager_get_commit (seaf->commit_mgr,
                                                 task->repo_id, task->repo_version,
                                                 local->commit_id);
    if (!local_head) {
        seaf_warning ("Local head commit not found for repo %.8s.\n",
                      task->repo_id);
        priv->success = FALSE;
        goto out;
    }
    master_head = seaf_commit_manager_get_commit (seaf->commit_mgr,
                                                 task->repo_id, task->repo_version,
                                                 master->commit_id);
    if (!master_head) {
        seaf_warning ("Master head commit not found for repo %.8s.\n",
                      task->repo_id);
        priv->success = FALSE;
        goto out;
    }

    DiffOptions opts;
    memset (&opts, 0, sizeof(opts));
    memcpy (opts.store_id, task->repo_id, 36);
    opts.version = task->repo_version;
    opts.file_cb = diff_files;
    opts.dir_cb = diff_dirs;
    opts.data = processor;

    const char *trees[2];
    trees[0] = local_head->root_id;
    trees[1] = master_head->root_id;
    if (diff_trees (2, trees, &opts) < 0) {
        seaf_warning ("Failed to diff local and master head for repo %.8s.\n",
                      task->repo_id);
        priv->success = FALSE;
        goto out;
    }

    priv->success = TRUE;

out:
    seaf_branch_unref (local);
    seaf_branch_unref (master);
    seaf_commit_unref (local_head);
    seaf_commit_unref (master_head);
    return vdata;
}

static void
calculate_delta_done (void *vdata)
{
    CcnetProcessor *processor = vdata;
    SeafileCheckQuotaProc *proc = (SeafileCheckQuotaProc *)processor;
    TransferTask *task = proc->task;
    USE_PRIV;

    if (!priv->success) {
        ccnet_processor_done (processor, FALSE);
        return;
    }

    seaf_debug ("Upload size delta is %"G_GINT64_FORMAT".\n", priv->delta);

    GString *buf = g_string_new ("");

    g_string_printf (buf, "remote %s seafile-check-quota %s %"G_GINT64_FORMAT"",
                     processor->peer_id,
                     task->repo_id, priv->delta);
    ccnet_processor_send_request (processor, buf->str);
    g_string_free (buf, TRUE);
}

static int
start (CcnetProcessor *processor, int argc, char **argv)
{
    SeafileCheckQuotaProc *proc = (SeafileCheckQuotaProc *)processor;
    TransferTask *task = proc->task;

    ccnet_processor_thread_create (processor,
                                   seaf->job_mgr,
                                   calculate_upload_size_delta,
                                   calculate_delta_done,
                                   processor);

    return 0;
}

static void
handle_response (CcnetProcessor *processor,
                 char *code, char *code_msg,
                 char *content, int clen)
{
    SeafileCheckQuotaProc *proc = (SeafileCheckQuotaProc *)processor;

    if (memcmp (code, SC_OK, 3) == 0) {
        proc->passed = TRUE;
        ccnet_processor_done (processor, TRUE);
    } else if (memcmp (code, SC_QUOTA_EXCEED, 3) == 0) {
        proc->passed = FALSE;
        ccnet_processor_done (processor, TRUE);
    } else {
        seaf_warning ("Bad response: %s %s.\n", code, code_msg);
        ccnet_processor_done (processor, FALSE);
    }
}
