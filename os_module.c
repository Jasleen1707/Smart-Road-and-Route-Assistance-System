/*
 * Smart Road and Route Assistance System -- OS + DBMS MODULE
 * Owner: Krish Jaisingh (OS) | Schema: Jasleen Kaur (DBMS)
 *
 * Implements: Process Management (fork/waitpid), 4 Threads, Producer-Consumer,
 * Priority Scheduling, 3 IPCs (Pipe, POSIX SHM, POSIX MQ), and DBMS Integration.
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

#define BUF_CAP    4
#define ALERT_CAP  16
#define MQ_NAME    "/smart_road_det"
#define SHM_NAME   "/smart_road_gps"

typedef enum { PRI_SAFETY = 0, PRI_DIVERSION, PRI_POTHOLE, PRI_SERVICE, PRI_COUNT } AlertPriority;
static const char *PRI_NAME[PRI_COUNT] = { "SAFETY", "DIVERSION", "POTHOLE", "SERVICE" };

typedef struct { double lat, lon; } Location;
typedef struct { long id; double conf, lat, lon; char sev[16]; } Detection;
typedef struct { int pri; char msg[128]; } Alert;
typedef struct { pthread_mutex_t lock; double lat, lon; } GpsShm;

static atomic_int g_stop;
static GpsShm    *g_gps;
static mqd_t      g_mq;
static int        g_pipe_w = -1;

/* Bounded Buffer (Producer-Consumer) */
static struct {
    long buf[BUF_CAP];
    int tail;
    sem_t empty, full;
    pthread_mutex_t lock;
} g_bb;

/* Priority Alert Queue */
static Alert g_aq[ALERT_CAP];
static int   g_an = 0, g_aq_closed = 0;
static pthread_mutex_t g_aq_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  g_aq_cv   = PTHREAD_COND_INITIALIZER;

#define LOG(tag, ...) do { printf("[%-9s] ", tag); printf(__VA_ARGS__); printf("\n"); } while (0)

/* ---------- Priority Alert Scheduler ---------- */
static void push_alert(int pri, const char *fmt, ...) {
    Alert a = { .pri = pri };
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
        int best = 0;
        for (int i = 1; i < g_an; i++) if (g_aq[i].pri < g_aq[best].pri) best = i;
        Alert a = g_aq[best]; g_aq[best] = g_aq[--g_an];
        pthread_mutex_unlock(&g_aq_lock);
        usleep(100000);
        LOG("ALERT", "P%d (%-9s): %s", a.pri, PRI_NAME[a.pri], a.msg);
    }
    return NULL;
}

/* ---------- Child Process (Detector - AI Subprocess) ---------- */
static void detector_process(int rfd) {
    LOG("DETECTOR", "Child process started (PID %d), reading from pipe", getpid());
    static const char *SEV[] = { "Low", "Medium", "High" };
    long frame_id; int n = 0;

    while (read(rfd, &frame_id, sizeof frame_id) == (ssize_t)sizeof frame_id && frame_id >= 0) {
        pthread_mutex_lock(&g_gps->lock);
        Location cur = { g_gps->lat, g_gps->lon };
        pthread_mutex_unlock(&g_gps->lock);

        /* HOOK (Shivika): YOLO model runs here. Simulation: hit every 3rd frame */
        if (frame_id % 3 == 0) {
            Detection d = { .id = frame_id, .conf = 0.85 + 0.02 * (n % 6), .lat = cur.lat, .lon = cur.lon };
            snprintf(d.sev, sizeof d.sev, "%s", SEV[n++ % 3]);
            mq_send(g_mq, (const char *)&d, sizeof d, (d.sev[0] == 'H') ? 30 : 10);
        }
    }
    Detection end = { .id = -1 };
    mq_send(g_mq, (const char *)&end, sizeof end, 0);
    LOG("DETECTOR", "Child process exiting cleanly");
    _exit(0);
}

/* ---------- Parent Threads ---------- */
/* Producer: Camera captures frames into bounded buffer & forwards to pipe */
static void *camera_thread(void *arg) {
    (void)arg; long id = 0;
    while (!atomic_load(&g_stop)) {
        usleep(200000);
        long f_id = ++id;
        if (sem_trywait(&g_bb.empty) == -1) {
            LOG("CAMERA", "Buffer full -> frame %ld dropped (QoS Drop)", f_id);
            continue;
        }
        pthread_mutex_lock(&g_bb.lock);
        g_bb.buf[g_bb.tail] = f_id; g_bb.tail = (g_bb.tail + 1) % BUF_CAP;
        pthread_mutex_unlock(&g_bb.lock);
        sem_post(&g_bb.full);
        write(g_pipe_w, &f_id, sizeof f_id);
    }
    long end = -1;
    write(g_pipe_w, &end, sizeof end);
    return NULL;
}

