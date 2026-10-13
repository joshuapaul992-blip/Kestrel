/* linux/i2c.h -- Kestrel's Linux KPI: I2C types (no I2C core yet: EDID comes from the device itself) */
#ifndef _LKPI_LINUX_I2C_H
#define _LKPI_LINUX_I2C_H

#include <linux/types.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/errno.h>

struct i2c_algorithm;
struct i2c_client;
struct i2c_msg {
    u16 addr;
    u16 flags;
#define I2C_M_RD                0x0001
#define I2C_M_TEN               0x0010
#define I2C_M_NOSTART           0x4000
    u16 len;
    u8 *buf;
};
struct i2c_adapter {
    struct module *owner;
    unsigned int class;
    const struct i2c_algorithm *algo;
    void *algo_data;
    int timeout;
    int retries;
    struct device dev;
    int nr;
    char name[48];
};
struct i2c_algorithm {
    int (*master_xfer)(struct i2c_adapter *adap, struct i2c_msg *msgs, int num);
    u32 (*functionality)(struct i2c_adapter *adap);
};
struct i2c_board_info {
    char type[20];
    unsigned short flags;
    unsigned short addr;
    void *platform_data;
};
struct i2c_device_id { char name[20]; unsigned long driver_data; };
struct i2c_driver {
    int (*probe)(struct i2c_client *client);
    void (*remove)(struct i2c_client *client);
    struct device_driver driver;
    const struct i2c_device_id *id_table;
};
struct i2c_client {
    unsigned short flags;
    unsigned short addr;
    char name[20];
    struct i2c_adapter *adapter;
    struct device dev;
};
#define to_i2c_adapter(d)               container_of(d, struct i2c_adapter, dev)
#define to_i2c_client(d)                container_of(d, struct i2c_client, dev)
static inline void *i2c_get_adapdata(const struct i2c_adapter *adap) { return dev_get_drvdata(&adap->dev); }
static inline void i2c_set_adapdata(struct i2c_adapter *adap, void *data) { dev_set_drvdata(&adap->dev, data); }
static inline int i2c_transfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num)
{
    return adap->algo && adap->algo->master_xfer ? adap->algo->master_xfer(adap, msgs, num) : -EOPNOTSUPP;
}
static inline int i2c_register_driver(struct module *owner, struct i2c_driver *driver) { return 0; }
static inline void i2c_del_driver(struct i2c_driver *driver) { }
#define i2c_add_driver(driver)          i2c_register_driver(THIS_MODULE, driver)
static inline struct i2c_client *i2c_new_client_device(struct i2c_adapter *adap, struct i2c_board_info const *info)
{ return ERR_PTR(-ENODEV); }
static inline void i2c_unregister_device(struct i2c_client *client) { }

#endif
