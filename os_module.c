/*
 * Smart Road and Route Assistance System -- OS MODULE
 *
 * Implements (All Phase-II Requirements):
 *   1. Process Management : fork() detector child & waitpid() reaping (prevents zombies)
 *   2. Thread Management  : 5 POSIX threads (camera, forwarder, db, gps, alert)
 *   3. Producer-Consumer  : Bounded buffer with counting semaphores & mutex
 *   4. Priority Scheduling: Strict priority with AGING (prevents starvation)
 *   5. Inter-Process Comm : Pipe, POSIX Shared Memory, POSIX Message Queue
 *   6. Crash & Hang Guard : Broken-pipe ignore, buffer drain on child crash,
 *                           mq_timedreceive/mq_timedsend timeouts, WNOHANG liveness checks
 *   7. GPS Integration    : Reads live location.txt from server.py (with smooth fallback)
 *   8. DBMS Integration   : SQL generation matching updated POTHOLES, DETECTION, and GPS_LOCATIONS
 *
 * Build:  gcc -O2 -Wall -Wextra -pthread os_module.c -o os_module -lrt
 * Run:    ./os_module
 * Load:   mysql -u root -p smart_road_assistance < db_inserts.sql
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>
#include <mqueue.h>
#include <stdatomic.h>
#include <sys/mman.h>
#include <sys/wait.h>

#define BUF_CAP      4
#define ALERT_CAP    16
#define MQ_NAME      "/smart_road_det"
#define SHM_NAME     "/smart_road_gps"
#define AGING_DIV    4.0    /* Every 4 ms of wait improves effective priority by 1 ms */

typedef enum { PRI_SAFETY = 0, PRI_DIVERSION, PRI_POTHOLE, PRI_SERVICE, PRI_COUNT } AlertPriority;
static const char *PRI_NAME[PRI_COUNT] = { "SAFETY", "DIVERSION", "POTHOLE", "SERVICE" };

typedef struct { double lat, lon; } Location;
typedef struct { long id; int pid; double conf, lat, lon; char sev[16]; char road[56]; } Detection;

/* Matches Jasleen's seed records in database.sql / tables_schema.sql */
typedef struct { int pothole_id; const char *road; double conf; } SampleDet;
static const SampleDet SAMPLE[] = {
    {1, "Subhash Road", 0.95},
    {2, "MKP Chowk", 0.87},
    {3, "Approach Road to Government Doon Medical College", 0.92},
    {4, "Canal Road", 0.89},
    {5, "Dharampur", 0.96},
};
#define N_SAMPLE ((int)(sizeof SAMPLE / sizeof SAMPLE[0]))

/* Matches FACILITIES and ROAD_DIVERSIONS tables */
static const char *FAC_MSG[] = {
    "Facility nearby: Fuel Station (Petrol Pump)",
    "Facility nearby: City Hospital (Hospital)"
};
static const char *DIV_MSG[] = {
    "Road repair diversion on Subhash Road",
    "Pothole repair diversion on Canal Road"
};

typedef struct {
    int pri;
    long created_ms;
    char msg[128];
} Alert;

typedef struct {
    pthread_mutex_t lock;
    double lat, lon;
} GpsShm;

/* Global state & atomic counters */
static atomic_int   g_stop;
static atomic_int   g_child_dead;
static pid_t        g_child_pid = -1;
static atomic_long  s_captured, s_dropped, s_forwarded, s_stored;

static GpsShm      *g_gps;
static mqd_t        g_mq;
static int          g_pipe_w = -1;

/* Bounded Buffer for Producer-Consumer */
static struct {
    long buf[BUF_CAP];
    int head, tail;
    sem_t empty, full;
    pthread_mutex_t lock;
} g_bb;

/* Priority Alert Queue */
static Alert           g_aq[ALERT_CAP];
static int             g_an = 0, g_aq_closed = 0;
static pthread_mutex_t g_aq_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  g_aq_cv   = PTHREAD_COND_INITIALIZER;

/* Monotonic time helper */
static long mono_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

#define LOG(tag, ...) do { printf("[%6ld ms] [%-9s] ", mono_ms() % 1000000L, tag); printf(__VA_ARGS__); printf("\n"); } while (0)

/* ---------------- 1. PRIORITY ALERT SCHEDULER WITH AGING ---------------- */
static void push_alert(int pri, const char *fmt, ...) {
    Alert a;
    a.pri = pri;
    a.created_ms = mono_ms();

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(a.msg, sizeof a.msg, fmt, ap);
    va_end(ap);

    pthread_mutex_lock(&g_aq_lock);
    if (g_an < ALERT_CAP) {
        g_aq[g_an++] = a;
        pthread_cond_signal(&g_aq_cv);
    }
    pthread_mutex_unlock(&g_aq_lock);
}

