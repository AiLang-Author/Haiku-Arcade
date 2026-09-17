/* arcade_shell_haiku — native Haiku Interface Kit host for NOWAY HOME.
 *
 * Kernel (arcade_app.x) owns pixels. This process is Haiku-native (libbe):
 * BBitmap blit of frame.raw, keys/size/cmd over the CAD/Paint IPC files.
 * Audio goes through the system mixer (media_server / Game Kit), not a
 * hypervisor: BFileGameSound for SFX and BGM from sfx.bin.
 *
 *   g++ -O2 -o arcade_shell_haiku arcade_shell_haiku.cxx -lbe -lgame
 *   ARCADE_APP_STATE=/tmp/arcade_app ./arcade_shell_haiku
 *
 * Forked from cad_shell_haiku (blit + poll). CAD chrome stripped.
 *
 * Copyright © 2026 Sean Collins, 2 Paws Machine and Engineering. SCSL v1.0.
 */
#ifndef __HAIKU__
#error arcade_shell_haiku.cxx is the Haiku Interface Kit host. Build on Haiku: g++ -O2 -o arcade_shell_haiku arcade_shell_haiku.cxx -lbe -lgame
#endif

#include <Application.h>
#include <Bitmap.h>
#include <FileGameSound.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <MessageRunner.h>
#include <OS.h>
#include <Path.h>
#include <Screen.h>
#include <StringView.h>
#include <View.h>
#include <Window.h>

#include <dirent.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

enum { MSG_POLL = 'poll', MSG_CMD = 'cmd_' };

static char g_dir[512];
static char g_data[512];
static char path_meta[600], path_frame[600], path_gen[600];
static char path_cmd[600], path_keys[600], path_size[600];
static char path_sfx[600], path_vol[600];

static int k_l, k_r, k_u, k_d, k_f, k_s, k_p;

static void paths_init(const char *dir) {
    snprintf(g_dir, sizeof g_dir, "%s", dir);
    snprintf(path_meta, sizeof path_meta, "%s/meta.bin", dir);
    snprintf(path_frame, sizeof path_frame, "%s/frame.raw", dir);
    snprintf(path_gen, sizeof path_gen, "%s/gen.txt", dir);
    snprintf(path_cmd, sizeof path_cmd, "%s/cmd.txt", dir);
    snprintf(path_keys, sizeof path_keys, "%s/keys.txt", dir);
    snprintf(path_size, sizeof path_size, "%s/size.txt", dir);
    snprintf(path_sfx, sizeof path_sfx, "%s/sfx.bin", dir);
    snprintf(path_vol, sizeof path_vol, "%s/vol.txt", dir);
}

static void write_file(const char *path, const char *s) {
    char tmp[640];
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = fopen(tmp, "w");
    if (!f) return;
    fputs(s, f);
    if (s[0] && s[strlen(s) - 1] != '\n') fputc('\n', f);
    fclose(f);
    rename(tmp, path);
}

static void write_cmd(const char *s) {
    write_file(path_cmd, s);
}

static void write_keys(void) {
    char buf[64];
    snprintf(buf, sizeof buf, "%d %d %d %d %d %d %d\n",
             k_l, k_r, k_u, k_d, k_f, k_s, k_p);
    write_file(path_keys, buf);
}

static void write_size(int w, int h) {
    if (w < 1 || h < 1) return;
    char buf[64];
    snprintf(buf, sizeof buf, "%d %d\n", w, h);
    write_file(path_size, buf);
}

/* System mixer: media_server via Game Kit. Kernel only writes sfx.bin. */
#define MUSIC_CAP 16
#define SFX_VOICES 12

static const char *kSfxName[8] = {
    "", "shot.wav", "eshot.wav", "boom.wav",
    "hit.wav", "death.wav", "wave.wav", "start.wav"
};

static char clip_path[8][640];
static BFileGameSound *voice[SFX_VOICES];
static int voice_steal;
static int sfx_plays;
static BFileGameSound *bgm;
static int music_mode;
static int audio_ok;
static int primed;
static uint32_t seen;
static float vol_music = 0.35f;
static float vol_sfx = 0.80f;
static int last_mv = -1, last_sv = -1;

