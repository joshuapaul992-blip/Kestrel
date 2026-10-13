/* linux/i2c.h -- Kestrel's Linux KPI: I2C adapters and transfers
 *
 * Enough of the I2C core for display drivers: adapters a driver registers
 * (its own algorithm, or i2c-algo-bit's bit-banging), transfers with
 * retries and bus locking (src/i2c.c), and the message and capability bits
 * of Linux's I2C interface. There are no I2C client drivers:
 * i2c_new_client_device() finds none. */
#ifndef _LKPI_LINUX_I2C_H
#define _LKPI_LINUX_I2C_H

#include <linux/types.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/bits.h>
#include <linux/workqueue.h>                     /* users expect it, as Linux's device.h brings it */

struct i2c_algorithm;
struct i2c_client;
struct i2c_adapter;

/* ---- messages (the values of Linux's user-space interface) ------------------- */
struct i2c_msg {
    u16 addr;
    u16 flags;
#define I2C_M_RD                0x0001
#define I2C_M_TEN               0x0010
#define I2C_M_DMA_SAFE          0x0200
#define I2C_M_RECV_LEN          0x0400
#define I2C_M_NO_RD_ACK         0x0800
#define I2C_M_IGNORE_NAK        0x1000
#define I2C_M_REV_DIR_ADDR      0x2000
#define I2C_M_NOSTART           0x4000
#define I2C_M_STOP              0x8000
    u16 len;
    u8 *buf;
};

#define I2C_SMBUS_BLOCK_MAX     32

#define I2C_FUNC_I2C                    0x00000001
#define I2C_FUNC_10BIT_ADDR             0x00000002
#define I2C_FUNC_PROTOCOL_MANGLING      0x00000004
#define I2C_FUNC_SMBUS_PEC              0x00000008
#define I2C_FUNC_NOSTART                0x00000010
#define I2C_FUNC_SLAVE                  0x00000020
#define I2C_FUNC_SMBUS_BLOCK_PROC_CALL  0x00008000
#define I2C_FUNC_SMBUS_QUICK            0x00010000
#define I2C_FUNC_SMBUS_READ_BYTE        0x00020000
#define I2C_FUNC_SMBUS_WRITE_BYTE       0x00040000
#define I2C_FUNC_SMBUS_READ_BYTE_DATA   0x00080000
#define I2C_FUNC_SMBUS_WRITE_BYTE_DATA  0x00100000
#define I2C_FUNC_SMBUS_READ_WORD_DATA   0x00200000
#define I2C_FUNC_SMBUS_WRITE_WORD_DATA  0x00400000
#define I2C_FUNC_SMBUS_PROC_CALL        0x00800000
#define I2C_FUNC_SMBUS_READ_BLOCK_DATA  0x01000000
#define I2C_FUNC_SMBUS_WRITE_BLOCK_DATA 0x02000000
#define I2C_FUNC_SMBUS_READ_I2C_BLOCK   0x04000000
#define I2C_FUNC_SMBUS_WRITE_I2C_BLOCK  0x08000000
#define I2C_FUNC_SMBUS_HOST_NOTIFY      0x10000000
#define I2C_FUNC_SMBUS_BYTE             (I2C_FUNC_SMBUS_READ_BYTE | I2C_FUNC_SMBUS_WRITE_BYTE)
#define I2C_FUNC_SMBUS_BYTE_DATA        (I2C_FUNC_SMBUS_READ_BYTE_DATA | I2C_FUNC_SMBUS_WRITE_BYTE_DATA)
#define I2C_FUNC_SMBUS_WORD_DATA        (I2C_FUNC_SMBUS_READ_WORD_DATA | I2C_FUNC_SMBUS_WRITE_WORD_DATA)
#define I2C_FUNC_SMBUS_BLOCK_DATA       (I2C_FUNC_SMBUS_READ_BLOCK_DATA | I2C_FUNC_SMBUS_WRITE_BLOCK_DATA)
#define I2C_FUNC_SMBUS_I2C_BLOCK        (I2C_FUNC_SMBUS_READ_I2C_BLOCK | I2C_FUNC_SMBUS_WRITE_I2C_BLOCK)
#define I2C_FUNC_SMBUS_EMUL             (I2C_FUNC_SMBUS_QUICK | I2C_FUNC_SMBUS_BYTE | I2C_FUNC_SMBUS_BYTE_DATA | \
                                         I2C_FUNC_SMBUS_WORD_DATA | I2C_FUNC_SMBUS_PROC_CALL | \
                                         I2C_FUNC_SMBUS_WRITE_BLOCK_DATA | I2C_FUNC_SMBUS_I2C_BLOCK | \
                                         I2C_FUNC_SMBUS_PEC)
