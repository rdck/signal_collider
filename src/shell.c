/*******************************************************************************
 * shell.c - program entry point
 ******************************************************************************/

#define WIN32_LEAN_AND_MEAN
#define INITGUID
#define CINTERFACE
#define COBJMACROS
#define CONST_VTABLE
#include <windows.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <avrt.h>
#include <dbghelp.h>
#include <intrin.h>

#define GLAD_GL_IMPLEMENTATION
#define GLAD_WGL_IMPLEMENTATION
#include "glad/wgl.h"

#include "linear_algebra.h"
#include "input.h"
#include "sim.h"
#include "view.h"
#include "render.h"
#include "config.h"
#include "audio_guid.c"

#define Shell_CLASS_NAME "SHELL_WINDOW_CLASS"
#define Shell_EXIT_SUCCESS 0
#define Shell_EXIT_FAILURE 1
#define Shell_GL_VERSION_MAJOR 4
#define Shell_GL_VERSION_MINOR 5
#define Shell_TITLE "ORCASYNTH"

#define Shell_AUDIO_BIT_DEPTH 32
#define Shell_AUDIO_TIMEOUT 2000

/*******************************************************************************
 * STATIC DATA
 ******************************************************************************/

static const Char* Shell_error_caption = "ERROR";
static InputFrame Shell_input = {0};
static Index Shell_input_index = 0;
static Index Shell_char_index = 0;
static S64 Shell_performance_frequency = 0;

static IAudioClient*        Shell_audio_client;
static IAudioRenderClient*  Shell_render_client;
static UINT32               Shell_buffer_size;
static HANDLE               Shell_event_handle;

// @rdk: determine max keycode properly
static S32 Shell_key_table[0xFF] = {
  [ VK_LBUTTON    ] = KEYCODE_MOUSE_LEFT,
  [ VK_RBUTTON    ] = KEYCODE_MOUSE_RIGHT,
  [ VK_ESCAPE     ] = KEYCODE_ESCAPE,
  [ VK_RETURN     ] = KEYCODE_ENTER,
  [ VK_BACK       ] = KEYCODE_BACKSPACE,
  [ VK_TAB        ] = KEYCODE_TAB,
  [ VK_F1         ] = KEYCODE_F1,
  [ VK_F2         ] = KEYCODE_F2,
  [ VK_F3         ] = KEYCODE_F3,
  [ VK_F4         ] = KEYCODE_F4,
  [ VK_F5         ] = KEYCODE_F5,
  [ VK_F6         ] = KEYCODE_F6,
  [ VK_F7         ] = KEYCODE_F7,
  [ VK_F8         ] = KEYCODE_F8,
  [ VK_F9         ] = KEYCODE_F9,
  [ VK_F10        ] = KEYCODE_F10,
  [ VK_F11        ] = KEYCODE_F11,
  [ VK_F12        ] = KEYCODE_F12,
  [ VK_LEFT       ] = KEYCODE_ARROW_LEFT,
  [ VK_RIGHT      ] = KEYCODE_ARROW_RIGHT,
  [ VK_UP         ] = KEYCODE_ARROW_UP,
  [ VK_DOWN       ] = KEYCODE_ARROW_DOWN,
  [ ' '           ] = KEYCODE_SPACE,
  [ 'A'           ] = KEYCODE_A,
  [ 'B'           ] = KEYCODE_B,
  [ 'C'           ] = KEYCODE_C,
  [ 'D'           ] = KEYCODE_D,
  [ 'E'           ] = KEYCODE_E,
  [ 'F'           ] = KEYCODE_F,
  [ 'G'           ] = KEYCODE_G,
  [ 'H'           ] = KEYCODE_H,
  [ 'I'           ] = KEYCODE_I,
  [ 'J'           ] = KEYCODE_J,
  [ 'K'           ] = KEYCODE_K,
  [ 'L'           ] = KEYCODE_L,
  [ 'M'           ] = KEYCODE_M,
  [ 'N'           ] = KEYCODE_N,
  [ 'O'           ] = KEYCODE_O,
  [ 'P'           ] = KEYCODE_P,
  [ 'Q'           ] = KEYCODE_Q,
  [ 'R'           ] = KEYCODE_R,
  [ 'S'           ] = KEYCODE_S,
  [ 'T'           ] = KEYCODE_T,
  [ 'U'           ] = KEYCODE_U,
  [ 'V'           ] = KEYCODE_V,
  [ 'W'           ] = KEYCODE_W,
  [ 'X'           ] = KEYCODE_X,
  [ 'Y'           ] = KEYCODE_Y,
  [ 'Z'           ] = KEYCODE_Z,
  [ '0'           ] = KEYCODE_0,
  [ '1'           ] = KEYCODE_1,
  [ '2'           ] = KEYCODE_2,
  [ '3'           ] = KEYCODE_3,
  [ '4'           ] = KEYCODE_4,
  [ '5'           ] = KEYCODE_5,
  [ '6'           ] = KEYCODE_6,
  [ '7'           ] = KEYCODE_7,
  [ '8'           ] = KEYCODE_8,
  [ '9'           ] = KEYCODE_9,
  [ VK_OEM_PLUS   ] = KEYCODE_PLUS,
  [ VK_OEM_MINUS  ] = KEYCODE_MINUS,
};

