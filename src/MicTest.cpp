// MicTest.cpp - R36S Tamagotchi Project: 3.5mm Microphone Probe Tool
// Tests audio capture via TRRS headset mic using SDL2 + ALSA Golden Fix
// Architecture: Zero Dependency (stb_truetype only), SDL2 only

#include <SDL.h>
#include <string>
#include <vector>
#include <fstream>
#include <thread>
#include <atomic>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <cmath>
#include <stdint.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

// ===== SCREEN CONFIG (R36S = 640x480, same as RG351V) =====
#define SCREEN_WIDTH     640
#define SCREEN_HEIGHT    480
#define FONT_SIZE        20
#define FONT_SIZE_SMALL  15
#define LINE_HEIGHT      28
#define MS_PER_FRAME     33

#define REC_DURATION_SEC    5
#define SAMPLE_RATE         44100
#define CHANNELS            1      // Mono mic
#define AUDIO_BUFFER_FRAMES 1024   // Safe zone: well under 2048 frame limit
#define OUTPUT_WAV_PATH     "/storage/roms/ports/App_MicTest/test_mic.wav"

// ===== STATES =====
enum AppState {
    STATE_IDLE,
    STATE_RECORDING,
    STATE_DONE,
    STATE_ERROR
};

// ===== GLOBALS =====
SDL_Window*   g_window   = NULL;
SDL_Renderer* g_renderer = NULL;
SDL_Joystick* g_joystick = NULL;
SDL_AudioDeviceID g_audio_dev = 0;

std::atomic<AppState> g_state(STATE_IDLE);
std::vector<int16_t>  g_pcm_buffer;      // Raw captured PCM samples
std::atomic<int>      g_rec_frames(0);   // How many samples captured so far
int                   g_total_frames = SAMPLE_RATE * REC_DURATION_SEC;
std::string           g_status_msg = "Ready. Press [A] to record.";

// Waveform display (last N amplitude values)
#define WAVE_POINTS 80
float g_waveform[WAVE_POINTS] = {0};
std::atomic<int> g_wave_idx(0);

// ===== GOLDEN FIX: Reset A33 Audio DMA =====
// Based on udt_pwr_events.sh backdoor discovered in heritage research
void goldenFix() {
    system("echo '2,0x07,0xf4' > /sys/devices/platform/sunxi-pcm-codec/audio_reg_debug/audio_reg 2>/dev/null");
    system("amixer cset name='Speaker Function' headset 2>/dev/null");
    system("amixer cset name='Speaker Function' spk 2>/dev/null");
}

// ===== ENABLE MIC via amixer switches =====
void enableMic() {
    // Enable headset mic input chain based on amixer scan
    system("amixer cset name='Audio phone headsetmic' on 2>/dev/null");
    system("amixer cset name='Audio phone voicerecord' on 2>/dev/null");
    system("amixer cset name='Audio phone mic' on 2>/dev/null");
    system("amixer cset name='Audio phone in' on 2>/dev/null");
    system("amixer cset name='Audio phone in left' on 2>/dev/null");
    system("amixer cset name='Audio linein in' on 2>/dev/null");
    
    // Maximize all pre-amp gains
    system("amixer cset name='MIC1 boost AMP gain control' 7 2>/dev/null");
    system("amixer cset name='MIC2 boost AMP gain control' 7 2>/dev/null");
    system("amixer cset name='ADC input gain ctrl' 7 2>/dev/null");
    
    // Enable ADC input paths
    system("amixer cset name='Audio adc phonein' on 2>/dev/null");
    system("amixer cset name='Audio linein record' on 2>/dev/null");
    
    // Connect all pre-amps to the ADC mixer
    system("amixer cset name='MIC1_G boost stage output mixer control' 7 2>/dev/null");
    system("amixer cset name='MIC2_G boost stage output mixer control' 7 2>/dev/null");
    system("amixer cset name='LINEIN_G boost stage output mixer control' 7 2>/dev/null");
    system("amixer cset name='PHONE_G boost stage output mixer control' 7 2>/dev/null");
    system("amixer cset name='PHONE_NG boost stage output mixer control' 7 2>/dev/null");
    system("amixer cset name='PHONE_PG boost stage output mixer control' 7 2>/dev/null");
}

