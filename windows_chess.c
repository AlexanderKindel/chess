#include "chess.c"
#include <windowsx.h>

typedef struct WindowsGame
{
    Game game;
    union
    {
        char*text;
        uint8_t status;
    };
    HWND dialog_handle;
} WindowsGame;

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

void draw_and_render_main_window(WindowsGame*game, HWND window_handle)
{
    draw_main_window(&game->game);
    render_window(game->game.windows + WINDOW_MAIN, window_handle);
}

void run_message_loop(WindowsGame*game, HWND main_window_handle);

void run_dialog(WindowsGame*game, Window*dialog, HWND dialog_handle, HWND main_window_handle)
{
    game->dialog_handle = dialog_handle;
    SetWindowLongPtrW(dialog_handle, GWLP_USERDATA, (LONG_PTR)game);
    dialog->hovered_control_id = NULL_CONTROL;
    dialog->clicked_control_id = NULL_CONTROL;
    center_window(dialog, dialog_handle, g_dialog_style);
    run_message_loop(game, main_window_handle);
    VirtualFree(dialog->pixels, 0, MEM_RELEASE);
    game->dialog_handle = 0;
}

char g_start_window_class_name[] = "Start";

void run_game_over_dialog(WindowsGame*game, HWND main_window_handle)
{
    KillTimer(main_window_handle, 1);
    if (game->dialog_handle)
    {
        DestroyWindow(game->dialog_handle);
        VirtualFree(game->game.windows[WINDOW_PROMOTION].pixels, 0, MEM_RELEASE);
    }
    free_game(&game->game);
    HWND dialog_handle = CreateWindowEx(WS_EX_TOPMOST, g_start_window_class_name, "Game Over",
        g_dialog_style, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, 0, 0);
    init_start_window(&game->game, game->text, GetDpiForWindow(dialog_handle));
    run_dialog(game, game->game.windows + WINDOW_START, dialog_handle, main_window_handle);
    BringWindowToTop(main_window_handle);
    switch (game->status)
    {
    case START_NEW_GAME:
    {
        draw_and_render_main_window(game, main_window_handle);
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

void handle_game_over_status(WindowsGame*game, HWND main_window_handle)
{
    switch (game->status)
    {
    case STATUS_CHECKMATE:
    {
        if (game->game.position_pool[game->game.current_position_index].active_player_index ==
            WHITE_PLAYER_INDEX)
        {
            game->text = "Checkmate. White wins.";
        }
        else
        {
            game->text = "Checkmate. Black wins.";
        }
        break;
    }
    case STATUS_REPETITION:
    {
        game->text = "Draw by repetition.";
        break;
    }
    case STATUS_STALEMATE:
    {
        game->text = "Stalemate.";
        break;
    }
    case STATUS_TIME_OUT:
    {
        if (game->game.position_pool[game->game.current_position_index].active_player_index ==
            WHITE_PLAYER_INDEX)
        {
            game->text = "White is out of time. Black wins.";
        }
        else
        {
            game->text = "Black is out of time. White wins.";
        }
        break;
    }
    }
    run_game_over_dialog(game, main_window_handle);
}

void run_message_loop(WindowsGame*game, HWND main_window_handle)
{
    while (true)
    {
        MSG message;
        if (game->game.run_engine)
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
            game->status = do_engine_iteration(&game->game);
            if (game->status != STATUS_CONTINUE)
            {
                draw_and_render_main_window(game, main_window_handle);
                if (game->status != STATUS_END_TURN)
                {
                    handle_game_over_status(game, main_window_handle);
                }
            }
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

char g_time_control_window_class_name[] = "Set Time Control";

void draw_and_render_time_control_window(WindowsGame*game, HWND window_handle)
{
    Window*window = game->game.windows + WINDOW_TIME_CONTROL;
    draw_controls(&game->game, window);
    render_window(window, window_handle);
}

LRESULT CALLBACK time_control_proc(HWND window_handle, UINT message, WPARAM w_param, LPARAM l_param)
{
    switch (message)
    {
    case WM_LBUTTONDOWN:
    {
        SetCapture(window_handle);
        WindowsGame*game = (WindowsGame*)GetWindowLongPtrW(window_handle, GWLP_USERDATA);
        if (handle_left_mouse_button_down(game->game.windows + WINDOW_TIME_CONTROL,
            GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)))
        {
            draw_and_render_time_control_window(game, window_handle);
        }
        return 0;
    }
    case WM_LBUTTONUP:
    {
        ReleaseCapture();
        WindowsGame*game = (WindowsGame*)GetWindowLongPtrW(window_handle, GWLP_USERDATA);
        Window*window = game->game.windows + WINDOW_TIME_CONTROL;
        uint8_t old_clicked_control_id = window->clicked_control_id;
        if (time_control_window_handle_left_mouse_button_up(&game->game, window,
            GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)))
        {
            DestroyWindow(window_handle);
            PostQuitMessage(0);
        }
        else if (window->clicked_control_id != old_clicked_control_id)
        {
            draw_and_render_time_control_window(game, window_handle);
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
        WindowsGame*game = (WindowsGame*)GetWindowLongPtrW(window_handle, GWLP_USERDATA);
        if (time_control_window_handle_key_down(&game->game,
            game->game.windows + WINDOW_TIME_CONTROL, w_param))
        {
            draw_and_render_time_control_window(game, window_handle);
        }
        return 0;
    }
    case WM_MOUSEMOVE:
    {
        WindowsGame*game = (WindowsGame*)GetWindowLongPtrW(window_handle, GWLP_USERDATA);
        if (handle_mouse_move(game->game.windows + WINDOW_PROMOTION, GET_X_LPARAM(l_param),
            GET_Y_LPARAM(l_param)))
        {
            draw_and_render_time_control_window(game, window_handle);
        }
        return 0;
    }
    case WM_PAINT:
    {
        draw_and_render_time_control_window(
            (WindowsGame*)GetWindowLongPtrW(window_handle, GWLP_USERDATA), window_handle);
        return 0;
    }
    case WM_SIZE:
    {
        resize_window(((WindowsGame*)GetWindowLongPtrW(window_handle, GWLP_USERDATA))->
            game.windows + WINDOW_TIME_CONTROL, LOWORD(l_param), HIWORD(l_param));
        return 0;
    }
    default:
    {
        return DefWindowProc(window_handle, message, w_param, l_param);
    }
    }
}

char g_promotion_window_class_name[] = "Choose Promotion Type";

void draw_and_render_promotion_window(WindowsGame*game, HWND window_handle)
{
    draw_promotion_window(&game->game);
    render_window(game->game.windows + WINDOW_PROMOTION, window_handle);
}

LRESULT CALLBACK promotion_proc(HWND window_handle, UINT message, WPARAM w_param, LPARAM l_param)
{
    switch (message)
    {
    case WM_LBUTTONDOWN:
    {
        SetCapture(window_handle);
        WindowsGame*game = (WindowsGame*)GetWindowLongPtrW(window_handle, GWLP_USERDATA);
        if (handle_left_mouse_button_down(game->game.windows + WINDOW_PROMOTION,
            GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)))
        {
            draw_and_render_promotion_window(game, window_handle);
        }
        return 0;
    }
    case WM_LBUTTONUP:
    {
        ReleaseCapture();
        WindowsGame*game = (WindowsGame*)GetWindowLongPtrW(window_handle, GWLP_USERDATA);
        Window*window = game->game.windows + WINDOW_PROMOTION;
        uint8_t old_clicked_control_id = window->clicked_control_id;
        uint8_t id =
            handle_left_mouse_button_up(window, GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param));
        if (id == NULL_CONTROL)
        {
            if (old_clicked_control_id != NULL_CONTROL)
            {
                draw_and_render_promotion_window(game, window_handle);
            }
        }
        else
        {
            game->status = g_promotion_options[id];
            DestroyWindow(window_handle);
            PostQuitMessage(0);
        }
        return 0;
    }
    case WM_MOUSEMOVE:
    {
        WindowsGame*game = (WindowsGame*)GetWindowLongPtrW(window_handle, GWLP_USERDATA);
        if (handle_mouse_move(game->game.windows + WINDOW_PROMOTION, GET_X_LPARAM(l_param),
            GET_Y_LPARAM(l_param)))
        {
            draw_and_render_promotion_window(game, window_handle);
        }
        return 0;
    }
    case WM_PAINT:
    {
        draw_and_render_promotion_window(
            (WindowsGame*)GetWindowLongPtrW(window_handle, GWLP_USERDATA), window_handle);
        return 0;
    }
    case WM_SIZE:
    {
        resize_window(((WindowsGame*)GetWindowLongPtrW(window_handle, GWLP_USERDATA))->
            game.windows + WINDOW_PROMOTION, LOWORD(l_param), HIWORD(l_param));
        return 0;
    }
    default:
    {
        return DefWindowProc(window_handle, message, w_param, l_param);
    }
    }
}

void draw_and_render_start_window(WindowsGame*game, HWND window_handle)
{
    draw_start_window(&game->game, game->text);
    render_window(game->game.windows + WINDOW_START, window_handle);
}

LRESULT CALLBACK start_window_proc(HWND window_handle, UINT message, WPARAM w_param, LPARAM l_param)
{
    switch (message)
    {
    case WM_LBUTTONDOWN:
    {
        SetCapture(window_handle);
        WindowsGame*game = (WindowsGame*)GetWindowLongPtrW(window_handle, GWLP_USERDATA);
        if (handle_left_mouse_button_down(game->game.windows + WINDOW_START, GET_X_LPARAM(l_param),
            GET_Y_LPARAM(l_param)))
        {
            draw_and_render_start_window(game, window_handle);
        }
        return 0;
    }
    case WM_LBUTTONUP:
    {
        ReleaseCapture();
        WindowsGame*game = (WindowsGame*)GetWindowLongPtrW(window_handle, GWLP_USERDATA);
        Window*window = game->game.windows + WINDOW_START;
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
            if (GetOpenFileNameA(&open_file_name) &&
                load_game(open_file_name.lpstrFile, &game->game))
            {
                game->status = id;
                DestroyWindow(window_handle);
                PostQuitMessage(0);
                return 0;
            }
        }
        else if (id == window->controls[START_NEW_GAME].base_id)
        {
            DestroyWindow(window_handle);
            HWND dialog_handle = CreateWindowEx(WS_EX_TOPMOST, g_time_control_window_class_name,
                g_time_control_window_class_name, g_dialog_style, CW_USEDEFAULT, CW_USEDEFAULT,
                CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, 0, 0);
            init_time_control_window(&game->game, GetDpiForWindow(dialog_handle));
            run_dialog(game, game->game.windows + WINDOW_TIME_CONTROL, dialog_handle, 0);
            PostQuitMessage(0);
            game->status = id;
            init_game(&game->game);
            return 0;
        }
        else if (id == window->controls[START_QUIT].base_id)
        {
            game->status = id;
            DestroyWindow(window_handle);
            PostQuitMessage(0);
            return 0;
        }
        if (id != old_clicked_control_id)
        {
            draw_and_render_start_window(game, window_handle);
        }
        return 0;
    }
    case WM_DPICHANGED:
    {
        WindowsGame*game = (WindowsGame*)GetWindowLongPtrW(window_handle, GWLP_USERDATA);
        free_dpi_data(game->game.windows + WINDOW_START);
        set_start_window_dpi(&game->game, game->text, HIWORD(w_param));
        RECT*client_rect = (RECT*)l_param;
        SetWindowPos(window_handle, 0, client_rect->left, client_rect->top,
            client_rect->right - client_rect->left, client_rect->bottom - client_rect->top,
            SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOZORDER);
        return 0;
    }
    case WM_MOUSEMOVE:
    {
        WindowsGame*game = (WindowsGame*)GetWindowLongPtrW(window_handle, GWLP_USERDATA);
        if (handle_mouse_move(game->game.windows + WINDOW_START, GET_X_LPARAM(l_param),
            GET_Y_LPARAM(l_param)))
        {
            draw_and_render_start_window(game, window_handle);
        }
        return 0;
    }
    case WM_PAINT:
    {
        draw_and_render_start_window((WindowsGame*)GetWindowLongPtrW(window_handle, GWLP_USERDATA),
            window_handle);
        return 0;
    }
    case WM_SIZE:
    {
        resize_window(((WindowsGame*)GetWindowLongPtrW(window_handle, GWLP_USERDATA))->
            game.windows + WINDOW_START, LOWORD(l_param), HIWORD(l_param));
        return 0;
    }
    default:
    {
        return DefWindowProc(window_handle, message, w_param, l_param);
    }
    }
}

void handle_timer_update(WindowsGame*game, HWND main_window_handle)
{
    UpdateTimerStatus status = update_timer(&game->game);
    if (status & UPDATE_TIMER_REDRAW)
    {
        draw_and_render_main_window(game, main_window_handle);
    }
    if (status & UPDATE_TIMER_TIME_OUT)
    {
        if (game->game.position_pool[game->game.current_position_index].active_player_index ==
            WHITE_PLAYER_INDEX)
        {
            game->text = "White is out of time. Black wins.";
        }
        else
        {
            game->text = "Black is out of time. White wins.";
        }
        run_game_over_dialog(game, main_window_handle);
    }
}

LRESULT CALLBACK main_window_proc(HWND window_handle, UINT message, WPARAM w_param, LPARAM l_param)
{
    switch (message)
    {
    case WM_DPICHANGED:
    {
        WindowsGame*game = (WindowsGame*)GetWindowLongPtrW(window_handle, GWLP_USERDATA);
        free_dpi_data(game->game.windows + WINDOW_MAIN);
        set_main_window_dpi(&game->game, HIWORD(w_param));
        RECT*client_rect = (RECT*)l_param;
        SetWindowPos(window_handle, 0, client_rect->left, client_rect->top,
            client_rect->right - client_rect->left, client_rect->bottom - client_rect->top,
            SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOZORDER);
        return 0;
    }
    case WM_PAINT:
    {
        draw_and_render_main_window((WindowsGame*)GetWindowLongPtrW(window_handle, GWLP_USERDATA),
            window_handle);
        return 0;
    }
    case WM_SIZE:
    {
        resize_window(((WindowsGame*)GetWindowLongPtrW(window_handle, GWLP_USERDATA))->
            game.windows + WINDOW_MAIN, LOWORD(l_param), HIWORD(l_param));
        return 0;
    }
    case WM_TIMER:
    {
        WindowsGame*game = (WindowsGame*)GetWindowLongPtrW(window_handle, GWLP_USERDATA);
        UpdateTimerStatus status = update_timer(&game->game);
        if (status & UPDATE_TIMER_REDRAW)
        {
            draw_and_render_main_window(game, window_handle);
        }
        if (status & UPDATE_TIMER_TIME_OUT)
        {
            if (game->game.position_pool[game->game.current_position_index].active_player_index ==
                WHITE_PLAYER_INDEX)
            {
                game->text = "White is out of time. Black wins.";
            }
            else
            {
                game->text = "Black is out of time. White wins.";
            }
            run_game_over_dialog(game, window_handle);
        }
        return 0;
    }
    }
    WindowsGame*game = (WindowsGame*)GetWindowLongPtrW(window_handle, GWLP_USERDATA);
    if (game && game->dialog_handle)
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
        if (main_window_handle_left_mouse_button_down(&game->game, GET_X_LPARAM(l_param),
            GET_Y_LPARAM(l_param)))
        {
            draw_and_render_main_window(game, window_handle);
        }
        return 0;
    }
    case WM_LBUTTONUP:
    {
        ReleaseCapture();
        switch (main_window_handle_left_mouse_button_up(&game->game, GET_X_LPARAM(l_param),
            GET_Y_LPARAM(l_param)))
        {
        case ACTION_SAVE_GAME:
        {
            draw_and_render_main_window(game, window_handle);
            char file_name[256];
            OPENFILENAMEA open_file_name = { 0 };
            open_file_name.lStructSize = sizeof(open_file_name);
            open_file_name.hwndOwner = window_handle;
            open_file_name.lpstrFile = file_name;
            open_file_name.nMaxFile = ARRAY_COUNT(file_name);
            open_file_name.lpstrDefExt = "chs";
            if (GetSaveFileNameA(&open_file_name))
            {
                save_game(open_file_name.lpstrFile, &game->game);
            }
            break;
        }
        case ACTION_PROMOTE:
        {
            HWND dialog_handle = CreateWindowEx(WS_EX_TOPMOST, g_promotion_window_class_name,
                g_promotion_window_class_name, g_dialog_style, CW_USEDEFAULT, CW_USEDEFAULT,
                CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, 0, 0);
            init_promotion_window(&game->game);
            run_dialog(game, game->game.windows + WINDOW_PROMOTION, dialog_handle, window_handle);
            select_promotion(&game->game, game->status);
            BringWindowToTop(window_handle);
        }
        case ACTION_MOVE:
        {
            game->status = end_turn(&game->game);
            draw_and_render_main_window(game, window_handle);
            if (game->status == STATUS_END_TURN)
            {
                return 0;
            }
        }
        case ACTION_CLAIM_DRAW:
        {
            handle_game_over_status(game, window_handle);
            break;
        }
        case ACTION_REDRAW:
        {
            draw_and_render_main_window(game, window_handle);
            break;
        }
        }
        return 0;
    }
    case WM_MOUSEMOVE:
    {
        if (handle_mouse_move(game->game.windows + WINDOW_MAIN, GET_X_LPARAM(l_param),
            GET_Y_LPARAM(l_param)))
        {
            draw_and_render_main_window(game, window_handle);
        }
        return 0;
    }
    default:
    {
        return DefWindowProc(window_handle, message, w_param, l_param);
    }
    }
}