/* Pick alert with lowest effective priority: aging prevents starvation of low-priority alerts */
static void *alert_thread(void *arg) {
    (void)arg;
    for (;;) {
        pthread_mutex_lock(&g_aq_lock);
        while (g_an == 0 && !g_aq_closed) {
            pthread_cond_wait(&g_aq_cv, &g_aq_lock);
        }
        if (g_an == 0 && g_aq_closed) {
            pthread_mutex_unlock(&g_aq_lock);
            break;
        }

        /* AGING: Calculate effective priority. Lower score is dispatched first */
        long now = mono_ms();
        int best = 0;
        double min_eff = 1e18;
        for (int i = 0; i < g_an; i++) {
            long waited = now - g_aq[i].created_ms;
            double eff = g_aq[i].pri * 1000.0 - (double)waited / AGING_DIV;
            if (eff < min_eff) {
                min_eff = eff;
                best = i;
            }
        }

        Alert a = g_aq[best];
        g_aq[best] = g_aq[--g_an];
        pthread_mutex_unlock(&g_aq_lock);

        usleep(100000); /* 100ms display delay */
        LOG("ALERT", "P%d (%-9s): %s", a.pri, PRI_NAME[a.pri], a.msg);
    }
    return NULL;
}

/* ---------------- 2. CHILD PROCESS (DETECTOR - AI SUBPROCESS) ---------------- */
static void detector_process(int rfd) {
    LOG("DETECTOR", "Child process started (PID %d), reading from pipe", getpid());
    static const char *SEV[] = { "Low", "Medium", "High" };
    long frame_id;
    int n = 0;

    while (read(rfd, &frame_id, sizeof frame_id) == (ssize_t)sizeof frame_id && frame_id >= 0) {
        pthread_mutex_lock(&g_gps->lock);
        Location cur = { g_gps->lat, g_gps->lon };
        pthread_mutex_unlock(&g_gps->lock);

        /*
         * HOOK (Shivika): YOLO model integration with ai/models/pothole_final.pt
         * When real YOLO runs, populate Detection from model output.
         * Default fallback: replays structured sample detections matching tables_schema.sql
         */
        if (frame_id % 3 == 0 && n < N_SAMPLE) {
            const SampleDet *sd = &SAMPLE[n];
            Detection d = {
                .id   = frame_id,
                .pid  = sd->pothole_id,
                .conf = sd->conf,
                .lat  = cur.lat,
                .lon  = cur.lon
            };
            snprintf(d.sev, sizeof d.sev, "%s", SEV[n % 3]);
            snprintf(d.road, sizeof d.road, "%s", sd->road);
            n++;

            /* SAFEGUARD: mq_timedsend with 500ms timeout prevents stall if queue is full */
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_nsec += 500000000L;
            if (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }
            mq_timedsend(g_mq, (const char *)&d, sizeof d, (d.sev[0] == 'H') ? 30 : 10, &ts);
        }
    }

    /* Send end-of-stream sentinel packet */
    Detection end = { .id = -1, .pid = 0, .conf = 0, .lat = 0, .lon = 0, .sev = "", .road = "" };
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += 1;
    mq_timedsend(g_mq, (const char *)&end, sizeof end, 0, &ts);
    LOG("DETECTOR", "Child process exiting cleanly");
    _exit(0);
}

/* ---------------- 3. PARENT THREADS ---------------- */

/* Producer: Camera captures frames into bounded buffer */
static void put_frame(long f_id) {
    sem_wait(&g_bb.empty);
    pthread_mutex_lock(&g_bb.lock);
    g_bb.buf[g_bb.tail] = f_id;
    g_bb.tail = (g_bb.tail + 1) % BUF_CAP;
    pthread_mutex_unlock(&g_bb.lock);
    sem_post(&g_bb.full);
}

static void *camera_thread(void *arg) {
    (void)arg;
    long id = 0;

    while (!atomic_load(&g_stop)) {
        usleep(200000); /* 5 FPS */
        long f_id = ++id;
        atomic_fetch_add(&s_captured, 1);

        /* SAFEGUARD: Non-blocking put drops frames when detector lags */
        if (sem_trywait(&g_bb.empty) == -1) {
            atomic_fetch_add(&s_dropped, 1);
            LOG("CAMERA", "Buffer full -> frame %ld dropped (QoS Drop)", f_id);
            continue;
        }
        sem_post(&g_bb.empty);
        put_frame(f_id);
    }

    put_frame(-1); /* End marker */
    return NULL;
}