void disableMic() {
    system("amixer cset name='Audio phone headsetmic' off 2>/dev/null");
    system("amixer cset name='Audio phone voicerecord' off 2>/dev/null");
    system("amixer cset name='Audio phone mic' off 2>/dev/null");
    system("amixer cset name='Audio linein record' off 2>/dev/null");
}

// ===== WAV FILE WRITER =====
struct WavHeader {
    char     riff[4]       = {'R','I','F','F'};
    uint32_t chunkSize;
    char     wave[4]       = {'W','A','V','E'};
    char     fmt[4]        = {'f','m','t',' '};
    uint32_t subchunk1Size = 16;
    uint16_t audioFormat   = 1;  // PCM
    uint16_t numChannels   = CHANNELS;
    uint32_t sampleRate    = SAMPLE_RATE;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample = 16;
    char     data[4]       = {'d','a','t','a'};
    uint32_t subchunk2Size;
};

bool writeWav(const char* path, const int16_t* samples, int num_samples) {
    WavHeader hdr;
    hdr.blockAlign    = CHANNELS * 2;
    hdr.byteRate      = SAMPLE_RATE * hdr.blockAlign;
    hdr.subchunk2Size = num_samples * 2;
    hdr.chunkSize     = 36 + hdr.subchunk2Size;

    FILE* f = fopen(path, "wb");
    if (!f) return false;
    fwrite(&hdr, sizeof(hdr), 1, f);
    fwrite(samples, 2, num_samples, f);
    fclose(f);
    return true;
}

// ===== SDL AUDIO CAPTURE CALLBACK =====
void audioCallback(void* userdata, Uint8* stream, int len) {
    (void)userdata;
    if (g_state != STATE_RECORDING) return;

    int samples_in = len / sizeof(int16_t);
    int16_t* src = (int16_t*)stream;

    for (int i = 0; i < samples_in; i++) {
        if ((int)g_pcm_buffer.size() < g_total_frames) {
            g_pcm_buffer.push_back(src[i]);
        }

        // Update waveform
        float amp = fabsf((float)src[i] / 32768.0f);
        int wi = g_wave_idx % WAVE_POINTS;
        g_waveform[wi] = amp;
        g_wave_idx++;
    }

    g_rec_frames = (int)g_pcm_buffer.size();

    // Auto-stop when done
    if ((int)g_pcm_buffer.size() >= g_total_frames) {
        // Signal recording done - main thread will handle state change
        g_state = STATE_DONE;
    }
}

// ===== SDL INIT =====
bool initSDL() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_AUDIO) < 0) return false;

    if (SDL_NumJoysticks() >= 1)
        g_joystick = SDL_JoystickOpen(0);

    g_window = SDL_CreateWindow("MicTest",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN);
    if (!g_window) return false;

    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_SOFTWARE);
    if (!g_renderer) return false;

    return true;
}