#define I2C_FUNC_SMBUS_EMUL_ALL         (I2C_FUNC_SMBUS_EMUL | I2C_FUNC_SMBUS_READ_BLOCK_DATA | \
                                         I2C_FUNC_SMBUS_BLOCK_PROC_CALL)

static inline u8 i2c_8bit_addr_from_msg(const struct i2c_msg *msg)
{
    return (msg->addr << 1) | (msg->flags & I2C_M_RD);
}

/* ---- adapters ------------------------------------------------------------------ */
struct i2c_algorithm {
    union {                                     /* Linux has both names */
        int (*xfer)(struct i2c_adapter *adap, struct i2c_msg *msgs, int num);
        int (*master_xfer)(struct i2c_adapter *adap, struct i2c_msg *msgs, int num);
    };
    union {
        int (*xfer_atomic)(struct i2c_adapter *adap, struct i2c_msg *msgs, int num);
        int (*master_xfer_atomic)(struct i2c_adapter *adap, struct i2c_msg *msgs, int num);
    };
    u32 (*functionality)(struct i2c_adapter *adap);
};

#define I2C_LOCK_ROOT_ADAPTER   BIT(0)
#define I2C_LOCK_SEGMENT        BIT(1)

struct i2c_lock_operations {
    void (*lock_bus)(struct i2c_adapter *adap, unsigned int flags);
    int  (*trylock_bus)(struct i2c_adapter *adap, unsigned int flags);
    void (*unlock_bus)(struct i2c_adapter *adap, unsigned int flags);
};

#define I2C_AQ_COMB                     BIT(0)
#define I2C_AQ_COMB_WRITE_FIRST         BIT(1)
#define I2C_AQ_COMB_READ_SECOND         BIT(2)
#define I2C_AQ_COMB_SAME_ADDR           BIT(3)
#define I2C_AQ_NO_CLK_STRETCH           BIT(4)
#define I2C_AQ_NO_ZERO_LEN_READ         BIT(5)
#define I2C_AQ_NO_ZERO_LEN_WRITE        BIT(6)
#define I2C_AQ_NO_REP_START             BIT(7)

struct i2c_adapter_quirks {
    u64 flags;
    int max_num_msgs;
    u16 max_write_len;
    u16 max_read_len;
    u16 max_comb_1st_msg_len;
    u16 max_comb_2nd_msg_len;
};

struct i2c_adapter {
    struct module *owner;
    unsigned int class;
    const struct i2c_algorithm *algo;
    void *algo_data;
    const struct i2c_lock_operations *lock_ops;
    struct mutex bus_lock;                      /* the default lock_ops' lock */
    int timeout;                                /* jiffies; HZ if 0 at registration */
    int retries;
    struct device dev;
    int nr;
    char name[48];
    const struct i2c_adapter_quirks *quirks;
    bool registered;
};
#define to_i2c_adapter(d)               container_of(d, struct i2c_adapter, dev)