/* Consumer: Forwards frames from bounded buffer to pipe */
static void *forwarder_thread(void *arg) {
    (void)arg;
    for (;;) {
        sem_wait(&g_bb.full);
        pthread_mutex_lock(&g_bb.lock);
        long f_id = g_bb.buf[g_bb.head];
        g_bb.head = (g_bb.head + 1) % BUF_CAP;
        pthread_mutex_unlock(&g_bb.lock);
        sem_post(&g_bb.empty);

        if (f_id < 0) {
            /* Forward shutdown marker if pipe is open, then break */
            if (g_pipe_w >= 0) {
                write(g_pipe_w, &f_id, sizeof f_id);
            }
            break;
        }

        /* SAFEGUARD: If pipe broken (detector died), drain buffer to unblock camera */
        if (g_pipe_w >= 0) {
            ssize_t w = write(g_pipe_w, &f_id, sizeof f_id);
            if (w < 0) {
                if (errno == EPIPE || errno == EBADF) {
                    LOG("FWD", "Pipe broken (detector closed). Draining buffer safely.");
                    close(g_pipe_w);
                    g_pipe_w = -1;
                    atomic_store(&g_child_dead, 1);
                }
            } else {
                atomic_fetch_add(&s_forwarded, 1);
            }
        }
    }
    return NULL;
}

/* DB Consumer: reads detections from MQ, writes SQL matching Jasleen's schema */
static void *db_thread(void *arg) {
    (void)arg;
    FILE *out = fopen("db_inserts.sql", "w");
    if (out) fprintf(out, "USE smart_road_assistance;\n\n");

    for (;;) {
        Detection d;
        unsigned pr;
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 1; /* 1-second timeout safeguard */

        /* SAFEGUARD: mq_timedreceive prevents permanent hang if detector dies mid-run */
        ssize_t n = mq_timedreceive(g_mq, (char *)&d, sizeof d, &pr, &ts);
        if (n < 0) {
            if (errno == ETIMEDOUT || errno == EINTR) {
                /* Check if child died or stop was signaled */
                int status;
                pid_t wp = waitpid(g_child_pid, &status, WNOHANG);
                if (wp != 0 || atomic_load(&g_stop) || atomic_load(&g_child_dead)) {
                    LOG("DB", "Queue empty & detector finished. DB thread exiting cleanly.");
                    break;
                }
                continue;
            }
            break;
        }

        if (d.id < 0) break; /* End of stream marker */

        LOG("DB", "Frame %ld: %s pothole on %s (conf %.2f) at %.5f, %.5f -> POTHOLE_ID %d logged",
            d.id, d.sev, d.road, d.conf, d.lat, d.lon, d.pid);
        atomic_fetch_add(&s_stored, 1);

        if (out) {
            /* 1. Updates POTHOLES row (sets coordinates, severity, status, and last_detected timestamp) */
            fprintf(out,
                "UPDATE POTHOLES SET LATITUDE=%.7f, LONGITUDE=%.7f, SEVERITY='%s', LAST_DETECTED=NOW(), STATUS='Reported' WHERE POTHOLE_ID=%d;\n",
                d.lat, d.lon, d.sev, d.pid);

            /* 2. Inserts into DETECTION history table including LATITUDE & LONGITUDE (per updated schema) */
            fprintf(out,
                "INSERT INTO DETECTION (POTHOLE_ID, CONFIDENCE, DETECTED_AT, LATITUDE, LONGITUDE) VALUES (%d, %.4f, NOW(), %.7f, %.7f);\n\n",
                d.pid, d.conf, d.lat, d.lon);
            fflush(out);
        }

        push_alert(d.sev[0] == 'H' ? PRI_SAFETY : PRI_POTHOLE,
                   "%s severity pothole on %s (conf %.2f)", d.sev, d.road, d.conf);
    }

    if (out) fclose(out);
    return NULL;
}

/* GPS thread: updates vehicle coordinates in Shared Memory */
static void *gps_thread(void *arg) {
    (void)arg;
    int step = 0;
    FILE *out = fopen("db_inserts.sql", "a");

    while (!atomic_load(&g_stop)) {
        usleep(500000);

        /*
         * GPS INTEGRATION: Check for live location.txt from server.py (Anushka)
         * If server.py / gps.html is running, read coordinates from location.txt.
         * Otherwise, fall back to smooth simulated progression.
         */
        double live_lat = 0.0, live_lon = 0.0;
        int have_live = 0;
        FILE *lf = fopen("location.txt", "r");
        if (lf) {
            if (fscanf(lf, "%lf %lf", &live_lat, &live_lon) == 2 &&
                live_lat >= -90.0 && live_lat <= 90.0 &&
                live_lon >= -180.0 && live_lon <= 180.0) {
                have_live = 1;
            }
            fclose(lf);
        }

        pthread_mutex_lock(&g_gps->lock);
        if (have_live) {
            g_gps->lat = live_lat;
            g_gps->lon = live_lon;
        } else {
            g_gps->lat = 30.3255000 + 0.0002 * step;
            g_gps->lon = 78.0413000 + 0.0003 * step;
        }
        double cur_lat = g_gps->lat;
        double cur_lon = g_gps->lon;
        pthread_mutex_unlock(&g_gps->lock);

        /* Record breadcrumb into newly created GPS_LOCATIONS table (per updated schema) */
        if (out && (step % 2 == 0)) {
            fprintf(out, "INSERT INTO GPS_LOCATIONS (LATITUDE, LONGITUDE, RECORDED_AT) VALUES (%.7f, %.7f, NOW());\n",
                    cur_lat, cur_lon);
            fflush(out);
        }

        if (step == 2) {
            /* Burst: low-priority services arrive first, high-priority diversions after */
            push_alert(PRI_SERVICE,   "%s", FAC_MSG[0]);
            push_alert(PRI_SERVICE,   "%s", FAC_MSG[1]);
            push_alert(PRI_DIVERSION, "%s", DIV_MSG[0]);
            push_alert(PRI_DIVERSION, "%s", DIV_MSG[1]);
        }
        step++;
    }

    if (out) fclose(out);
    return NULL;
}

