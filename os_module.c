/*
 * Smart Road and Route Assistance System -- OS 
 *
 * Implements: Process Management , 5 Threads, Producer-Consumer,
 * Priority Scheduling, 3 IPCs (Pipe, POSIX SHM, POSIX MQ), and DBMS Integration.
 *
 * gcc -O2 -Wall -pthread os_module.c -o os_module -lrt
 * ./os_module
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
typedef struct { long id; int pid; double conf, lat, lon; char sev[16]; char road[56]; } Detection;

/* ---- SAMPLE DATA copied from the DBMS (tables_schema.sql: DETECTION / POTHOLES / FACILITIES / ROAD_DIVERSIONS) ---- */
typedef struct { int pothole_id; const char *road; double conf; } SampleDet;
static const SampleDet SAMPLE[] = {            /* POTHOLES.road_name + DETECTION.CONFIDENCE */
    {1, "Subhash Road", 0.95}, {2, "MKP Chowk", 0.87},
    {3, "Approach Road to Government Doon Medical College", 0.92},
    {4, "Canal Road", 0.89}, {5, "Dharampur", 0.96},
};
#define N_SAMPLE ((int)(sizeof SAMPLE / sizeof SAMPLE[0]))
/* FACILITIES rows 1 and 2, ROAD_DIVERSIONS rows 1 and 2 */
static const char *FAC_MSG[] = { "Facility nearby: Fuel Station (Petrol Pump)", "Facility nearby: City Hospital (Hospital)" };
static const char *DIV_MSG[] = { "Road repair diversion on Subhash Road", "Pothole repair diversion on Canal Road" };
typedef struct { int pri; char msg[128]; } Alert;
typedef struct { pthread_mutex_t lock; double lat, lon; } GpsShm;

static atomic_int g_stop;
static atomic_long s_captured, s_dropped, s_forwarded, s_stored;
static GpsShm    *g_gps;
static mqd_t      g_mq;
static int        g_pipe_w = -1;

/* Bounded Buffer (Producer-Consumer) */
static struct {
    long buf[BUF_CAP];
    int head, tail;
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

        /* HOOK (Shivika): YOLO model runs here. Simulation: hit every 3rd frame,
         * replaying the sample detections stored in the DBMS */
        if (frame_id % 3 == 0 && n < N_SAMPLE) {
            const SampleDet *sd = &SAMPLE[n];
            Detection d = { .id = frame_id, .pid = sd->pothole_id, .conf = sd->conf, .lat = cur.lat, .lon = cur.lon };
            snprintf(d.sev, sizeof d.sev, "%s", SEV[n % 3]);
            snprintf(d.road, sizeof d.road, "%s", sd->road);
            n++;
            mq_send(g_mq, (const char *)&d, sizeof d, (d.sev[0] == 'H') ? 30 : 10);
        }
    }
    Detection end = { .id = -1 };
    mq_send(g_mq, (const char *)&end, sizeof end, 0);
    LOG("DETECTOR", "Child process exiting cleanly");
    _exit(0);
}

/* ---------- Parent Threads ---------- */
/* Producer: Camera captures frames into the bounded buffer */
static void put_frame(long f_id) {
    sem_wait(&g_bb.empty);
    pthread_mutex_lock(&g_bb.lock);
    g_bb.buf[g_bb.tail] = f_id; g_bb.tail = (g_bb.tail + 1) % BUF_CAP;
    pthread_mutex_unlock(&g_bb.lock);
    sem_post(&g_bb.full);
}
static void *camera_thread(void *arg) {
    (void)arg; long id = 0;
    while (!atomic_load(&g_stop)) {
        usleep(200000);
        long f_id = ++id;
        atomic_fetch_add(&s_captured, 1);
        if (sem_trywait(&g_bb.empty) == -1) {
            atomic_fetch_add(&s_dropped, 1);
            LOG("CAMERA", "Buffer full -> frame %ld dropped (QoS Drop)", f_id);
            continue;
        }
        sem_post(&g_bb.empty);          /* slot was only probed; put_frame takes it */
        put_frame(f_id);
    }
    put_frame(-1);                      /* end-of-stream marker */
    return NULL;
}