static char music_dir[512];
static char music_files[MUSIC_CAP][512];
static int music_n;
static char stage_files[MUSIC_CAP][512];
static int stage_n;
static char title_file[512];
static char queen_file[512];
static int music_cur = -1;

static int le32u(const uint8_t *p) {
    return (int)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

static int try_root(const char *root) {
    char probe[640];
    if (!root || !root[0]) return 0;
    snprintf(probe, sizeof probe, "%s/assets/sfx/shot.wav", root);
    return access(probe, R_OK) == 0;
}

static int has_ci(const char *h, const char *n) {
    size_t nh = strlen(h), nn = strlen(n);
    if (nn == 0 || nn > nh) return 0;
    size_t i;
    for (i = 0; i + nn <= nh; i++) {
        if (strncasecmp(h + i, n, nn) == 0) return 1;
    }
    return 0;
}

static void scan_music(const char *root) {
    music_n = 0;
    stage_n = 0;
    title_file[0] = 0;
    queen_file[0] = 0;
    snprintf(music_dir, sizeof music_dir, "%s/assets/music", root);
    DIR *d = opendir(music_dir);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        const char *n = e->d_name;
        size_t L = strlen(n);
        if (L < 5) continue;
        if (strcasecmp(n + L - 4, ".wav") != 0) continue;
        if (music_n >= MUSIC_CAP) break;
        snprintf(music_files[music_n], sizeof music_files[0], "%s", n);
        music_n++;
        if (has_ci(n, "turn us around")) {
            snprintf(title_file, sizeof title_file, "%s", n);
        } else if (has_ci(n, "queen")) {
            snprintf(queen_file, sizeof queen_file, "%s", n);
        } else if (stage_n < MUSIC_CAP) {
            snprintf(stage_files[stage_n], sizeof stage_files[0], "%s", n);
            stage_n++;
        }
    }
    closedir(d);
}

static void bgm_stop(void) {
    if (!bgm) return;
    bgm->StopPlaying();
    delete bgm;
    bgm = NULL;
    music_mode = 0;
}

static void bgm_play_file(const char *name, int loop, int mode) {
    if (!audio_ok || !name || !name[0]) return;
    bgm_stop();
    char path[640];
    snprintf(path, sizeof path, "%s/%s", music_dir, name);
    BFileGameSound *s = new BFileGameSound(path, loop ? true : false);
    if (!s || s->InitCheck() != B_OK) {
        fprintf(stderr, "arcade audio: bgm failed %s (%d)\n",
                path, s ? (int)s->InitCheck() : -1);
        delete s;
        return;
    }
    s->SetGain(vol_music);
    s->StartPlaying();
    bgm = s;
    music_mode = mode;
    fprintf(stderr, "arcade audio: bgm %s\n", name);
}

static void bgm_next(void) {
    if (!audio_ok || stage_n < 1) return;
    int pick = 0;
    if (stage_n == 1) {
        pick = 0;
    } else {
        pick = rand() % stage_n;
        if (pick == music_cur) pick = (pick + 1) % stage_n;
    }
    music_cur = pick;
    bgm_play_file(stage_files[pick], 0, 1);
}

static void bgm_title(void) {
    if (title_file[0]) bgm_play_file(title_file, 1, 2);
    else bgm_next();
}

static void bgm_queen(void) {
    if (queen_file[0]) bgm_play_file(queen_file, 1, 3);
    else bgm_next();
}

static void alog(const char *fmt, ...) {
    FILE *f = fopen("/tmp/arcade_audio.log", "a");
    if (!f) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fclose(f);
}

static void reap_voices(void) {
    int i;
    for (i = 0; i < SFX_VOICES; i++) {
        if (voice[i] && !voice[i]->IsPlaying()) {
            delete voice[i];
            voice[i] = NULL;
        }
    }
}

