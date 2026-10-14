/*
 * Smart Road and Route Assistance System  --  OS MODULE
 *
 * Implements:
 *   1. Process Management (fork, waitpid)
 *   2. Thread Management  (4 pthreads: camera, db, gps, alert)
 *   3. Priority Scheduling (P0: Safety > P1: Diversion > P2: Pothole > P3: Service)
 *   4. Producer-Consumer   (Bounded buffer with sem_wait/post & mutex)
 *   5. Inter-Process Comm  (Pipe, POSIX Shared Memory, POSIX Message Queue)
 *   6. Synchronization     (Mutexes, Semaphores, Condition Variables)
 *   7. Resource Management (Bounded buffers, frame drops, clean unlinking)
 *
 * Build: gcc -O2 -Wall -pthread os_module.c -o os_module -lrt
 * Run  : ./os_module
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <mqueue.h>
#include <stdatomic.h>
#include <sys/mman.h>
#include <sys/wait.h>

#define BUF_CAP     4
#define ALERT_CAP   16
#define MQ_NAME     "/smart_road_det"
#define SHM_NAME    "/smart_road_gps"

typedef struct { double lat, lon; } Location;
typedef struct { long id; double conf, lat, lon; char sev[8]; } Detection;
typedef struct { int pri; char msg[100]; } Alert;
typedef struct { pthread_mutex_t lock; double lat, lon; } GpsShm;

static const char *PRI_NAME[] = { "SAFETY", "DIVERSION", "POTHOLE", "SERVICE" };

/* Globals & Sync primitives */
static atomic_int   g_stop;
static GpsShm      *g_gps;
static mqd_t        g_mq;
static int          g_pipe_w = -1;

/* Bounded Buffer (Producer-Consumer) */
static struct {
    long buf[BUF_CAP];
    int tail;
    sem_t empty, full;
    pthread_mutex_t lock;
} g_bb;

/* Priority Alert Queue */
static Alert           g_aq[ALERT_CAP];
static int             g_an = 0, g_aq_closed = 0;
static pthread_mutex_t g_aq_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  g_aq_cv   = PTHREAD_COND_INITIALIZER;

#define LOG(tag, ...) do { printf("[%-9s] ", tag); printf(__VA_ARGS__); printf("\n"); } while(0)

/* ---------------- PRIORITY ALERT SCHEDULER ---------------- */
static void push_alert(int pri, const char *fmt, ...) {
    Alert a; a.pri = pri;
    va_list ap; va_start(ap, fmt); vsnprintf(a.msg, sizeof a.msg, fmt, ap); va_end(ap);

    pthread_mutex_lock(&g_aq_lock);
    if (g_an < ALERT_CAP) { g_aq[g_an++] = a; pthread_cond_signal(&g_aq_cv); }
    pthread_mutex_unlock(&g_aq_lock);
}

static void *alert_thread(void *arg) {
    (void)arg;
    for (;;) {
        pthread_mutex_lock(&g_aq_lock);
        while (g_an == 0 && !g_aq_closed) pthread_cond_wait(&g_aq_cv, &g_aq_lock);
        if (g_an == 0 && g_aq_closed) { pthread_mutex_unlock(&g_aq_lock); break; }

        /* Strict Priority: Pick lowest numerical value (0=Safety is highest) */
        int best = 0;
        for (int i = 1; i < g_an; i++) if (g_aq[i].pri < g_aq[best].pri) best = i;
        Alert a = g_aq[best];
        g_aq[best] = g_aq[--g_an];
        pthread_mutex_unlock(&g_aq_lock);

        usleep(100000);
        LOG("ALERT", "P%d (%-9s): %s", a.pri, PRI_NAME[a.pri], a.msg);
    }
    return NULL;
}

/* ---------------- CHILD PROCESS (DETECTOR) ---------------- */
static void detector_process(int rfd) {
    LOG("DETECTOR", "Subprocess started (PID: %d)", getpid());
    long frame_id;

    while (read(rfd, &frame_id, sizeof frame_id) > 0 && frame_id >= 0) {
        usleep(120000); /* Simulate YOLO inference */

        pthread_mutex_lock(&g_gps->lock);
        Location cur = { g_gps->lat, g_gps->lon };
        pthread_mutex_unlock(&g_gps->lock);

        if (frame_id == 3 || frame_id == 6) {
            Detection d = { frame_id, 0.90, cur.lat, cur.lon, (frame_id == 3) ? "High" : "Low" };
            unsigned prio = (d.sev[0] == 'H') ? 30 : 10;
            mq_send(g_mq, (const char *)&d, sizeof d, prio);
        }
    }
    Detection end = { -1 };
    mq_send(g_mq, (const char *)&end, sizeof end, 0);
    LOG("DETECTOR", "Subprocess complete");
    _exit(0);
}