static Void Shell_error_message(const Char* message)
{
  MessageBox(NULL, message, Shell_error_caption, MB_ICONERROR);
}

static S64 Shell_query_clock()
{
  LARGE_INTEGER pc;
  QueryPerformanceCounter(&pc);
  return pc.QuadPart * MEGA / Shell_performance_frequency;
}

/*******************************************************************************
 * AUDIO CALLBACKS
 ******************************************************************************/

static Index Shell_audio_acquire_buffer(F32** data)
{
  UINT32 padding = 0;

  HRESULT hr;

  const DWORD wait_status = WaitForSingleObject(Shell_event_handle, Shell_AUDIO_TIMEOUT);
  if (wait_status != WAIT_OBJECT_0) {
    return Shell_EXIT_FAILURE;
  }

  hr = IAudioClient_GetCurrentPadding(Shell_audio_client, &padding);
  if (FAILED(hr)) {
    return Shell_EXIT_FAILURE;
  }

  ASSERT(Shell_buffer_size >= padding);
  const UINT32 _frames = Shell_buffer_size - padding;

  hr = IAudioRenderClient_GetBuffer(Shell_render_client, _frames, (BYTE**) data);
  if (FAILED(hr)) {
    return Shell_EXIT_FAILURE;
  }

  return (Index) _frames;
}

static Void Shell_audio_release_buffer(Index frames)
{
  const U32 uframes = (U32) frames;
  const HRESULT hr = IAudioRenderClient_ReleaseBuffer(Shell_render_client, uframes, 0);
  ASSERT(hr == S_OK); // failed to release buffer
}