static void play_sfx(int code) {
    if (!audio_ok) return;
    if (code < 1 || code > 7 || !clip_path[code][0]) return;
    reap_voices();
    /* Same mixer path as BGM. BSimpleGameSound skipped these 48k clips. */
    BFileGameSound *s = new BFileGameSound(clip_path[code], false);
    if (!s || s->InitCheck() != B_OK) {
        if (sfx_plays < 8)
            alog("sfx fail code=%d %s (%d)", code, clip_path[code],
                 s ? (int)s->InitCheck() : -1);
        delete s;
        return;
    }
    s->SetGain(vol_sfx);
    s->StartPlaying();
    sfx_plays++;
    if (sfx_plays <= 8)
        alog("sfx play code=%d gain=%.2f %s", code, vol_sfx, clip_path[code]);
    int i;
    for (i = 0; i < SFX_VOICES; i++) {
        if (!voice[i]) {
            voice[i] = s;
            return;
        }
    }
    int slot = voice_steal % SFX_VOICES;
    voice_steal++;
    voice[slot]->StopPlaying();
    delete voice[slot];
    voice[slot] = s;
}

static void audio_play(int code) {
    if (code == 8) { bgm_next(); return; }
    if (code == 9) { bgm_stop(); return; }
    if (code == 10) { bgm_title(); return; }
    if (code == 11) { bgm_queen(); return; }
    play_sfx(code);
}

static void audio_set_vol(int music10, int sfx10) {
    if (music10 < 0) music10 = 0;
    if (music10 > 10) music10 = 10;
    if (sfx10 < 0) sfx10 = 0;
    if (sfx10 > 10) sfx10 = 10;
    vol_music = (music10 / 10.0f) * 0.50f;
    vol_sfx = (sfx10 / 10.0f) * 0.80f;
    if (bgm) bgm->SetGain(vol_music);
    int i;
    for (i = 0; i < SFX_VOICES; i++) {
        if (voice[i]) voice[i]->SetGain(vol_sfx);
    }
}

static void audio_poll_vol(const char *vol_path) {
    if (!vol_path) return;
    FILE *f = fopen(vol_path, "r");
    if (!f) return;
    int m = 7, s = 8;
    int n = fscanf(f, "%d %d", &m, &s);
    fclose(f);
    if (n != 2) return;
    if (m == last_mv && s == last_sv) return;
    last_mv = m;
    last_sv = s;
    audio_set_vol(m, s);
}

static void audio_poll(const char *sfx_bin) {
    if (!sfx_bin) return;
    int fd = open(sfx_bin, O_RDONLY);
    if (fd < 0) return;
    uint8_t buf[72];
    ssize_t n = read(fd, buf, 72);
    close(fd);
    if (n < 72) return;
    uint32_t w = (uint32_t)le32u(buf);
    if (!primed) {
        seen = w;
        primed = 1;
        return;
    }
    uint32_t nnew = w - seen;
    if (nnew == 0) {
        if (bgm && music_mode == 1 && !bgm->IsPlaying()) bgm_next();
        reap_voices();
        return;
    }
    if (nnew > 64) nnew = 64;
    uint32_t i;
    for (i = w - nnew; i != w; i++)
        audio_play((int)buf[8 + (i % 64)]);
    seen = w;
    if (bgm && music_mode == 1 && !bgm->IsPlaying()) bgm_next();
    reap_voices();
}

static void audio_shutdown(void) {
    int i;
    bgm_stop();
    for (i = 0; i < SFX_VOICES; i++) {
        if (voice[i]) {
            voice[i]->StopPlaying();
            delete voice[i];
            voice[i] = NULL;
        }
    }
    for (i = 1; i < 8; i++)
        clip_path[i][0] = 0;
    audio_ok = 0;
    primed = 0;
    sfx_plays = 0;
}

static int audio_init(const char *root) {
    const char *use = NULL;
    if (try_root(root)) use = root;
    else if (try_root(".")) use = ".";
    else if (try_root("..")) use = "..";
    else if (try_root("/boot/home/config/non-packaged/data/ailang_arcade"))
        use = "/boot/home/config/non-packaged/data/ailang_arcade";
    if (!use) {
        fprintf(stderr, "arcade audio: no assets/sfx (media_server still used if clips appear)\n");
        return 0;
    }
    scan_music(use);
    srand((unsigned)time(NULL) ^ (unsigned)getpid());
    BGameSound::SetMaxSoundCount(64);
    int loaded = 0;
    int i;
    for (i = 1; i < 8; i++) {
        clip_path[i][0] = 0;
        snprintf(clip_path[i], sizeof clip_path[i], "%s/assets/sfx/%s", use, kSfxName[i]);
        if (access(clip_path[i], R_OK) != 0) {
            alog("sfx missing %s", clip_path[i]);
            clip_path[i][0] = 0;
            continue;
        }
        loaded++;
    }
    audio_ok = 1;
    primed = 0;
    seen = 0;
    sfx_plays = 0;
    alog("mixer=media_server sfx=%d/7 music=%d stage=%d title=%s queen=%s",
         loaded, music_n, stage_n,
         title_file[0] ? title_file : "-",
         queen_file[0] ? queen_file : "-");
    fprintf(stderr, "arcade audio: mixer=media_server  sfx=%d/%d  music=%d stage=%d title=%s queen=%s\n",
            loaded, 7, music_n, stage_n,
            title_file[0] ? title_file : "-",
            queen_file[0] ? queen_file : "-");
    bgm_title();
    return 1;
}