void load_piece_icon_bitmap(Game*game, PieceType piece_type, uint8_t player_index, char*file_name,
    char*exe_path, size_t exe_path_length)
{
    while (*file_name)
    {
        exe_path[exe_path_length] = *file_name;
        ++exe_path_length;
        ++file_name;
    }
    exe_path[exe_path_length] = 0;
    HANDLE file_handle =
        CreateFileA(exe_path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    ReadFile(file_handle, game->icon_bitmaps[player_index][piece_type],
        sizeof(Color) * PIXELS_PER_ICON, 0, 0);
    CloseHandle(file_handle);
}

HWND windows_init(WindowsGame*game)
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
    wc.lpfnWndProc = time_control_proc;
    wc.lpszClassName = g_time_control_window_class_name;
    RegisterClassEx(&wc);
    wc.lpfnWndProc = start_window_proc;
    wc.lpszClassName = g_start_window_class_name;
    RegisterClassEx(&wc);
    HWND dialog_handle = CreateWindow(g_start_window_class_name, g_start_window_class_name,
        g_dialog_style, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, 0, 0);
    SetWindowLongPtrW(dialog_handle, GWLP_USERDATA, (LONG_PTR)game);
    uint16_t dpi = GetDpiForWindow(dialog_handle);
    NONCLIENTMETRICSW non_client_metrics;
    non_client_metrics.cbSize = sizeof(NONCLIENTMETRICSW);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, non_client_metrics.cbSize, &non_client_metrics,
        0);
    game->game.font_size = ((-non_client_metrics.lfMessageFont.lfHeight * 72) << 6) / dpi;
    HFONT win_font = CreateFont(0, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
        OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, 0, "Ariel");
    HDC device_context = GetDC(dialog_handle);
    SelectFont(device_context, win_font);
    size_t font_data_size = GetFontData(device_context, 0, 0, 0, 0);
    void*font_data = VirtualAlloc(0, font_data_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    GetFontData(device_context, 0, 0, font_data, font_data_size);
    ReleaseDC(dialog_handle, device_context);
    DeleteObject(win_font);
    init(&game->game, font_data, font_data_size);
    game->text = "";
    init_start_window(&game->game, game->text, GetDpiForWindow(dialog_handle));
    run_dialog(game, game->game.windows + WINDOW_START, dialog_handle, 0);
    if (game->status == START_QUIT)
    {
        return 0;
    }
    char exe_path[256];
    size_t exe_path_length =
        GetModuleFileNameA(0, exe_path, sizeof(exe_path)) + 1 - sizeof("windows_chess.exe");
    load_piece_icon_bitmap(&game->game, PIECE_ROOK, WHITE_PLAYER_INDEX, "whiteRook.bin", exe_path,
        exe_path_length);
    load_piece_icon_bitmap(&game->game, PIECE_KNIGHT, WHITE_PLAYER_INDEX, "whiteKnight.bin",
        exe_path, exe_path_length);
    load_piece_icon_bitmap(&game->game, PIECE_BISHOP, WHITE_PLAYER_INDEX, "whiteBishop.bin",
        exe_path, exe_path_length);
    load_piece_icon_bitmap(&game->game, PIECE_QUEEN, WHITE_PLAYER_INDEX, "whiteQueen.bin", exe_path,
        exe_path_length);
    load_piece_icon_bitmap(&game->game, PIECE_PAWN, WHITE_PLAYER_INDEX, "whitePawn.bin", exe_path,
        exe_path_length);
    load_piece_icon_bitmap(&game->game, PIECE_KING, WHITE_PLAYER_INDEX, "whiteKing.bin", exe_path,
        exe_path_length);
    load_piece_icon_bitmap(&game->game, PIECE_ROOK, BLACK_PLAYER_INDEX, "blackRook.bin", exe_path,
        exe_path_length);
    load_piece_icon_bitmap(&game->game, PIECE_KNIGHT, BLACK_PLAYER_INDEX, "blackKnight.bin",
        exe_path, exe_path_length);
    load_piece_icon_bitmap(&game->game, PIECE_BISHOP, BLACK_PLAYER_INDEX, "blackBishop.bin",
        exe_path, exe_path_length);
    load_piece_icon_bitmap(&game->game, PIECE_QUEEN, BLACK_PLAYER_INDEX, "blackQueen.bin", exe_path,
        exe_path_length);
    load_piece_icon_bitmap(&game->game, PIECE_PAWN, BLACK_PLAYER_INDEX, "blackPawn.bin", exe_path,
        exe_path_length);
    load_piece_icon_bitmap(&game->game, PIECE_KING, BLACK_PLAYER_INDEX, "blackKing.bin", exe_path,
        exe_path_length);
    wc.lpfnWndProc = promotion_proc;
    wc.lpszClassName = g_promotion_window_class_name;
    RegisterClassEx(&wc);
    wc.lpfnWndProc = main_window_proc;
    wc.lpszClassName = "Chess";
    RegisterClassEx(&wc);
    game->dialog_handle = 0;
    HWND window_handle = CreateWindow(wc.lpszClassName, "Chess", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, 0, 0);
    SetWindowLongPtrW(window_handle, GWLP_USERDATA, (LONG_PTR)game);
    init_main_window(&game->game, dpi);
    center_window(game->game.windows + WINDOW_MAIN, window_handle, WS_OVERLAPPEDWINDOW);
    SetTimer(window_handle, 1, USER_TIMER_MINIMUM, 0);
    return window_handle;
}

int WINAPI wWinMain(HINSTANCE instance_handle, HINSTANCE previous_instance_handle,
    PWSTR command_line, int show)
{
    WindowsGame game;
    HWND main_window_handle = windows_init(&game);
    if (main_window_handle)
    {
        run_message_loop(&game, main_window_handle);
    }
    return 0;
}