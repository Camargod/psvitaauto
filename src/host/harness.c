#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "control.h"
#include "discover.h"
#include "mock_phone.h"
#include "transport.h"

typedef struct {
    FILE *out;
    int frames;
} dump_ctx;

static void video_cb(void *ctx, uint64_t ts, const uint8_t *h264, size_t len) {
    (void)ts;
    dump_ctx *d = ctx;
    if (d->out) {
        fwrite(h264, 1, len, d->out);
        fflush(d->out);
    }
    d->frames++;
}

static void usage(const char *prog) {
    printf("usage: %s [options]\n", prog);
    printf("  --host <ip>         phone head unit server host (default 127.0.0.1)\n");
    printf("  --port <n>          port (default 5277)\n");
    printf("  --output <file>     write H.264 frames to file\n");
    printf("  --mock              run against an in-process mock phone (no network)\n");
    printf("  --discover          find the phone on the LAN (probe 5277), then connect\n");
    printf("  --resolution <r>    480p | 720p | 1080p (default 720p)\n");
    printf("  --fps <f>           30 | 60 (default 30)\n");
    printf("  --help              show this help\n");
}

static dump_ctx open_output(const char *output) {
    dump_ctx d = {0};
    if (output) {
        d.out = fopen(output, "wb");
        if (!d.out) {
            fprintf(stderr, "cannot open %s\n", output);
            exit(1);
        }
    }
    return d;
}

static int run_mock(aa_video_resolution res, aa_video_fps fps, const char *output) {
    dump_ctx d = open_output(output);

    int fds[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
    aa_transport hu_t;
    hu_t.fd = fds[0];

    mock_phone *ph = mock_phone_create(fds[1]);
    if (!ph) {
        return 1;
    }
    aa_control *hu = aa_control_create(&hu_t);
    if (!hu) {
        mock_phone_destroy(ph);
        return 1;
    }
    aa_control_set_video_callback(hu, video_cb, &d);
    aa_control_set_video_config(hu, res, fps);

    int iter = 0;
    while (iter++ < 2000) {
        mock_phone_step(ph);
        aa_control_state s = aa_control_poll(hu);
        if (mock_phone_done(ph)) {
            break;
        }
        if (s == AA_CONTROL_FAILED) {
            break;
        }
    }

    mock_phone_result r;
    mock_phone_result_get(ph, &r);

    if (d.out) {
        fclose(d.out);
    }

    int ok = (aa_control_get_state(hu) == AA_CONTROL_ACTIVE) && r.video_open_ok && d.frames > 0;
    printf("mock session: %s, video frames: %d, advertised resolution: %d\n",
           ok ? "OK" : "FAILED", d.frames, r.video_resolution);

    aa_control_destroy(hu);
    mock_phone_destroy(ph);
    close(fds[0]);
    close(fds[1]);
    return ok ? 0 : 1;
}

static int run_phone(const char *host, int port, aa_video_resolution res,
                     aa_video_fps fps, const char *output) {
    dump_ctx d = open_output(output);

    aa_transport t;
    if (aa_transport_connect(&t, host, (uint16_t)port) != 0) {
        fprintf(stderr, "connect to %s:%d failed\n", host, port);
        return 1;
    }
    printf("connected to %s:%d\n", host, port);

    aa_control *hu = aa_control_create(&t);
    if (!hu) {
        aa_transport_close(&t);
        return 1;
    }
    aa_control_set_video_callback(hu, video_cb, &d);
    aa_control_set_video_config(hu, res, fps);
    aa_control_set_log(hu, 1);

    int iter = 0;
    int active = 0;
    while (iter++ < 120000) { /* ~60 s at 0.5 ms sleep */
        aa_control_state s = aa_control_poll(hu);
        if (s == AA_CONTROL_FAILED) {
            printf("session failed\n");
            break;
        }
        if (s == AA_CONTROL_ACTIVE && !active) {
            active = 1;
            printf("session ACTIVE\n");
        }
        if (s == AA_CONTROL_ACTIVE && (iter % 2000 == 0)) {
            printf("active, video frames: %d\n", d.frames);
        }
        usleep(500);
    }

    if (!active) {
        printf("timed out waiting for handshake\n");
    }

    aa_control_destroy(hu);
    aa_transport_close(&t);
    if (d.out) {
        fclose(d.out);
    }
    return active ? 0 : 1;
}

int main(int argc, char **argv) {
    const char *host = "127.0.0.1";
    int port = 5277;
    const char *output = NULL;
    int mock = 0;
    int discover = 0;
    aa_video_resolution res = AA_VIDEO_720P;
    aa_video_fps fps = AA_FPS_30;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--host") && i + 1 < argc) {
            host = argv[++i];
        } else if (!strcmp(argv[i], "--port") && i + 1 < argc) {
            port = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--output") && i + 1 < argc) {
            output = argv[++i];
        } else if (!strcmp(argv[i], "--mock")) {
            mock = 1;
        } else if (!strcmp(argv[i], "--discover")) {
            discover = 1;
        } else if (!strcmp(argv[i], "--resolution") && i + 1 < argc) {
            const char *r = argv[++i];
            if (!strcmp(r, "480p")) {
                res = AA_VIDEO_480P;
            } else if (!strcmp(r, "720p")) {
                res = AA_VIDEO_720P;
            } else if (!strcmp(r, "1080p")) {
                res = AA_VIDEO_1080P;
            }
        } else if (!strcmp(argv[i], "--fps") && i + 1 < argc) {
            const char *f = argv[++i];
            if (!strcmp(f, "30")) {
                fps = AA_FPS_30;
            } else if (!strcmp(f, "60")) {
                fps = AA_FPS_60;
            }
        } else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            usage(argv[0]);
            return 0;
        } else {
            usage(argv[0]);
            return 2;
        }
    }

    if (mock) {
        return run_mock(res, fps, output);
    }
    if (discover) {
        char ip[64];
        if (aa_discover_head_unit((uint16_t)port, ip, sizeof(ip)) == 0) {
            printf("discovered head unit at %s\n", ip);
            return run_phone(ip, port, res, fps, output);
        }
        printf("no head unit server found on the LAN\n");
        return 1;
    }
    return run_phone(host, port, res, fps, output);
}
