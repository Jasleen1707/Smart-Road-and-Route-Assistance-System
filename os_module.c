#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

#define MAX_FRAMES      15
#define ALERT_BUF_SIZE  16

#define PRIO_SAFETY     3
#define PRIO_DIVERSION  2
#define PRIO_SERVICE    1

#define LOG(tag, fmt, ...) printf("[%s] " fmt "\n", tag, ##__VA_ARGS__)

/* -------------------------------------------------------------------------- */
/*  PACKET DEFINITIONS (Unified Inter-Process Telemetry)                      */
/* -------------------------------------------------------------------------- */

/* Sent Parent -> Child: Camera frame bundled with synchronized GPS */
typedef struct {
    int    frame_id;
    double lat;
    double lon;
} FramePacket;

/* Sent Child -> Parent: AI Detection results */
typedef struct {
    int    priority;
    char   label[64];
    float  confidence;
    double lat;
    double lon;
} AlertPacket;

/* -------------------------------------------------------------------------- */
/*  GLOBAL SYNCHRONIZATION STATE                                              */
/* -------------------------------------------------------------------------- */
static volatile int g_running = 1;
static int          g_pipe_to_child[2];   // Parent writes, Child reads
static int          g_pipe_to_parent[2];  // Child writes, Parent reads
static pid_t        g_child_pid = -1;

/* GPS: Single Mutex-Protected Register (Overwrites latest coordinates) */
static struct {
    double lat;
    double lon;
    pthread_mutex_t lock;
} g_gps = { 30.316494, 78.032191, PTHREAD_MUTEX_INITIALIZER };

/* Alert Queue: Producer-Consumer with Counting Semaphores + Mutex */
static struct {
    AlertPacket items[ALERT_BUF_SIZE];
    int count;
    pthread_mutex_t lock;
    sem_t sem_empty; // Counting semaphore: available slots
    sem_t sem_full;  // Counting semaphore: filled alerts
} g_alert_q;

/* -------------------------------------------------------------------------- */
/*  CHILD PROCESS: AI DETECTOR (Concurrently processes video & coordinates)   */
/* -------------------------------------------------------------------------- */
static void detector_process(int read_fd, int write_fd) {
    LOG("AI-DETECTOR", "Detector process running (PID: %d)", getpid());

    FramePacket pkt;
    while (read(read_fd, &pkt, sizeof(FramePacket)) > 0) {
        if (pkt.frame_id == -1) break; // End sentinel

        /* Procedural detection simulation (or invoke YOLO model here) */
        int type = pkt.frame_id % 3;
        AlertPacket alert;
        alert.priority   = (type == 0) ? PRIO_SAFETY : (type == 1) ? PRIO_DIVERSION : PRIO_SERVICE;
        alert.confidence = 0.82f + (pkt.frame_id % 15) * 0.01f;
        alert.lat        = pkt.lat;
        alert.lon        = pkt.lon;

        if (type == 0)      strncpy(alert.label, "Pothole detected", sizeof(alert.label));
        else if (type == 1) strncpy(alert.label, "Diversion ahead", sizeof(alert.label));
        else                strncpy(alert.label, "Facility: Fuel Station", sizeof(alert.label));

        if (write(write_fd, &alert, sizeof(AlertPacket)) < 0) {
            perror("[CHILD] write to parent pipe failed");
            break;
        }

        usleep(30000); // 30ms inference latency
    }

    close(read_fd);
    close(write_fd);
    LOG("AI-DETECTOR", "Inference engine stopped cleanly");
    exit(0);
}

/* -------------------------------------------------------------------------- */
/*  PARENT THREADS                                                            */
/* -------------------------------------------------------------------------- */

