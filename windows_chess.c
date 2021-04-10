#include "chess.c"
#include <windowsx.h>

typedef union StatusData
{
    char*text;
    uint8_t status;
} StatusData;

StatusData g_status_data;
HWND g_dialog_handle;
DWORD g_dialog_style = WS_OVERLAPPED | WS_CAPTION | WS_MINIMIZEBOX;

void resize_window(Window*window, uint32_t width, uint32_t height)
{
    window->width = width;
    window->height = height;
    size_t new_pixel_buffer_capacity =
        GetSystemMetrics(SM_CXVIRTUALSCREEN) * GetSystemMetrics(SM_CXVIRTUALSCREEN);
    if (new_pixel_buffer_capacity > window->pixel_buffer_capacity)
    {
        VirtualFree(window->pixels, 0, MEM_RELEASE);
        window->pixels = VirtualAlloc(0, sizeof(Color) * new_pixel_buffer_capacity,
            MEM_COMMIT, PAGE_READWRITE);
        window->pixel_buffer_capacity = new_pixel_buffer_capacity;
    }
}

void render_window(Window*window, HWND window_handle)
{
    HDC device_context = GetDC(window_handle);
    BITMAPINFO bitmap_info;
    bitmap_info.bmiHeader.biSize = sizeof(bitmap_info.bmiHeader);
    bitmap_info.bmiHeader.biPlanes = 1;
    bitmap_info.bmiHeader.biBitCount = 8 * sizeof(Color);
    bitmap_info.bmiHeader.biCompression = BI_RGB;
    bitmap_info.bmiHeader.biWidth = window->width;
    bitmap_info.bmiHeader.biHeight = -(int32_t)window->height;
    StretchDIBits(device_context, 0, 0, window->width, window->height, 0, 0, window->width,
        window->height, window->pixels, &bitmap_info, DIB_RGB_COLORS, SRCCOPY);
    ValidateRect(window_handle, 0);
    ReleaseDC(window_handle, device_context);
}

void center_window(Window*window, HWND window_handle, DWORD window_style)
{
    RECT screen_rect;
    GetWindowRect(GetDesktopWindow(), &screen_rect);
    RECT window_rect = { .top = 0, .left = 0, .bottom = window->height, .right = window->width };
    AdjustWindowRect(&window_rect, window_style, false);
    SetWindowPos(window_handle, HWND_TOP, (screen_rect.right - window->width) / 2,
        (screen_rect.bottom - window->height) / 2, window_rect.right - window_rect.left,
        window_rect.bottom - window_rect.top, SWP_SHOWWINDOW);
}

void draw_and_render_main_window(HWND window_handle)
{
    draw_main_window();
    render_window(g_windows + WINDOW_MAIN, window_handle);
}

void run_message_loop(HWND main_window_handle);

void run_dialog(Window*dialog, HWND dialog_handle, HWND main_window_handle)
{
    g_dialog_handle = dialog_handle;
    dialog->hovered_control_id = NULL_CONTROL;
    dialog->clicked_control_id = NULL_CONTROL;
    center_window(dialog, dialog_handle, g_dialog_style);
    run_message_loop(main_window_handle);
    VirtualFree(dialog->pixels, 0, MEM_RELEASE);
    g_dialog_handle = 0;
}

char g_start_window_class_name[] = "Start";

void run_game_over_dialog(HWND main_window_handle)
{
    KillTimer(main_window_handle, 1);
    if (g_dialog_handle)
    {
        DestroyWindow(g_dialog_handle);
        VirtualFree(g_windows[WINDOW_PROMOTION].pixels, 0, MEM_RELEASE);
    }
    free_game();
    HWND dialog_handle = CreateWindowEx(WS_EX_TOPMOST, g_start_window_class_name, "Game Over",
        g_dialog_style, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, 0, 0);
    init_start_window(g_status_data.text, GetDpiForWindow(dialog_handle));
    run_dialog(g_windows + WINDOW_START, dialog_handle, main_window_handle);
    BringWindowToTop(main_window_handle);
    switch (g_status_data.status)
    {
    case START_NEW_GAME:
    {
        draw_and_render_main_window(main_window_handle);
    }
    case START_LOAD_GAME:
    {
        SetTimer(main_window_handle, 1, USER_TIMER_MINIMUM, 0);
        return;
    }
    case START_QUIT:
    {
        PostQuitMessage(0);
    }
    }
}