/* ---------------- 4. MAIN ORCHESTRATION & SHUTDOWN ---------------- */
int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);

    /* SAFEGUARD: Ignore broken pipe signals so child crash doesn't kill parent */
    signal(SIGPIPE, SIG_IGN);

    /* 1. IPC Shared Memory with Cross-Process Mutex */
    shm_unlink(SHM_NAME);
    int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0600);
    if (fd < 0 || ftruncate(fd, sizeof(GpsShm)) < 0) {
        perror("shm");
        return 1;
    }
    g_gps = mmap(NULL, sizeof(GpsShm), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);

    pthread_mutexattr_t ma;
    pthread_mutexattr_init(&ma);
    pthread_mutexattr_setpshared(&ma, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&g_gps->lock, &ma);
    pthread_mutexattr_destroy(&ma);
    g_gps->lat = 30.3255000;
    g_gps->lon = 78.0413000;

    /* 2. IPC Message Queue, Pipe & Semaphores */
    struct mq_attr attr = { .mq_maxmsg = 10, .mq_msgsize = sizeof(Detection) };
    mq_unlink(MQ_NAME);
    g_mq = mq_open(MQ_NAME, O_CREAT | O_RDWR, 0600, &attr);
    if (g_mq == (mqd_t)-1) {
        perror("mq_open");
        return 1;
    }

    int pfd[2];
    if (pipe(pfd) < 0) {
        perror("pipe");
        return 1;
    }
    sem_init(&g_bb.empty, 0, BUF_CAP);
    sem_init(&g_bb.full, 0, 0);
    pthread_mutex_init(&g_bb.lock, NULL);

    printf("=== Smart Road OS + DBMS Pipeline Started (PID %d) ===\n", getpid());

    /* 3. Fork Detector Child */
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }
    if (pid == 0) {
        close(pfd[1]);
        detector_process(pfd[0]);
    }
    close(pfd[0]);
    g_pipe_w = pfd[1];
    g_child_pid = pid;

    /* 4. Launch 5 Threads */
    pthread_t t_alert, t_db, t_cam, t_gps, t_fwd;
    pthread_create(&t_alert, NULL, alert_thread,     NULL);
    pthread_create(&t_db,    NULL, db_thread,        NULL);
    pthread_create(&t_fwd,   NULL, forwarder_thread, NULL);
    pthread_create(&t_cam,   NULL, camera_thread,    NULL);
    pthread_create(&t_gps,   NULL, gps_thread,       NULL);

    sleep(5);
    atomic_store(&g_stop, 1);

    /* 5. Clean Ordered Shutdown (Safe against child crashes) */
    pthread_join(t_gps, NULL);
    pthread_join(t_cam, NULL);
    pthread_join(t_fwd, NULL);

    if (g_pipe_w >= 0) {
        close(g_pipe_w);
        g_pipe_w = -1;
    }

    int status;
    waitpid(g_child_pid, &status, 0); /* Reap child process (prevents zombies) */
    pthread_join(t_db, NULL);

    pthread_mutex_lock(&g_aq_lock);
    g_aq_closed = 1;
    pthread_cond_broadcast(&g_aq_cv);
    pthread_mutex_unlock(&g_aq_lock);
    pthread_join(t_alert, NULL);

    /* 6. Release Resources */
    mq_close(g_mq);
    mq_unlink(MQ_NAME);
    munmap(g_gps, sizeof(GpsShm));
    shm_unlink(SHM_NAME);
    sem_destroy(&g_bb.empty);
    sem_destroy(&g_bb.full);

    printf("--- Summary: frames captured=%ld forwarded=%ld dropped=%ld | detections stored in DB=%ld ---\n",
           (long)s_captured, (long)s_forwarded, (long)s_dropped, (long)s_stored);
    printf("=== Done. IPC resources unlinked. SQL saved in db_inserts.sql ===\n");
    return 0;
}
