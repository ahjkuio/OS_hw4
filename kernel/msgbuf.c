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

    argaddr(0, &buf_addr);

    acquire(&kmsgbuf.lock);

    if (kmsgbuf.begin <= kmsgbuf.end) {
        if(copyout(p->pagetable, buf_addr,
                   kmsgbuf.buf + kmsgbuf.begin,
                   kmsgbuf.end - kmsgbuf.begin) < 0) {
            release(&kmsgbuf.lock);
            return -1;
        }
    } else {
        if(copyout(p->pagetable, buf_addr,
                   kmsgbuf.buf + kmsgbuf.begin,
                   MSGBUFLEN - kmsgbuf.begin) < 0) {
            release(&kmsgbuf.lock);
            return -1;
        }
        if(copyout(p->pagetable, buf_addr + (MSGBUFLEN - kmsgbuf.begin),
                   kmsgbuf.buf, kmsgbuf.end) < 0) {
            release(&kmsgbuf.lock);
            return -1;
        }
    }

    release(&kmsgbuf.lock);
    return 0;
}
