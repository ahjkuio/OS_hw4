#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "proc.h"
#include "defs.h"
#include "msgbuf.h"
#include <stdarg.h>

extern char digits[];
extern struct spinlock tickslock;
extern uint ticks;

msgbuf kmsgbuf;  // Глобальный буфер сообщений
static int initialized = 0;

void
msgbufinit(void)
{
    initlock(&kmsgbuf.lock, "msgbuf");
    acquire(&kmsgbuf.lock);
    kmsgbuf.begin = 0;
    kmsgbuf.end = 0;
    initialized = 1;
    release(&kmsgbuf.lock);
}

// Вспомогательная функция для записи символа в буфер
static void
buf_putc(char c)
{
    kmsgbuf.buf[kmsgbuf.end] = c;
    kmsgbuf.end = (kmsgbuf.end + 1) % MSGBUFLEN;

    // Если буфер заполнился, сдвигаем начало до следующей строки
    if (kmsgbuf.end == kmsgbuf.begin) {
        while (kmsgbuf.buf[kmsgbuf.begin] != '\n') {
            kmsgbuf.begin = (kmsgbuf.begin + 1) % MSGBUFLEN;
        }
        kmsgbuf.begin = (kmsgbuf.begin + 1) % MSGBUFLEN;
    }
}

// Вспомогательная функция для вывода числа
static void
buf_printint(int xx, int base, int sign)
{
    char digits[16];
    int i = 0;
    uint x;

    if(sign && (sign = xx < 0))
        x = -xx;
    else
        x = xx;

    do {
        digits[i++] = "0123456789abcdef"[x % base];
    } while((x /= base) != 0);

    if(sign)
        digits[i++] = '-';

    while(--i >= 0)
        buf_putc(digits[i]);
}

void
pr_msg(const char *fmt, ...)
{
    if(!initialized)
        return;

    acquire(&kmsgbuf.lock);

    // Добавляем метку времени
    acquire(&tickslock);
    int t = ticks;
    release(&tickslock);

    buf_putc('[');
    buf_printint(t, 10, 0);
    buf_putc(']');
    buf_putc(' ');

    int i, c;
    char *s;
    va_list ap;

    va_start(ap, fmt);

    for(i = 0; (c = fmt[i] & 0xff) != 0; i++){
        if(c != '%'){
            buf_putc(c);
            continue;
        }
        c = fmt[++i] & 0xff;
        if(c == 0)
            break;
        switch(c){
        case 'd':
            buf_printint(va_arg(ap, int), 10, 1);
            break;
        case 'x':
        case 'p':
            buf_printint(va_arg(ap, uint), 16, 0);
            break;
        case 's':
            s = va_arg(ap, char*);
            if(s == 0)
                s = "(null)";
            for(; *s; s++)
                buf_putc(*s);
            break;
        case '%':
            buf_putc('%');
            break;
        default:
            buf_putc('%');
            buf_putc(c);
            break;
        }
    }
    va_end(ap);
    buf_putc('\n');
    release(&kmsgbuf.lock);
}

// Системный вызов dmesg
uint64
sys_dmesg(void)
{
    uint64 buf_addr;
    struct proc *p = myproc();
    int len;

    argaddr(0, &buf_addr);

    // Проверяем, что buf_addr хороший адрес в пользовательском пространстве
    if (buf_addr >= p->sz) {
        printf("sys_dmesg: invalid buf_addr 0x%lx >= p->sz 0x%lx\n", buf_addr, p->sz);
        return -1; // Плохой адрес
    }

    acquire(&kmsgbuf.lock);

    len = 0;
    if (kmsgbuf.begin <= kmsgbuf.end) {
        len = kmsgbuf.end - kmsgbuf.begin;
        printf("sys_dmesg: copying from begin %d to end %d, len %d\n", kmsgbuf.begin, kmsgbuf.end, len);
        printf("sys_dmesg: buf_addr=0x%lx, len=%d, p->sz=0x%lx\n", buf_addr, len, p->sz);
        if(copyout(p->pagetable, buf_addr,
                   kmsgbuf.buf + kmsgbuf.begin,
                   len) < 0) {
            printf("sys_dmesg: copyout failed (1)\n");
            release(&kmsgbuf.lock);
            return -1;
        }
    } else {
        len = MSGBUFLEN - kmsgbuf.begin + kmsgbuf.end;
        printf("sys_dmesg: copying wrapped buffer, len %d\n", len);
        printf("sys_dmesg: buf_addr=0x%lx, len=%d, p->sz=0x%lx\n", buf_addr, len, p->sz);

        if(copyout(p->pagetable, buf_addr,
                   kmsgbuf.buf + kmsgbuf.begin,
                   MSGBUFLEN - kmsgbuf.begin) < 0) {
            printf("sys_dmesg: copyout failed (2)\n");
            release(&kmsgbuf.lock);
            return -1;
        }
        if(copyout(p->pagetable, buf_addr + (MSGBUFLEN - kmsgbuf.begin),
                   kmsgbuf.buf, kmsgbuf.end) < 0) {
            printf("sys_dmesg: copyout failed (3)\n");
            release(&kmsgbuf.lock);
            return -1;
        }
    }

    // Нулевая терминация
    if (len < MSGBUFLEN) { // Убеждаемся, что есть место для '\0'
        char zero = '\0';
        printf("sys_dmesg: adding null terminator at buf_addr + len = 0x%lx\n", buf_addr + len);
        if (copyout(p->pagetable, buf_addr + len, &zero, 1) < 0) {
            printf("sys_dmesg: copyout failed (null terminator)\n");
            release(&kmsgbuf.lock);
            return -1;
        }
    } else {
        // Буфер заполнен, не можем добавить '\0'
        printf("sys_dmesg: buffer full, cannot add null terminator\n");
        release(&kmsgbuf.lock);
        return -1;
    }


    release(&kmsgbuf.lock);
    return 0;
}