void handle_turn_action(HWND main_window_handle, GUIAction action)
{
    if (action == ACTION_NONE)
    {
        return;
    }
    draw_and_render_main_window(main_window_handle);
    switch (action)
    {
    case ACTION_CHECKMATE:
    {
        if (g_position_pool[g_current_position_index].active_player_index == PLAYER_INDEX_WHITE)
        {
            g_status_data.text = "Checkmate. Black wins.";
        }
        else
        {
            g_status_data.text = "Checkmate. White wins.";
        }
        break;
    }
    case ACTION_FLAG:
    {
        if (g_position_pool[g_current_position_index].active_player_index == PLAYER_INDEX_WHITE)
        {
            g_status_data.text = "White is out of time. Black wins.";
        }
        else
        {
            g_status_data.text = "Black is out of time. White wins.";
        }
        break;
    }
    case ACTION_REPETITION_DRAW:
    {
        g_status_data.text = "Draw by repetition.";
        break;
    }
    case ACTION_STALEMATE:
    {
        g_status_data.text = "Stalemate.";
        break;
    }
    default:
    {
        return;
    }
    }
    run_game_over_dialog(main_window_handle);
}

void run_message_loop(HWND main_window_handle)
{
    while (true)
    {
        MSG message;
        if (g_run_engine)
        {
            while (PeekMessage(&message, 0, 0, 0, PM_REMOVE))
            {
                if (message.message == WM_QUIT)
                {
                    return;
                }
                TranslateMessage(&message);
                DispatchMessage(&message);
            }
            handle_turn_action(main_window_handle, do_engine_iteration());
        }
        else
        {
            if (GetMessage(&message, 0, 0, 0))
            {
                TranslateMessage(&message);
                DispatchMessage(&message);
            }
            else
            {
                return;
            }
        }
    }
}

char g_setup_window_class_name[] = "Set Up Game";

void draw_and_render_setup_window(HWND window_handle)
{
    Window*window = g_windows + WINDOW_SETUP;
    draw_controls(window);
    render_window(window, window_handle);
}

LRESULT CALLBACK setup_proc(HWND window_handle, UINT message, WPARAM w_param, LPARAM l_param)
{
    switch (message)
    {
    case WM_LBUTTONDOWN:
    {
        SetCapture(window_handle);
        if (handle_left_mouse_button_down(g_windows + WINDOW_SETUP, GET_X_LPARAM(l_param),
            GET_Y_LPARAM(l_param)))
        {
            draw_and_render_setup_window(window_handle);
        }
        return 0;
    }
    case WM_LBUTTONUP:
    {
        ReleaseCapture();
        Window*window = g_windows + WINDOW_SETUP;
        uint8_t old_clicked_control_id = window->clicked_control_id;
        if (setup_window_handle_left_mouse_button_up(window, GET_X_LPARAM(l_param),
            GET_Y_LPARAM(l_param)))
        {
            DestroyWindow(window_handle);
            PostQuitMessage(0);
        }
        else if (window->clicked_control_id != old_clicked_control_id)
        {
            draw_and_render_setup_window(window_handle);
        }
        return 0;
    }
    case WM_KEYDOWN:
    {
        if (w_param < 0x30 || w_param > 0x39)
        {
            switch (w_param)
            {
            case VK_BACK:
            case VK_DELETE:
            case VK_LEFT:
            case VK_RIGHT:
            {
                break;
            }
            default:
            {
                return 0;
            }
            }
        }
        if (setup_window_handle_key_down(g_windows + WINDOW_SETUP, w_param))
        {
            draw_and_render_setup_window(window_handle);
        }
        return 0;
    }
    case WM_MOUSEMOVE:
    {
        if (handle_mouse_move(g_windows + WINDOW_PROMOTION, GET_X_LPARAM(l_param),
            GET_Y_LPARAM(l_param)))
        {
            draw_and_render_setup_window(window_handle);
        }
        return 0;
    }
    case WM_PAINT:
    {
        draw_and_render_setup_window(window_handle);
        return 0;
    }
    case WM_SIZE:
    {
        resize_window(g_windows + WINDOW_SETUP, LOWORD(l_param), HIWORD(l_param));
        return 0;
    }
    default:
    {
        return DefWindowProc(window_handle, message, w_param, l_param);
    }
    }
}