static int read_gen(void) {
    FILE *f = fopen(path_gen, "r");
    if (!f) return -1;
    int g = -1;
    if (fscanf(f, "%d", &g) != 1) g = -1;
    fclose(f);
    return g;
}

static int load_frame(uint8_t **out_pix, int *out_w, int *out_h, int *out_pitch) {
    int fd = open(path_meta, O_RDONLY);
    if (fd < 0) return -1;
    int32_t hdr[3];
    if (read(fd, hdr, 12) != 12) { close(fd); return -1; }
    close(fd);
    int w = hdr[0], h = hdr[1], pitch = hdr[2];
    if (w < 16 || h < 16 || pitch < w * 4) return -1;
    if (w > 4096 || h > 4096) return -1;
    size_t sz = (size_t)pitch * (size_t)h;
    uint8_t *pix = (uint8_t *)malloc(sz);
    if (!pix) return -1;
    fd = open(path_frame, O_RDONLY);
    if (fd < 0) { free(pix); return -1; }
    size_t got = 0;
    while (got < sz) {
        ssize_t n = read(fd, pix + got, sz - got);
        if (n <= 0) break;
        got += (size_t)n;
    }
    close(fd);
    if (got < sz) { free(pix); return -1; }
    *out_pix = pix;
    *out_w = w;
    *out_h = h;
    *out_pitch = pitch;
    return 0;
}

static int map_key(unsigned char ch, int on) {
    int *slot = NULL;
    if (ch == B_LEFT_ARROW || ch == 'a' || ch == 'A') slot = &k_l;
    else if (ch == B_RIGHT_ARROW || ch == 'd' || ch == 'D') slot = &k_r;
    else if (ch == B_UP_ARROW || ch == 'w' || ch == 'W') slot = &k_u;
    else if (ch == B_DOWN_ARROW || ch == 's' || ch == 'S') slot = &k_d;
    else if (ch == 'z' || ch == 'Z' || ch == B_SPACE) slot = &k_f;
    else if (ch == B_ENTER) slot = &k_s;
    else if (ch == 'p' || ch == 'P') slot = &k_p;
    else return 0;
    *slot = on ? 1 : 0;
    write_keys();
    return 1;
}

class PlayView : public BView {
public:
    PlayView(BRect frame)
        : BView(frame, "playfield", B_FOLLOW_ALL_SIDES,
                B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE | B_NAVIGABLE | B_FRAME_EVENTS),
          fBitmap(NULL), fW(0), fH(0), fLastW(0), fLastH(0) {
        SetViewColor(5, 10, 24);
        SetLowColor(5, 10, 24);
    }

    ~PlayView() { delete fBitmap; }

    void AttachedToWindow() { MakeFocus(true); }

    void Draw(BRect) {
        if (fBitmap && fW > 0 && fH > 0) {
            BRect src(0, 0, fW - 1, fH - 1);
            DrawBitmap(fBitmap, src, Bounds());
        } else {
            SetHighColor(5, 10, 24);
            FillRect(Bounds());
            SetHighColor(180, 200, 220);
            DrawString("waiting for arcade_app.x", BPoint(24, 40));
        }
    }

    void FrameResized(float width, float height) {
        int w = (int)width + 1;
        int h = (int)height + 1;
        if (w != fLastW || h != fLastH) {
            fLastW = w;
            fLastH = h;
            write_size(w, h);
        }
        Invalidate();
    }