/* Consumer: takes frames out of the bounded buffer and sends them to the detector over the pipe
 * (FIX: the original code had no consumer, so the buffer filled after 4 frames and every later
 *  frame was dropped) */
static void *forwarder_thread(void *arg) {
    (void)arg;
    for (;;) {
        sem_wait(&g_bb.full);
        pthread_mutex_lock(&g_bb.lock);
        long f_id = g_bb.buf[g_bb.head]; g_bb.head = (g_bb.head + 1) % BUF_CAP;
        pthread_mutex_unlock(&g_bb.lock);
        sem_post(&g_bb.empty);
        if (write(g_pipe_w, &f_id, sizeof f_id) != (ssize_t)sizeof f_id) break;
        if (f_id < 0) break;
        atomic_fetch_add(&s_forwarded, 1);
    }
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
        LOG("DB", "Frame %ld: %s pothole on %s (conf %.2f) at %.5f, %.5f -> POTHOLE_ID %d updated", d.id, d.sev, d.road, d.conf, d.lat, d.lon, d.pid);
        atomic_fetch_add(&s_stored, 1);
        if (out) {
            /* pothole already exists in POTHOLES (duplicate check) -> only update the old record */
            fprintf(out, "UPDATE POTHOLES SET LATITUDE=%.7f, LONGITUDE=%.7f, SEVERITY='%s', LAST_DETECTED=NOW(), STATUS='Reported' WHERE POTHOLE_ID=%d;\n", d.lat, d.lon, d.sev, d.pid);
            fprintf(out, "INSERT INTO DETECTION (POTHOLE_ID, CONFIDENCE, LATITUDE, LONGITUDE) VALUES (%d, %.4f, %.7f, %.7f);\n\n", d.pid, d.conf, d.lat, d.lon);
            fflush(out);
        }
        push_alert(d.sev[0] == 'H' ? PRI_SAFETY : PRI_POTHOLE, "%s severity pothole on %s (conf %.2f)", d.sev, d.road, d.conf);
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
        if (step == 2) {                       /* burst: services arrive first, diversions after */
            push_alert(PRI_SERVICE,   "%s", FAC_MSG[0]);
            push_alert(PRI_SERVICE,   "%s", FAC_MSG[1]);
            push_alert(PRI_DIVERSION, "%s", DIV_MSG[0]);
            push_alert(PRI_DIVERSION, "%s", DIV_MSG[1]);
        }
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
    int pfd[2]; if (pipe(pfd) < 0) { perror("pipe"); return 1; }
    sem_init(&g_bb.empty, 0, BUF_CAP); sem_init(&g_bb.full, 0, 0);
    pthread_mutex_init(&g_bb.lock, NULL);

    printf("=== Smart Road OS + DBMS Pipeline Started (PID %d) ===\n", getpid());

    /* 3. Fork Detector Child */
    pid_t pid = fork();
    if (pid == 0) { close(pfd[1]); detector_process(pfd[0]); }
    close(pfd[0]); g_pipe_w = pfd[1];

    /* 4. Launch 5 Threads */
    pthread_t t_alert, t_db, t_cam, t_gps, t_fwd;
    pthread_create(&t_alert, NULL, alert_thread,  NULL);
    pthread_create(&t_db,    NULL, db_thread,     NULL);
    pthread_create(&t_fwd,   NULL, forwarder_thread, NULL);
    pthread_create(&t_cam,   NULL, camera_thread, NULL);
    pthread_create(&t_gps,   NULL, gps_thread,    NULL);

    sleep(5);
    atomic_store(&g_stop, 1);

    /* 5. Clean Ordered Shutdown */
    pthread_join(t_gps, NULL);
    pthread_join(t_cam, NULL);
    pthread_join(t_fwd, NULL);
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
    printf("--- Summary: frames captured=%ld forwarded=%ld dropped=%ld | detections stored in DB=%ld ---\n",
           (long)s_captured, (long)s_forwarded, (long)s_dropped, (long)s_stored);
    printf("=== Done. IPC resources unlinked. SQL saved in db_inserts.sql ===\n");
    return 0;
}