/* ---------------- PARENT THREADS ---------------- */
/* 1. Camera Producer: writes to bounded buffer & pipe */
static void *camera_thread(void *arg) {
    (void)arg; long id = 0;
    while (!atomic_load(&g_stop)) {
        usleep(200000);
        long f_id = ++id;

        if (sem_trywait(&g_bb.empty) == -1) {
            LOG("CAMERA", "Buffer full -> frame %ld dropped", f_id);
            continue;
        }
        pthread_mutex_lock(&g_bb.lock);
        g_bb.buf[g_bb.tail] = f_id;
        g_bb.tail = (g_bb.tail + 1) % BUF_CAP;
        pthread_mutex_unlock(&g_bb.lock);
        sem_post(&g_bb.full);

        write(g_pipe_w, &f_id, sizeof f_id); /* IPC Pipe to Child */
    }
    long end = -1;
    write(g_pipe_w, &end, sizeof end);
    return NULL;
}

/* 2. DB Consumer: reads from POSIX Message Queue */
static void *db_thread(void *arg) {
    (void)arg;
    for (;;) {
        Detection d; unsigned pr;
        if (mq_receive(g_mq, (char *)&d, sizeof d, &pr) < 0 || d.id < 0) break;

        LOG("DB", "INSERT INTO POTHOLES (LAT, LON, SEV) VALUES (%.4f, %.4f, '%s');", d.lat, d.lon, d.sev);
        push_alert(d.sev[0] == 'H' ? 0 : 2, "%s pothole detected at %.4f, %.4f", d.sev, d.lat, d.lon);
    }
    return NULL;
}

/* 3. GPS Thread: moves vehicle, writes to Shared Memory */
static void *gps_thread(void *arg) {
    (void)arg;
    for (int step = 1; step <= 8 && !atomic_load(&g_stop); step++) {
        usleep(400000);
        pthread_mutex_lock(&g_gps->lock);
        g_gps->lat += 0.0005; g_gps->lon += 0.0005;
        LOG("GPS", "Position: %.4f, %.4f (Step %d/8)", g_gps->lat, g_gps->lon, step);
        pthread_mutex_unlock(&g_gps->lock);

        if (step == 2) push_alert(1, "Diversion ahead on Subhash Road");
        if (step == 4) push_alert(3, "Nearby Fuel Station 250m");
        if (step == 5) push_alert(2, "Known Medium pothole ahead - slow down");
    }
    atomic_store(&g_stop, 1);
    return NULL;
}

/* ---------------- MAIN PROGRAM ---------------- */
int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);

    /* 1. IPC & Synchronization Setup */
    shm_unlink(SHM_NAME);
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0600);
    ftruncate(shm_fd, sizeof(GpsShm));
    g_gps = mmap(NULL, sizeof(GpsShm), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    close(shm_fd);

    pthread_mutexattr_t mattr; pthread_mutexattr_init(&mattr);
    pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&g_gps->lock, &mattr);
    g_gps->lat = 30.3165; g_gps->lon = 78.0322;

    struct mq_attr attr = { .mq_maxmsg = 10, .mq_msgsize = sizeof(Detection) };
    mq_unlink(MQ_NAME);
    g_mq = mq_open(MQ_NAME, O_CREAT | O_RDWR, 0600, &attr);

    int pfd[2]; pipe(pfd);

    sem_init(&g_bb.empty, 0, BUF_CAP);
    sem_init(&g_bb.full, 0, 0);
    pthread_mutex_init(&g_bb.lock, NULL);

    printf("=== Smart Road OS Module Started (PID: %d) ===\n", getpid());

    /* 2. Fork Child Detector */
    pid_t pid = fork();
    if (pid == 0) { close(pfd[1]); detector_process(pfd[0]); }
    close(pfd[0]); g_pipe_w = pfd[1];

    /* 3. Launch Core Threads */
    pthread_t t_cam, t_db, t_gps, t_alert;
    pthread_create(&t_alert, NULL, alert_thread,  NULL);
    pthread_create(&t_db,    NULL, db_thread,     NULL);
    pthread_create(&t_cam,   NULL, camera_thread, NULL);
    pthread_create(&t_gps,   NULL, gps_thread,    NULL);

    /* 4. Ordered Pipeline Shutdown */
    pthread_join(t_gps, NULL);
    pthread_join(t_cam, NULL);
    close(g_pipe_w);

    waitpid(pid, NULL, 0); /* Reap child process */
    pthread_join(t_db, NULL);

    pthread_mutex_lock(&g_aq_lock);
    g_aq_closed = 1;
    pthread_cond_broadcast(&g_aq_cv);
    pthread_mutex_unlock(&g_aq_lock);
    pthread_join(t_alert, NULL);

    /* 5. Resource Cleanup */
    mq_close(g_mq); mq_unlink(MQ_NAME);
    munmap(g_gps, sizeof(GpsShm)); shm_unlink(SHM_NAME);
    sem_destroy(&g_bb.empty); sem_destroy(&g_bb.full);

    printf("=== Execution Finished. Resources Released. ===\n");
    return 0;
}