/* 1. Camera Thread: Bundles frames with live GPS and writes to Kernel Pipe */
static void *camera_thread(void *arg) {
    (void)arg;
    for (int f = 1; f <= MAX_FRAMES && g_running; f++) {
        usleep(25000); // 40 FPS simulated

        FramePacket pkt;
        pkt.frame_id = f;

        /* Sample current synchronized GPS */
        pthread_mutex_lock(&g_gps.lock);
        pkt.lat = g_gps.lat;
        pkt.lon = g_gps.lon;
        pthread_mutex_unlock(&g_gps.lock);

        /* Write directly to kernel pipe buffer */
        if (write(g_pipe_to_child[1], &pkt, sizeof(FramePacket)) < 0) {
            LOG("CAMERA", "Pipe write error; child may have exited");
            break;
        }
    }

    /* Send end sentinel */
    FramePacket end_pkt = { .frame_id = -1, .lat = 0.0, .lon = 0.0 };
    write(g_pipe_to_child[1], &end_pkt, sizeof(FramePacket));
    close(g_pipe_to_child[1]); // Signal EOF to child
    return NULL;
}

/* 2. GPS Thread: Real-time sensor polling (Overwrites latest coordinates) */
static void *gps_thread(void *arg) {
    (void)arg;
    double lat = 30.316494, lon = 78.032191; // Dehradun baseline

    while (g_running) {
        FILE *fp = fopen("location.txt", "r");
        if (fp) {
            fscanf(fp, "%lf,%lf", &lat, &lon);
            fclose(fp);
        } else {
            lat += 0.0001; // Step forward along route
            lon += 0.0001;
        }

        pthread_mutex_lock(&g_gps.lock);
        g_gps.lat = lat;
        g_gps.lon = lon;
        pthread_mutex_unlock(&g_gps.lock);

        usleep(100000); // 10 Hz polling
    }
    return NULL;
}

/* 3. Alert Thread: Priority Scheduling & Database Logging (Consumer) */
static void *alert_thread(void *arg) {
    (void)arg;
    FILE *sql_file = fopen("db_inserts.sql", "w");
    if (!sql_file) {
        perror("fopen db_inserts.sql failed");
        return NULL;
    }
    fprintf(sql_file, "-- Smart Road System DB Inserts (Phase-II Schema)\n");

    int pothole_seq = 1;

    while (g_running || g_alert_q.count > 0) {
        /* Wait on Counting Semaphore: blocks until an alert is queued */
        sem_wait(&g_alert_q.sem_full);
        if (!g_running && g_alert_q.count == 0) break;

        pthread_mutex_lock(&g_alert_q.lock);
        if (g_alert_q.count == 0) {
            pthread_mutex_unlock(&g_alert_q.lock);
            continue;
        }

        /* Priority Scheduling: Pick highest priority alert first */
        int best_idx = 0;
        int max_prio = -1;
        for (int i = 0; i < g_alert_q.count; i++) {
            if (g_alert_q.items[i].priority > max_prio) {
                max_prio = g_alert_q.items[i].priority;
                best_idx = i;
            }
        }

        AlertPacket chosen = g_alert_q.items[best_idx];

        /* Pop selected item from queue */
        for (int i = best_idx; i < g_alert_q.count - 1; i++) {
            g_alert_q.items[i] = g_alert_q.items[i + 1];
        }
        g_alert_q.count--;
        pthread_mutex_unlock(&g_alert_q.lock);

        /* Signal that an empty slot is now freed */
        sem_post(&g_alert_q.sem_empty);

        /* Dispatch visual alert */
        const char *prio_label = (chosen.priority == PRIO_SAFETY) ? "CRITICAL SAFETY" :
                                 (chosen.priority == PRIO_DIVERSION) ? "DIVERSION" : "SERVICE";
        LOG("ALERT-UI", "[%s] %s (Conf: %.2f) at (%.6f, %.6f)",
            prio_label, chosen.label, chosen.confidence, chosen.lat, chosen.lon);

        /* Log to SQL (Matches Jasleen's latest database tables) */
        if (chosen.priority == PRIO_SAFETY) {
            fprintf(sql_file,
                "INSERT INTO POTHOLES (LATITUDE, LONGITUDE, SEVERITY, STATUS) "
                "VALUES (%.6f, %.6f, 'HIGH', 'DETECTED');\n", chosen.lat, chosen.lon);
            fprintf(sql_file,
                "INSERT INTO DETECTION (POTHOLE_ID, CONFIDENCE, LATITUDE, LONGITUDE, DETECTED_AT) "
                "VALUES (%d, %.2f, %.6f, %.6f, NOW());\n", pothole_seq++, chosen.confidence, chosen.lat, chosen.lon);
            fflush(sql_file);
        }
    }

    fclose(sql_file);
    return NULL;
}

