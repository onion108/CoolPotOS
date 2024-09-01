#include "../include/keyboard.h"
#include "../include/graphics.h"
#include "../include/memory.h"
#include "../include/queue.h"
#include "../include/io.h"
#include "../include/task.h"
#include "../include/klog.h"

static int caps_lock, shift, e0_flag = 0, ctrl = 0;
int disable_flag = 0;

static int handle_keyboard_input(registers_t *reg);

char keytable[0x54] = { // 按下Shift
        0,   0x01, '!', '@', '#', '$', '%',  '^', '&', '*', '(', ')', '_', '+', '\b', '\t', 'Q',
        'W', 'E',  'R', 'T', 'Y', 'U', 'I',  'O', 'P', '{', '}', 10,  0,   'A', 'S',  'D',  'F',
        'G', 'H',  'J', 'K', 'L', ':', '\"', '~', 0,   '|', 'Z', 'X', 'C', 'V', 'B',  'N',  'M',
        '<', '>',  '?', 0,   '*', 0,   ' ',  0,   0,   0,   0,   0,   0,   0,   0,    0,    0,
        0,   0,    0,   '7', 'D', '8', '-',  '4', '5', '6', '+', '1', '2', '3', '0',  '.'};

char keytable1[0x54] = { // 未按下Shift
        0,   0x01, '1', '2', '3', '4', '5',  '6', '7', '8',  '9', '0', '-', '=', '\b', '\t', 'q',
        'w', 'e',  'r', 't', 'y', 'u', 'i',  'o', 'p', '[',  ']', 10,  0,   'a', 's',  'd',  'f',
        'g', 'h',  'j', 'k', 'l', ';', '\'', '`', 0,   '\\', 'z', 'x', 'c', 'v', 'b',  'n',  'm',
        ',', '.',  '/', 0,   '*', 0,   ' ',  0,   0,   0,    0,   0,   0,   0,   0,    0,    0,
        0,   0,    0,   '7', '8', '9', '-',  '4', '5', '6',  '+', '1', '2', '3', '0',  '.'};


void init_keyboard(){
    register_interrupt_handler(0x21,handle_keyboard_input);

    klogf(true,"Load PS/2 Keyboard device.\n");
    return;
    error:
    klogf(false,"Load PS/2 Keyboard device.\n");
}

int handle_keyboard_input(registers_t *reg){
    uint8_t data = 0;
    io_out8(0x0020,0x61);
    data = io_in8(0x0060);

    if (data == 0xe0) {
        e0_flag = 1;
        return;
    }
    if (data == 0x2a || data == 0x36) { // Shift按下
        shift = 1;
    }
    if (data == 0x1d) { // Ctrl按下
        ctrl = 1;
    }
    if (data == 0x3a) { // Caps Lock按下
        caps_lock = caps_lock ^ 1;
    }
    if (data == 0xaa || data == 0xb6) { // Shift松开
        shift = 0;
    }
    if (data == 0x9d) { // Ctrl按下
        ctrl = 0;
    }

    extern struct task_struct *running_proc_head;
    if (data < 0x80) {
        struct task_struct *task = running_proc_head;
        while (1){
            if(task->tty != NULL){
                fifo8_put(task->tty->fifo,data);
            }
            task = task->next;
            if(task == NULL || task == running_proc_head) break;
        }
    }

    return 0;
}

int input_char_inSM() {
    int    i;
    struct task_struct *task = get_current();
    if (task->tty->is_using == false) {
    } else {
        do{
            i = fifo8_get(task->tty->fifo);
        } while (i == -1);
    }
    return i;
}

int kernel_getch() {
    uint8_t ch;
    ch = input_char_inSM(); // 扫描码
    if (ch == 0xe0) {       // keytable之外的键（↑,↓,←,→）
        ch = input_char_inSM();
        if (ch == 0x48) { // ↑
            return -2;
        } else if (ch == 0x50) { // ↓
            return -3;
        } else if (ch == 0x4b) { // ←
            return -4;
        } else if (ch == 0x4d) { // →
            return -5;
        }
    }
    // 返回扫描码(keytable之内)对应的ASCII码
    if (keytable[ch] == 0x00) {
        return 0;
    } else if (shift == 0 && caps_lock == 0) {
        return keytable1[ch];
    } else if (shift == 1 || caps_lock == 1) {
        return keytable[ch];
    } else if (shift == 1 && caps_lock == 1) {
        return keytable1[ch];
    }
    return -1;
}