    void KeyDown(const char *bytes, int32 numBytes) {
        if (numBytes < 1 || !bytes) return;
        unsigned char ch = (unsigned char)bytes[0];
        if (ch == B_ESCAPE || ch == 'q' || ch == 'Q') {
            write_cmd("quit");
            Window()->PostMessage(B_QUIT_REQUESTED);
            return;
        }
        if (!map_key(ch, 1))
            BView::KeyDown(bytes, numBytes);
    }

    void KeyUp(const char *bytes, int32 numBytes) {
        if (numBytes < 1 || !bytes) return;
        map_key((unsigned char)bytes[0], 0);
    }

    bool AdoptFrame(uint8_t *pix, int w, int h, int pitch) {
        BBitmap *nb = new BBitmap(BRect(0, 0, w - 1, h - 1), B_RGB32, true);
        if (!nb || !nb->IsValid() || !nb->Bits()) {
            delete nb;
            free(pix);
            return false;
        }
        uint8_t *dst = (uint8_t *)nb->Bits();
        int32 bpr = nb->BytesPerRow();
        int copy = w * 4;
        if (copy > pitch) copy = pitch;
        if (copy > bpr) copy = bpr;
        for (int y = 0; y < h; y++)
            memcpy(dst + (size_t)y * (size_t)bpr, pix + (size_t)y * (size_t)pitch, (size_t)copy);
        delete fBitmap;
        free(pix);
        fBitmap = nb;
        fW = w;
        fH = h;
        Invalidate();
        return true;
    }

private:
    BBitmap *fBitmap;
    int fW, fH, fLastW, fLastH;
};

class ArcadeWindow : public BWindow {
public:
    ArcadeWindow(BRect r)
        : BWindow(r, "NOWAY HOME", B_TITLED_WINDOW,
                  B_QUIT_ON_WINDOW_CLOSE | B_ASYNCHRONOUS_CONTROLS),
          fView(NULL), fStatus(NULL), fLastGen(-1) {
        BRect b = Bounds();
        BMenuBar *bar = new BMenuBar(BRect(0, 0, b.right, 20), "menu");
        BMenu *game = new BMenu("Game");
        BMessage *ms = new BMessage(MSG_CMD);
        ms->AddString("cmd", "start");
        game->AddItem(new BMenuItem("Start", ms, 'N'));
        BMessage *mp = new BMessage(MSG_CMD);
        mp->AddString("cmd", "pause");
        game->AddItem(new BMenuItem("Pause / Settings", mp, 'P'));
        game->AddSeparatorItem();
        game->AddItem(new BMenuItem("Quit", new BMessage(B_QUIT_REQUESTED), 'Q'));
        bar->AddItem(game);
        AddChild(bar);
        float barH = bar->Bounds().Height();
        BRect play(0, barH, b.right, b.bottom - 18);
        fView = new PlayView(play);
        AddChild(fView);
        BRect st(0, b.bottom - 17, b.right, b.bottom);
        fStatus = new BStringView(st, "status",
            "Arrows move  Z/Space fire  Enter start  P settings  Esc quit");
        fStatus->SetViewColor(5, 10, 24);
        fStatus->SetHighColor(160, 180, 200);
        fStatus->SetResizingMode(B_FOLLOW_LEFT_RIGHT | B_FOLLOW_BOTTOM);
        AddChild(fStatus);
        fView->MakeFocus(true);
        BMessage poll(MSG_POLL);
        fRunner = new BMessageRunner(BMessenger(this), &poll, 16000);
    }

    ~ArcadeWindow() { delete fRunner; }

    bool QuitRequested() {
        write_cmd("quit");
        return true;
    }

    void MessageReceived(BMessage *msg) {
        if (msg->what == MSG_POLL) {
            OnPoll();
            return;
        }
        if (msg->what == MSG_CMD) {
            const char *c = NULL;
            if (msg->FindString("cmd", &c) == B_OK && c)
                write_cmd(c);
            return;
        }
        BWindow::MessageReceived(msg);
    }

