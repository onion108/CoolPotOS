#include "tty.h"
#include "gop.h"
#include "keyboard.h"
#include "klog.h"
#include "kprint.h"
#include "krlibc.h"
#include "lock.h"
#include "mpmc_queue.h"
#include "pcb.h"
#include "terminal.h"
#include "vdisk.h"

tty_t        *defualt_tty = NULL;
mpmc_queue_t *queue;
spin_t        tty_lock = SPIN_INIT;

extern bool open_flush; // terminal.c

static void tty_kernel_print(tty_t *tty, const char *msg) {
    if (open_flush) {
        terminal_puts(msg);
    } else {
        spin_lock(tty_lock);
        for (size_t i = 0; msg[i] != '\0'; i++) {
            char c = msg[i];
            if (c == '\n') { terminal_process_byte('\r'); }
            terminal_process_byte(c);
        }
        spin_unlock(tty_lock);
    }
}

static void tty_kernel_putc(tty_t *tty, int c) {
    if (open_flush)
        terminal_putc((char)c);
    else {
        spin_lock(tty_lock);
        if (c == '\n') { terminal_process_byte('\r'); }
        terminal_process_byte(c);
        spin_unlock(tty_lock);
    }
}

tty_t *alloc_default_tty() {
    tty_t *tty     = malloc(sizeof(tty_t));
    tty->video_ram = framebuffer->address;
    tty->height    = framebuffer->height;
    tty->width     = framebuffer->width;
    tty->print     = tty_kernel_print;
    tty->putchar   = tty_kernel_putc;
    tty->mode      = ECHO;
    return tty;
}

void free_tty(tty_t *tty) {
    if (tty == NULL) return;
    free(tty);
}

tty_t *get_default_tty() {
    return defualt_tty;
}

static void stdin_read(int drive, uint8_t *buffer, uint32_t number, uint32_t lba) {
    for (size_t i = 0; i < number; i++) {
        char c = (char)kernel_getch();
        if (get_current_task()->parent_group->tty->mode == ECHO) printk("%c", c);
        if (c == '\n' || c == '\r') {
            buffer[i] = 0x0a;
            break;
        }
        buffer[i] = c;
    }
}

static void stdout_write(int drive, uint8_t *buffer, uint32_t number, uint32_t lba) {
    tty_t *tty;
    if (get_current_task() == NULL) {
        tty = get_default_tty();
    } else
        tty = get_current_task()->parent_group->tty;

    for (size_t i = 0; i < number; i++) {
        tty->putchar(tty, buffer[i]);
    }
}

static void tty_ioctl(size_t req, void *arg) {}

void build_tty_device() {
    vdisk stdio;
    stdio.type = VDISK_STREAM;
    strcpy(stdio.drive_name, "stdio");
    stdio.flag        = 1;
    stdio.sector_size = 1;
    stdio.size        = 1;
    stdio.read        = stdin_read;
    stdio.write       = stdout_write;
    stdio.ioctl       = (void *)empty;
    regist_vdisk(stdio);

    vdisk tty0;
    tty0.type = VDISK_STREAM;
    strcpy(tty0.drive_name, "tty0");
    tty0.flag        = 1;
    tty0.sector_size = 1;
    tty0.size        = 1;
    tty0.read        = (void *)empty;
    tty0.write       = (void *)empty;
    tty0.ioctl       = tty_ioctl;
    regist_vdisk(tty0);
}

void init_tty() {
    defualt_tty            = malloc(sizeof(tty_t));
    defualt_tty->video_ram = framebuffer->address;
    defualt_tty->width     = framebuffer->width;
    defualt_tty->height    = framebuffer->height;
    defualt_tty->mode      = ECHO;
    defualt_tty->print     = tty_kernel_print;
    defualt_tty->putchar   = tty_kernel_putc;
    queue                  = malloc(sizeof(mpmc_queue_t));
    if (init(queue, 1024, sizeof(uint32_t), FAIL) != SUCCESS) {
        kerror("Cannot enable mpmc buffer\n");
        free(queue);
        queue = NULL;
    }
    build_tty_device();
    kinfo("Default tty device init done.");
}