static DWORD WINAPI Shell_audio_entry(Void* data)
{
  UNUSED_PARAMETER(data);

  HRESULT hr;

  hr = CoInitializeEx(NULL, COINIT_SPEED_OVER_MEMORY | COINIT_MULTITHREADED);
  if (FAILED(hr)) {
    Shell_error_message("failed to initialize COM");
    return Shell_EXIT_FAILURE;
  }

  DWORD task_index = 0;
  const HANDLE task_handle = AvSetMmThreadCharacteristicsA("Pro Audio", &task_index);
  if (!task_handle) { return 1; }

  IMMDevice* device = NULL;

  IMMDeviceEnumerator* enumerator = NULL;
  hr = CoCreateInstance(
      &CLSID_MMDeviceEnumerator,
      NULL,
      CLSCTX_ALL,
      &IID_IMMDeviceEnumerator,
      &enumerator
      );
  if (FAILED(hr)) {
    Shell_error_message("failed to create audio device enumerator");
    return Shell_EXIT_FAILURE;
  }

  hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(enumerator, eRender, eConsole, &device);
  if (FAILED(hr)) {
    Shell_error_message("failed to get default audio endpoint");
    return Shell_EXIT_FAILURE;
  }
  IMMDeviceEnumerator_Release(enumerator);

  hr = IMMDevice_Activate(device, &IID_IAudioClient, CLSCTX_ALL, NULL, &Shell_audio_client);
  if (FAILED(hr)) {
    Shell_error_message("failed to activate audio device");
    return Shell_EXIT_FAILURE;
  }
  IMMDevice_Release(device);

  WAVEFORMATEXTENSIBLE audio_format;
  audio_format.Format.cbSize = sizeof(audio_format);
  audio_format.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
  audio_format.Format.wBitsPerSample = Shell_AUDIO_BIT_DEPTH;
  audio_format.Format.nChannels = 2;
  audio_format.Format.nSamplesPerSec = (DWORD) Config_AUDIO_SAMPLE_RATE;
  audio_format.Format.nBlockAlign = (WORD) (audio_format.Format.nChannels * audio_format.Format.wBitsPerSample / 8);
  audio_format.Format.nAvgBytesPerSec = audio_format.Format.nSamplesPerSec * audio_format.Format.nBlockAlign;
  audio_format.Samples.wValidBitsPerSample = Shell_AUDIO_BIT_DEPTH;
  audio_format.dwChannelMask = KSAUDIO_SPEAKER_STEREO;
  audio_format.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;

  // @rdk: I don't think IAudioClient_IsFormatSupported is needed with AUTOCONVERTPCM
  WAVEFORMATEX* closest_match = &audio_format.Format;

  REFERENCE_TIME duration;
  hr = IAudioClient_GetDevicePeriod(Shell_audio_client, &duration, NULL);
  if (FAILED(hr)) {
    Shell_error_message("failed to get audio device period");
    return Shell_EXIT_FAILURE;
  }

  hr = IAudioClient_Initialize(
      Shell_audio_client, 
      AUDCLNT_SHAREMODE_SHARED,
      AUDCLNT_STREAMFLAGS_NOPERSIST
      | AUDCLNT_STREAMFLAGS_EVENTCALLBACK
      | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM
      | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
      duration,                                       // buffer duration
      0,                                              // periodicity
      closest_match,
      NULL
      );
  if (FAILED(hr)) {
    Shell_error_message("failed to initialize audio client");
    return Shell_EXIT_FAILURE;
  }

  Shell_event_handle = CreateEvent(NULL, FALSE, FALSE, NULL);
  if (Shell_event_handle == NULL) {
    Shell_error_message("failed to create wasapi event");
    return Shell_EXIT_FAILURE;
  }

  hr = IAudioClient_SetEventHandle(Shell_audio_client, Shell_event_handle);
  if (FAILED(hr)) {
    Shell_error_message("failed to set wasapi event handle");
    return Shell_EXIT_FAILURE;
  }

  hr = IAudioClient_GetService(Shell_audio_client, &IID_IAudioRenderClient, &Shell_render_client);
  if (FAILED(hr)) {
    Shell_error_message("failed to get wasapi render service");
    return Shell_EXIT_FAILURE;
  }

  hr = IAudioClient_GetBufferSize(Shell_audio_client, &Shell_buffer_size);
  if (FAILED(hr)) {
    Shell_error_message("failed to get wasapi buffer size");
    return Shell_EXIT_FAILURE;
  }

  hr = IAudioClient_Start(Shell_audio_client);
  if (FAILED(hr)) {
    Shell_error_message("failed to start audio client");
    return Shell_EXIT_FAILURE;
  }

  sim_init();
  while (1)
  {
    F32* audio_buffer = NULL;
    const Index frames = Shell_audio_acquire_buffer(&audio_buffer);
    if (frames < 0) { break; }
    sim_step(audio_buffer, frames);
    Shell_audio_release_buffer(frames);
  }

  return 0; // unreachable

}

