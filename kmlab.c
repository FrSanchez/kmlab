
#include <linux/init.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/timekeeping.h>
#include <linux/workqueue.h>>
#include <linux/jiffies.h>

// Module metadata
MODULE_AUTHOR("Bruno Sanchez");
MODULE_DESCRIPTION("Hello world driver");
MODULE_LICENSE("GPL");

#define MODULE_VERS "1.0"
#define MODULE_NAME "kmlab"

void my_work_handler(struct work_struct *work);

DECLARE_DELAYED_WORK(my_work, my_work_handler);

static struct proc_dir_entry *example_dir, *proc_entry;

#define BUFFER_LEN 1024

/*Linked List Node*/
struct my_list
{
    struct list_head list; // linux kernel list implementation
    unsigned long pid;
    unsigned long cpu_time;
};

LIST_HEAD(myLinkedList);

static char proc_buffer[BUFFER_LEN];

static void addData(unsigned long pid)
{
    struct my_list *node = kmalloc(sizeof(struct my_list), GFP_KERNEL);
    node->pid = pid;
    node->cpu_time = 0;
    INIT_LIST_HEAD(&node->list);
    /*Add Node to Linked List*/
    list_add_tail(&node->list, &myLinkedList);
}

static void deleteList(void)
{
    struct my_list *cursor, *temp;
    list_for_each_entry_safe(cursor, temp, &myLinkedList, list)
    {
        list_del(&cursor->list);
        kfree(cursor);
    }
}

static ssize_t custom_read(struct file *file, char __user *user_buffer, size_t count, loff_t *offset)
{
    struct my_list *temp;
    int pos = 0;
    printk(KERN_INFO "calling our very own custom read method.");
    list_for_each_entry(temp, &myLinkedList, list)
    {
        pos += snprintf(&proc_buffer[pos], BUFFER_LEN - pos, "PID %lu time: %lu\n", temp->pid, temp->cpu_time);
        printk(KERN_INFO "Node %d data = %d\n", pos, temp->pid);
    }
    proc_buffer[pos++] = '\0';
    if (*offset > 0) {
        return 0;
    }
    copy_to_user(user_buffer, proc_buffer, pos);
    *offset = pos;
    return pos;
}

void my_work_handler(struct work_struct *work) 
{
    struct my_list *temp;
    printk(KERN_INFO "calling my_work_handler method.");
    list_for_each_entry(temp, &myLinkedList, list)
    {
        printk(KERN_INFO "data = %d\n", temp->pid);
        temp->cpu_time += (ktime_get() % 32);
    }
    schedule_delayed_work(&my_work, msecs_to_jiffies(5000));
}

static ssize_t custom_write(struct file *file, const char __user *data, size_t count, loff_t *offset)
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
    addData(pid);
    printk(KERN_INFO "Reading info from user %s -> %lu", proc_buffer, pid);
    return len;
}

static struct proc_ops fops =
    {
        .proc_write = custom_write,
        .proc_read = custom_read};

// Custom init and exit methods
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
}
module_init(custom_init);
module_exit(custom_exit);
