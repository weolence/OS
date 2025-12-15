#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

#include <pthread.h>
#include <sched.h>

#include "queue.h"

#define RED "\033[41m"
#define NOCOLOR "\033[0m"

void set_cpu(int n) {
	int err;
	cpu_set_t cpuset;
	pthread_t tid = pthread_self();

	CPU_ZERO(&cpuset);
	CPU_SET(n, &cpuset);

	err = pthread_setaffinity_np(tid, sizeof(cpu_set_t), &cpuset);
	if (err) {
		printf("set_cpu: pthread_setaffinity failed for cpu %d\n", n);
		return;
	}

	printf("set_cpu: set cpu %d\n", n);
}

void *reader(void *arg) {
	int expected = 0;
	queue_t *q = (queue_t *)arg;
	printf("reader [%d %d %d]\n", getpid(), getppid(), gettid());

	set_cpu(1);

	while (1) {
		int val = -1;
		int ok = queue_get(q, &val);
		if (!ok)
			continue;

		if (expected != val)
			printf(RED"ERROR: get value is %d but expected - %d" NOCOLOR "\n", val, expected);

		expected = val + 1;
	}

	return NULL;
}

void *writer(void *arg) {
	int i = 0;
	queue_t *q = (queue_t *)arg;
	printf("writer [%d %d %d]\n", getpid(), getppid(), gettid());

	set_cpu(1);

	while (1) {
		int ok = queue_add(q, i);
		if (!ok)
			continue;
		i++;
	}

	return NULL;
}

/*
с размером очереди в 1000 всё хорошо и вывод имеет вид:

main [4323 1023 4323]
qmonitor: [4323 1023 4324]
queue stats: current size 0; attempts: (0 0 0); counts (0 0 0)
reader [4323 1023 4325]
writer [4323 1023 4326]
set_cpu: set cpu 1
set_cpu: set cpu 1
queue stats: current size 1000; attempts: (95087664 122263264 -27175600); counts (125000 124000 1000)
queue stats: current size 1000; attempts: (186799634 238482244 -51682610); counts (250000 249000 1000)
queue stats: current size 0; attempts: (275752528 341427814 -65675286); counts (374000 374000 0)
queue stats: current size 0; attempts: (367069293 456256179 -89186886); counts (499000 499000 0)
queue stats: current size 1000; attempts: (454885177 571299166 -116413989); counts (622000 621000 1000)

скорее всего критических ошибок в работе программы не возникает из-за работы механизма псведопараллелизма и привязки
к одному ядру. за выделенные кванты поток-writer успевает полностью заполнить очередь из-за её малого размера, а
поток-reader в свою очередь опустошить. отрицательная разница в числе попыток скорее всего из-за того, что удаление элемента
как операция быстрее добавления элемента

при привязывании потоков к двум различным ядрам даже при размере очереди 1000 проблемы начинаются сразу,
потому что с самого начала возникает гонка данных. в худшем случае получается так, что get пытается удалить
элемент из очереди, контекст меняется на поток-writer и тот пытается записать в поле next только что освобождённой
структуры указатель на новый элемент

с удалением sched_yeild(), судя по выводу в работе программ ничего особо не изменилось, при 1000 на
одном ядре всё также относительно неплохо, при 1000000 на разных всё страшно. сама эта функция давала
лишь некую фору для запуска потока-writer и единственное, что можно заметить по выводу, теперь в начале
запуска программы фигурирует больше статистик с полной очередью ну потому что writer запустился раньше и управление
перед его запуском скорее всего никуда не передавалось
*/
int main() {
	pthread_t tid_reader, tid_writer;
	queue_t *q;
	int err;

	printf("main [%d %d %d]\n", getpid(), getppid(), gettid());

	q = queue_init(1000);

	err = pthread_create(&tid_reader, NULL, reader, q);
	if (err) {
		printf("main: pthread_create() failed: %s\n", strerror(err));
		return -1;
	}

	sched_yield();

	err = pthread_create(&tid_writer, NULL, writer, q);
	if (err) {
		printf("main: pthread_create() failed: %s\n", strerror(err));
		return -1;
	}

	pthread_join(tid_writer, NULL);
	pthread_join(tid_reader, NULL);

	queue_destroy(q);

	return 0;
}

/*
error 1:
main [2127 1023 2127]
qmonitor: [2127 1023 2128]
queue stats: current size 0; attempts: (0 0 0); counts (0 0 0)
reader [2127 1023 2129]
set_cpu: set cpu 1
writer [2127 1023 2130]
set_cpu: set cpu 1
ERROR: get value is 5643184 but expected - 5643183
[1]    2127 segmentation fault (core dumped)  ./queue-threads

error 2:
main [2203 1023 2203]
qmonitor: [2203 1023 2204]
queue stats: current size 0; attempts: (0 0 0); counts (0 0 0)
reader [2203 1023 2205]
writer [2203 1023 2206]
set_cpu: set cpu 1
set_cpu: set cpu 1
[1]    2203 segmentation fault (core dumped)  ./queue-threads

при увеличении размера очереди процессор за выделенные кванты не успевает заполнить или опустошить её
полностью, а значит контекст выполнения потока меняется во время выполнения get/add, что
приводит к гонке данных и этим двум ошибкам при попытке работы с уже освобождённой памятью
*/