/* -------------------------------------------------------------------------- */
/*  MAIN ENTRY POINT                                                          */
/* -------------------------------------------------------------------------- */
int main(void) {
    signal(SIGPIPE, SIG_IGN); // Prevent crash on closed pipe

    /* Initialize Counting Semaphores and Mutexes */
    if (sem_init(&g_alert_q.sem_empty, 0, ALERT_BUF_SIZE) != 0 ||
        sem_init(&g_alert_q.sem_full,  0, 0) != 0 ||
        pthread_mutex_init(&g_alert_q.lock, NULL) != 0) {
        perror("semaphore/mutex init failed");
        exit(1);
    }
    g_alert_q.count = 0;

    /* Create bidirectional pipes */
    if (pipe(g_pipe_to_child) < 0 || pipe(g_pipe_to_parent) < 0) {
        perror("pipe creation failed");
        exit(1);
    }

    /* Process Management: fork() child for AI inference */
    g_child_pid = fork();
    if (g_child_pid < 0) {
        perror("fork failed");
        exit(1);
    }

    if (g_child_pid == 0) {
        close(g_pipe_to_child[1]);  // Close unused write end
        close(g_pipe_to_parent[0]); // Close unused read end
        detector_process(g_pipe_to_child[0], g_pipe_to_parent[1]);
    }

    /* Parent Process Execution */
    close(g_pipe_to_child[0]);  // Close unused read end
    close(g_pipe_to_parent[1]); // Close unused write end

    pthread_t t_cam, t_gps, t_alert;
    if (pthread_create(&t_cam, NULL, camera_thread, NULL) != 0 ||
        pthread_create(&t_gps, NULL, gps_thread, NULL) != 0 ||
        pthread_create(&t_alert, NULL, alert_thread, NULL) != 0) {
        perror("pthread_create failed");
        exit(1);
    }

    /* Producer Loop: Receives detections from child and enqueues using semaphores */
    AlertPacket recv_alert;
    while (read(g_pipe_to_parent[0], &recv_alert, sizeof(AlertPacket)) > 0) {
        sem_wait(&g_alert_q.sem_empty); // Wait for an empty slot

        pthread_mutex_lock(&g_alert_q.lock);
        g_alert_q.items[g_alert_q.count++] = recv_alert;
        pthread_mutex_unlock(&g_alert_q.lock);

        sem_post(&g_alert_q.sem_full);  // Signal that an alert is available
    }
    close(g_pipe_to_parent[0]);

    /* Join camera thread and harvest child process */
    pthread_join(t_cam, NULL);
    waitpid(g_child_pid, NULL, 0);

    /* Signal alert thread to exit */
    g_running = 0;
    sem_post(&g_alert_q.sem_full); // Wake alert thread if blocked

    pthread_join(t_gps, NULL);
    pthread_join(t_alert, NULL);

    /* Cleanup */
    sem_destroy(&g_alert_q.sem_empty);
    sem_destroy(&g_alert_q.sem_full);
    pthread_mutex_destroy(&g_alert_q.lock);
    pthread_mutex_destroy(&g_gps.lock);

    LOG("SYSTEM", "Pipeline closed cleanly. All semaphores & mutexes destroyed.");
    return 0;
}
