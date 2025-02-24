#ifndef _MSG_BUFFER_H_
#define _MSG_BUFFER_H_

#define MSGBUFLEN (4*PGSIZE)  // 4 страницы для буфера

typedef struct msgbuf {
    struct spinlock lock;     // Спин-блокировка для защиты буфера
    char buf[MSGBUFLEN];      // Буфер для сообщений
    int begin;                // Начало данных
    int end;                  // Конец данных
} msgbuf;

extern msgbuf kmsgbuf;        // Глобальный буфер сообщений

void            msgbufinit(void);
void            msgadd(char*);
void            pr_msg(const char*, ...);

#endif