static Void Shell_record_key(KeyCode c, KeyState s) {
  KeyEvent* const event = &Shell_input.events[Shell_input_index];
  event->code = c;
  event->state = s;
  Shell_input_index = (Shell_input_index + 1) % MAX_INPUT_EVENTS;
}

static Void Shell_record_char(Char c)
{
  Shell_input.chars[Shell_char_index] = c;
  Shell_char_index = (Shell_char_index + 1) % MAX_INPUT_EVENTS;
}

static LRESULT CALLBACK Shell_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{

  switch (msg) {

    case WM_DESTROY:
      PostQuitMessage(0);
      break;

    case WM_KEYDOWN:
    case WM_KEYUP:
      {
        const WORD vkcode = LOWORD(wparam);
        const WORD flags = HIWORD(lparam);
        const BOOL upflag = (flags & KF_UP) == KF_UP;
        const KeyState ks = upflag ? KEYSTATE_UP : KEYSTATE_DOWN;
        const KeyCode kc = Shell_key_table[vkcode];
        if (kc != KEYCODE_NONE) {
          const Bool repeat = (flags & KF_REPEAT) == KF_REPEAT;
          if ((ks == KEYSTATE_DOWN && !repeat) || (ks == KEYSTATE_UP)) {
            Shell_record_key(kc, ks);
          }
        }
      } break;

    case WM_CHAR:
      {
        const Char c = (Char) wparam;
        Shell_record_char(c);
      } break;

    case WM_LBUTTONDOWN:
      Shell_record_key(KEYCODE_MOUSE_LEFT, KEYSTATE_DOWN);
      break;
    case WM_LBUTTONUP:
      Shell_record_key(KEYCODE_MOUSE_LEFT, KEYSTATE_UP);
      break;
    case WM_RBUTTONDOWN:
      Shell_record_key(KEYCODE_MOUSE_RIGHT, KEYSTATE_DOWN);
      break;
    case WM_RBUTTONUP:
      Shell_record_key(KEYCODE_MOUSE_RIGHT, KEYSTATE_UP);
      break;

    default:
      return DefWindowProc(hwnd, msg, wparam, lparam);

  }

  return 0;

}