// ===== OPEN AUDIO CAPTURE DEVICE =====
bool openMicDevice() {
    SDL_AudioSpec want, got;
    SDL_memset(&want, 0, sizeof(want));
    want.freq     = SAMPLE_RATE;
    want.format   = AUDIO_S16SYS;
    want.channels = CHANNELS;
    want.samples  = AUDIO_BUFFER_FRAMES;  // 1024 frames - safely under A33's 2048 limit
    want.callback = audioCallback;
    want.userdata = NULL;

    int num_devs = SDL_GetNumAudioDevices(1); // 1 = capture devices
    const char* dev_name = NULL;
    char dev_list[512] = {0};
    if (num_devs > 0) {
        dev_name = SDL_GetAudioDeviceName(0, 1);
        for(int i=0; i<num_devs && i<4; i++) {
            strcat(dev_list, SDL_GetAudioDeviceName(i, 1));
            strcat(dev_list, " | ");
        }
    }

    // Try forcing hw:0,0 to bypass broken ALSA plugins (dmix/plug)
    g_audio_dev = SDL_OpenAudioDevice("hw:0,0", 1, &want, &got, 0);
    
    // If hw:0,0 fails, fallback to SDL's default device
    if (g_audio_dev == 0) {
        g_audio_dev = SDL_OpenAudioDevice(dev_name, 1, &want, &got, 0);
    }

    // Write debug info to a file
    char buf[256];
    snprintf(buf, sizeof(buf), "Req: hw:0,0\nFallback: %s\nAvail: %s\nGot Format: %d, Freq: %d\nDevice ID: %d", 
             dev_name ? dev_name : "NULL", dev_list, got.format, got.freq, g_audio_dev);
    system((std::string("echo '") + buf + "' > /tmp/mictest_debug.txt").c_str());

    if (g_audio_dev > 0) {
        snprintf(buf, sizeof(buf), "Opened DEV ID: %d (Check /tmp/mictest_debug.txt)", g_audio_dev);
        g_status_msg = buf;
    } else {
        g_status_msg = "WARNING: Failed to open any capture device!";
    }

    return (g_audio_dev > 0);
}

// ===== FONT ENGINE (stb_truetype) =====
class CustomFont {
public:
    SDL_Texture* atlas = NULL;
    stbtt_bakedchar cdata[96];
    int tex_w = 512, tex_h = 512;
    float size = 20.0f;

    bool load(SDL_Renderer* renderer, const char* path, float font_size) {
        size = font_size;
        FILE* f = fopen(path, "rb");
        if (!f) return false;
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        rewind(f);
        std::vector<unsigned char> buf(sz);
        fread(buf.data(), 1, sz, f);
        fclose(f);

        std::vector<unsigned char> bitmap(tex_w * tex_h);
        stbtt_BakeFontBitmap(buf.data(), 0, size, bitmap.data(), tex_w, tex_h, 32, 96, cdata);

        std::vector<unsigned char> rgba(tex_w * tex_h * 4, 255);
        for (int i = 0; i < tex_w * tex_h; i++)
            rgba[i*4+3] = bitmap[i];

        SDL_Surface* surf = SDL_CreateRGBSurfaceFrom(rgba.data(), tex_w, tex_h, 32, tex_w*4,
            0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
        if (!surf) return false;
        atlas = SDL_CreateTextureFromSurface(renderer, surf);
        SDL_FreeSurface(surf);
        SDL_SetTextureBlendMode(atlas, SDL_BLENDMODE_BLEND);
        return atlas != NULL;
    }

    void draw(SDL_Renderer* r, float x, float y, const char* text, SDL_Color c) {
        if (!atlas) return;
        SDL_SetTextureColorMod(atlas, c.r, c.g, c.b);
        for (const char* p = text; *p; p++) {
            if (*p >= 32 && *p < 128) {
                stbtt_aligned_quad q;
                stbtt_GetBakedQuad(cdata, tex_w, tex_h, *p - 32, &x, &y, &q, 1);
                SDL_Rect src = {(int)(q.s0*tex_w),(int)(q.t0*tex_h),(int)((q.s1-q.s0)*tex_w),(int)((q.t1-q.t0)*tex_h)};
                SDL_Rect dst = {(int)q.x0,(int)q.y0,(int)(q.x1-q.x0),(int)(q.y1-q.y0)};
                SDL_RenderCopy(r, atlas, &src, &dst);
            }
        }
    }

    ~CustomFont() { if (atlas) SDL_DestroyTexture(atlas); }
};

// ===== DRAW HELPERS =====
void drawGlowLine(SDL_Renderer* r, int x1, int y1, int x2, int y2, SDL_Color c) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 40);
    SDL_RenderDrawLine(r, x1-1, y1, x2-1, y2);
    SDL_RenderDrawLine(r, x1+1, y1, x2+1, y2);
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 255);
    SDL_RenderDrawLine(r, x1, y1, x2, y2);
}