static inline void *i2c_get_adapdata(const struct i2c_adapter *adap) { return dev_get_drvdata(&adap->dev); }
static inline void i2c_set_adapdata(struct i2c_adapter *adap, void *data) { dev_set_drvdata(&adap->dev, data); }

int  i2c_add_adapter(struct i2c_adapter *adap);
int  i2c_add_numbered_adapter(struct i2c_adapter *adap);
void i2c_del_adapter(struct i2c_adapter *adap);

static inline void i2c_lock_bus(struct i2c_adapter *adap, unsigned int flags) { adap->lock_ops->lock_bus(adap, flags); }
static inline int i2c_trylock_bus(struct i2c_adapter *adap, unsigned int flags) { return adap->lock_ops->trylock_bus(adap, flags); }
static inline void i2c_unlock_bus(struct i2c_adapter *adap, unsigned int flags) { adap->lock_ops->unlock_bus(adap, flags); }

/* Number of messages transferred, or a negative errno. __i2c_transfer
 * expects the bus to be locked already. */
int  i2c_transfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num);
int  __i2c_transfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num);
int  i2c_transfer_buffer_flags(const struct i2c_client *client, char *buf, int count, u16 flags);

static inline u32 i2c_get_functionality(struct i2c_adapter *adap) { return adap->algo->functionality(adap); }
static inline int i2c_check_functionality(struct i2c_adapter *adap, u32 func)
{
    return (func & i2c_get_functionality(adap)) == func;
}

/* ---- clients (none bind: Kestrel has no I2C client drivers) --------------------- */
#define I2C_NAME_SIZE                   20
#define I2C_MODULE_PREFIX               "i2c:"

struct i2c_board_info {
    char type[I2C_NAME_SIZE];
    unsigned short flags;
    unsigned short addr;
    const char *dev_name;
    void *platform_data;
    int irq;
};
#define I2C_BOARD_INFO(dev_type, dev_addr)      .type = dev_type, .addr = (dev_addr)

struct i2c_device_id { char name[I2C_NAME_SIZE]; unsigned long driver_data; };

struct i2c_client {
    unsigned short flags;
    unsigned short addr;
    char name[I2C_NAME_SIZE];
    struct i2c_adapter *adapter;
    struct device dev;
    int irq;
};
#define to_i2c_client(d)                container_of(d, struct i2c_client, dev)
static inline void *i2c_get_clientdata(const struct i2c_client *client) { return dev_get_drvdata(&client->dev); }
static inline void i2c_set_clientdata(struct i2c_client *client, void *data) { dev_set_drvdata(&client->dev, data); }

struct i2c_driver {
    unsigned int class;
    int (*detect)(struct i2c_client *client, struct i2c_board_info *info);
    const unsigned short *address_list;
    int (*probe)(struct i2c_client *client);
    void (*remove)(struct i2c_client *client);
    struct device_driver driver;
    const struct i2c_device_id *id_table;
};

#define to_i2c_driver(d)                container_of_const(d, struct i2c_driver, driver)
static inline int i2c_register_driver(struct module *owner, struct i2c_driver *driver) { return 0; }
static inline void i2c_del_driver(struct i2c_driver *driver) { }
#define i2c_add_driver(driver)          i2c_register_driver(THIS_MODULE, driver)
static inline struct i2c_client *i2c_new_client_device(struct i2c_adapter *adap, struct i2c_board_info const *info)
{ return ERR_PTR(-ENODEV); }
static inline void i2c_unregister_device(struct i2c_client *client) { }
static inline bool i2c_client_has_driver(struct i2c_client *client)
{ return !IS_ERR_OR_NULL(client) && client->dev.driver; }

static inline int i2c_master_send(const struct i2c_client *client, const char *buf, int count)
{ return i2c_transfer_buffer_flags(client, (char *)buf, count, 0); }
static inline int i2c_master_recv(const struct i2c_client *client, char *buf, int count)
{ return i2c_transfer_buffer_flags(client, buf, count, I2C_M_RD); }

#endif
