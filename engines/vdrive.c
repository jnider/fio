/*
 * vdrive engine
 *
 * IO engine for virtio-drive
 */
#include <stdlib.h>
#include <assert.h>

#include "../fio.h"
#include "fuse.h"
#include "posix.h"
#include "platform.h"

#define FILE_INO(f)		(int)((uint64_t)(f)->engine_data)
#define FILE_SET_INO(f, data)	((f)->engine_data = (void *)((uint64_t)data))

struct vdrive_data {
	struct io_u **io_us;
	int queued;
	int events;
};

static struct io_u *vdrive_event(struct vdrive_data *nd, int event)
{
	printf("%s\n", __func__);
	return nd->io_us[event];
}

static int vdrive_getevents(struct vdrive_data *nd, unsigned int min_events,
			  unsigned int fio_unused max,
			  const struct timespec fio_unused *t)
{
	int ret = 0;

	printf("%s\n", __func__);

	if (min_events) {
		ret = nd->events;
		nd->events = 0;
	}

	return ret;
}

static int vdrive_commit(struct thread_data *td, struct vdrive_data *nd)
{
	printf("%s\n", __func__);

	if (!nd->events) {
		nd->events = nd->queued;
		nd->queued = 0;
	}

	return 0;
}

static struct vdrive_data *vdrive_init(struct thread_data *td)
{
	int ret;
	struct vdrive_data *nd = (struct vdrive_data *) malloc(sizeof(*nd));

	printf("%s\n", __func__);

	memset(nd, 0, sizeof(*nd));

	td->io_ops->flags |= FIO_SYNCIO;

	// TODO: only initialize once per process

	ret = platform_init(0, 1);
	if (ret < 0)
		printf("Error %i initializing virtio drive\n", ret);

	return nd;
}

static struct io_u *fio_vdrive_event(struct thread_data *td, int event)
{
	return vdrive_event(td->io_ops_data, event);
}

static int fio_vdrive_getevents(struct thread_data *td, unsigned int min_events,
			      unsigned int max, const struct timespec *t)
{
	struct vdrive_data *nd = td->io_ops_data;
	printf("%s\n", __func__);
	return vdrive_getevents(nd, min_events, max, t);
}

static int fio_vdrive_commit(struct thread_data *td)
{
	printf("%s\n", __func__);
	return vdrive_commit(td, td->io_ops_data);
}

static enum fio_q_status fio_vdrive_queue(struct thread_data *td,
					struct io_u *req)
{
	if (!(td->io_ops->flags & FIO_SYNCIO))
	{
		printf("Got async IO request!\n");
		return FIO_Q_BUSY;
	}

	switch (req->ddir)
	{
	case DDIR_READ:
		printf("read\n");
		break;
 
	case DDIR_WRITE: 
		g_pwrite(FILE_INO(req->file), req->offset, req->buflen, (uint8_t *)req->buf);
		break;

	case DDIR_TRIM:
		printf("trim\n");
		break;

	case DDIR_SYNC: 
		printf("sync\n");
		break;

	case DDIR_DATASYNC:
		printf("data sync\n");
		break;

	case DDIR_SYNC_FILE_RANGE:
		printf("data sync range\n");
		break;

	case DDIR_WAIT:
		printf("wait\n");
		break;

	default:
		printf("Unhandled IO\n");
	}

	return FIO_Q_COMPLETED;
}

static int fio_vdrive_open(struct thread_data *td, struct fio_file *f)
{
	int ino;
	int flags = 0;
	printf("%s: %s\n", __func__, f->file_name);

	if (td->o.td_ddir == TD_DDIR_WRITE) {
		flags |= O_CREAT | O_RDWR;
	} else {
		flags |= O_RDWR;
	}
	ino = g_open(f->file_name, flags);
	if (ino < 0)
		return ino;

	FILE_SET_INO(f, ino);

	return 0;
}

static int fio_vdrive_close(struct thread_data fio_unused *td,
				  struct fio_file *f)
{
	return g_close(FILE_INO(f));
}

static int fio_vdrive_init(struct thread_data *td)
{
	td->io_ops_data = vdrive_init(td);
	assert(td->io_ops_data);
	return 0;
}

static void fio_vdrive_cleanup(struct thread_data *td)
{
	struct vdrive_data *nd;
	nd = td->io_ops_data;

	printf("%s\n", __func__);
	platform_cleanup();

	if (nd) {
		free(nd->io_us);
		free(nd);
	}
}

int fio_vdrive_unlink(struct thread_data *, struct fio_file *)
{
	printf("%s\n", __func__);
	return 0;
}

int fio_vdrive_size(struct thread_data *, struct fio_file *)
{
	printf("%s\n", __func__);
	return 0;
}

static struct ioengine_ops ioengine = {
	.name		= "vdrive",
	.version	= FIO_IOOPS_VERSION,
	.init		= fio_vdrive_init,
	.queue		= fio_vdrive_queue,
	.commit		= fio_vdrive_commit,
	.getevents	= fio_vdrive_getevents,
	.event		= fio_vdrive_event,
	.cleanup	= fio_vdrive_cleanup,
	.open_file	= fio_vdrive_open,
	.close_file = fio_vdrive_close,
	.unlink_file = fio_vdrive_unlink,
	.get_file_size = fio_vdrive_size,
	.flags		= FIO_SYNCIO | FIO_RAWIO | FIO_DISKLESSIO,
};

static void fio_init fio_vdrive_register(void)
{
	register_ioengine(&ioengine);
}

static void fio_exit fio_vdrive_unregister(void)
{
	unregister_ioengine(&ioengine);
}