void drawRect(SDL_Renderer* r, int x, int y, int w, int h, SDL_Color c, bool fill = false) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_Rect rect = {x, y, w, h};
    if (fill) SDL_RenderFillRect(r, &rect);
    else      SDL_RenderDrawRect(r, &rect);
}

void drawScanlines(SDL_Renderer* r) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 70);
    for (int y = 0; y < SCREEN_HEIGHT; y += 3)
        SDL_RenderDrawLine(r, 0, y, SCREEN_WIDTH, y);
}

// ===== MAIN =====
int main(int argc, char* argv[]) {
    if (!initSDL()) return 1;

    CustomFont font, fontSmall;
    font.load(g_renderer, "./res/NotoSansMono-Regular.ttf", FONT_SIZE);
    fontSmall.load(g_renderer, "./res/NotoSansMono-Regular.ttf", FONT_SIZE_SMALL);

    // Colors
    SDL_Color cyan    = {0, 255, 255, 255};
    SDL_Color magenta = {255, 0, 255, 255};
    SDL_Color yellow  = {255, 230, 0, 255};
    SDL_Color green   = {0, 255, 100, 255};
    SDL_Color red     = {255, 60, 60, 255};
    SDL_Color white   = {220, 220, 220, 255};
    SDL_Color bg      = {8, 8, 18, 255};
    SDL_Color dimcyan = {0, 130, 130, 255};

    bool quit = false;
    bool show_menu = false;
    int  menu_sel = 0;

    while (!quit) {
        // ====== EVENT HANDLING ======
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            bool press_a = false, press_b = false, press_x = false;
            bool press_up = false, press_down = false;

            if (ev.type == SDL_QUIT) { quit = true; break; }
            if (ev.type == SDL_JOYBUTTONDOWN) {
                int b = ev.jbutton.button;
                if (b == 0) press_a = true;
                if (b == 1) press_b = true;
                if (b == 2 || b == 3) press_x = true;
                if (b == 6 || b == 13) press_up = true;      // L2 or DPad Up
                if (b == 7 || b == 14) press_down = true;    // R2 or DPad Down
            }
            if (ev.type == SDL_JOYHATMOTION) {
                if (ev.jhat.value & SDL_HAT_UP)    press_up   = true;
                if (ev.jhat.value & SDL_HAT_DOWN)  press_down = true;
            }
            if (ev.type == SDL_JOYAXISMOTION) {
                if (ev.jaxis.axis == 1) { // Left Analog Y axis
                    if (ev.jaxis.value < -16000) press_up = true;
                    if (ev.jaxis.value > 16000)  press_down = true;
                }
            }
            if (ev.type == SDL_KEYDOWN) {
                if (ev.key.keysym.sym == SDLK_a || ev.key.keysym.sym == SDLK_RETURN) press_a = true;
                if (ev.key.keysym.sym == SDLK_b)      press_b = true;
                if (ev.key.keysym.sym == SDLK_x || ev.key.keysym.sym == SDLK_ESCAPE) press_x = true;
                if (ev.key.keysym.sym == SDLK_UP)     press_up = true;
                if (ev.key.keysym.sym == SDLK_DOWN)   press_down = true;
            }

            // Menu navigation
            if (press_x && !show_menu) {
                show_menu = true;
                menu_sel = 0;
            } else if (show_menu) {
                if (press_up || press_down) menu_sel = 1 - menu_sel;
                if (press_a || press_b) {
                    if (menu_sel == 1) { quit = true; }
                    show_menu = false;
                }
                if (press_x) show_menu = false;
            } else {
                // Main action: A = Record/Stop
                if (press_a) {
                    AppState cur = g_state.load();
                    if (cur == STATE_IDLE || cur == STATE_DONE || cur == STATE_ERROR) {
                        // Start recording
                        g_pcm_buffer.clear();
                        g_pcm_buffer.reserve(g_total_frames);
                        g_rec_frames = 0;
                        g_wave_idx   = 0;
                        memset(g_waveform, 0, sizeof(g_waveform));

                        // Apply Golden Fix first to reset DMA, THEN open audio
                        // Disable goldenFix during capture, as it might turn off the ADC!
                        // goldenFix();
                        SDL_Delay(200);
                        enableMic();
                        SDL_Delay(100);

                        if (g_audio_dev) { SDL_CloseAudioDevice(g_audio_dev); g_audio_dev = 0; }

                        if (!openMicDevice()) {
                            g_state = STATE_ERROR;
                            g_status_msg = "ERROR: SDL_OpenAudioDevice failed!";
                        } else {
                            g_state = STATE_RECORDING;
                            g_status_msg = "RECORDING... speak into mic";
                            SDL_PauseAudioDevice(g_audio_dev, 0); // Unpause = start capturing
                        }
                    } else if (cur == STATE_RECORDING) {
                        // Manual stop
                        SDL_PauseAudioDevice(g_audio_dev, 1);
                        SDL_CloseAudioDevice(g_audio_dev);
                        g_audio_dev = 0;
                        g_state = STATE_DONE;
                        g_status_msg = "Stopped manually.";
                    }
                }
            }
        }

        // Check if recording finished automatically
        if (g_state == STATE_DONE && g_audio_dev > 0) {
            SDL_PauseAudioDevice(g_audio_dev, 1);
            SDL_CloseAudioDevice(g_audio_dev);
            g_audio_dev = 0;
            disableMic();
            // Save WAV
            if (writeWav(OUTPUT_WAV_PATH, g_pcm_buffer.data(), (int)g_pcm_buffer.size())) {
                g_status_msg = "SAVED: " OUTPUT_WAV_PATH;
            } else {
                g_status_msg = "DONE (WAV write failed - check /tmp perms)";
            }
        }

        // ====== RENDER ======
        SDL_SetRenderDrawColor(g_renderer, bg.r, bg.g, bg.b, 255);
        SDL_RenderClear(g_renderer);

        // Grid background
        SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(g_renderer, 0, 255, 255, 10);
        for (int x = 0; x < SCREEN_WIDTH; x += 40) SDL_RenderDrawLine(g_renderer, x, 0, x, SCREEN_HEIGHT);
        for (int y = 0; y < SCREEN_HEIGHT; y += 40) SDL_RenderDrawLine(g_renderer, 0, y, SCREEN_WIDTH, y);

        // Outer border (chamfered)
        int pad = 8, ch = 20;
        int bw = SCREEN_WIDTH - pad*2 - 1, bh = SCREEN_HEIGHT - pad*2 - 1;
        drawGlowLine(g_renderer, pad+ch, pad,    pad+bw-ch, pad,    cyan);
        drawGlowLine(g_renderer, pad+bw-ch, pad, pad+bw, pad+ch,    cyan);
        drawGlowLine(g_renderer, pad+bw, pad+ch, pad+bw, pad+bh-ch, cyan);
        drawGlowLine(g_renderer, pad+bw, pad+bh-ch, pad+bw-ch, pad+bh, cyan);
        drawGlowLine(g_renderer, pad+bw-ch, pad+bh, pad+ch, pad+bh, cyan);
        drawGlowLine(g_renderer, pad+ch, pad+bh, pad, pad+bh-ch, cyan);
        drawGlowLine(g_renderer, pad, pad+bh-ch, pad, pad+ch,   cyan);
        drawGlowLine(g_renderer, pad, pad+ch,    pad+ch, pad,   cyan);

        // Header divider
        drawGlowLine(g_renderer, pad, pad+38, pad+bw, pad+38, magenta);

        // Title
        font.draw(g_renderer, 20, 34, ">>> MIC PROBE v1.0  [3.5mm TRRS]", cyan);

        // State indicator badge (top right)
        AppState cur_state = g_state.load();
        SDL_Color stateCol = dimcyan;
        const char* stateStr = "IDLE";
        if (cur_state == STATE_RECORDING) { stateCol = red; stateStr = "REC"; }
        else if (cur_state == STATE_DONE)  { stateCol = green; stateStr = "DONE"; }
        else if (cur_state == STATE_ERROR) { stateCol = red; stateStr = "ERR"; }

        // Blinking indicator when recording
        bool blink = (SDL_GetTicks() / 400) % 2 == 0;
        if (cur_state == STATE_RECORDING && blink) {
            SDL_Rect dot = {SCREEN_WIDTH - 90, pad+10, 12, 12};
            SDL_SetRenderDrawColor(g_renderer, red.r, red.g, red.b, 255);
            SDL_RenderFillRect(g_renderer, &dot);
        }
        font.draw(g_renderer, SCREEN_WIDTH - 75, 34, stateStr, stateCol);

        // Status message
        {
            SDL_Color msgCol = yellow;
            if (cur_state == STATE_RECORDING) msgCol = red;
            if (cur_state == STATE_DONE)      msgCol = green;
            if (cur_state == STATE_ERROR)     msgCol = red;
            fontSmall.draw(g_renderer, 20, 68, g_status_msg.c_str(), msgCol);
        }

        // ---- WAVEFORM DISPLAY ----
        int wx = 20, wy = 110, ww = SCREEN_WIDTH - 40, wh = 100;
        drawGlowLine(g_renderer, wx, wy, wx+ww, wy, dimcyan);           // top
        drawGlowLine(g_renderer, wx, wy+wh, wx+ww, wy+wh, dimcyan);    // bottom
        drawGlowLine(g_renderer, wx, wy, wx, wy+wh, dimcyan);           // left
        drawGlowLine(g_renderer, wx+ww, wy, wx+ww, wy+wh, dimcyan);    // right
        fontSmall.draw(g_renderer, wx+2, wy+1, "WAVEFORM", dimcyan);

        // Center line
        SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(g_renderer, 0, 255, 255, 40);
        SDL_RenderDrawLine(g_renderer, wx, wy+wh/2, wx+ww, wy+wh/2);

        // Draw waveform bars
        float bar_w = (float)ww / WAVE_POINTS;
        int wave_start = g_wave_idx % WAVE_POINTS;
        for (int i = 0; i < WAVE_POINTS; i++) {
            int idx = (wave_start + i) % WAVE_POINTS;
            float amp = g_waveform[idx];
            int bar_h = (int)(amp * (wh/2 - 4));
            if (bar_h < 1) bar_h = 1;
            int bx = wx + (int)(i * bar_w);
            int by = wy + wh/2 - bar_h;
            SDL_Color wc = {0, 200, 255, 200};
            if (amp > 0.5f) wc = magenta;
            SDL_SetRenderDrawColor(g_renderer, wc.r, wc.g, wc.b, wc.a);
            SDL_RenderDrawLine(g_renderer, bx, wy+wh/2+bar_h, bx, by);
        }

        // ---- PROGRESS BAR ----
        int px = 20, py = 230, pw = SCREEN_WIDTH - 40, ph = 20;
        int frames_done = g_rec_frames.load();
        float progress = (g_total_frames > 0) ? (float)frames_done / g_total_frames : 0.0f;
        if (progress > 1.0f) progress = 1.0f;

        drawRect(g_renderer, px, py, pw, ph, dimcyan, false);
        if (progress > 0.0f) {
            SDL_Color pbarCol = (cur_state == STATE_RECORDING) ? red : green;
            SDL_Rect fill = {px+1, py+1, (int)((pw-2)*progress), ph-2};
            SDL_SetRenderDrawColor(g_renderer, pbarCol.r, pbarCol.g, pbarCol.b, 180);
            SDL_RenderFillRect(g_renderer, &fill);
        }

        // Progress text
        char prog_buf[64];
        float sec_done = (float)frames_done / SAMPLE_RATE;
        snprintf(prog_buf, sizeof(prog_buf), "%.1fs / %ds  [%d samples]",
                 sec_done, REC_DURATION_SEC, frames_done);
        fontSmall.draw(g_renderer, px, py + ph + 6, prog_buf, white);

        // ---- INFO PANEL ----
        int iy = 280;
        fontSmall.draw(g_renderer, 20, iy,      "HW CONFIG:", dimcyan);
        fontSmall.draw(g_renderer, 20, iy+20,   "  CHIP:    Allwinner A33 audiocodec", white);
        fontSmall.draw(g_renderer, 20, iy+38,   "  SAMPLE:  48000 Hz / S16 / Mono", white);
        fontSmall.draw(g_renderer, 20, iy+56,   "  BUFFER:  1024 frames (safe zone < 2048)", white);
        fontSmall.draw(g_renderer, 20, iy+74,   "  OUTPUT:  " OUTPUT_WAV_PATH, yellow);
        fontSmall.draw(g_renderer, 20, iy+92,   "  DMA FIX: 0x07=0xf4 (Golden Fix applied)", green);

        // ---- FOOTER CONTROLS ----
        int fy = SCREEN_HEIGHT - 30;
        drawGlowLine(g_renderer, pad, fy - 8, pad+bw, fy - 8, magenta);
        if (!show_menu) {
            if (cur_state == STATE_IDLE || cur_state == STATE_DONE || cur_state == STATE_ERROR) {
                font.draw(g_renderer, 20, fy, "[A] START REC", green);
            } else {
                font.draw(g_renderer, 20, fy, "[A] STOP", red);
            }
            font.draw(g_renderer, SCREEN_WIDTH - 130, fy, "[X] SYS MENU", white);
        }

        // ---- SYS MENU OVERLAY ----
        if (show_menu) {
            SDL_Rect mbg = {60, SCREEN_HEIGHT - 80, SCREEN_WIDTH - 120, 60};
            SDL_SetRenderDrawColor(g_renderer, 8, 8, 18, 230);
            SDL_RenderFillRect(g_renderer, &mbg);
            drawGlowLine(g_renderer, mbg.x, mbg.y, mbg.x+mbg.w, mbg.y, magenta);

            const char* items[] = {"[0] RESUME", "[1] QUIT APP"};
            for (int i = 0; i < 2; i++) {
                int ix = 80 + i * 220;
                if (menu_sel == i) {
                    SDL_Rect hl = {ix - 4, SCREEN_HEIGHT - 58, 200, 28};
                    SDL_SetRenderDrawColor(g_renderer, magenta.r, magenta.g, magenta.b, 200);
                    SDL_RenderFillRect(g_renderer, &hl);
                    fontSmall.draw(g_renderer, ix, SCREEN_HEIGHT - 48, items[i], bg);
                } else {
                    fontSmall.draw(g_renderer, ix, SCREEN_HEIGHT - 48, items[i], cyan);
                }
            }
        }

        drawScanlines(g_renderer);
        SDL_RenderPresent(g_renderer);
        SDL_Delay(MS_PER_FRAME);
    }

    // Cleanup: restore audio to ES
    if (g_audio_dev) { SDL_PauseAudioDevice(g_audio_dev, 1); SDL_CloseAudioDevice(g_audio_dev); }
    disableMic();
    goldenFix();  // Restore audio for ES

    if (g_joystick) SDL_JoystickClose(g_joystick);
    if (g_renderer) SDL_DestroyRenderer(g_renderer);
    if (g_window)   SDL_DestroyWindow(g_window);
    SDL_Quit();
    return 0;
}
