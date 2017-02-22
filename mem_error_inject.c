#define pr_fmt(s) KBUILD_MODNAME ": " s

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/pgtable.h>
#include <linux/debugfs.h>
#include <linux/string.h>
#include <linux/spinlock.h>
#include <linux/sched.h>

#define MODNAME "mei"

#define BYTESIZE 8

#define MEI_MAGIC 'M'
#define SET_READ_PADDR _IOW(MEI_MAGIC, 0, unsigned long)
#define DEL_INJ_ERR _IOW(MEI_MAGIC, 1, unsigned long)

/* Use spinlock in order to prevent sleep when validate the memory 
   tester implemented in kernel space. */
static spinlock_t inject_mem_list_lock;

struct inject_memory_err {
	unsigned long phy_addr; /* where the error to inject in physical address */
	int err_bit_num; /* number of error bits in a byte */
	int bit[BYTESIZE]; /* which bit in the byte have error */
	int bit_value[BYTESIZE]; /* the bits stuck-at error values */
	struct list_head lists;
};

/* The inject errors lists, protected by inject_mem_list_lock */
static LIST_HEAD(inject_mem_err_list);

/* The number of memory errors have already been injected,
 * protected by inject_mem_list_lock
 */
static int inj_mem_err_cnt;

/* Use slab to make inject_memory_err allocation more efficiency */
struct kmem_cache *inj_err_pool;

/* The physical address to read, it should be set every time before read
 * a byte by ioctl.
 */
static unsigned long read_paddr;

/* translate inject_memory_err structure into error value */
static char inject_mem_err_value(struct inject_memory_err *inj_err, char real_value)
{
	int i = 0;

	for (i = 0; i < BYTESIZE; i++) {
		if (inj_err->bit[i]) {
			if (inj_err->bit_value[i]) /* set bit i to 1 */
				real_value |= (1<<i);
			else /* set bit i to 0 */
				real_value &= ~(1<<i);
		}
	}

	return real_value;
}

/* inject_mem_list_lock must be held before calling this function */
static struct inject_memory_err *find_inj_err(unsigned long phy_addr)
{
	struct inject_memory_err *inj_err;

	list_for_each_entry(inj_err, &inject_mem_err_list, lists) {
		if (inj_err->phy_addr == phy_addr)
			return inj_err;
	}

	return NULL;
}

/* This function must be called with inject_mem_list_lock held
 * return true for copy success, false for not found
 */
static bool copy_into_exist_inj(struct inject_memory_err *new)
{
	int i;
	struct inject_memory_err *old = find_inj_err(new->phy_addr);
	if (!old)
		return false;

	old->err_bit_num = new->err_bit_num;
	for (i = 0; i < BYTESIZE; i++) {
		old->bit[i] = new->bit[i];
		old->bit_value[i] = new->bit_value[i];
	}

	return true;
}

/* The inject operation */
static ssize_t mem_err_inject(struct file *filp, const char __user *ubuf, size_t cnt, loff_t *pos)
{
	struct inject_memory_err *inj_err = kmem_cache_alloc(inj_err_pool, GFP_KERNEL);
	if (!inj_err)
		return -ENOMEM;

	if (copy_from_user(inj_err, ubuf, sizeof(*inj_err)))
		return -EFAULT;

	if (inj_err->err_bit_num > 8) {
		pr_err("error bit should not exceed 8 bits\n");
		kmem_cache_free(inj_err_pool, inj_err);
		return -EINVAL;
	}

	spin_lock(&inject_mem_list_lock);
	if (copy_into_exist_inj(inj_err))
		kmem_cache_free(inj_err_pool, inj_err);
	else {
		list_add(&inj_err->lists, &inject_mem_err_list);
		inj_mem_err_cnt++;
	}
	spin_unlock(&inject_mem_list_lock);

	return sizeof(*inj_err);
}

/* user space read function */
static ssize_t mem_err_inj_read(struct file *filp, char __user *ubuf, size_t cnt, loff_t *pos)
{
	struct inject_memory_err *inj_err;
	char *kvaddr = __va(read_paddr);
	char real_value = *kvaddr;

	spin_lock(&inject_mem_list_lock);
	inj_err = find_inj_err(read_paddr);
	if (inj_err) { /* return the error value here */
		real_value = inject_mem_err_value(inj_err, real_value);
	}
	spin_unlock(&inject_mem_list_lock);

	if (copy_to_user(ubuf, &real_value, sizeof(char))) {
		spin_unlock(&inject_mem_list_lock);
		return -EFAULT;
	}

	return 1;	
}

/* kernel space read function */
char mei_read_byte(unsigned long vaddr)
{
	struct inject_memory_err *inj_err = NULL;
	unsigned long paddr = __pa(vaddr);
	char value = *((char *)vaddr);
	static bool test = false;
	
	if (!test) {
		test = true;
		return 0xff;
	}

	spin_lock(&inject_mem_list_lock);
	inj_err = find_inj_err(paddr);
	if (inj_err) { /* return the simulated error value */
		value = inject_mem_err_value(inj_err, value);
	}
	spin_unlock(&inject_mem_list_lock);

	return value;
}
EXPORT_SYMBOL(mei_read_byte);

static struct page *virt2page(struct mm_struct *mm, unsigned long addr)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	struct page *page = NULL;;

	pgd = pgd_offset(mm, addr);
	if (pgd_none(*pgd) || pgd_bad(*pgd))
		return NULL;
	
	pud = pud_offset(pgd, addr);
	if (pud_none(*pud) || pud_bad(*pud))
		return NULL;

	pmd = pmd_offset(pud, addr);
	if (pmd_none(*pmd) || pmd_bad(*pmd))
		return NULL;

	if (!(pte = pte_offset_map(pmd, addr)))
		return NULL;

	if (pte_none(*pte))
		goto out;

	if (!pte_present(*pte))
		goto out;

	if (!(page = pte_page(*pte)))
		goto out;

