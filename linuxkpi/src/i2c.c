/* linuxkpi/src/i2c.c -- the I2C core: adapters, bus locking, transfers
 *
 * Adapters are numbered as they register; each gets a mutex as its bus
 * lock unless the driver brings its own lock_ops (DRM's DisplayPort AUX
 * adapters do). A transfer is retried while the algorithm says -EAGAIN,
 * for up to adap->retries more tries within adap->timeout, as Linux does. */
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/jiffies.h>
#include <linux/atomic.h>
#include <linux/printk.h>

static atomic_t next_nr = ATOMIC_INIT(0);

static void lock_bus(struct i2c_adapter *adap, unsigned int flags) { mutex_lock(&adap->bus_lock); }
static int trylock_bus(struct i2c_adapter *adap, unsigned int flags) { return mutex_trylock(&adap->bus_lock); }
static void unlock_bus(struct i2c_adapter *adap, unsigned int flags) { mutex_unlock(&adap->bus_lock); }

static const struct i2c_lock_operations default_lock_ops = {
    .lock_bus = lock_bus, .trylock_bus = trylock_bus, .unlock_bus = unlock_bus,
};

static int add_adapter(struct i2c_adapter *adap)
{
    if (WARN(!adap->name[0], "i2c adapter has no name") || !adap->algo) return -EINVAL;
    if (!adap->lock_ops) adap->lock_ops = &default_lock_ops;
    mutex_init(&adap->bus_lock);
    if (!adap->timeout) adap->timeout = HZ;
    dev_set_name(&adap->dev, "i2c-%d", adap->nr);
    adap->registered = true;
    pr_debug("i2c-%d: adapter [%s] registered\n", adap->nr, adap->name);
    return 0;
}

int i2c_add_adapter(struct i2c_adapter *adap)
{
    adap->nr = atomic_inc_return(&next_nr) - 1;
    return add_adapter(adap);
}
EXPORT_SYMBOL(i2c_add_adapter);

int i2c_add_numbered_adapter(struct i2c_adapter *adap)
{
    if (adap->nr == -1) return i2c_add_adapter(adap);
    return add_adapter(adap);
}
EXPORT_SYMBOL(i2c_add_numbered_adapter);

void i2c_del_adapter(struct i2c_adapter *adap)
{
    if (!adap->registered) return;
    i2c_lock_bus(adap, I2C_LOCK_ROOT_ADAPTER);      /* wait for a transfer in progress */
    adap->registered = false;
    i2c_unlock_bus(adap, I2C_LOCK_ROOT_ADAPTER);
}
EXPORT_SYMBOL(i2c_del_adapter);

int __i2c_transfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num)
{
    unsigned long start = jiffies;
    int ret = -EOPNOTSUPP;

    if (!adap->algo->master_xfer) return -EOPNOTSUPP;
    if (num <= 0) return -EINVAL;
    for (int try = 0; try <= adap->retries; try++) {
        ret = adap->algo->master_xfer(adap, msgs, num);
        if (ret != -EAGAIN || time_after(jiffies, start + adap->timeout)) break;
    }
    return ret;
}
EXPORT_SYMBOL(__i2c_transfer);

int i2c_transfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num)
{
    int ret;

    if (!adap->registered) return -ENODEV;
    i2c_lock_bus(adap, I2C_LOCK_SEGMENT);
    ret = __i2c_transfer(adap, msgs, num);
    i2c_unlock_bus(adap, I2C_LOCK_SEGMENT);
    return ret;
}
EXPORT_SYMBOL(i2c_transfer);

int i2c_transfer_buffer_flags(const struct i2c_client *client, char *buf, int count, u16 flags)
{
    struct i2c_msg msg = {
        .addr = client->addr, .flags = flags | (client->flags & I2C_M_TEN), .len = count, .buf = (u8 *)buf,
    };
    int ret = i2c_transfer(client->adapter, &msg, 1);
    return ret == 1 ? count : ret < 0 ? ret : -EIO;
}
EXPORT_SYMBOL(i2c_transfer_buffer_flags);
