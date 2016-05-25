#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/poll.h>
#include <linux/vfio.h>
#include <linux/wait.h>
#include <linux/workqueue.h>

#if 0
struct virqfd {
	void			*opaque;
	struct eventfd_ctx	*eventfd;
	int			(*handler)(void *, void *);
	void			(*thread)(void *, void *);
	void			*data;
	struct work_struct	inject;
	wait_queue_t		wait;
	poll_table		pt;
	struct work_struct	shutdown;
	struct virqfd		**pvirqfd;
};
#endif

struct data {
	struct virqfd *virqfd;
	unsigned long flags;
};

static int entry_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	struct data *data = (struct data *)ri->data;
	wait_queue_t *wait = (wait_queue_t *)regs->di; /* only x86_64 */
	void *key = (void *)regs->cx; /* only x86_64 */

	data->virqfd = container_of(wait, struct virqfd, wait);
	data->flags = (unsigned long)key;

	return 0;
}

static int handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	struct data *data = (struct data *)ri->data;
	unsigned long flags = data->flags;
	struct virqfd *virqfd = data->virqfd;

	if ((flags & POLLIN) && virqfd->thread)
		schedule_work(&virqfd->inject);

	return 0;
}

static struct kretprobe kretprobe = {
	.handler = handler,
	.entry_handler = entry_handler,
	.data_size = sizeof(struct data),
	.kp = {
		.symbol_name = "virqfd_wakeup",
	},
	.maxactive = NR_CPUS,
};

int __init my_init(void)
{
	return register_kretprobe(&kretprobe);
}

void __exit my_exit(void)
{
	unregister_kretprobe(&kretprobe);
}

module_init(my_init);
module_exit(my_exit);
MODULE_LICENSE("GPL v2");