out:
	pte_unmap(pte);

	return page;
}

/* del an element from list */
static void del_inject_error(unsigned long paddr)
{
	struct inject_memory_err *inj_err, *tmp;

	spin_lock(&inject_mem_list_lock);
	list_for_each_entry_safe(inj_err, tmp, &inject_mem_err_list, lists) {
		if (inj_err->phy_addr == paddr) {
			list_del(&inj_err->lists);
			kmem_cache_free(inj_err_pool, inj_err);
			inj_mem_err_cnt--;
			spin_unlock(&inject_mem_list_lock);
			return;
		}
	}
	spin_unlock(&inject_mem_list_lock);
	
	pr_warn("Couldn't find the address %lu\n", paddr);
}

/* ioctl used to set read_paddr */
static long mem_err_inj_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	struct task_struct *t = current;
	struct page *page = NULL;

	switch(cmd) {
	case SET_READ_PADDR:
		page = virt2page(t->mm, arg);
		if (!page) {
			pr_err("invalid address\n");
			return -EINVAL;
		}
		read_paddr = ((page_to_pfn(page)) << 12) + (arg & 0xfff);
		return 0;
	case DEL_INJ_ERR:
		del_inject_error(arg);
		return 0;
	default:
		return -ENOTTY;
	}
	
}

struct file_operations mei_fops = {
	.read = mem_err_inj_read,
	.write = mem_err_inject,
	.unlocked_ioctl = mem_err_inj_ioctl,
};

struct miscdevice mei_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = MODNAME,
	.fops = &mei_fops,
};

/* clear all inject errors */
static void clear_all_inject_errors(void)
{
	struct inject_memory_err *inj_err, *tmp;

	spin_lock(&inject_mem_list_lock);
	list_for_each_entry_safe(inj_err, tmp, &inject_mem_err_list, lists) {
		list_del(&inj_err->lists);
		kmem_cache_free(inj_err_pool, inj_err);
		inj_mem_err_cnt--;
	}
	INIT_LIST_HEAD(&inject_mem_err_list);
	spin_unlock(&inject_mem_list_lock);
	
	BUG_ON(inj_mem_err_cnt);
}

/* debugfs interface */
static ssize_t inject_errors_write(struct file *filp, const char __user *ubuf, size_t len, loff_t *pos)
{
	char buf[64] = {'\0'};

	if (len > 64)
		len = 64;
	if (copy_from_user(buf, ubuf, len))
		return -EFAULT;
	
	if (buf[strlen(buf)-1] == '\n')
		buf[strlen(buf)-1] = '\0';

	if (!strcmp(buf, "clear"))
		clear_all_inject_errors();
	else
		pr_warn("Unknown command. Please use 'clear'\n");

	return len;
}

static int inject_errors_show(struct seq_file *m, void *v)
{
	struct inject_memory_err *inj_err;
	int i;
	spin_lock(&inject_mem_list_lock);
	seq_printf(m, "Inject Errors Count: %d\n", inj_mem_err_cnt);
	list_for_each_entry(inj_err, &inject_mem_err_list, lists) {
		seq_printf(m, "physical address: %lu\t", inj_err->phy_addr);
		seq_printf(m, "error bits number: %d\t", inj_err->err_bit_num);
		seq_printf(m, "error bits: ");
		for (i = 0; i < BYTESIZE; i++)
			seq_printf(m, "%d ", inj_err->bit[i]);
		seq_printf(m, "\terror value: ");
		for (i = 0; i < BYTESIZE; i++)
			seq_printf(m, "%d ", inj_err->bit_value[i]);
		seq_printf(m, "\n");
	}
	spin_unlock(&inject_mem_list_lock);

	return 0;
}

static int inject_errors_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, inject_errors_show, NULL);
}

static const struct file_operations debugfs_fops = {
	.open = inject_errors_open,
	.read = seq_read,
	.write = inject_errors_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static struct dentry *inject_error_dir;

static int debugfs_init(void)
{
	umode_t mode = S_IFREG | S_IRUSR | S_IWUSR;

	inject_error_dir = debugfs_create_dir("MEI", NULL);
	if (!inject_error_dir)
		return -ENOMEM;

	if (!debugfs_create_file("inject_errors", mode, inject_error_dir, NULL, &debugfs_fops))
		goto fail;

	return 0;

fail:
	debugfs_remove_recursive(inject_error_dir);
	return -ENOMEM;
}

static void debugfs_destroy(void)
{
	debugfs_remove_recursive(inject_error_dir);
}

static int mei_init(void)
{
	spin_lock_init(&inject_mem_list_lock);
	if (debugfs_init())
		return -ENOMEM;
	misc_register(&mei_dev);
	inj_err_pool = kmem_cache_create("inj_err_pool", sizeof(struct inject_memory_err), 0, 
			SLAB_HWCACHE_ALIGN, NULL);
	if (!inj_err_pool)
		return -ENOMEM;

	return 0;
}

static void mei_exit(void)
{
	clear_all_inject_errors();
	misc_deregister(&mei_dev);
	kmem_cache_destroy(inj_err_pool);
	debugfs_destroy();
}

module_init(mei_init);
module_exit(mei_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("wang_xiaoq@126.com");
