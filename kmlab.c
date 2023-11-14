#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/timekeeping.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include "kmlab_given.h"

// Module metadata
MODULE_AUTHOR("Bruno Sanchez");
MODULE_DESCRIPTION("Hello world driver");
MODULE_LICENSE("GPL");

#define MODULE_VERS "1.0"
#define MODULE_NAME "kmlab"
#define MSEC_INTERVAL 5000

void cpu_update_handler(struct work_struct *work);
static void add_entry(unsigned long pid);
static void update_entry(unsigned long pid, unsigned long cpu_value);

DECLARE_DELAYED_WORK(my_work, cpu_update_handler);

static struct proc_dir_entry *example_dir, *proc_entry;

#define BUFFER_LEN 1024

/*Linked List Node*/
typedef struct pid_list
{
    struct list_head list; // linux kernel list implementation
    unsigned long pid;
    unsigned long cpu_time;
};

LIST_HEAD(process_linked_list);

static char proc_buffer[BUFFER_LEN];

static void add_entry(unsigned long pid)
{
    struct pid_list *node = kmalloc(sizeof(struct pid_list), GFP_KERNEL);
    node->pid = pid;
    node->cpu_time = 0;
    INIT_LIST_HEAD(&node->list);
    /*Add Node to Linked List*/
    list_add_tail(&node->list, &process_linked_list);
}

static void delete_list(void)
{
    struct pid_list *cursor, *temp;
    list_for_each_entry_safe(cursor, temp, &process_linked_list, list)
    {
        list_del(&cursor->list);
        kfree(cursor);
    }
}

static ssize_t proc_read(struct file *file, char __user *user_buffer, size_t count, loff_t *offset)
{
    struct pid_list *temp;
    int pos = 0;
    printk(KERN_INFO "calling our very own custom read method.");
    list_for_each_entry(temp, &process_linked_list, list)
    {
        pos += snprintf(&proc_buffer[pos], BUFFER_LEN - pos, "%lu: %lu\n", temp->pid, temp->cpu_time);
    }
    proc_buffer[pos++] = '\0';
    if (*offset > 0)
    {
        return 0;
    }
    copy_to_user(user_buffer, proc_buffer, pos);
    *offset = pos;
    return pos;
}

void cpu_update_handler(struct work_struct *work)
{
    printk(KERN_INFO "calling cpu_update_handler method.");
    struct pid_list *cursor, *temp;
    list_for_each_entry_safe(cursor, temp, &process_linked_list, list)
    {
        unsigned long cpu_use = 0;
        int get_cpu = get_cpu_use(cursor->pid, &cpu_use);
        if (get_cpu < 0)
        {
            printk(KERN_INFO "Pid %u not found - removing it", cursor->pid);
            list_del(&cursor->list);
            kfree(cursor);
        }
        else
        {
            cursor->cpu_time = cpu_use;
        }
    }
    schedule_delayed_work(&my_work, msecs_to_jiffies(MSEC_INTERVAL));
}

static ssize_t proc_write(struct file *file, const char __user *data, size_t count, loff_t *offset)
{
    int len;
    if (count > BUFFER_LEN)
        len = BUFFER_LEN;
    else
        len = count;
    if (copy_from_user(proc_buffer, data, len))
    {
        return -EFAULT;
    }
    proc_buffer[len] = '\0';
    unsigned long pid = simple_strtoul(proc_buffer, NULL, 10);
    add_entry(pid);
    printk(KERN_INFO "Reading info from user %s -> %lu", proc_buffer, pid);
    return len;
}

/** */
static struct proc_ops fops =
    {
        .proc_write = proc_write,
        .proc_read = proc_read};

static int __init custom_init(void)
{
    example_dir = proc_mkdir(MODULE_NAME, NULL);
    if (example_dir == NULL)
    {
        return -ENOMEM;
    }
    proc_entry = proc_create("status", 0666, example_dir, &fops);
    printk(KERN_INFO "Hello world driver loaded.");
    schedule_delayed_work(&my_work, msecs_to_jiffies(5000));
    return 0;
}

static void __exit custom_exit(void)
{
    proc_remove(example_dir);
    proc_remove(proc_entry);
    printk(KERN_INFO "Goodbye my friend, I shall miss you dearly...");
    cancel_delayed_work_sync(&my_work);
    flush_scheduled_work();
    delete_list();
}
module_init(custom_init);
module_exit(custom_exit);