INT WINAPI WinMain(HINSTANCE instance, HINSTANCE prev_instance, PSTR cmdline, INT ncmdshow)
{

  UNUSED_PARAMETER(prev_instance);
  UNUSED_PARAMETER(cmdline);
  UNUSED_PARAMETER(ncmdshow);

  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);

  // set working directory to the parent of the executable
  Char exe_path[MAX_PATH] = {0};
  const U32 exe_length = GetModuleFileName(NULL, exe_path, MAX_PATH);
  if (exe_length == 0) {
    Shell_error_message("failed to get executable filename");
    return Shell_EXIT_FAILURE;
  }
  Char full_path[MAX_PATH] = {0};
  Char* file_part = NULL;
  const U32 path_length = GetFullPathName(exe_path, MAX_PATH, full_path, &file_part);
  if (path_length == 0) {
    Shell_error_message("failed to get executable directory");
    return Shell_EXIT_FAILURE;
  }
  Char exe_dir[MAX_PATH] = {0};
  strncpy(exe_dir, full_path, file_part - full_path);
  const Bool cd_status = SetCurrentDirectory(exe_dir);
  if (cd_status == 0) {
    Shell_error_message("failed to set working directory");
    return Shell_EXIT_FAILURE;
  }

  /*******************************************************************************
   * AUDIO INITIALIZATION
   ******************************************************************************/

  DWORD audio_thread_id = 0;
  HANDLE h = CreateThread( 
      NULL,               // default security attributes
      0,                  // default stack size
      Shell_audio_entry,  // entry point
      NULL,
      0,                  // default creation flags
      &audio_thread_id    // receive thread identifier
      );
  if (h == NULL) {
    Shell_error_message("failed to start audio thread");
    return Shell_EXIT_FAILURE;
  }
  SetThreadPriority(h, THREAD_PRIORITY_HIGHEST);

  /*******************************************************************************
   * WINDOW INITIALIZATION
   ******************************************************************************/

  V2S primary_display;
  primary_display.x = GetSystemMetrics(SM_CXSCREEN);
  primary_display.y = GetSystemMetrics(SM_CYSCREEN);
  ASSERT(primary_display.x != 0);
  ASSERT(primary_display.y != 0);

  V2S v_scale;
  v_scale.x = primary_display.x / 320;
  v_scale.y = primary_display.y / 180;
  const S32 scale = MIN(v_scale.x, v_scale.y) - 1;

  const V2S render_dims = v2s_scale(v2s(320, 180), scale);

  WNDCLASSEX wc = {0};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = Shell_wnd_proc;
  wc.hInstance = instance;
  wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.lpszClassName = Shell_CLASS_NAME;

  const ATOM class_id = RegisterClassEx(&wc);
  if (class_id == 0) {
    Shell_error_message("failed to register window class");
    return Shell_EXIT_FAILURE;
  }

  RECT client_rect = {0};
  client_rect.left = 0;
  client_rect.top = 0;
  client_rect.right = render_dims.x;
  client_rect.bottom = render_dims.y;

  const DWORD style = WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME ^ WS_MAXIMIZEBOX;
  const DWORD exstyle = WS_EX_APPWINDOW;

  const BOOL adjust_result = AdjustWindowRectEx(&client_rect, style, FALSE, exstyle);
  if (adjust_result == FALSE) {
    Shell_error_message("failed to adjust window client area");
    return Shell_EXIT_FAILURE;
  }

  V2S window_size;
  window_size.x = client_rect.right - client_rect.left;
  window_size.y = client_rect.bottom - client_rect.top;

  V2S window_pos;
  window_pos.x = (primary_display.x - window_size.x) / 2;
  window_pos.y = (primary_display.y - window_size.y) / 2;

#if 0
  const HWND hwnd = CreateWindowEx(
      0,                              // optional window styles
      Shell_CLASS_NAME,               // window class
      Shell_TITLE,                    // window text
      WS_POPUP,   // window style
      0, 0,       // position
      primary_display.x + 0, primary_display.y + 0, // size
      NULL,       // parent window
      NULL,       // menu
      instance,   // instance handle
      NULL        // additional application data
      );
#else
  const HWND hwnd = CreateWindowEx(
      exstyle,
      Shell_CLASS_NAME,
      Shell_TITLE,
      style,
      // CW_USEDEFAULT,
      // 0,
      window_pos.x,
      window_pos.y,
      window_size.x,
      window_size.y,
      NULL,
      NULL,
      instance,
      NULL
      );