void draw_and_render_start_window(HWND window_handle)
{
    draw_start_window(g_status_data.text);
    render_window(g_windows + WINDOW_START, window_handle);
}

LRESULT CALLBACK start_window_proc(HWND window_handle, UINT message, WPARAM w_param, LPARAM l_param)
{
    switch (message)
    {
    case WM_LBUTTONDOWN:
    {
        SetCapture(window_handle);
        if (handle_left_mouse_button_down(g_windows + WINDOW_START, GET_X_LPARAM(l_param),
            GET_Y_LPARAM(l_param)))
        {
            draw_and_render_start_window(window_handle);
        }
        return 0;
    }
    case WM_LBUTTONUP:
    {
        ReleaseCapture();
        Window*window = g_windows + WINDOW_START;
        uint8_t old_clicked_control_id = window->clicked_control_id;
        uint8_t id = handle_left_mouse_button_up(window, GET_X_LPARAM(l_param),
            GET_Y_LPARAM(l_param));
        if (id == window->controls[START_LOAD_GAME].base_id)
        {
            char file_name[256];
            OPENFILENAMEA open_file_name = { 0 };
            open_file_name.lStructSize = sizeof(open_file_name);
            open_file_name.hwndOwner = window_handle;
            open_file_name.lpstrFile = file_name;
            open_file_name.nMaxFile = ARRAY_COUNT(file_name);
            open_file_name.lpstrDefExt = "chs";
            if (GetOpenFileNameA(&open_file_name))
            {
                HANDLE file_handle = CreateFileA(open_file_name.lpstrFile, GENERIC_READ,
                    FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
                if (file_handle != INVALID_HANDLE_VALUE)
                {
                    LARGE_INTEGER file_size;
                    if (GetFileSizeEx(file_handle, &file_size))
                    {
                        void*file_memory =
                            VirtualAlloc(0, file_size.QuadPart, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
                        if (file_memory)
                        {
                            DWORD bytes_read;
                            if (ReadFile(file_handle, file_memory, (DWORD)file_size.QuadPart,
                                &bytes_read, 0))
                            {
                                if (bytes_read == file_size.QuadPart)
                                {
                                    CloseHandle(file_handle);
                                    load_game(file_memory, file_size.QuadPart);
                                    g_status_data.status = id;
                                    DestroyWindow(window_handle);
                                    PostQuitMessage(0);
                                    return 0;
                                }
                            }
                            VirtualFree(file_memory, 0, MEM_RELEASE);
                        }
                    }
                    CloseHandle(file_handle);
                }
            }
        }
        else if (id == window->controls[START_NEW_GAME].base_id)
        {
            DestroyWindow(window_handle);
            HWND dialog_handle = CreateWindowEx(WS_EX_TOPMOST, g_setup_window_class_name,
                g_setup_window_class_name, g_dialog_style, CW_USEDEFAULT, CW_USEDEFAULT,
                CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, 0, 0);
            init_setup_window(GetDpiForWindow(dialog_handle));
            run_dialog(g_windows + WINDOW_SETUP, dialog_handle, 0);
            PostQuitMessage(0);
            g_status_data.status = id;
            init_game();
            return 0;
        }
        else if (id == window->controls[START_QUIT].base_id)
        {
            g_status_data.status = id;
            DestroyWindow(window_handle);
            PostQuitMessage(0);
            return 0;
        }
        if (id != old_clicked_control_id)
        {
            draw_and_render_start_window(window_handle);
        }
        return 0;
    }
    case WM_DPICHANGED:
    {
        free_dpi_data(g_windows + WINDOW_START);
        set_start_window_dpi(g_status_data.text, HIWORD(w_param));
        RECT*client_rect = (RECT*)l_param;
        SetWindowPos(window_handle, 0, client_rect->left, client_rect->top,
            client_rect->right - client_rect->left, client_rect->bottom - client_rect->top,
            SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOZORDER);
        return 0;
    }
    case WM_MOUSEMOVE:
    {
        if (handle_mouse_move(g_windows + WINDOW_START, GET_X_LPARAM(l_param),
            GET_Y_LPARAM(l_param)))
        {
            draw_and_render_start_window(window_handle);
        }
        return 0;
    }
    case WM_PAINT:
    {
        draw_and_render_start_window(window_handle);
        return 0;
    }
    case WM_SIZE:
    {
        resize_window(g_windows + WINDOW_START, LOWORD(l_param), HIWORD(l_param));
        return 0;
    }
    default:
    {
        return DefWindowProc(window_handle, message, w_param, l_param);
    }
    }
}

LRESULT CALLBACK main_window_proc(HWND window_handle, UINT message, WPARAM w_param, LPARAM l_param)
{
    switch (message)
    {
    case WM_DPICHANGED:
    {
        free_dpi_data(g_windows + WINDOW_MAIN);
        set_main_window_dpi(HIWORD(w_param));
        RECT*client_rect = (RECT*)l_param;
        SetWindowPos(window_handle, 0, client_rect->left, client_rect->top,
            client_rect->right - client_rect->left, client_rect->bottom - client_rect->top,
            SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOZORDER);
        return 0;
    }
    case WM_PAINT:
    {
        draw_and_render_main_window(window_handle);
        return 0;
    }
    case WM_SIZE:
    {
        resize_window(g_windows + WINDOW_MAIN, LOWORD(l_param), HIWORD(l_param));
        return 0;
    }
    case WM_TIMER:
    {
        UpdateTimerStatus status = update_timer();
        if (status & UPDATE_TIMER_REDRAW)
        {
            draw_and_render_main_window(window_handle);
        }
        if (status & UPDATE_TIMER_TIME_OUT)
        {
            if (g_position_pool[g_current_position_index].active_player_index == PLAYER_INDEX_WHITE)
            {
                g_status_data.text = "White is out of time. Black wins.";
            }
            else
            {
                g_status_data.text = "Black is out of time. White wins.";
            }
            run_game_over_dialog(window_handle);
        }
        return 0;
    }
    }
    if (g_dialog_handle)
    {
        return DefWindowProc(window_handle, message, w_param, l_param);
    }
    switch (message)
    {
    case WM_DESTROY:
    {
        PostQuitMessage(0);
        return 0;
    }
    case WM_LBUTTONDOWN:
    {
        SetCapture(window_handle);
        if (main_window_handle_left_mouse_button_down(GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)))
        {
            draw_and_render_main_window(window_handle);
        }
        return 0;
    }
    case WM_LBUTTONUP:
    {
        ReleaseCapture();
        GUIAction action =
            main_window_handle_left_mouse_button_up(GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param));
        if (action == ACTION_SAVE_GAME)
        {
            draw_and_render_main_window(window_handle);
            char file_name[256];
            OPENFILENAMEA open_file_name = { 0 };
            open_file_name.lStructSize = sizeof(open_file_name);
            open_file_name.hwndOwner = window_handle;
            open_file_name.lpstrFile = file_name;
            open_file_name.nMaxFile = ARRAY_COUNT(file_name);
            open_file_name.lpstrDefExt = "chs";
            if (GetSaveFileNameA(&open_file_name))
            {
                HANDLE file_handle =
                    CreateFileA(open_file_name.lpstrFile, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
                if (file_handle != INVALID_HANDLE_VALUE)
                {
                    void*file_memory = VirtualAlloc(0, SAVE_FILE_STATIC_PART_SIZE +
                        g_unique_state_count * sizeof(BoardState),
                        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
                    uint32_t file_size = save_game(file_memory);
                    DWORD bytes_written;
                    WriteFile(file_handle, file_memory, file_size, &bytes_written, 0);
                    VirtualFree(file_memory, 0, MEM_RELEASE);
                    CloseHandle(file_handle);
                }
            }
        }
        else
        {
            handle_turn_action(window_handle, action);
        }
        return 0;
    }
    case WM_MOUSEMOVE:
    {
        if (handle_mouse_move(g_windows + WINDOW_MAIN, GET_X_LPARAM(l_param),
            GET_Y_LPARAM(l_param)))
        {
            draw_and_render_main_window(window_handle);
        }
        return 0;
    }
    default:
    {
        return DefWindowProc(window_handle, message, w_param, l_param);
    }
    }
}

HWND windows_init(void)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);
    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);
    g_page_size = system_info.dwPageSize;
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    g_counts_per_second = frequency.QuadPart;
    WNDCLASSEX wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.hCursor = LoadCursor(0, IDC_ARROW);
    wc.lpfnWndProc = setup_proc;
    wc.lpszClassName = g_setup_window_class_name;
    RegisterClassEx(&wc);
    wc.lpfnWndProc = start_window_proc;
    wc.lpszClassName = g_start_window_class_name;
    RegisterClassEx(&wc);
    HWND dialog_handle = CreateWindow(g_start_window_class_name, g_start_window_class_name,
        g_dialog_style, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, 0, 0);
    uint16_t dpi = GetDpiForWindow(dialog_handle);
    NONCLIENTMETRICSW non_client_metrics;
    non_client_metrics.cbSize = sizeof(NONCLIENTMETRICSW);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, non_client_metrics.cbSize, &non_client_metrics,
        0);
    g_font_size = ((-non_client_metrics.lfMessageFont.lfHeight * 72) << 6) / dpi;
    HFONT win_font = CreateFont(0, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
        OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, 0, "Ariel");
    HDC device_context = GetDC(dialog_handle);
    SelectFont(device_context, win_font);
    size_t text_font_data_size = GetFontData(device_context, 0, 0, 0, 0);
    char exe_path[256];
    size_t exe_path_length =
        GetModuleFileNameA(0, exe_path, sizeof(exe_path)) + 1 - sizeof("windows_chess.exe");
    strcpy(exe_path + exe_path_length, "piece_icons.ttf");
    HANDLE file_handle =
        CreateFileA(exe_path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    LARGE_INTEGER icon_font_file_size;
    GetFileSizeEx(file_handle, &icon_font_file_size);
    void*font_data = VirtualAlloc(0, text_font_data_size + icon_font_file_size.QuadPart,
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    GetFontData(device_context, 0, 0, font_data, text_font_data_size);
    ReleaseDC(dialog_handle, device_context);
    DeleteObject(win_font);
    ReadFile(file_handle, (void*)((uintptr_t)font_data + text_font_data_size),
        icon_font_file_size.QuadPart, 0, 0);
    CloseHandle(file_handle);
    init(font_data, text_font_data_size, icon_font_file_size.QuadPart);
    g_status_data.text = "";
    init_start_window(g_status_data.text, GetDpiForWindow(dialog_handle));
    run_dialog(g_windows + WINDOW_START, dialog_handle, 0);
    if (g_status_data.status == START_QUIT)
    {
        return 0;
    }
    wc.lpfnWndProc = main_window_proc;
    wc.lpszClassName = "Chess";
    RegisterClassEx(&wc);
    g_dialog_handle = 0;
    HWND window_handle = CreateWindow(wc.lpszClassName, "Chess", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, 0, 0);
    init_main_window(dpi);
    center_window(g_windows + WINDOW_MAIN, window_handle, WS_OVERLAPPEDWINDOW);
    SetTimer(window_handle, 1, USER_TIMER_MINIMUM, 0);
    return window_handle;
}

int WINAPI wWinMain(HINSTANCE instance_handle, HINSTANCE previous_instance_handle,
    PWSTR command_line, int show)
{
    HWND main_window_handle = windows_init();
    if (main_window_handle)
    {
        run_message_loop(main_window_handle);
    }
    return 0;
}