/* Consumer: DB thread reads detections & formats SQL matching schema */
static void *db_thread(void *arg) {
    (void)arg;
    FILE *out = fopen("db_inserts.sql", "w");
    if (out) fprintf(out, "USE smart_road_assistance;\n\n");

    for (;;) {
        Detection d; unsigned pr;
        if (mq_receive(g_mq, (char *)&d, sizeof d, &pr) < 0 || d.id < 0) break;
        LOG("DB", "Frame %ld: %s pothole (conf %.2f) at %.5f, %.5f -> SQL logged", d.id, d.sev, d.conf, d.lat, d.lon);
        if (out) {
            fprintf(out, "INSERT INTO POTHOLES (LATITUDE, LONGITUDE, SEVERITY, STATUS) VALUES (%.7f, %.7f, '%s', 'Reported');\n", d.lat, d.lon, d.sev);
            fprintf(out, "INSERT INTO DETECTION (POTHOLE_ID, CONFIDENCE, LATITUDE, LONGITUDE) VALUES (LAST_INSERT_ID(), %.4f, %.7f, %.7f);\n\n", d.conf, d.lat, d.lon);
            fflush(out);
        }
        push_alert(d.sev[0] == 'H' ? PRI_SAFETY : PRI_POTHOLE, "%s severity pothole detected at %.5f, %.5f", d.sev, d.lat, d.lon);
    }
    if (out) fclose(out);
    return NULL;
}

/* GPS thread: updates vehicle coordinates in Shared Memory */
static void *gps_thread(void *arg) {
    (void)arg; int step = 0;
    while (!atomic_load(&g_stop)) {
        usleep(500000);
        /* HOOK (Anushka): Replace with real NMEA GPS stream */
        pthread_mutex_lock(&g_gps->lock);
        g_gps->lat = 30.3255000 + 0.0002 * step;
        g_gps->lon = 78.0413000 + 0.0003 * step;
        pthread_mutex_unlock(&g_gps->lock);
        if (step == 2) push_alert(PRI_DIVERSION, "Road repair diversion on Subhash Road");
        if (step == 4) push_alert(PRI_SERVICE, "Facility nearby: Fuel Station (Petrol Pump)");
        step++;
    }
    return NULL;
}

/* ---------- Main & Ordered Shutdown ---------- */
int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);

    /* 1. IPC Shared Memory with Cross-Process Mutex */
    shm_unlink(SHM_NAME);
    int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0600);
    if (fd < 0 || ftruncate(fd, sizeof(GpsShm)) < 0) { perror("shm"); return 1; }
    g_gps = mmap(NULL, sizeof(GpsShm), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    pthread_mutexattr_t ma; pthread_mutexattr_init(&ma);
    pthread_mutexattr_setpshared(&ma, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&g_gps->lock, &ma);
    pthread_mutexattr_destroy(&ma);
    g_gps->lat = 30.3255000; g_gps->lon = 78.0413000;

    /* 2. IPC Message Queue, Pipe & Semaphores */
    struct mq_attr attr = { .mq_maxmsg = 10, .mq_msgsize = sizeof(Detection) };
    mq_unlink(MQ_NAME);
    g_mq = mq_open(MQ_NAME, O_CREAT | O_RDWR, 0600, &attr);
    int pfd[2]; pipe(pfd);
    sem_init(&g_bb.empty, 0, BUF_CAP); sem_init(&g_bb.full, 0, 0);
    pthread_mutex_init(&g_bb.lock, NULL);

    printf("=== Smart Road OS + DBMS Pipeline Started (PID %d) ===\n", getpid());

    /* 3. Fork Detector Child */
    pid_t pid = fork();
    if (pid == 0) { close(pfd[1]); detector_process(pfd[0]); }
    close(pfd[0]); g_pipe_w = pfd[1];

    /* 4. Launch 4 Threads */
    pthread_t t_alert, t_db, t_cam, t_gps;
    pthread_create(&t_alert, NULL, alert_thread,  NULL);
    pthread_create(&t_db,    NULL, db_thread,     NULL);
    pthread_create(&t_cam,   NULL, camera_thread, NULL);
    pthread_create(&t_gps,   NULL, gps_thread,    NULL);

    sleep(5);
    atomic_store(&g_stop, 1);

    /* 5. Clean Ordered Shutdown */
    pthread_join(t_gps, NULL);
    pthread_join(t_cam, NULL);
    close(g_pipe_w);
    waitpid(pid, NULL, 0);
    pthread_join(t_db, NULL);

    pthread_mutex_lock(&g_aq_lock); g_aq_closed = 1;
    pthread_cond_broadcast(&g_aq_cv); pthread_mutex_unlock(&g_aq_lock);
    pthread_join(t_alert, NULL);

    /* 6. Release Resources */
    mq_close(g_mq); mq_unlink(MQ_NAME);
    munmap(g_gps, sizeof(GpsShm)); shm_unlink(SHM_NAME);
    sem_destroy(&g_bb.empty); sem_destroy(&g_bb.full);
    printf("=== Done. IPC resources unlinked. SQL saved in db_inserts.sql ===\n");
    return 0;
}