    void OnPoll() {
        write_keys();
        if (fView) {
            BRect pb = fView->Bounds();
            write_size((int)pb.Width() + 1, (int)pb.Height() + 1);
        }
        int gen = read_gen();
        if (gen >= 0 && gen != fLastGen) {
            uint8_t *pix = NULL;
            int w, h, pitch;
            if (load_frame(&pix, &w, &h, &pitch) == 0) {
                if (fView->AdoptFrame(pix, w, h, pitch))
                    fLastGen = gen;
            }
        }
        audio_poll(path_sfx);
        audio_poll_vol(path_vol);
    }

private:
    PlayView *fView;
    BStringView *fStatus;
    BMessageRunner *fRunner;
    int fLastGen;
};

static int file_ok(const char *p) {
    return p && p[0] && access(p, R_OK) == 0;
}

static void pick(char *dst, size_t n, const char **cands) {
    dst[0] = 0;
    int i;
    for (i = 0; cands[i]; i++) {
        if (file_ok(cands[i])) {
            snprintf(dst, n, "%s", cands[i]);
            return;
        }
    }
}

static void kernel_start(void) {
    char runner[512], kernel[512], data[512];
    const char *run_c[] = {
        "/boot/home/sys_compat_run",
        "/boot/home/config/non-packaged/bin/sys_compat_run",
        "/boot/system/non-packaged/bin/sys_compat_run",
        NULL
    };
    const char *kern_c[] = {
        "/boot/home/config/non-packaged/data/ailang_arcade/arcade_app.x",
        "/boot/home/arcade_app.x",
        NULL
    };
    const char *data_c[] = {
        "/boot/home/config/non-packaged/data/ailang_arcade",
        "/boot/home",
        NULL
    };
    pick(runner, sizeof runner, run_c);
    pick(kernel, sizeof kernel, kern_c);
    data[0] = 0;
    g_data[0] = 0;
    {
        int i;
        for (i = 0; data_c[i]; i++) {
            char probe[600];
            snprintf(probe, sizeof probe, "%s/assets/ships/player.svg", data_c[i]);
            if (access(probe, R_OK) == 0) {
                snprintf(data, sizeof data, "%s", data_c[i]);
                snprintf(g_data, sizeof g_data, "%s", data_c[i]);
                break;
            }
        }
    }
    if (!runner[0] || !kernel[0]) {
        fprintf(stderr, "arcade_shell_haiku: kernel/runner missing (install.sh?)\n");
        return;
    }
    mkdir(g_dir, 0755);
    write_file(path_cmd, "\n");
    write_keys();
    pid_t pid = fork();
    if (pid < 0) return;
    if (pid == 0) {
        int fd = open("/tmp/arcade_kern.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd >= 0) {
            dup2(fd, 1);
            dup2(fd, 2);
            if (fd > 2) close(fd);
        }
        setsid();
        if (data[0]) chdir(data);
        execl(runner, runner, kernel, (char *)NULL);
        _exit(127);
    }
    fprintf(stderr, "arcade_shell_haiku: kernel pid %d (%s %s)\n",
            (int)pid, runner, kernel);
}

class ArcadeApp : public BApplication {
public:
    ArcadeApp() : BApplication("application/x-vnd.Ailang-Arcade") {}

    void ReadyToRun() {
        mkdir(g_dir, 0755);
        kernel_start();
        audio_init(g_data[0] ? g_data : NULL);
        BScreen screen;
        BRect s = screen.Frame();
        float w = 900, h = 720;
        BRect r(60, 40, 60 + w, 40 + h);
        if (s.Width() < w + 40) r.right = s.right - 20;
        if (s.Height() < h + 40) r.bottom = s.bottom - 40;
        ArcadeWindow *win = new ArcadeWindow(r);
        win->Show();
        fprintf(stderr, "arcade_shell_haiku: %s  (BBitmap B_RGB32)\n", g_dir);
    }
};

int main(int argc, char **argv) {
    const char *dir = getenv("ARCADE_APP_STATE");
    if (!dir || !dir[0]) dir = "/tmp/arcade_app";
    if (argc > 1 && argv[1] && argv[1][0] && argv[1][0] != '-')
        dir = argv[1];
    mkdir("/dev/shm", 01777);
    mkdir(dir, 0755);
    paths_init(dir);
    ArcadeApp app;
    app.Run();
    audio_shutdown();
    write_cmd("quit");
    return 0;
}