#endif
  if (hwnd == NULL) {
    Shell_error_message("failed to create window");
    return Shell_EXIT_FAILURE;
  }

  const HDC hdc = GetDC(hwnd);
  if (hdc == NULL) {
    Shell_error_message("failed to get device context");
    return Shell_EXIT_FAILURE;
  }

  PIXELFORMATDESCRIPTOR pfd = {0};
  pfd.nSize = sizeof(pfd);
  pfd.dwFlags = PFD_DOUBLEBUFFER | PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW;
  pfd.iPixelType = PFD_TYPE_RGBA;
  pfd.cColorBits = 32;
  pfd.cDepthBits = 32;
  pfd.iLayerType = PFD_MAIN_PLANE;

  const S32 format = ChoosePixelFormat(hdc, &pfd);
  if (format == 0) {
    Shell_error_message("failed to choose pixel format");
    return Shell_EXIT_FAILURE;
  }

  const BOOL set_pixel_format_result = SetPixelFormat(hdc, format, &pfd);
  if (set_pixel_format_result == FALSE) {
    Shell_error_message("failed to set pixel format");
    return Shell_EXIT_FAILURE;
  }

  const HGLRC bootstrap = wglCreateContext(hdc);
  if (bootstrap == NULL) {
    Shell_error_message("failed to create bootstrap OpenGL context");
    return Shell_EXIT_FAILURE;
  }

  const BOOL bootstrap_current = wglMakeCurrent(hdc, bootstrap);
  if (bootstrap_current == FALSE) {
    Shell_error_message("failed to activate bootstrap OpenGL context");
    return Shell_EXIT_FAILURE;
  }

  const S32 glad_wgl_version = gladLoaderLoadWGL(hdc);
  if (glad_wgl_version == 0) {
    Shell_error_message("GLAD WGL loader failed");
    return Shell_EXIT_FAILURE;
  }

  if (GLAD_WGL_ARB_create_context == 0) {
    Shell_error_message("missing required extension: WGL_ARB_create_context");
    return Shell_EXIT_FAILURE;
  }

  if (GLAD_WGL_ARB_create_context_profile == 0) {
    Shell_error_message("missing required extension: WGL_ARB_create_context_profile");
    return Shell_EXIT_FAILURE;
  }

  S32 wgl_attributes[] = {
    WGL_CONTEXT_MAJOR_VERSION_ARB, Shell_GL_VERSION_MAJOR,
    WGL_CONTEXT_MINOR_VERSION_ARB, Shell_GL_VERSION_MINOR,
    WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
#if 0
    WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
    WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_DEBUG_BIT_ARB,
#endif
    0 // end
  };

  const HGLRC glctx = wglCreateContextAttribsARB(hdc, NULL, wgl_attributes);
  if (glctx == NULL) {
    Shell_error_message("failed to create OpenGL context");
    return Shell_EXIT_FAILURE;
  }

  const BOOL clear_context = wglMakeCurrent(NULL, NULL);
  if (clear_context == FALSE) {
    Shell_error_message("failed to deactivate bootstrap OpenGL context");
    return Shell_EXIT_FAILURE;
  }

  const BOOL deleted_context = wglDeleteContext(bootstrap);
  if (deleted_context == FALSE) {
    Shell_error_message("failed to delete OpenGL context");
    return Shell_EXIT_FAILURE;
  }

  const BOOL glctx_current = wglMakeCurrent(hdc, glctx);
  if (glctx_current == FALSE) {
    Shell_error_message("failed to activate OpenGL context");
    return Shell_EXIT_FAILURE;
  }

  const S32 gl_version = gladLoaderLoadGL();
  if (gl_version == 0) {
    Shell_error_message("GLAD loader failed");
    return Shell_EXIT_FAILURE;
  }

  if (GLAD_WGL_EXT_swap_control) {
    const BOOL vsync_status = wglSwapIntervalEXT(1);
    UNUSED_PARAMETER(vsync_status);
  }

  glClearColor(0.f, 0.f, 0.f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT);
  SwapBuffers(hdc);
  ShowWindow(hwnd, SW_SHOW);

  LARGE_INTEGER frequency;
  QueryPerformanceFrequency(&frequency);
  Shell_performance_frequency = frequency.QuadPart;

  render_init(render_dims);

  S64 now = Shell_query_clock();

  Bool live = true;
  S64 then, render = 0;
  while (live) {

    then = now;
    now = Shell_query_clock();

    Shell_input = (InputFrame) {0};
    Shell_input_index = 0;

    POINT mouse = {0};
    GetCursorPos(&mouse);
    ScreenToClient(hwnd, &mouse);
    Shell_input.mouse.x = mouse.x;
    Shell_input.mouse.y = mouse.y;

    MSG msg = {0};
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) {
        live = false;
      }
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }

    process_input(&Shell_input);
    render_frame();

    glFinish();
    render = Shell_query_clock() - now;

    SwapBuffers(hdc);
    glFinish();

  }

  return 0;

}
