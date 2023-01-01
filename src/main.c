#include <zephyr/init.h>

void main(void)
{
    int enable_rc = usb_enable(NULL);
    printk("enable_rc %d\n", enable_rc);
}
