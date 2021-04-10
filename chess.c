#include <ft2build.h>
#include FT_FREETYPE_H
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <setjmp.h>
#include "platform_dependency.c"

#define ARRAY_COUNT(arr) (sizeof(arr) / sizeof(arr[0]))

char g_codepoints[] = { ' ', '.', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', 'B', 'C',
'D', 'I', 'L', 'N', 'Q', 'S', 'T', 'W', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'k', 'l', 'm',
'n', 'o', 'p', 'r', 's', 't', 'u', 'v', 'w', 'y' };

typedef struct Glyph
{
    FT_Bitmap bitmap;
    FT_Int left_bearing;
    FT_Int top_bearing;
    FT_Pos advance;
} Glyph;

FT_Library g_freetype_library;
FT_Face g_text_face;
FT_Face g_icon_face;

typedef struct Button
{
    char*label;
    uint32_t width;
    int32_t min_x;
    int32_t min_y;
} Button;

typedef struct Board
{
    int32_t min_x;
    int32_t min_y;
} Board;

typedef struct RadioButtons
{
    int32_t min_x;
    int32_t label_baseline_y;
} RadioButtons;

typedef struct DigitInput
{
    uint8_t digit : 4;
    bool is_before_colon;
} DigitInput;

typedef struct PromotionSelector
{
    int32_t min_x;
} PromotionSelector;

typedef struct TimeInput
{
    char*label;
    DigitInput*digits;
    int32_t min_x;
    int32_t min_y;
    uint8_t digit_count;
} TimeInput;

typedef enum ControlType
{
    CONTROL_BOARD,
    CONTROL_BUTTON,
    CONTROL_PROMOTION_SELECTOR,
    CONTROL_RADIO,
    CONTROL_TIME_INPUT
} ControlType;

typedef struct Control
{
    union
    {
        Board board;
        Button button;
        RadioButtons radio;
        PromotionSelector promotion_selector;
        TimeInput time_input;
    };
    uint8_t base_id;
    uint8_t control_type;
} Control;

char*g_radio_labels[2] = { "White", "Black" };

typedef enum SetupWindowControlIndex
{
    SETUP_WINDOW_PLAYER_COLOR,
    SETUP_WINDOW_TIME,
    SETUP_WINDOW_INCREMENT,
    SETUP_WINDOW_START_GAME,
    SETUP_WINDOW_CONTROL_COUNT
} SetupWindowControlIndex;

typedef enum MainWindowControlIndex
{
    MAIN_WINDOW_BOARD,
    MAIN_WINDOW_PROMOTION_SELECTOR,
    MAIN_WINDOW_SAVE_GAME,
    MAIN_WINDOW_CLAIM_DRAW,
    MAIN_WINDOW_CONTROL_COUNT
} MainWindowControlIndex;

typedef enum StartWindowControlIndex
{
    START_NEW_GAME,
    START_LOAD_GAME,
    START_QUIT,
    START_CONTROL_COUNT
} StartWindowControlIndex;

typedef union DialogControlCount
{
    char setup[SETUP_WINDOW_CONTROL_COUNT];
    char start[START_CONTROL_COUNT];
} DialogControlCount;

typedef enum PieceType
{
    PIECE_BISHOP,
    PIECE_KING,
    PIECE_KNIGHT,
    PIECE_QUEEN,
    PIECE_ROOK,
    PIECE_PAWN,
    PIECE_TYPE_COUNT
} PieceType; 

PieceType g_promotion_options[] = { PIECE_QUEEN, PIECE_ROOK, PIECE_BISHOP, PIECE_KNIGHT };

typedef struct Piece
{
    uint8_t square_index;
    uint8_t piece_type : 3;
    bool has_moved : 1;
} Piece;

#define RANK_COUNT 8
#define FILE_COUNT 8

typedef struct Position
{
    uint8_t squares[RANK_COUNT * FILE_COUNT];
    Piece pieces[32];
    union
    {
        struct
        {
            uint16_t previous_leaf_index;
            uint16_t next_leaf_index;
        };
        uint16_t first_move_index;
    };
    int16_t evaluation;
    uint16_t parent_index;
    uint16_t next_move_index;
    uint8_t en_passant_file;
    uint8_t active_player_index : 1;
    bool reset_draw_by_50_count : 1;
    bool moves_have_been_found : 1;
    bool is_leaf : 1;
} Position;

typedef struct BoardState
{
    uint64_t square_mask;
    uint8_t occupied_square_hashes[16];
} BoardState;

typedef struct BoardStateNode
{
    BoardState state;
    uint16_t index_of_next_node;
    uint8_t count;
    uint8_t generation;
} BoardStateNode;

typedef union Color
{
    struct
    {
        uint8_t blue;
        uint8_t green;
        uint8_t red;
        uint8_t alpha;
    };
    uint32_t value;
} Color;

typedef struct Grey
{
    uint8_t grey;
    uint8_t alpha;
} Grey;

typedef struct Circle
{
    uint8_t*alpha;
    uint32_t radius;
} Circle;

typedef struct DPIData
{
    Circle circles[3];
    Glyph glyphs['y' - ' ' + 1];
    Grey*icons;
    uint32_t square_size;
    uint32_t control_border_thickness;
    uint32_t text_line_height;
    uint32_t text_control_height;
    uint32_t text_control_padding;
    uint32_t digit_input_width;
    uint16_t dpi;
    uint8_t reference_count;
} DPIData;

typedef struct Window
{
    Color*pixels;
    Control*controls;
    DPIData*dpi_data;
    uint8_t*circles[3];
    size_t pixel_buffer_capacity;
    uint32_t width;
    uint32_t height;
    uint8_t hovered_control_id;
    uint8_t clicked_control_id;
    uint8_t control_count;
} Window;

typedef enum WindowIndex
{
    WINDOW_MAIN = 0,
    WINDOW_PROMOTION = 1,
    WINDOW_START = 1,
    WINDOW_SETUP = 1,
    WINDOW_COUNT = 2
} WindowIndex;

Position*g_position_pool;
BoardStateNode*g_state_buckets;
uint16_t*g_selected_move_index;
Control g_dialog_controls[sizeof(DialogControlCount)];
Control g_main_window_controls[MAIN_WINDOW_CONTROL_COUNT];
Window g_windows[WINDOW_COUNT];
DPIData g_dpi_datas[WINDOW_COUNT];
uint64_t g_times_left_as_of_last_move[2];
uint64_t g_last_move_time;
uint64_t g_time_increment;
uint32_t g_font_size;
uint16_t g_seconds_left[2];
uint16_t g_current_position_index;
uint16_t g_first_leaf_index;
uint16_t g_next_leaf_to_evaluate_index;
uint16_t g_index_of_first_free_pool_position;
uint16_t g_pool_cursor;
uint16_t g_state_bucket_count;
uint16_t g_external_state_node_count;
uint16_t g_unique_state_count;
uint16_t g_draw_by_50_count;
DigitInput g_time_control[5];
DigitInput g_increment[3];
uint8_t g_state_generation;
uint8_t g_selected_piece_index;
uint8_t g_selected_digit_id;
uint8_t g_engine_player_index;
bool g_run_engine;
bool g_is_promoting;

#define NULL_CONTROL UINT8_MAX
#define NULL_PIECE 32
#define NULL_SQUARE 64
#define NULL_POSITION UINT16_MAX
#define NULL_STATE UINT16_MAX
#define PLAYER_INDEX_WHITE 0
#define PLAYER_INDEX_BLACK 1

typedef enum Player0PieceIndices
{
    A_ROOK_INDEX,
    B_KNIGHT_INDEX,
    C_BISHOP_INDEX,
    QUEEN_INDEX,
    KING_INDEX,
    F_BISHOP_INDEX,
    G_KNIGHT_INDEX,
    H_ROOK_INDEX,
    A_PAWN_INDEX
} Player0PieceIndices;

#ifdef DEBUG
#define ASSERT(condition) if (!(condition)) *((int*)0) = 0

Position*get_position(uint16_t position_index)
{
    ASSERT(position_index != NULL_POSITION);
    return g_position_pool + position_index;
}

uint16_t get_previous_leaf_index(Position*position)
{
    ASSERT(position->is_leaf);
    return position->previous_leaf_index;
}

uint16_t get_next_leaf_index(Position*position)
{
    ASSERT(position->is_leaf);
    return position->next_leaf_index;
}

uint16_t get_first_move_index(Position*position)
{
    ASSERT(!position->is_leaf);
    return position->first_move_index;
}

void set_previous_leaf_index(Position*position, uint16_t value)
{
    ASSERT(position->is_leaf);
    position->previous_leaf_index = value;
}

void set_next_leaf_index(Position*position, uint16_t value)
{
    ASSERT(position->is_leaf);
    position->next_leaf_index = value;
}

void set_first_move_index(Position*position, uint16_t value)
{
    ASSERT(!position->is_leaf);
    position->first_move_index = value;
}

#define GET_POSITION(position_index) get_position(position_index)
#define GET_PREVIOUS_LEAF_INDEX(position) get_previous_leaf_index(position)
#define GET_NEXT_LEAF_INDEX(position) get_next_leaf_index(position)
#define GET_FIRST_MOVE_INDEX(position) get_first_move_index(position)
#define SET_PREVIOUS_LEAF_INDEX(position, value) set_previous_leaf_index(position, value)
#define SET_NEXT_LEAF_INDEX(position, value) set_next_leaf_index(position, value)
#define SET_FIRST_MOVE_INDEX(position, value) set_first_move_index(position, value)

void export_position_tree(void)
{
    HANDLE file_handle;
    if (g_position_pool[g_current_position_index].active_player_index == PLAYER_INDEX_WHITE)
    {
        file_handle = CreateFileA("white_move_tree", GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    }
    else
    {
        file_handle = CreateFileA("black_move_tree", GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    }
    if (file_handle != INVALID_HANDLE_VALUE)
    {
        DWORD bytes_written;
        WriteFile(file_handle, &g_current_position_index, sizeof(g_current_position_index),
            &bytes_written, 0);
        OVERLAPPED overlapped = { 0 };
        overlapped.Offset = 0xffffffff;
        overlapped.OffsetHigh = 0xffffffff;
        WriteFile(file_handle, g_position_pool, NULL_POSITION * sizeof(Position),
            &bytes_written, &overlapped);
        CloseHandle(file_handle);
    }
}

#define EXPORT_POSITION_TREE() export_position_tree()
#else
#define ASSERT(condition)
#define GET_POSITION(position_index) (g_position_pool + (position_index))
#define GET_PREVIOUS_LEAF_INDEX(position) (position)->previous_leaf_index
#define GET_NEXT_LEAF_INDEX(position) (position)->next_leaf_index
#define GET_FIRST_MOVE_INDEX(position) (position)->first_move_index
#define SET_PREVIOUS_LEAF_INDEX(position, value) ((position)->previous_leaf_index = (value))
#define SET_NEXT_LEAF_INDEX(position, value) ((position)->next_leaf_index = (value))
#define SET_FIRST_MOVE_INDEX(position, value) ((position)->first_move_index = (value))
#define EXPORT_POSITION_TREE()
#endif

#define PLAYER_INDEX(piece_index) ((piece_index) >> 4)
#define KING_RANK(player_index) (7 * !(player_index))
#define FORWARD_DELTA(player_index) (((player_index) << 1) - 1)
#define RANK(square_index) ((square_index) >> 3)
#define FILE(square_index) ((square_index) & 0b111)
#define SCREEN_SQUARE_INDEX(square_index) (g_engine_player_index == PLAYER_INDEX_WHITE ? (63 - (square_index)) : (square_index))
#define SQUARE_INDEX(rank, file) (FILE_COUNT * (rank) + (file))
#define EXTERNAL_STATE_NODES() (g_state_buckets + g_state_bucket_count)
#define PLAYER_PIECES_INDEX(player_index) ((player_index) << 4)
#define EN_PASSANT_RANK(player_index, forward_delta) KING_RANK(player_index) + ((forward_delta) << 2)
#define PLAYER_WIN(player_index) (((int16_t[]){INT16_MAX, -INT16_MAX})[player_index])

size_t g_page_size;
uint64_t g_counts_per_second;
uint64_t g_max_time;

void init_board_state_archive(uint8_t bucket_count)
{
    g_state_generation = 0;
    g_state_bucket_count = bucket_count;
    g_state_buckets = RESERVE_AND_COMMIT_MEMORY(2 * sizeof(BoardStateNode) * bucket_count);
    for (size_t i = 0; i < bucket_count; ++i)
    {
        g_state_buckets[i] = (BoardStateNode) { (BoardState) { 0 }, NULL_POSITION, 0, 0 };
    }
    g_unique_state_count = 0;
    g_external_state_node_count = 0;
}

bool archive_board_state(BoardState*state);

void increment_unique_state_count(void)
{
    ++g_unique_state_count;
    if (g_unique_state_count == g_state_bucket_count)
    {
        BoardStateNode*old_buckets = g_state_buckets;
        BoardStateNode*old_external_nodes = EXTERNAL_STATE_NODES();
        init_board_state_archive(g_state_bucket_count << 1);
        for (size_t i = 0; i < g_state_bucket_count; ++i)
        {
            BoardStateNode*node = old_buckets + i;
            if (node->count && node->generation == g_state_generation)
            { 
                while (true)
                {
                    archive_board_state(&node->state);
                    if (node->index_of_next_node < NULL_STATE)
                    {
                        node = old_external_nodes + node->index_of_next_node;
                    }
                    else
                    {
                        break;
                    }
                }
            }
        }
        FREE_MEMORY(old_buckets);
    }
}

bool archive_board_state(BoardState*state)
{
    //FNV hash the state into an index less than g_state_bucket_count.
    uint32_t bucket_index = 2166136261;
    for (size_t i = 0; i < sizeof(BoardState); ++i)
    {
        bucket_index ^= ((uint8_t*)state)[i];
        bucket_index *= 16777619;
    }
    uint32_t bucket_count_bit_count;
    BIT_SCAN_REVERSE(&bucket_count_bit_count, g_state_bucket_count);
    bucket_index = ((bucket_index >> bucket_count_bit_count) ^ bucket_index) &
        ((1 << bucket_count_bit_count) - 1);

    BoardStateNode*node = g_state_buckets + bucket_index;
    if (node->count && node->generation == g_state_generation)
    {
        while (memcmp(state, &node->state, sizeof(BoardState)))
        {
            if (node->index_of_next_node == NULL_STATE)
            {
                node->index_of_next_node = g_external_state_node_count;
                EXTERNAL_STATE_NODES()[g_external_state_node_count] =
                    (BoardStateNode) { *state, NULL_STATE, 1, g_state_generation };
                ++g_external_state_node_count;
                increment_unique_state_count();
                return true;
            }
            else
            {
                node = EXTERNAL_STATE_NODES() + node->index_of_next_node;
            }
        }
        ++node->count;
        if (node->count == 3)
        {
            return false;
        }
    }
    else
    {
        node->state = *state;
        node->index_of_next_node = NULL_STATE;
        node->count = 1;
        node->generation = g_state_generation;
        increment_unique_state_count();
    }
    return true;
}

bool piece_attacks_king_square_along_diagonal(Position*position, uint8_t piece_index,
    uint8_t king_square_index)
{
    Piece attacking_piece = position->pieces[piece_index];
    int8_t rank_delta = RANK(king_square_index) - RANK(attacking_piece.square_index);
    int8_t file_delta = FILE(king_square_index) - FILE(attacking_piece.square_index);
    if (rank_delta != file_delta && rank_delta != -file_delta)
    {
        return false;
    }
    int8_t square_index_increment;
    if (RANK(attacking_piece.square_index) < RANK(king_square_index))
    {
        square_index_increment = FILE_COUNT;
    }
    else
    {
        square_index_increment = -FILE_COUNT;
    }
    if (FILE(attacking_piece.square_index) < FILE(king_square_index))
    {
        square_index_increment += 1;
    }
    else
    {
        square_index_increment -= 1;
    }
    while (true)
    {
        attacking_piece.square_index += square_index_increment;
        if (RANK(attacking_piece.square_index) == RANK(king_square_index))
        {
            return true;
        }
        if (position->squares[attacking_piece.square_index] != NULL_PIECE)
        {
            return false;
        }
    }
}

bool piece_attacks_king_square_along_axis(Position*position, uint8_t piece_index,
    uint8_t king_square_index)
{
    Piece attacking_piece = position->pieces[piece_index];
    int8_t square_index_increment;
    if (RANK(king_square_index) == RANK(attacking_piece.square_index))
    {
        if (FILE(attacking_piece.square_index) < FILE(king_square_index))
        {
            square_index_increment = 1;
        }
        else
        {
            square_index_increment = -1;
        }
    }
    else if (FILE(king_square_index) == FILE(attacking_piece.square_index))
    {
        if (RANK(attacking_piece.square_index) < RANK(king_square_index))
        {
            square_index_increment = 8;
        }
        else
        {
            square_index_increment = -8;
        }
    }
    else
    {
        return false;
    }
    while (true)
    {
        attacking_piece.square_index += square_index_increment;
        if (attacking_piece.square_index == king_square_index)
        {
            return true;
        }
        if (position->squares[attacking_piece.square_index] != NULL_PIECE)
        {
            return false;
        }
    }
}

uint8_t abs_delta(uint8_t coord_a, uint8_t coord_b)
{
    if (coord_a > coord_b)
    {
        return coord_a - coord_b;
    }
    return coord_b - coord_a;
}

bool player_attacks_king_square(Position*position, uint8_t player_index, uint8_t king_square_index)
{
    uint8_t army_base_piece_index = 16 * player_index;
    uint8_t max_piece_index = army_base_piece_index + 16;
    for (size_t piece_index = army_base_piece_index; piece_index < max_piece_index; ++piece_index)
    {
        Piece attacking_piece = position->pieces[piece_index];
        if (attacking_piece.square_index == NULL_SQUARE)
        {
            continue;
        }
        switch (attacking_piece.piece_type)
        {
        case PIECE_PAWN:
        {
            uint8_t king_file = FILE(king_square_index);
            if (abs_delta(king_file, FILE(attacking_piece.square_index)) == 1 &&
                RANK(king_square_index) == RANK(attacking_piece.square_index) +
                    FORWARD_DELTA(position->active_player_index))
            {
                return true;
            }
            break;
        }
        case PIECE_BISHOP:
        {
            if (piece_attacks_king_square_along_diagonal(position, piece_index, king_square_index))
            {
                return true;
            }
            break;
        }
        case PIECE_KING:
        {
            if (abs_delta(RANK(king_square_index), RANK(attacking_piece.square_index)) <= 1 &&
                abs_delta(FILE(king_square_index), FILE(attacking_piece.square_index)) <= 1)
            {
                return true;
            }
            break;
        }
        case PIECE_KNIGHT:
        {
            uint8_t rank_delta =
                abs_delta(RANK(king_square_index), RANK(attacking_piece.square_index));
            uint8_t file_delta =
                abs_delta(FILE(king_square_index), FILE(attacking_piece.square_index));
            if ((rank_delta == 2 && file_delta == 1) || (rank_delta == 1 && file_delta == 2))
            {
                return true;
            }
            break;
        }
        case PIECE_ROOK:
        {
            if (piece_attacks_king_square_along_axis(position, piece_index, king_square_index))
            {
                return true;
            }
            break;
        }
        case PIECE_QUEEN:
        {
            if (piece_attacks_king_square_along_diagonal(position, piece_index,
                king_square_index) ||
                piece_attacks_king_square_along_axis(position, piece_index, king_square_index))
            {
                return true;
            }
            break;
        }
        }
    }
    return false;
}

bool player_is_checked(Position*position, uint8_t player_index)
{
    return player_attacks_king_square(position, !player_index,
        position->pieces[KING_INDEX + 16 * player_index].square_index);
}

void capture_piece(Position*position, uint8_t piece_index)
{
    position->pieces[piece_index].square_index = NULL_SQUARE;
    position->reset_draw_by_50_count = true;
}

void move_piece_to_square(Position*position, uint8_t piece_index, uint8_t square_index)
{
    position->squares[square_index] = piece_index;
    Piece*piece = position->pieces + piece_index;
    position->squares[piece->square_index] = NULL_PIECE;
    piece->square_index = square_index;
    piece->has_moved = true;
}

jmp_buf out_of_memory_jump_buffer;

uint16_t allocate_position(void)
{
    uint16_t new_position_index;
    Position*new_position;
    if (g_index_of_first_free_pool_position == NULL_POSITION)
    {
        if (g_pool_cursor == NULL_POSITION)
        {
            g_run_engine = false;
            longjmp(out_of_memory_jump_buffer, 1);
        }
        new_position_index = g_pool_cursor;
        new_position = GET_POSITION(new_position_index);
        COMMIT_MEMORY(new_position, g_page_size);
        ++g_pool_cursor;
        new_position->is_leaf = true;
    }
    else
    {
        new_position_index = g_index_of_first_free_pool_position;
        new_position = GET_POSITION(new_position_index);
        new_position->is_leaf = true;
        if (new_position->parent_index == NULL_POSITION)
        {
            g_index_of_first_free_pool_position = GET_NEXT_LEAF_INDEX(new_position);
        }
        else if (new_position->next_move_index == NULL_POSITION)
        {
            GET_POSITION(new_position->parent_index)->next_leaf_index =
                GET_NEXT_LEAF_INDEX(new_position);
            g_index_of_first_free_pool_position = new_position->parent_index;
        }
        else
        {
            g_index_of_first_free_pool_position = GET_NEXT_LEAF_INDEX(new_position);
        }
    }
    new_position->en_passant_file = FILE_COUNT;
    new_position->reset_draw_by_50_count = false;
    new_position->moves_have_been_found = false;
    return new_position_index;
}

uint16_t copy_position(uint16_t position_index)
{
    Position*position = GET_POSITION(position_index);
    uint16_t copy_index = allocate_position();
    Position*copy = GET_POSITION(copy_index);
    memcpy(&copy->squares, &position->squares, sizeof(position->squares));
    memcpy(&copy->pieces, &position->pieces, sizeof(position->pieces));
    return copy_index;
}

void add_move(uint16_t position_index, uint16_t move_index)
{
    Position*position = GET_POSITION(position_index);
    Position*move = GET_POSITION(move_index);
    move->parent_index = position_index;
    if (position->is_leaf)
    {
        SET_PREVIOUS_LEAF_INDEX(move, GET_PREVIOUS_LEAF_INDEX(position));
        SET_NEXT_LEAF_INDEX(move, GET_NEXT_LEAF_INDEX(position));
        if (GET_NEXT_LEAF_INDEX(move) != NULL_POSITION)
        {
            SET_PREVIOUS_LEAF_INDEX(GET_POSITION(GET_NEXT_LEAF_INDEX(move)), move_index);
        }
        position->is_leaf = false;
        move->next_move_index = NULL_POSITION;
    }
    else
    {
        Position*next_move = GET_POSITION(GET_FIRST_MOVE_INDEX(position));
        SET_PREVIOUS_LEAF_INDEX(move, GET_PREVIOUS_LEAF_INDEX(next_move));
        SET_NEXT_LEAF_INDEX(move, GET_FIRST_MOVE_INDEX(position));
        SET_PREVIOUS_LEAF_INDEX(next_move, move_index);
        move->next_move_index = GET_FIRST_MOVE_INDEX(position);
    }
    if (GET_PREVIOUS_LEAF_INDEX(move) == NULL_POSITION)
    {
        g_first_leaf_index = move_index;
    }
    else
    {
        SET_NEXT_LEAF_INDEX(GET_POSITION(GET_PREVIOUS_LEAF_INDEX(move)), move_index);
    }
    SET_FIRST_MOVE_INDEX(position, move_index);
    move->active_player_index = !position->active_player_index;
}

void free_position(uint16_t position_index)
{
    Position*position = GET_POSITION(position_index);
    position->parent_index = NULL_POSITION;
    position->next_leaf_index = g_index_of_first_free_pool_position;
    g_index_of_first_free_pool_position = position_index;
}

bool add_move_if_not_king_hang(uint16_t position_index, uint16_t move_index)
{
    Position*move = GET_POSITION(move_index);
    if (player_is_checked(move, GET_POSITION(position_index)->active_player_index))
    {
        free_position(move_index);
        return false;
    }
    else
    {
        add_move(position_index, move_index);
        return true;
    }
}

Position*move_piece_and_add_as_move(uint16_t position_index, uint8_t piece_index,
    uint8_t destination_square_index)
{
    Position*position = GET_POSITION(position_index);
    uint8_t destination_square = position->squares[destination_square_index];
    if (destination_square == NULL_PIECE)
    {
        uint16_t move_index = copy_position(position_index);
        Position*move = GET_POSITION(move_index);
        move_piece_to_square(move, piece_index, destination_square_index);
        add_move_if_not_king_hang(position_index, move_index);
        return move;
    }
    else if (PLAYER_INDEX(destination_square) != position->active_player_index)
    {
        uint16_t move_index = copy_position(position_index);
        Position*move = GET_POSITION(move_index);
        capture_piece(move, destination_square);
        move_piece_to_square(move, piece_index, destination_square_index);
        add_move_if_not_king_hang(position_index, move_index);
    }
    return 0;
}

uint16_t add_position_copy_as_move(uint16_t position_index, uint16_t position_to_copy_index)
{
    uint16_t copy_index = copy_position(position_to_copy_index);
    add_move(position_index, copy_index);
    return copy_index;
}

void add_forward_pawn_move_with_promotions(uint16_t position_index, uint16_t promotion_index,
    uint8_t piece_index)
{
    if (add_move_if_not_king_hang(position_index, promotion_index))
    {
        Position*promotion = GET_POSITION(promotion_index);
        if (RANK(promotion->pieces[piece_index].square_index) ==
            KING_RANK(!PLAYER_INDEX(piece_index)))
        {
            promotion->pieces[piece_index].piece_type = PIECE_ROOK;
            promotion = GET_POSITION(add_position_copy_as_move(position_index, promotion_index));
            promotion->pieces[piece_index].piece_type = PIECE_KNIGHT;
            promotion->reset_draw_by_50_count = true;
            promotion = GET_POSITION(add_position_copy_as_move(position_index, promotion_index));
            promotion->pieces[piece_index].piece_type = PIECE_BISHOP;
            promotion->reset_draw_by_50_count = true;
            promotion = GET_POSITION(add_position_copy_as_move(position_index, promotion_index));
            promotion->pieces[piece_index].piece_type = PIECE_QUEEN;
            promotion->reset_draw_by_50_count = true;
        }
    }
}

void add_diagonal_pawn_move(uint16_t position_index, uint8_t piece_index, uint8_t file)
{
    Position*position = GET_POSITION(position_index);
    uint8_t rank = RANK(position->pieces[piece_index].square_index);
    int8_t forward_delta = FORWARD_DELTA(position->active_player_index);
    uint8_t destination_square_index = SQUARE_INDEX(rank + forward_delta, file);
    if (file == position->en_passant_file && rank ==
        EN_PASSANT_RANK(position->active_player_index, forward_delta))
    {
        uint16_t move_index = copy_position(position_index);
        Position*move = GET_POSITION(move_index);
        uint8_t en_passant_square_index = SQUARE_INDEX(rank, file);
        capture_piece(move, position->squares[en_passant_square_index]);
        move_piece_to_square(move, piece_index, destination_square_index);
        move->squares[en_passant_square_index] = NULL_PIECE;
        add_move_if_not_king_hang(position_index, move_index);
    }
    else
    {
        uint8_t destination_square = position->squares[destination_square_index];
        if (PLAYER_INDEX(destination_square) == !position->active_player_index)
        {
            uint16_t move_index = copy_position(position_index);
            Position*move = GET_POSITION(move_index);
            capture_piece(move, destination_square);
            move_piece_to_square(move, piece_index, destination_square_index);
            add_forward_pawn_move_with_promotions(position_index, move_index, piece_index);
        }
    }
}

void add_half_diagonal_moves(uint16_t position_index, uint8_t piece_index, int8_t rank_delta,
    int8_t file_delta)
{
    uint8_t destination_square_index =
        GET_POSITION(position_index)->pieces[piece_index].square_index;
    uint8_t destination_rank = RANK(destination_square_index);
    uint8_t destination_file = FILE(destination_square_index);
    do
    {
        destination_rank += rank_delta;
        destination_file += file_delta;
    } while (destination_rank < 8 && destination_file < 8 && move_piece_and_add_as_move(
        position_index, piece_index, SQUARE_INDEX(destination_rank, destination_file)));
}

void add_diagonal_moves(uint16_t position_index, uint8_t piece_index)
{
    add_half_diagonal_moves(position_index, piece_index, 1, 1);
    add_half_diagonal_moves(position_index, piece_index, 1, -1);
    add_half_diagonal_moves(position_index, piece_index, -1, 1);
    add_half_diagonal_moves(position_index, piece_index, -1, -1);
}

void add_half_horizontal_moves(uint16_t position_index, uint8_t piece_index, int8_t delta,
    bool loses_castling_rights)
{
    uint8_t destination_square_index =
        GET_POSITION(position_index)->pieces[piece_index].square_index;
    uint8_t destination_rank = RANK(destination_square_index);
    uint8_t destination_file = FILE(destination_square_index);
    while (true)
    {
        destination_file += delta;
        if (destination_file > 7)
        {
            break;
        }
        Position*move = move_piece_and_add_as_move(position_index, piece_index,
            SQUARE_INDEX(destination_rank, destination_file));
        if (move)
        {
            move->reset_draw_by_50_count |= loses_castling_rights;
        }
        else
        {
            break;
        }
    }
}

void add_half_vertical_moves(uint16_t position_index, uint8_t piece_index, int8_t delta,
    bool loses_castling_rights)
{
    uint8_t destination_square_index =
        GET_POSITION(position_index)->pieces[piece_index].square_index;
    uint8_t destination_rank = RANK(destination_square_index);
    uint8_t destination_file = FILE(destination_square_index);
    while (true)
    {
        destination_rank += delta;
        if (destination_rank > 7)
        {
            break;
        }
        Position*move = move_piece_and_add_as_move(position_index, piece_index,
            SQUARE_INDEX(destination_rank, destination_file));
        if (move)
        {
            move->reset_draw_by_50_count |= loses_castling_rights;
        }
        else
        {
            break;
        }
    }
}

void add_moves_along_axes(uint16_t position_index, uint8_t piece_index, bool loses_castling_rights)
{
    add_half_horizontal_moves(position_index, piece_index, 1, loses_castling_rights);
    add_half_horizontal_moves(position_index, piece_index, -1, loses_castling_rights);
    add_half_vertical_moves(position_index, piece_index, 1, loses_castling_rights);
    add_half_vertical_moves(position_index, piece_index, -1, loses_castling_rights);
}

size_t add_king_move_ranks_or_files(uint8_t move_ranks_or_files[3], uint8_t king_rank_or_file)
{
    size_t count = 0;
    if (king_rank_or_file > 0)
    {
        move_ranks_or_files[count] = king_rank_or_file - 1;
        ++count;
    }
    move_ranks_or_files[count] = king_rank_or_file;
    ++count;
    if (king_rank_or_file < 7)
    {
        move_ranks_or_files[count] = king_rank_or_file + 1;
        ++count;
    }
    return count;
}

void get_moves(uint16_t position_index)
{
    Position*position = GET_POSITION(position_index);
    if (setjmp(out_of_memory_jump_buffer))
    {
        if (!position->is_leaf)
        {
            uint16_t move_index = GET_FIRST_MOVE_INDEX(position);
            Position*move = GET_POSITION(move_index);
            position->is_leaf = true;
            SET_PREVIOUS_LEAF_INDEX(position, GET_PREVIOUS_LEAF_INDEX(move));
            while (true)
            {
                if (move->next_move_index == NULL_POSITION)
                {
                    SET_NEXT_LEAF_INDEX(position, GET_NEXT_LEAF_INDEX(move));
                    free_position(move_index);
                    break;
                }
                free_position(move_index);
                move_index = move->next_move_index;
                move = GET_POSITION(move_index);
            }
            if (GET_PREVIOUS_LEAF_INDEX(position) != NULL_POSITION)
            {
                SET_NEXT_LEAF_INDEX(GET_POSITION(GET_PREVIOUS_LEAF_INDEX(position)),
                    position_index);
            }
            if (GET_NEXT_LEAF_INDEX(position) != NULL_POSITION)
            {
                SET_PREVIOUS_LEAF_INDEX(GET_POSITION(GET_NEXT_LEAF_INDEX(position)),
                    position_index);
            }
        }
        return;
    }
    uint8_t player_pieces_index = PLAYER_PIECES_INDEX(position->active_player_index);
    uint8_t max_piece_index = player_pieces_index + 16;
    for (uint8_t piece_index = player_pieces_index; piece_index < max_piece_index; ++piece_index)
    {
        Piece piece = position->pieces[piece_index];
        if (piece.square_index == NULL_SQUARE)
        {
            continue;
        }
        switch (piece.piece_type)
        {
        case PIECE_BISHOP:
        {
            add_diagonal_moves(position_index, piece_index);
            break;
        }
        case PIECE_KING:
        {
            uint8_t a_rook_index = player_pieces_index + A_ROOK_INDEX;
            uint8_t h_rook_index = player_pieces_index + H_ROOK_INDEX;
            bool loses_castling_rights = !piece.has_moved && 
                (!position->pieces[a_rook_index].has_moved ||
                    !position->pieces[h_rook_index].has_moved);
            uint8_t move_ranks[3];
            size_t move_rank_count =
                add_king_move_ranks_or_files(move_ranks, RANK(piece.square_index));
            uint8_t move_files[3];
            size_t move_file_count =
                add_king_move_ranks_or_files(move_files, FILE(piece.square_index));
            for (size_t rank_index = 0; rank_index < move_rank_count; ++rank_index)
            {
                for (size_t file_index = 0; file_index < move_file_count; ++file_index)
                {
                    Position*move = move_piece_and_add_as_move(position_index, piece_index,
                        SQUARE_INDEX(move_ranks[rank_index], move_files[file_index]));
                    if (move)
                    {
                        move->reset_draw_by_50_count |= loses_castling_rights;
                    }
                }
            }
            if (!piece.has_moved &&
                !player_attacks_king_square(position, !position->active_player_index,
                    piece.square_index))
            {
                if (!position->pieces[a_rook_index].has_moved &&
                    position->squares[piece.square_index - 1] == NULL_PIECE &&
                    position->squares[piece.square_index - 2] == NULL_PIECE &&
                    position->squares[piece.square_index - 3] == NULL_PIECE &&
                    !player_attacks_king_square(position, !position->active_player_index,
                        piece.square_index - 1) &&
                    !player_attacks_king_square(position, !position->active_player_index,
                        piece.square_index - 2))
                {
                    Position*move = move_piece_and_add_as_move(position_index, piece_index,
                        piece.square_index - 2);
                    move->reset_draw_by_50_count = true;
                    move_piece_to_square(move, a_rook_index, piece.square_index - 1);
                }
                if (!position->pieces[h_rook_index].has_moved &&
                    position->squares[piece.square_index + 1] == NULL_PIECE &&
                    position->squares[piece.square_index + 2] == NULL_PIECE &&
                    !player_attacks_king_square(position, !position->active_player_index,
                        piece.square_index + 1) &&
                    !player_attacks_king_square(position, !position->active_player_index,
                        piece.square_index + 2))
                {
                    Position*move = move_piece_and_add_as_move(position_index, piece_index,
                        piece.square_index + 2);
                    move->reset_draw_by_50_count = true;
                    move_piece_to_square(move, h_rook_index, piece.square_index + 1);
                }
            }
            break;
        }
        case PIECE_KNIGHT:
        {
            uint8_t square_index = piece.square_index;
            uint8_t rank = RANK(square_index);
            uint8_t file = FILE(square_index);
            if (file > 0)
            {
                if (rank > 1)
                {
                    move_piece_and_add_as_move(position_index, piece_index,
                        SQUARE_INDEX(rank - 2, file - 1));
                }
                if (rank < 6)
                {
                    move_piece_and_add_as_move(position_index, piece_index,
                        SQUARE_INDEX(rank + 2, file - 1));
                }
                if (file > 1)
                {
                    if (rank > 0)
                    {
                        move_piece_and_add_as_move(position_index, piece_index,
                            SQUARE_INDEX(rank - 1, file - 2));
                    }
                    if (rank < 7)
                    {
                        move_piece_and_add_as_move(position_index, piece_index,
                            SQUARE_INDEX(rank + 1, file - 2));
                    }
                }
            }
            if (file < 7)
            {
                if (rank > 1)
                {
                    move_piece_and_add_as_move(position_index, piece_index,
                        SQUARE_INDEX(rank - 2, file + 1));
                }
                if (rank < 6)
                {
                    move_piece_and_add_as_move(position_index, piece_index,
                        SQUARE_INDEX(rank + 2, file + 1));
                }
                if (file < 6)
                {
                    if (rank > 0)
                    {
                        move_piece_and_add_as_move(position_index, piece_index,
                            SQUARE_INDEX(rank - 1, file + 2));
                    }
                    if (rank < 7)
                    {
                        move_piece_and_add_as_move(position_index, piece_index,
                            SQUARE_INDEX(rank + 1, file + 2));
                    }
                }
            }
            break;
        }
        case PIECE_PAWN:
        {
            int8_t forward_delta = FORWARD_DELTA(position->active_player_index);
            uint8_t file = FILE(piece.square_index);
            if (file)
            {
                add_diagonal_pawn_move(position_index, piece_index, file - 1);
            }
            if (file < 7)
            {
                add_diagonal_pawn_move(position_index, piece_index, file + 1);
            }
            uint8_t destination_square_index = piece.square_index + forward_delta * FILE_COUNT;
            if (position->squares[destination_square_index] == NULL_PIECE)
            {
                uint16_t move_index = copy_position(position_index);
                Position*move = GET_POSITION(move_index);
                move_piece_to_square(move, piece_index, destination_square_index);
                move->reset_draw_by_50_count = true;
                destination_square_index += forward_delta * FILE_COUNT;
                add_forward_pawn_move_with_promotions(position_index, move_index,
                    piece_index);
                if (!piece.has_moved && position->squares[destination_square_index] == NULL_PIECE)
                {
                    move_index = copy_position(position_index);
                    move = GET_POSITION(move_index);
                    move_piece_to_square(move, piece_index, destination_square_index);
                    move->reset_draw_by_50_count = true;
                    move->en_passant_file = file;
                    add_move_if_not_king_hang(position_index, move_index);
                }
            }
            break;
        }
        case PIECE_QUEEN:
        {
            add_diagonal_moves(position_index, piece_index);
            add_moves_along_axes(position_index, piece_index, false);
            break;
        }
        case PIECE_ROOK:
        {
            add_moves_along_axes(position_index, piece_index, !piece.has_moved &&
                !position->pieces[(position->active_player_index << 4) + KING_INDEX].has_moved);
        }
        }
    }
    position->moves_have_been_found = true;
    if (position->is_leaf)
    {
        int16_t new_evaluation;
        if (player_is_checked(position, position->active_player_index))
        {
            new_evaluation = PLAYER_WIN(!position->active_player_index);
        }
        else
        {
            new_evaluation = 0;
        }
        if (new_evaluation == position->evaluation)
        {
            return;
        }
        position->evaluation = new_evaluation;
        if (position->parent_index == NULL_POSITION)
        {
            return;
        }
        position = GET_POSITION(position->parent_index);
    }
    else
    {
        Position*move = GET_POSITION(GET_FIRST_MOVE_INDEX(position));
        while (true)
        {
            move->evaluation = 0;
            for (size_t player_index = 0; player_index < 2; ++player_index)
            {
                int16_t point = FORWARD_DELTA(!player_index);
                Piece*player_pieces = move->pieces + PLAYER_PIECES_INDEX(player_index);
                for (size_t piece_index = 0; piece_index < 16; ++piece_index)
                {
                    Piece piece = player_pieces[piece_index];
                    if (piece.square_index < NULL_SQUARE)
                    {
                        switch (piece.piece_type)
                        {
                        case PIECE_PAWN:
                        {
                            move->evaluation += point;
                            break;
                        }
                        case PIECE_BISHOP:
                        case PIECE_KNIGHT:
                        {
                            move->evaluation += 3 * point;
                            break;
                        }
                        case PIECE_ROOK:
                        {
                            move->evaluation += 5 * point;
                            break;
                        }
                        case PIECE_QUEEN:
                        {
                            move->evaluation += 9 * point;
                        }
                        }
                    }
                }
            }
            if (move->next_move_index == NULL_POSITION)
            {
                break;
            }
            move = GET_POSITION(move->next_move_index);
        }
    }
    while (true)
    {
        int16_t new_evaluation = PLAYER_WIN(!position->active_player_index);
        uint16_t move_index = GET_FIRST_MOVE_INDEX(position);
        while (move_index != NULL_POSITION)
        {
            Position*move = GET_POSITION(move_index);
            if (position->active_player_index == PLAYER_INDEX_WHITE)
            {
                if (move->evaluation > new_evaluation)
                {
                    new_evaluation = move->evaluation;
                }
            }
            else if (move->evaluation < new_evaluation)
            {
                new_evaluation = move->evaluation;
            }
            move_index = move->next_move_index;
        }
        if (new_evaluation == position->evaluation)
        {
            return;
        }
        position->evaluation = new_evaluation;
        if (position->parent_index == NULL_POSITION)
        {
            return;
        }
        position = GET_POSITION(position->parent_index);
    }
}

bool make_move_current(uint16_t move_index)
{
    g_current_position_index = move_index;
    Position*position = GET_POSITION(g_current_position_index);
    if (position->reset_draw_by_50_count)
    {
        g_draw_by_50_count = 0;
        ++g_state_generation;
    }
    ++g_draw_by_50_count;
    if (!position->moves_have_been_found)
    {
        get_moves(g_current_position_index);
    }
    g_next_leaf_to_evaluate_index = g_first_leaf_index;
    BoardState state = { 0 };
    uint64_t square_mask_digit = 1;
    size_t square_hash_index = 0;
    bool use_high_bits = false;
    for (size_t rank = 0; rank < 8; ++rank)
    {
        for (size_t file = 0; file < 8; ++file)
        {
            uint8_t square = position->squares[SQUARE_INDEX(rank, file)];
            if (square != NULL_PIECE)
            {
                uint8_t square_hash = position->pieces[square].piece_type +
                    PIECE_TYPE_COUNT * PLAYER_INDEX(square) + position->active_player_index;
                if (use_high_bits)
                {
                    state.occupied_square_hashes[square_hash_index] |= square_hash << 4;
                    use_high_bits = false;
                    ++square_hash_index;
                }
                else
                {
                    state.occupied_square_hashes[square_hash_index] |= square_hash;
                    use_high_bits = true;
                }
                state.square_mask |= square_mask_digit;
            }
            square_mask_digit = square_mask_digit << 1;
        }
    }
    return archive_board_state(&state);
}

bool point_is_in_rect(int32_t x, int32_t y, int32_t min_x, int32_t min_y, uint32_t width,
    uint32_t height)
{
    return x >= min_x && y >= min_y && x < min_x + width && y < min_y + height;
}

#define ROUND26_6_TO_PIXEL(subpixel) (((subpixel) + 32) >> 6)

uint8_t id_of_control_cursor_is_over(Window*window, int32_t cursor_x, int32_t cursor_y)
{
    for (size_t control_index = 0; control_index < window->control_count; ++control_index)
    {
        Control*control = window->controls + control_index;
        switch (control->control_type)
        {
        case CONTROL_BOARD:
        {
            if (cursor_x >= control->board.min_x && cursor_y >= control->board.min_y)
            {
                uint8_t rank = (cursor_y - control->board.min_y) / window->dpi_data->square_size;
                uint8_t file = (cursor_x - control->board.min_x) / window->dpi_data->square_size;
                if (rank < RANK_COUNT && file < FILE_COUNT)
                {
                    return control->base_id + SQUARE_INDEX(rank, file);
                }
            }
            break;
        }
        case CONTROL_BUTTON:
        {
            if (point_is_in_rect(cursor_x, cursor_y, control->button.min_x, control->button.min_y,
                control->button.width, window->dpi_data->text_control_height))
            {
                return control->base_id;
            }
            break;
        }
        case CONTROL_PROMOTION_SELECTOR:
        {
            if (point_is_in_rect(cursor_x, cursor_y, control->promotion_selector.min_x, 0,
                4 * window->dpi_data->square_size, window->dpi_data->square_size))
            {
                return control->base_id +
                    (cursor_x - control->promotion_selector.min_x) / window->dpi_data->square_size;
            }
            break;
        }
        case CONTROL_RADIO:
        {
            int32_t cursor_x_in_circle = cursor_x + window->dpi_data->circles[1].radius -
                (control->radio.min_x + window->dpi_data->circles[2].radius);
            int32_t diameter = 2 * window->dpi_data->circles[1].radius;
            if (cursor_x_in_circle >= 0 && cursor_x_in_circle < diameter)
            {
                int32_t cursor_y_in_circle = cursor_y + window->dpi_data->circles[2].radius +
                    window->dpi_data->circles[1].radius -
                    (control->radio.label_baseline_y + window->dpi_data->text_line_height);
                for (size_t option_index = 0;
                    option_index < ARRAY_COUNT(g_radio_labels); ++option_index)
                {
                    if (cursor_y_in_circle < 0)
                    {
                        break;
                    }
                    if (cursor_y_in_circle < diameter && window->dpi_data->circles[option_index].
                        alpha[cursor_y_in_circle * diameter + cursor_x_in_circle])
                    {
                        return control->base_id + option_index;
                    }
                    cursor_y_in_circle -= window->dpi_data->text_line_height;
                }
            }
            break;
        }
        case CONTROL_TIME_INPUT:
        {
            int32_t digit_input_min_x = control->time_input.min_x;
            for (size_t digit_index = 0; digit_index < control->time_input.digit_count;
                ++digit_index)
            {
                DigitInput*digit_input = control->time_input.digits + digit_index;
                if (point_is_in_rect(cursor_x, cursor_y, digit_input_min_x,
                    control->time_input.min_y, window->dpi_data->digit_input_width,
                    window->dpi_data->text_control_height))
                {
                    return control->base_id + digit_index;
                }
                digit_input_min_x += window->dpi_data->digit_input_width +
                    window->dpi_data->control_border_thickness;
                if (digit_input->is_before_colon)
                {
                    digit_input_min_x += window->dpi_data->control_border_thickness +
                        2 * ROUND26_6_TO_PIXEL(window->dpi_data->glyphs
                            [':' - g_codepoints[0]].advance);
                }
            }
        }
        }
    cursor_not_over_control:;
    }
    return NULL_CONTROL;
}

bool handle_left_mouse_button_down(Window*window, int32_t cursor_x, int32_t cursor_y)
{
    window->clicked_control_id = window->hovered_control_id;
    return window->clicked_control_id != NULL_CONTROL;
}

uint8_t handle_left_mouse_button_up(Window*window, int32_t cursor_x, int32_t cursor_y)
{
    if (window->clicked_control_id != NULL_CONTROL)
    {
        uint8_t id = id_of_control_cursor_is_over(window, cursor_x, cursor_y);
        if (id == window->clicked_control_id)
        {
            window->clicked_control_id = NULL_CONTROL;
            return id;
        }
        window->clicked_control_id = NULL_CONTROL;
    }
    return NULL_CONTROL;
}

bool handle_mouse_move(Window*window, int32_t cursor_x, int32_t cursor_y)
{
    uint8_t new_hovered_conrol_id = id_of_control_cursor_is_over(window, cursor_x, cursor_y);
    if (window->hovered_control_id != new_hovered_conrol_id)
    {
        window->hovered_control_id = new_hovered_conrol_id;
        return true;
    }
    return false;
}

typedef enum UpdateTimerStatus
{
    UPDATE_TIMER_CONTINUE = 0,
    UPDATE_TIMER_TIME_OUT = 0b1,
    UPDATE_TIMER_REDRAW = 0b10
} UpdateTimerStatus;

UpdateTimerStatus update_timer(void)
{
    UpdateTimerStatus out = UPDATE_TIMER_CONTINUE;
    uint64_t time_since_last_move = get_time() - g_last_move_time;
    uint16_t new_seconds_left;
    uint8_t active_player_index = GET_POSITION(g_current_position_index)->active_player_index;
    if (time_since_last_move >= g_times_left_as_of_last_move[active_player_index])
    {
        new_seconds_left = 0;
        out |= UPDATE_TIMER_TIME_OUT;
    }
    else
    {
        new_seconds_left = (g_times_left_as_of_last_move[active_player_index] -
            time_since_last_move) / g_counts_per_second;
    }
    if (new_seconds_left != g_seconds_left[active_player_index])
    {
        g_seconds_left[active_player_index] = new_seconds_left;
        out |= UPDATE_TIMER_REDRAW;
    }
    return out;
}

uint16_t get_first_tree_leaf_index(uint16_t root_position_index)
{
    while (true)
    {
        Position*position = GET_POSITION(root_position_index);
        if (position->is_leaf)
        {
            return root_position_index;
        }
        root_position_index = GET_FIRST_MOVE_INDEX(position);
    }
}

uint16_t get_last_tree_leaf_index(uint16_t root_position_index)
{
    Position*position = GET_POSITION(root_position_index);
    if (position->is_leaf)
    {
        return root_position_index;
    }
    uint16_t out = GET_FIRST_MOVE_INDEX(position);
    while (true)
    {
        position = GET_POSITION(out);
        if (position->next_move_index == NULL_POSITION)
        {
            if (position->is_leaf)
            {
                return out;
            }
            else
            {
                out = GET_FIRST_MOVE_INDEX(position);
            }
        }
        else
        {
            out = position->next_move_index;
        }
    }
}

typedef enum GUIAction
{
    ACTION_CHECKMATE,
    ACTION_CLAIM_DRAW,
    ACTION_REPETITION_DRAW,
    ACTION_FLAG,
    ACTION_NONE,
    ACTION_REDRAW,
    ACTION_SAVE_GAME,
    ACTION_STALEMATE
} GUIAction;

GUIAction end_turn(void)
{
    uint64_t move_time = get_time();
    uint64_t time_since_last_move = move_time - g_last_move_time;
    uint8_t active_player_index = GET_POSITION(g_current_position_index)->active_player_index;
    uint64_t*time_left_as_of_last_move = g_times_left_as_of_last_move + active_player_index;
    if (time_since_last_move >= *time_left_as_of_last_move)
    {
        g_seconds_left[active_player_index] = 0;
        return ACTION_FLAG;
    }
    *time_left_as_of_last_move -= time_since_last_move;
    if (g_max_time - g_time_increment >= *time_left_as_of_last_move)
    {
        *time_left_as_of_last_move += g_time_increment;
    }
    else
    {
        *time_left_as_of_last_move = g_max_time;
    }
    g_seconds_left[active_player_index] = *time_left_as_of_last_move / g_counts_per_second;
    g_last_move_time = move_time;
    EXPORT_POSITION_TREE();
    g_selected_piece_index = NULL_PIECE;
    uint16_t selected_move_index = *g_selected_move_index;
    Position*move = GET_POSITION(selected_move_index);
    *g_selected_move_index = move->next_move_index;
    move->parent_index = NULL_POSITION;
    Position*current_position = GET_POSITION(g_current_position_index);
    if (GET_FIRST_MOVE_INDEX(current_position) == NULL_POSITION)
    {
        free_position(g_current_position_index);
    }
    else
    {
        uint16_t first_leaf_index = get_first_tree_leaf_index(selected_move_index);
        Position*move_tree_first_leaf = GET_POSITION(first_leaf_index);
        Position*move_tree_last_leaf = GET_POSITION(get_last_tree_leaf_index(selected_move_index));
        if (GET_PREVIOUS_LEAF_INDEX(move_tree_first_leaf) != NULL_POSITION)
        {
            SET_NEXT_LEAF_INDEX(GET_POSITION(GET_PREVIOUS_LEAF_INDEX(move_tree_first_leaf)),
                GET_NEXT_LEAF_INDEX(move_tree_last_leaf));
        }
        if (GET_NEXT_LEAF_INDEX(move_tree_last_leaf) != NULL_POSITION)
        {
            SET_PREVIOUS_LEAF_INDEX(GET_POSITION(GET_NEXT_LEAF_INDEX(move_tree_last_leaf)),
                GET_PREVIOUS_LEAF_INDEX(move_tree_first_leaf));
            SET_NEXT_LEAF_INDEX(move_tree_last_leaf, NULL_POSITION);
        }
        SET_PREVIOUS_LEAF_INDEX(move_tree_first_leaf, NULL_POSITION);
        g_first_leaf_index = first_leaf_index;
        first_leaf_index = get_first_tree_leaf_index(g_current_position_index);
        SET_PREVIOUS_LEAF_INDEX(GET_POSITION(first_leaf_index), NULL_POSITION);
        SET_NEXT_LEAF_INDEX(GET_POSITION(get_last_tree_leaf_index(g_current_position_index)),
            g_index_of_first_free_pool_position);
        g_index_of_first_free_pool_position = first_leaf_index;
    }
    if (make_move_current(selected_move_index))
    {
        if (move->is_leaf)
        {
            if (move->evaluation == PLAYER_WIN(!move->active_player_index))
            {
                return ACTION_CHECKMATE;
            }
            else
            {
                return ACTION_STALEMATE;
            }
        }
        else
        {
            g_run_engine = true;
            return ACTION_REDRAW;
        }
    }
    else
    {
        return ACTION_REPETITION_DRAW;
    }
}

GUIAction do_engine_iteration(void)
{
    if (g_run_engine)
    {
        uint16_t first_leaf_checked_index = g_next_leaf_to_evaluate_index;
        uint16_t position_to_evaluate_index = g_next_leaf_to_evaluate_index;
        Position*position;
        while (true)
        {
            if (position_to_evaluate_index == NULL_POSITION)
            {
                position_to_evaluate_index = g_first_leaf_index;
            }
            position = GET_POSITION(position_to_evaluate_index);
            if (!position->moves_have_been_found)
            {
                g_next_leaf_to_evaluate_index = GET_NEXT_LEAF_INDEX(position);
                get_moves(position_to_evaluate_index);
                break;
            }
            if (first_leaf_checked_index == GET_NEXT_LEAF_INDEX(position))
            {
                g_run_engine = false;
                break;
            }
            position_to_evaluate_index = GET_NEXT_LEAF_INDEX(position);
        }
    }
    if (!g_run_engine)
    {
        Position*current_position = GET_POSITION(g_current_position_index);
        ASSERT(!current_position->is_leaf);
        if (current_position->active_player_index == g_engine_player_index)
        {
            int64_t best_evaluation = PLAYER_WIN(!g_engine_player_index);
            uint16_t*move_index = &current_position->first_move_index;
            g_selected_move_index = move_index;
            while (*move_index != NULL_POSITION)
            {
                Position*move = GET_POSITION(*move_index);
                if (g_engine_player_index == PLAYER_INDEX_WHITE)
                {
                    if (move->evaluation > best_evaluation)
                    {
                        best_evaluation = move->evaluation;
                        g_selected_move_index = move_index;
                    }
                }
                else if (move->evaluation < best_evaluation)
                {
                    best_evaluation = move->evaluation;
                    g_selected_move_index = move_index;
                }
                move_index = &move->next_move_index;
            }
            return end_turn();
        }
    }
    return ACTION_NONE;
}

int32_t min32(int32_t a, int32_t b)
{
    if (a < b)
    {
        return a;
    }
    return b;
}

uint32_t max32(uint32_t a, uint32_t b)
{
    if (a > b)
    {
        return a;
    }
    return b;
}

Color g_black = { .value = 0xff000000 };
Color g_white = { .value = 0xffffffff };
Color g_dark_square_gray = { .value = 0xffababab };
Color g_hovered_blue = { .red = 146,.green = 220,.blue = 255,.alpha = 255 };
Color g_clicked_red = { .red = 255,.green = 127,.blue = 127,.alpha = 255 };

void draw_rectangle(Window*window, int32_t min_x, int32_t min_y, uint32_t width, uint32_t height,
    Color color)
{
    int32_t max_x = min32(min_x + width, window->width);
    int32_t max_y = min32(min_y + height, window->height);
    Color*row = window->pixels + min_y * window->width;
    for (int32_t y = min_y; y < max_y; ++y)
    {
        for (int32_t x = min_x; x < max_x; ++x)
        {
            row[x].value = color.value;
        }
        row += window->width;
    }
}

void draw_rectangle_border(Window*window, int32_t min_x, int32_t min_y, uint32_t width,
    uint32_t height)
{
    int32_t outer_min_x = min_x - window->dpi_data->control_border_thickness;
    int32_t outer_min_y = min_y - window->dpi_data->control_border_thickness;
    uint32_t outer_width = width + 2 * window->dpi_data->control_border_thickness;
    draw_rectangle(window, outer_min_x, outer_min_y, outer_width,
        window->dpi_data->control_border_thickness, g_black);
    draw_rectangle(window, outer_min_x, min_y + height, outer_width,
        window->dpi_data->control_border_thickness, g_black);
    draw_rectangle(window, outer_min_x, min_y, window->dpi_data->control_border_thickness, height,
        g_black);
    draw_rectangle(window, min_x + width, min_y, window->dpi_data->control_border_thickness, height,
        g_black);
}

void draw_icon(Window*window, int32_t min_x, int32_t min_y, uint8_t player_index,
    uint8_t piece_type)
{
    size_t pixels_per_icon = window->dpi_data->square_size * window->dpi_data->square_size;
    Grey*icon =
        window->dpi_data->icons + (player_index * PIECE_TYPE_COUNT + piece_type) * pixels_per_icon;
    int32_t max_x_in_icon = min32(window->dpi_data->square_size, window->width - min_x);
    int32_t max_y_in_icon = min32(window->dpi_data->square_size, window->height - min_y);
    Color*icon_row_min_x_in_window = window->pixels + min_y * window->width + min_x;
    icon += pixels_per_icon;
    for (int32_t y_in_icon = 0; y_in_icon < max_y_in_icon; ++y_in_icon)
    {
        icon -= window->dpi_data->square_size;
        for (int32_t x_in_icon = 0; x_in_icon < max_x_in_icon; ++x_in_icon)
        {
            Color*window_pixel = icon_row_min_x_in_window + x_in_icon;
            Grey icon_pixel = icon[x_in_icon];
            uint32_t lerp_term = (255 - icon_pixel.alpha) * window_pixel->alpha;
            uint32_t icon_term = (icon_pixel.grey * icon_pixel.alpha) / 255;
            window_pixel->red = icon_term + (lerp_term * window_pixel->red) / 65025;
            window_pixel->green = icon_term + (lerp_term * window_pixel->green) / 65025;
            window_pixel->blue = icon_term + (lerp_term * window_pixel->blue) / 65025;
            window_pixel->alpha = icon_pixel.alpha + lerp_term / 255;
        }
        icon_row_min_x_in_window += window->width;
    }
}

void fill_circle_row_edge_points(uint8_t*center, int32_t diameter, int32_t max_x, int32_t y,
    uint8_t alpha)
{
    uint8_t*row = center + y * diameter;
    row[max_x] = alpha;
    row[-(max_x + 1)] = alpha;
}

void fill_circle_row_pair_edge_points(uint8_t*center, int32_t diameter, int32_t max_x,
    int32_t lower_row_y, uint8_t alpha)
{
    fill_circle_row_edge_points(center, diameter, max_x, lower_row_y, alpha);
    fill_circle_row_edge_points(center, diameter, max_x, -(lower_row_y + 1), alpha);
}

void fill_circle_row_interior(uint8_t*center, int32_t diameter, int32_t max_x, int32_t y)
{
    uint8_t*row = center + y * diameter;
    for (int32_t interior_x = -max_x; interior_x < max_x; ++interior_x)
    {
        row[interior_x] = 255;
    }
}

void fill_circle_row_pair_interiors(uint8_t*center, int32_t diameter, int32_t max_x,
    int32_t lower_row_y)
{
    fill_circle_row_interior(center, diameter, max_x, lower_row_y);
    fill_circle_row_interior(center, diameter, max_x, -(lower_row_y + 1));
}

void fill_circle_row_pair(uint8_t*center, int32_t diameter, int32_t max_x, int32_t lower_row_y,
    uint8_t max_x_alpha)
{
    fill_circle_row_pair_edge_points(center, diameter, max_x, lower_row_y, max_x_alpha);
    fill_circle_row_pair_interiors(center, diameter, max_x, lower_row_y);
}

void rasterize_piece_icon(DPIData*dpi_data, size_t icon_index, int32_t baseline_offset,
    uint8_t background_grey, uint8_t foreground_grey)
{
    FT_Load_Glyph(g_icon_face, 2 * icon_index + 1, FT_LOAD_RENDER);
    Grey*icon = dpi_data->icons + (icon_index + 1) * dpi_data->square_size * dpi_data->square_size;
    Grey*glyph_row_min_x_in_icon = icon + g_icon_face->glyph->bitmap_left -
        (baseline_offset - g_icon_face->glyph->bitmap_top) * dpi_data->square_size;
    uint8_t*glyph_row = g_icon_face->glyph->bitmap.buffer;
    for (int32_t y_in_glyph = 0; y_in_glyph < g_icon_face->glyph->bitmap.rows; ++y_in_glyph)
    {
        glyph_row_min_x_in_icon -= dpi_data->square_size;
        for (int32_t x_in_glyph = 0; x_in_glyph < g_icon_face->glyph->bitmap.width; ++x_in_glyph)
        {
            Grey*icon_pixel = glyph_row_min_x_in_icon + x_in_glyph;
            icon_pixel->grey = background_grey;
            icon_pixel->alpha = glyph_row[x_in_glyph];
        }
        glyph_row += g_icon_face->glyph->bitmap.width;
    }
    FT_Load_Glyph(g_icon_face, 2 * icon_index + 2, FT_LOAD_RENDER);
    glyph_row_min_x_in_icon = icon + g_icon_face->glyph->bitmap_left -
        (baseline_offset - g_icon_face->glyph->bitmap_top) * dpi_data->square_size;
    glyph_row = g_icon_face->glyph->bitmap.buffer;
    for (int32_t y_in_glyph = 0; y_in_glyph < g_icon_face->glyph->bitmap.rows; ++y_in_glyph)
    {
        glyph_row_min_x_in_icon -= dpi_data->square_size;
        for (int32_t x_in_glyph = 0; x_in_glyph < g_icon_face->glyph->bitmap.width; ++x_in_glyph)
        {
            Grey*icon_pixel = glyph_row_min_x_in_icon + x_in_glyph;
            uint32_t alpha = glyph_row[x_in_glyph];
            uint32_t lerp_term = (255 - alpha) * icon_pixel->alpha;
            icon_pixel->grey =
                (foreground_grey * alpha) / 255 + (lerp_term * icon_pixel->grey) / 65025;
            icon_pixel->alpha = alpha + lerp_term / 255;
        }
        glyph_row += g_icon_face->glyph->bitmap.width;
    }
}

void set_dpi_data(Window*window, uint16_t dpi)
{
    for (size_t i = 0; i < ARRAY_COUNT(g_dpi_datas); ++i)
    {
        DPIData*dpi_data = g_dpi_datas + i;
        if (dpi_data->reference_count)
        {
            if (dpi_data->dpi == dpi)
            {
                window->dpi_data = dpi_data;
                ++window->dpi_data->reference_count;
                return;
            }
        }
        else
        {
            window->dpi_data = dpi_data;
        }
    }
    ++window->dpi_data->reference_count;
    window->dpi_data->dpi = dpi;
    FT_Set_Char_Size(g_icon_face, 0, 4 * g_font_size, 0, dpi);
    FT_Set_Char_Size(g_text_face, 0, g_font_size, 0, dpi);
    FT_Load_Glyph(g_text_face, FT_Get_Char_Index(g_text_face, '|'), FT_LOAD_DEFAULT);
    window->dpi_data->control_border_thickness = g_text_face->glyph->bitmap.pitch;
    size_t rasterization_memory_size = 0;
    for (size_t i = 0; i < ARRAY_COUNT(window->dpi_data->circles); ++i)
    {
        Circle*circle = window->dpi_data->circles + i;
        circle->radius = (i + 1) * window->dpi_data->control_border_thickness;
        int32_t diameter = 2 * circle->radius;
        rasterization_memory_size += diameter * diameter;
    }
    FT_Load_Glyph(g_icon_face, 9, FT_LOAD_DEFAULT);
    window->dpi_data->square_size = ROUND26_6_TO_PIXEL(g_icon_face->glyph->metrics.horiAdvance);
    uint32_t square_size = ROUND26_6_TO_PIXEL(g_icon_face->glyph->metrics.horiAdvance);
    int32_t icon_baseline_offset = g_icon_face->glyph->bitmap_top +
        (window->dpi_data->square_size - g_icon_face->glyph->bitmap.rows) / 2;
    size_t pixels_per_icon = window->dpi_data->square_size * window->dpi_data->square_size;
    rasterization_memory_size += 12 * pixels_per_icon * sizeof(Grey);
    for (size_t i = 0; i < ARRAY_COUNT(g_codepoints); ++i)
    {
        FT_Load_Glyph(g_text_face, FT_Get_Char_Index(g_text_face, g_codepoints[i]),
            FT_LOAD_DEFAULT);
        rasterization_memory_size +=
            g_text_face->glyph->bitmap.rows * g_text_face->glyph->bitmap.pitch;
    }
    void*rasterization_memory = RESERVE_AND_COMMIT_MEMORY(rasterization_memory_size);
    for (size_t i = 0; i < ARRAY_COUNT(window->dpi_data->circles); ++i)
    {
        Circle*circle_rasterization = window->dpi_data->circles + i;
        circle_rasterization->alpha = rasterization_memory;
        uint32_t diameter = 2 * circle_rasterization->radius;
        rasterization_memory = (void*)((uintptr_t)rasterization_memory + diameter * diameter);
        uint8_t*center =
            circle_rasterization->alpha + (diameter + 1) * circle_rasterization->radius;
        int32_t radius_squared = circle_rasterization->radius * circle_rasterization->radius;
        int32_t int_x = 0;
        float y = circle_rasterization->radius;
        int32_t int_y = circle_rasterization->radius - 1;
        while (true)
        {
            int32_t int_next_x = int_x + 1;
            float next_y = sqrtf(radius_squared - int_next_x * int_next_x);
            int32_t int_next_y = next_y;
            if (next_y == int_next_y)
            {
                int_y = int_next_y;
                fill_circle_row_pair_interiors(center, diameter, int_x, int_y);
            }
            if (int_y == int_next_y)
            {
                uint8_t alpha = 255.0 * (0.5 * (y + next_y) - int_y) + 0.5;
                fill_circle_row_pair_edge_points(center, diameter, int_x, int_y, alpha);
                fill_circle_row_pair(center, diameter, int_y, int_x, alpha);
            }
            else
            {
                float scanline_intersection_x_in_pixel =
                    sqrtf(radius_squared - int_next_y * int_next_y);
                scanline_intersection_x_in_pixel -= (int32_t)scanline_intersection_x_in_pixel;
                uint8_t alpha = 255.0 * (scanline_intersection_x_in_pixel * (y - int_y)) + 0.5;
                fill_circle_row_pair_edge_points(center, diameter, int_x, int_y, alpha);
                fill_circle_row_pair_edge_points(center, diameter, int_y, int_x, alpha);
                alpha = 255.0 * (1.0 -
                    (1.0 - scanline_intersection_x_in_pixel) * (next_y - int_next_y)) + 0.5;
                fill_circle_row_pair(center, diameter, int_x, int_next_y, alpha);
                fill_circle_row_pair(center, diameter, int_next_y, int_x, alpha);
                int_y = int_next_y;
            }
            if (int_x >= int_y)
            {
                break;
            }
            int_x = int_next_x;
            y = next_y;
        }
    }
    window->dpi_data->icons = rasterization_memory;
    for (size_t icon_index = 0; icon_index < 6; ++icon_index)
    {
        rasterize_piece_icon(window->dpi_data, icon_index, icon_baseline_offset, 255, 0);
    }
    for (size_t icon_index = 6; icon_index < 12; ++icon_index)
    {
        rasterize_piece_icon(window->dpi_data, icon_index, icon_baseline_offset, 0, 255);
    }
    rasterization_memory =
        (void*)((uintptr_t)rasterization_memory + 12 * pixels_per_icon * sizeof(Grey));
    for (size_t i = 0; i < ARRAY_COUNT(g_codepoints); ++i)
    {
        char codepoint = g_codepoints[i];
        FT_Load_Glyph(g_text_face, FT_Get_Char_Index(g_text_face, codepoint), FT_LOAD_RENDER);
        Glyph*rasterization = window->dpi_data->glyphs + codepoint - g_codepoints[0];
        rasterization->bitmap = g_text_face->glyph->bitmap;
        rasterization->bitmap.buffer = rasterization_memory;
        size_t rasterization_size =
            g_text_face->glyph->bitmap.rows * g_text_face->glyph->bitmap.pitch;
        memcpy(rasterization->bitmap.buffer, g_text_face->glyph->bitmap.buffer, rasterization_size);
        rasterization->left_bearing = g_text_face->glyph->bitmap_left;
        rasterization->top_bearing = g_text_face->glyph->bitmap_top;
        rasterization->advance = g_text_face->glyph->metrics.horiAdvance;
        rasterization_memory = (void*)((uintptr_t)rasterization_memory + rasterization_size);
    }
    window->dpi_data->text_line_height = ROUND26_6_TO_PIXEL(g_text_face->size->metrics.height);
    window->dpi_data->text_control_height = ROUND26_6_TO_PIXEL(g_text_face->size->metrics.ascender -
        2 * g_text_face->size->metrics.descender);
    FT_Load_Glyph(g_text_face, FT_Get_Char_Index(g_text_face, 'M'), FT_LOAD_DEFAULT);
    window->dpi_data->text_control_padding =
        window->dpi_data->text_control_height - g_text_face->glyph->bitmap.rows / 2;
    window->dpi_data->digit_input_width = 0;
    for (char digit = '0'; digit <= '9'; ++digit)
    {
        window->dpi_data->digit_input_width = max32(window->dpi_data->digit_input_width,
            window->dpi_data->glyphs[digit - g_codepoints[0]].bitmap.width);
    }
    window->dpi_data->digit_input_width *= 2;
}

void free_dpi_data(Window*window)
{
    --window->dpi_data->reference_count;
    if (!window->dpi_data->reference_count)
    {
        FREE_MEMORY(window->dpi_data->circles);
    }
}

uint32_t get_string_length(Glyph*rasterizations, char*string)
{
    FT_Pos out = 0;
    FT_UInt previous_glyph_index = 0;
    while (*string)
    {
        FT_UInt glyph_index = FT_Get_Char_Index(g_text_face, *string);
        FT_Vector kerning_with_previous_glyph;
        FT_Get_Kerning(g_text_face, previous_glyph_index, glyph_index, FT_KERNING_DEFAULT,
            &kerning_with_previous_glyph);
        out += kerning_with_previous_glyph.x + rasterizations[*string - g_codepoints[0]].advance;
        previous_glyph_index = glyph_index;
        ++string;
    }
    return ROUND26_6_TO_PIXEL(out);
}

void draw_rasterization(Window*window, Glyph*rasterization, int32_t origin_x, int32_t origin_y,
    Color color)
{
    int32_t min_x_in_window = origin_x + rasterization->left_bearing;
    int32_t min_y_in_window = origin_y - rasterization->top_bearing;
    int32_t max_x_in_bitmap = min32(rasterization->bitmap.width, window->width - min_x_in_window);
    int32_t max_y_in_bitmap = min32(rasterization->bitmap.rows, window->height - min_y_in_window);
    Color*bitmap_row_min_x_in_window =
        window->pixels + min_y_in_window * window->width + min_x_in_window;
    uint8_t*bitmap_row = rasterization->bitmap.buffer;
    for (int32_t y_in_bitmap = 0; y_in_bitmap < max_y_in_bitmap; ++y_in_bitmap)
    {
        for (int32_t x_in_bitmap = 0; x_in_bitmap < max_x_in_bitmap; ++x_in_bitmap)
        {
            Color*window_pixel = bitmap_row_min_x_in_window + x_in_bitmap;
            uint32_t alpha = bitmap_row[x_in_bitmap];
            uint32_t lerp_term = (255 - alpha) * window_pixel->alpha;
            window_pixel->red = (color.red * alpha) / 255 + (lerp_term * window_pixel->red) / 65025;
            window_pixel->green =
                (color.green * alpha) / 255 + (lerp_term * window_pixel->green) / 65025;
            window_pixel->blue =
                (color.blue * alpha) / 255 + (lerp_term * window_pixel->blue) / 65025;
            window_pixel->alpha = alpha + lerp_term / 255;
        }
        bitmap_row += rasterization->bitmap.width;
        bitmap_row_min_x_in_window += window->width;
    }
}

void draw_string(Window*window, char*string, int32_t origin_x, int32_t origin_y, Color color)
{
    FT_Pos advance = origin_x << 6;
    FT_UInt previous_glyph_index = 0;
    while (*string)
    {
        FT_UInt glyph_index = FT_Get_Char_Index(g_text_face, *string);
        FT_Vector kerning_with_previous_glyph;
        FT_Get_Kerning(g_text_face, previous_glyph_index, glyph_index, FT_KERNING_DEFAULT,
            &kerning_with_previous_glyph);
        advance += kerning_with_previous_glyph.x;
        Glyph*rasterization = window->dpi_data->glyphs + *string - g_codepoints[0];
        draw_rasterization(window, rasterization, ROUND26_6_TO_PIXEL(advance), origin_y, color);
        advance += rasterization->advance;
        previous_glyph_index = glyph_index;
        ++string;
    }
}

void draw_circle(Window*window, Circle*rasterization, int32_t min_x, int32_t center_y, Color color)
{
    int32_t diameter = 2 * rasterization->radius;
    int32_t max_x_in_bitmap = min32(diameter, window->width - min_x);
    int32_t min_y = center_y - rasterization->radius;
    int32_t max_y_in_bitmap = min32(diameter, window->height - min_y);
    Color*bitmap_row_min_x_in_window = window->pixels + min_y * window->width + min_x;
    uint8_t*circle_row = rasterization->alpha;
    for (int32_t y_in_bitmap = 0; y_in_bitmap < max_y_in_bitmap; ++y_in_bitmap)
    {
        for (int32_t x_in_bitmap = 0; x_in_bitmap < max_x_in_bitmap; ++x_in_bitmap)
        {
            Color*window_pixel = bitmap_row_min_x_in_window + x_in_bitmap;
            uint32_t alpha = circle_row[x_in_bitmap];
            uint32_t lerp_term = (255 - alpha) * window_pixel->alpha;
            window_pixel->red = (color.red * alpha) / 255 + (lerp_term * window_pixel->red) / 65025;
            window_pixel->green =
                (color.green * alpha) / 255 + (lerp_term * window_pixel->green) / 65025;
            window_pixel->blue =
                (color.blue * alpha) / 255 + (lerp_term * window_pixel->blue) / 65025;
            window_pixel->alpha = alpha + lerp_term / 255;
        }
        circle_row += diameter;
        bitmap_row_min_x_in_window += window->width;
    }
}

Color tint_color(Color color, Color tint)
{
    return (Color) { .red = (color.red * tint.red) / 255, .green = (color.green * tint.green) / 255,
        .blue = (color.blue * tint.blue) / 255, .alpha = 255 };
}

void color_square_with_index(Window*window, Board*board, Color tint, uint8_t square_index)
{
    if (square_index < RANK_COUNT * FILE_COUNT)
    {
        uint8_t rank = RANK(square_index);
        uint8_t file = FILE(square_index);
        Color color;
        if ((rank + file) % 2)
        {
            color = tint_color(g_dark_square_gray, tint);
        }
        else
        {
            color = tint;
        }
        draw_rectangle(window, board->min_x + window->dpi_data->square_size * file,
            board->min_y + window->dpi_data->square_size * rank, window->dpi_data->square_size,
            window->dpi_data->square_size, color);
    }
}

void draw_button(Window*window, Button*button, Color cell_color, Color label_color)
{
    draw_rectangle(window, button->min_x, button->min_y, button->width,
        window->dpi_data->text_control_height, cell_color);
    draw_rectangle_border(window, button->min_x, button->min_y, button->width,
        window->dpi_data->text_control_height);
    draw_string(window, button->label,
        button->min_x +
            (button->width - get_string_length(window->dpi_data->glyphs, button->label)) / 2,
        button->min_y + window->dpi_data->text_line_height, label_color);
}

void draw_active_button(Window*window, Control*button)
{
    Color color;
    if (window->clicked_control_id == button->base_id)
    {
        color = tint_color(g_dark_square_gray, g_clicked_red);
    }
    else if (window->hovered_control_id == button->base_id)
    {
        color = tint_color(g_dark_square_gray, g_hovered_blue);
    }
    else
    {
        color = g_dark_square_gray;
    }
    draw_button(window, &button->button, color, g_black);
}

void clear_window(Window*window)
{
    size_t pixel_count = window->width * window->height;
    for (size_t i = 0; i < pixel_count; ++i)
    {
        window->pixels[i].value = 0xffffffff;
    }
}

void draw_controls(Window*window)
{
    clear_window(window);
    for (size_t control_index = 0; control_index < window->control_count; ++control_index)
    {
        Control*control = window->controls + control_index;
        switch (control->control_type)
        {
        case CONTROL_BUTTON:
        {
            draw_active_button(window, control);
            break;
        }
        case CONTROL_RADIO:
        {
            draw_string(window, "Color to play as:", control->radio.min_x,
                control->radio.label_baseline_y, g_black);
            int32_t option_label_x = control->radio.min_x + window->dpi_data->text_control_padding +
                2 * window->dpi_data->circles[2].radius;
            int32_t white_circle_center_x = control->radio.min_x +
                window->dpi_data->circles[2].radius - window->dpi_data->circles[1].radius;
            int32_t option_baseline_y = control->radio.label_baseline_y;
            for (size_t option_index = 0; option_index < ARRAY_COUNT(g_radio_labels);
                ++option_index)
            {
                option_baseline_y += window->dpi_data->text_line_height;
                int32_t circle_center_y = option_baseline_y - window->dpi_data->circles[2].radius;
                draw_circle(window, window->dpi_data->circles + 2, control->radio.min_x, 
                   circle_center_y, g_black);
                uint8_t option_id = control->base_id + option_index;
                Color color;
                if (window->clicked_control_id == option_id)
                {
                    color = g_clicked_red;
                }
                else if (window->hovered_control_id == option_id)
                {
                    color = g_hovered_blue;
                }
                else
                {
                    color = g_white;
                }
                draw_circle(window, window->dpi_data->circles + 1, white_circle_center_x,
                    circle_center_y, color);
                if (option_index != g_engine_player_index)
                {
                    draw_circle(window, window->dpi_data->circles,
                        control->radio.min_x + window->dpi_data->circles[2].radius -
                            window->dpi_data->circles[0].radius,
                        circle_center_y, g_black);
                }
                draw_string(window, g_radio_labels[option_index], option_label_x, option_baseline_y,
                    g_black);
            }
            break;
        }
        case CONTROL_TIME_INPUT:
        {
            int32_t digit_input_min_x = control->time_input.min_x;
            int32_t text_min_y = control->time_input.min_y + window->dpi_data->text_line_height;
            size_t clicked_digit_index = window->clicked_control_id - control->base_id;
            size_t hovered_digit_index = window->hovered_control_id - control->base_id;
            uint8_t selected_digit_index;
            if (g_selected_digit_id >= control->base_id)
            {
                selected_digit_index = g_selected_digit_id - control->base_id;
            }
            else
            {
                selected_digit_index = UINT8_MAX;
            }
            for (size_t digit_index = 0; digit_index < control->time_input.digit_count;
                ++digit_index)
            {
                DigitInput*digit_input = control->time_input.digits + digit_index;
                draw_rectangle_border(window, digit_input_min_x, control->time_input.min_y,
                    window->dpi_data->digit_input_width, window->dpi_data->text_control_height);
                if (digit_index == clicked_digit_index)
                {
                    draw_rectangle(window, digit_input_min_x, control->time_input.min_y,
                        window->dpi_data->digit_input_width, window->dpi_data->text_control_height,
                        g_clicked_red);
                }
                else if (digit_index == hovered_digit_index)
                {
                    draw_rectangle(window, digit_input_min_x, control->time_input.min_y,
                        window->dpi_data->digit_input_width, window->dpi_data->text_control_height,
                        g_hovered_blue);
                }
                Color digit_color;
                if (digit_index == selected_digit_index)
                {
                    digit_color = (Color) { .red = 255, .blue = 0, .green = 0, .alpha = 255 };
                }
                else
                {
                    digit_color = g_black;
                }
                Glyph*rasterization =
                    window->dpi_data->glyphs + digit_input->digit + '0' - g_codepoints[0];
                draw_rasterization(window, rasterization, digit_input_min_x +
                    (window->dpi_data->digit_input_width - rasterization->bitmap.width) / 2,
                    text_min_y, digit_color);
                digit_input_min_x += window->dpi_data->digit_input_width +
                    window->dpi_data->control_border_thickness;
                if (digit_input->is_before_colon)
                {
                    Glyph*colon = window->dpi_data->glyphs + ':' - g_codepoints[0];
                    uint32_t colon_space = 2 * ROUND26_6_TO_PIXEL(colon->advance);
                    draw_rasterization(window, colon, 
                        digit_input_min_x + (colon_space - colon->bitmap.width) / 2, text_min_y,
                        g_black);
                    digit_input_min_x += window->dpi_data->control_border_thickness + colon_space;
                }
            }
            draw_string(window, control->time_input.label, control->time_input.min_x,
                control->time_input.min_y + window->dpi_data->text_line_height -
                    window->dpi_data->control_border_thickness -
                    window->dpi_data->text_control_height,
                g_black);
        }
        }
    }
}

void set_control_ids(Window*window)
{
    uint8_t id = 0;
    for (size_t control_index = 0; control_index < window->control_count; ++control_index)
    {
        Control*control = window->controls + control_index;
        control->base_id = id;
        switch (control->control_type)
        {
        case CONTROL_BOARD:
        {
            id += RANK_COUNT * FILE_COUNT;
            break;
        }
        case CONTROL_BUTTON:
        {
            ++id;
            break;
        }
        case CONTROL_PROMOTION_SELECTOR:
        {
            id += ARRAY_COUNT(g_promotion_options);
            break;
        }
        case CONTROL_RADIO:
        {
            id += ARRAY_COUNT(g_radio_labels);
            break;
        }
        case CONTROL_TIME_INPUT:
        {
            id += control->time_input.digit_count;
        }
        }
    }
}

bool setup_window_handle_left_mouse_button_up(Window*window, int32_t cursor_x, int32_t cursor_y)
{
    if (window->clicked_control_id != NULL_CONTROL)
    {
        uint8_t id = id_of_control_cursor_is_over(window, cursor_x, cursor_y);
        if (id == window->clicked_control_id)
        {
            window->clicked_control_id = NULL_CONTROL;
            if (id == window->controls[SETUP_WINDOW_START_GAME].base_id)
            {
                return true;
            }
            if (id >= window->controls[SETUP_WINDOW_PLAYER_COLOR].base_id &&
                id < window->controls[SETUP_WINDOW_PLAYER_COLOR].base_id +
                    ARRAY_COUNT(g_radio_labels))
            {
                g_engine_player_index = !(id - window->controls[SETUP_WINDOW_PLAYER_COLOR].base_id);
            }
            else
            {
                g_selected_digit_id = id;
            }
            return false;
        }
        else
        {
            window->clicked_control_id = NULL_CONTROL;
        }
    }
    g_selected_digit_id = NULL_CONTROL;
    return false;
}

bool handle_time_input_key_down(Control*time_input, KeyId key)
{
    if (g_selected_digit_id >= time_input->base_id &&
        g_selected_digit_id < time_input->base_id + time_input->time_input.digit_count)
    {
        size_t digit_index = g_selected_digit_id - time_input->base_id;
        if (key >= KEY_0 && key < KEY_0 + 10)
        {
            time_input->time_input.digits[digit_index].digit = key - KEY_0;
            if (digit_index + 1 < time_input->time_input.digit_count)
            {
                ++g_selected_digit_id;
            }
            else
            {
                g_selected_digit_id = NULL_CONTROL;
            }
            return true;
        }
        switch (key)
        {
        case KEY_BACKSPACE:
        {
            time_input->time_input.digits[digit_index].digit = 0;
            if (digit_index)
            {
                --g_selected_digit_id;
            }
            return true;
        }
        case KEY_DELETE:
        {
            time_input->time_input.digits[digit_index].digit = 0;
            return true;
        }
        case KEY_LEFT:
        {
            if (digit_index)
            {
                --g_selected_digit_id;
            }
            return true;
        }
        case KEY_RIGHT:
        {
            if (digit_index + 1 < time_input->time_input.digit_count)
            {
                ++g_selected_digit_id;
            }
            return true;
        }
        }
    }
    return false;
}

bool setup_window_handle_key_down(Window*window, uint8_t digit)
{
    if (handle_time_input_key_down(window->controls + SETUP_WINDOW_TIME, digit))
    {
        return true;
    }
    else if (handle_time_input_key_down(window->controls + SETUP_WINDOW_INCREMENT, digit))
    {
        return true;
    }
    return false;
}

void set_setup_window_dpi(uint16_t dpi)
{
    Window*window = g_windows + WINDOW_SETUP;
    set_dpi_data(window, dpi);
    RadioButtons*player_color = &window->controls[SETUP_WINDOW_PLAYER_COLOR].radio;
    player_color->min_x = window->dpi_data->text_control_height;
    TimeInput*time = &window->controls[SETUP_WINDOW_TIME].time_input;
    time->min_x =
        window->dpi_data->control_border_thickness + window->dpi_data->text_control_height;
    TimeInput*increment = &window->controls[SETUP_WINDOW_INCREMENT].time_input;
    increment->min_x = time->min_x;
    Button*start_game_button = &window->controls[SETUP_WINDOW_START_GAME].button;
    start_game_button->min_x = time->min_x;
    start_game_button->width = max32(max32(5 * window->dpi_data->digit_input_width +
        4 * ROUND26_6_TO_PIXEL(window->dpi_data->glyphs[':' - g_codepoints[0]].advance) +
            6 * window->dpi_data->control_border_thickness,
        get_string_length(window->dpi_data->glyphs, increment->label)),
        get_string_length(window->dpi_data->glyphs, start_game_button->label) +
            2 * window->dpi_data->text_control_padding);
    window->width = 2 *(window->dpi_data->text_control_height +
        window->dpi_data->control_border_thickness) + start_game_button->width;
    player_color->label_baseline_y = 2 * window->dpi_data->text_line_height;
    time->min_y = player_color->label_baseline_y + 3 * window->dpi_data->text_line_height +
        window->dpi_data->text_control_height + window->dpi_data->control_border_thickness;
    increment->min_y = time->min_y + window->dpi_data->text_line_height +
        2 * (window->dpi_data->control_border_thickness + window->dpi_data->text_control_height);
    start_game_button->min_y = increment->min_y + 
        2 * (window->dpi_data->control_border_thickness + window->dpi_data->text_control_height);
    window->height = start_game_button->min_y + window->dpi_data->control_border_thickness +
        2 * window->dpi_data->text_control_height;
}

void init_setup_window(uint16_t dpi)
{
    Window*window = g_windows + WINDOW_SETUP;
    window->control_count = SETUP_WINDOW_CONTROL_COUNT;
    Control*control = window->controls + SETUP_WINDOW_PLAYER_COLOR;
    control->control_type = CONTROL_RADIO;
    g_engine_player_index = PLAYER_INDEX_BLACK;
    control = window->controls + SETUP_WINDOW_TIME;
    control->control_type = CONTROL_TIME_INPUT;
    control->time_input.label = "Time:";
    control->time_input.digits = g_time_control;
    control->time_input.digit_count = ARRAY_COUNT(g_time_control);
    control = window->controls + SETUP_WINDOW_INCREMENT;
    control->control_type = CONTROL_TIME_INPUT;
    control->time_input.label = "Increment:";
    control->time_input.digits = g_increment;
    control->time_input.digit_count = ARRAY_COUNT(g_increment);
    control = window->controls + SETUP_WINDOW_START_GAME;
    control->control_type = CONTROL_BUTTON;
    control->button.label = "Start";
    set_control_ids(window);
    set_setup_window_dpi(dpi);
    window->pixels = 0;
    window->pixel_buffer_capacity = 0;
    window->hovered_control_id = NULL_CONTROL;
    window->clicked_control_id = NULL_CONTROL;
    g_selected_digit_id = NULL_CONTROL;
}

void set_start_window_dpi(char*text, uint16_t dpi)
{
    Window*window = g_windows + WINDOW_START;
    set_dpi_data(window, dpi);
    uint32_t button_padding = 2 * window->dpi_data->text_control_padding;
    int32_t button_min_x =
        window->dpi_data->text_control_height + window->dpi_data->control_border_thickness;
    int32_t button_min_y = window->dpi_data->control_border_thickness;
    if (text[0])
    {
        button_min_y += 3 * window->dpi_data->text_line_height;
        window->width = get_string_length(window->dpi_data->glyphs, text);
    }
    else
    {
        button_min_y += window->dpi_data->text_control_height;
        window->width = 0;
    }
    for (size_t i = 0; i < START_CONTROL_COUNT; ++i)
    {
        Button*button = &window->controls[i].button;
        button->min_x = button_min_x;
        button->width = button_padding + get_string_length(window->dpi_data->glyphs, button->label);
        button_min_x += button->width + window->dpi_data->control_border_thickness;
        button->min_y = button_min_y;
    }
    window->width = window->dpi_data->text_control_height +
        max32(window->width, button_min_x + window->dpi_data->control_border_thickness);
    window->height = button_min_y + window->dpi_data->text_control_height +
        window->dpi_data->control_border_thickness + window->dpi_data->text_control_height;
}

void init_start_window(char*text, uint16_t dpi)
{
    Window*window = g_windows + WINDOW_START;
    window->control_count = START_CONTROL_COUNT;
    Control*button = window->controls + START_NEW_GAME;
    button->control_type = CONTROL_BUTTON;
    button->button.label = "New game";
    button = window->controls + START_LOAD_GAME;
    button->control_type = CONTROL_BUTTON;
    button->button.label = "Load game";
    button = window->controls + START_QUIT;
    button->control_type = CONTROL_BUTTON;
    button->button.label = "Quit";
    set_control_ids(window);
    set_start_window_dpi(text, dpi);
    window->hovered_control_id = NULL_CONTROL;
    window->clicked_control_id = NULL_CONTROL;
    window->pixels = 0;
    window->pixel_buffer_capacity = 0;
}

void draw_start_window(char*text)
{
    Window*window = g_windows + WINDOW_START;
    draw_controls(window);
    draw_string(window, text, window->dpi_data->text_control_height,
        2 * window->dpi_data->text_line_height, g_black);
}

#define DRAW_BY_50_CAN_BE_CLAIMED() (g_draw_by_50_count > 100)

bool main_window_handle_left_mouse_button_down(int32_t cursor_x, int32_t cursor_y)
{
    Window*window = g_windows + WINDOW_MAIN;
    window->clicked_control_id = window->hovered_control_id;
    if (window->hovered_control_id == window->controls[MAIN_WINDOW_CLAIM_DRAW].base_id)
    {
        if (!DRAW_BY_50_CAN_BE_CLAIMED())
        {
            window->clicked_control_id = NULL_CONTROL;
        }
    }
    else
    {
        uint8_t board_base_id = window->controls[MAIN_WINDOW_BOARD].base_id;
        if (window->hovered_control_id >= board_base_id &&
            window->hovered_control_id < board_base_id + 64 &&
            (GET_POSITION(g_current_position_index)->active_player_index == g_engine_player_index ||
                g_is_promoting))
        {
            window->clicked_control_id = NULL_CONTROL;
        }
        else
        {
            uint8_t promotion_selector_base_id =
                window->controls[MAIN_WINDOW_PROMOTION_SELECTOR].base_id;
            if (window->hovered_control_id >= promotion_selector_base_id &&
                window->hovered_control_id < promotion_selector_base_id +
                ARRAY_COUNT(g_promotion_options) && !g_is_promoting)
            {
                window->clicked_control_id = NULL_CONTROL;
            }
        }
    }
    return window->clicked_control_id != NULL_CONTROL;
}

GUIAction main_window_handle_left_mouse_button_up(int32_t cursor_x, int32_t cursor_y)
{
    Window*window = g_windows + WINDOW_MAIN;
    if (window->clicked_control_id == NULL_CONTROL)
    {
        return ACTION_NONE;
    }
    uint8_t id = id_of_control_cursor_is_over(window, cursor_x, cursor_y);
    if (id == window->clicked_control_id)
    {
        window->clicked_control_id = NULL_CONTROL;
        if (id == window->controls[MAIN_WINDOW_SAVE_GAME].base_id)
        {
            return ACTION_SAVE_GAME;
        }
        if (id == window->controls[MAIN_WINDOW_CLAIM_DRAW].base_id)
        {
            return ACTION_CLAIM_DRAW;
        }
        Position*current_position = GET_POSITION(g_current_position_index);
        if (g_selected_piece_index == NULL_PIECE)
        {
            uint8_t square_index =
                SCREEN_SQUARE_INDEX(id - window->controls[MAIN_WINDOW_BOARD].base_id);
            uint8_t selected_piece_index = current_position->squares[square_index];
            if (PLAYER_INDEX(selected_piece_index) == current_position->active_player_index)
            {
                g_selected_move_index = &current_position->first_move_index;
                while (*g_selected_move_index != NULL_POSITION)
                {
                    Position*move = GET_POSITION(*g_selected_move_index);
                    if (move->squares[square_index] != selected_piece_index)
                    {
                        g_selected_piece_index = selected_piece_index;
                        return ACTION_REDRAW;
                    }
                    g_selected_move_index = &move->next_move_index;
                }
            }
        }
        else
        {
            uint8_t promotion_selector_base_id =
                window->controls[MAIN_WINDOW_PROMOTION_SELECTOR].base_id;
            if (id >= promotion_selector_base_id &&
                id < promotion_selector_base_id + ARRAY_COUNT(g_promotion_options))
            {
                PieceType selected_piece_type =
                    g_promotion_options[id - promotion_selector_base_id];
                Position*move = GET_POSITION(*g_selected_move_index);
                while (move->pieces[g_selected_piece_index].piece_type != selected_piece_type)
                {
                    g_selected_move_index = &move->next_move_index;
                    move = GET_POSITION(move->next_move_index);
                }
                end_turn();
                g_is_promoting = false;
            }
            else
            {
                Piece current_position_selected_piece =
                    current_position->pieces[g_selected_piece_index];
                uint8_t square_index =
                    SCREEN_SQUARE_INDEX(id - window->controls[MAIN_WINDOW_BOARD].base_id);
                if (current_position_selected_piece.square_index != square_index)
                {
                    uint16_t*piece_first_move_index = g_selected_move_index;
                    do
                    {
                        Position*move = GET_POSITION(*g_selected_move_index);
                        if (move->squares[square_index] == g_selected_piece_index)
                        {
                            if (current_position_selected_piece.piece_type !=
                                move->pieces[g_selected_piece_index].piece_type)
                            {
                                g_is_promoting = true;
                                return ACTION_REDRAW;
                            }
                            return end_turn();
                        }
                        g_selected_move_index = &move->next_move_index;
                    } while (*g_selected_move_index != NULL_POSITION);
                    g_selected_move_index = piece_first_move_index;
                }
            }
        }
    }
    window->clicked_control_id = NULL_CONTROL;
    return ACTION_REDRAW;
}

void set_main_window_dpi(uint16_t dpi)
{
    Window*window = g_windows + WINDOW_MAIN;
    set_dpi_data(window, dpi);
    Board*board = &window->controls[MAIN_WINDOW_BOARD].board;
    board->min_x = window->dpi_data->square_size + window->dpi_data->control_border_thickness;
    board->min_y = board->min_x;
    window->controls[MAIN_WINDOW_PROMOTION_SELECTOR].promotion_selector.min_x =
        board->min_x + 2 * window->dpi_data->square_size;
    Button*save_button = &window->controls[MAIN_WINDOW_SAVE_GAME].button;
    save_button->min_x = board->min_x + 8 * window->dpi_data->square_size +
        window->dpi_data->text_control_height + 2 * window->dpi_data->control_border_thickness;
    save_button->min_y = board->min_y + 4 * window->dpi_data->square_size -
        (window->dpi_data->text_control_height + window->dpi_data->control_border_thickness / 2);
    Button*claim_draw_button = &window->controls[MAIN_WINDOW_CLAIM_DRAW].button;
    claim_draw_button->min_x = save_button->min_x;
    claim_draw_button->min_y = save_button->min_y + window->dpi_data->text_control_height +
        window->dpi_data->control_border_thickness;
    save_button->width = 2 * window->dpi_data->text_control_padding +
        max32(get_string_length(window->dpi_data->glyphs, save_button->label),
            get_string_length(window->dpi_data->glyphs, claim_draw_button->label));
    claim_draw_button->width = save_button->width;
    window->width = save_button->min_x + save_button->width +
        window->dpi_data->text_control_height + window->dpi_data->control_border_thickness;
    window->height = board->min_y + 9 * window->dpi_data->square_size +
        window->dpi_data->control_border_thickness;
}

void init_main_window(uint16_t dpi)
{
    Window*window = g_windows + WINDOW_MAIN;
    window->control_count = MAIN_WINDOW_CONTROL_COUNT;
    window->controls = g_main_window_controls;
    Control*control = window->controls + MAIN_WINDOW_BOARD;
    control->control_type = CONTROL_BOARD;
    control = window->controls + MAIN_WINDOW_PROMOTION_SELECTOR;
    control->control_type = CONTROL_PROMOTION_SELECTOR;
    control = window->controls + MAIN_WINDOW_SAVE_GAME;
    control->control_type = CONTROL_BUTTON;
    control->button.label = "Save game";
    control = window->controls + MAIN_WINDOW_CLAIM_DRAW;
    control->control_type = CONTROL_BUTTON;
    control->button.label = "Claim draw";
    set_control_ids(window);
    set_main_window_dpi(dpi);
    window->pixels = 0;
    window->pixel_buffer_capacity = 0;
    window->hovered_control_id = NULL_CONTROL;
    window->clicked_control_id = NULL_CONTROL;
}

void draw_player(int32_t captured_pieces_min_y, int32_t timer_text_origin_y, uint8_t player_index,
    bool draw_captured_pieces)
{
    Position*current_position = GET_POSITION(g_current_position_index);
    Window*window = g_windows + WINDOW_MAIN;
    Control*board = window->controls + MAIN_WINDOW_BOARD;
    int32_t captured_piece_min_x = board->board.min_x;
    uint8_t player_pieces_index = PLAYER_PIECES_INDEX(player_index);
    uint8_t max_piece_index = player_pieces_index + 16;
    for (size_t piece_index = player_pieces_index; piece_index < max_piece_index; ++piece_index)
    {
        Piece piece = current_position->pieces[piece_index];
        if (piece.square_index == NULL_SQUARE)
        {
            if (draw_captured_pieces)
            {
                draw_icon(window, captured_piece_min_x, captured_pieces_min_y, player_index,
                    piece.piece_type);
                captured_piece_min_x += window->dpi_data->square_size / 2;
            }
        }
        else
        {
            uint8_t screen_square_index = SCREEN_SQUARE_INDEX(piece.square_index);
            int32_t icon_min_x =
                board->board.min_x + window->dpi_data->square_size * FILE(screen_square_index);
            int32_t icon_min_y =
                board->board.min_y + window->dpi_data->square_size * RANK(screen_square_index);
            if (g_selected_piece_index == piece_index)
            {
                size_t pixels_per_icon =
                    window->dpi_data->square_size * window->dpi_data->square_size;
                Grey*icon = window->dpi_data->icons +
                    (player_index * PIECE_TYPE_COUNT + piece.piece_type) * pixels_per_icon;
                int32_t max_x_in_icon =
                    min32(window->dpi_data->square_size, window->width - icon_min_x);
                int32_t max_y_in_icon =
                    min32(window->dpi_data->square_size, window->height - icon_min_y);
                Color*icon_row_min_x_in_window =
                    window->pixels + icon_min_y * window->width + icon_min_x;
                icon += pixels_per_icon;
                for (int32_t y_in_icon = 0; y_in_icon < max_y_in_icon; ++y_in_icon)
                {
                    icon -= window->dpi_data->square_size;
                    for (int32_t x_in_icon = 0; x_in_icon < max_x_in_icon; ++x_in_icon)
                    {
                        Color*window_pixel = icon_row_min_x_in_window + x_in_icon;
                        Grey icon_pixel = icon[x_in_icon];
                        uint32_t lerp_term = (255 - icon_pixel.alpha) * window_pixel->alpha;
                        window_pixel->red = ((icon_pixel.grey + 255) * icon_pixel.alpha) / 510 +
                            (lerp_term * window_pixel->red) / 65025;
                        uint32_t blue_green_icon_term = (icon_pixel.grey * icon_pixel.alpha) / 510;
                        window_pixel->green =
                            blue_green_icon_term + (lerp_term * window_pixel->green) / 65025;
                        window_pixel->blue =
                            blue_green_icon_term + (lerp_term * window_pixel->blue) / 65025;
                        window_pixel->alpha = icon_pixel.alpha + lerp_term / 255;
                    }
                    icon_row_min_x_in_window += window->width;
                }
            }
            else
            {
                draw_icon(window, icon_min_x, icon_min_y, player_index, piece.piece_type);
            }
        }
    }
    uint64_t time = g_seconds_left[player_index];
    char time_text[9];
    size_t char_index = 9;
    while (char_index)
    {
        --char_index;
        time_text[char_index] = ':';
        uint32_t denomination = time % 60;
        --char_index;
        time_text[char_index] = denomination % 10 + '0';
        --char_index;
        time_text[char_index] = denomination / 10 + '0';
        time /= 60;
    }
    time_text[8] = 0;
    draw_string(window, time_text, window->controls[MAIN_WINDOW_SAVE_GAME].button.min_x,
        timer_text_origin_y, g_black);
}

void draw_main_window(void)
{
    Window*window = g_windows + WINDOW_MAIN;
    clear_window(window);
    Control*board = window->controls + MAIN_WINDOW_BOARD;
    uint32_t board_width = FILE_COUNT * window->dpi_data->square_size;
    draw_rectangle_border(window, board->board.min_x, board->board.min_y, board_width, board_width);
    for (size_t rank = 0; rank < RANK_COUNT; ++rank)
    {
        int32_t square_min_y = board->board.min_y + window->dpi_data->square_size * rank;
        for (size_t file = 0; file < FILE_COUNT; ++file)
        {
            if ((rank + file) % 2)
            {
                draw_rectangle(window, board->board.min_x + window->dpi_data->square_size * file,
                    square_min_y, window->dpi_data->square_size, window->dpi_data->square_size,
                    g_dark_square_gray);
            }
        }
    }
    color_square_with_index(window, &board->board, g_hovered_blue,
        window->hovered_control_id - board->base_id);
    color_square_with_index(window, &board->board, g_clicked_red,
        window->clicked_control_id - board->base_id);
    Control*save_game_button = window->controls + MAIN_WINDOW_SAVE_GAME;
    draw_active_button(window, save_game_button);
    if (DRAW_BY_50_CAN_BE_CLAIMED())
    {
        draw_active_button(window, window->controls + MAIN_WINDOW_CLAIM_DRAW);
    }
    else
    {
        draw_button(window, &window->controls[MAIN_WINDOW_CLAIM_DRAW].button, g_white,
            g_dark_square_gray);
    }
    if (g_is_promoting)
    {
        Control*promotion_selector = window->controls + MAIN_WINDOW_PROMOTION_SELECTOR;
        int32_t icon_min_x = promotion_selector->promotion_selector.min_x;
        draw_rectangle(window, icon_min_x - window->dpi_data->control_border_thickness, 0,
            window->dpi_data->control_border_thickness, window->dpi_data->square_size, g_black);
        for (size_t i = 0; i < ARRAY_COUNT(g_promotion_options); ++i)
        {
            uint8_t selection_id = promotion_selector->base_id + i;
            if (window->clicked_control_id == selection_id)
            {
                draw_rectangle(window, icon_min_x, 0, window->dpi_data->square_size,
                    window->dpi_data->square_size, g_clicked_red);
            }
            else if (window->hovered_control_id == selection_id)
            {
                draw_rectangle(window, icon_min_x, 0, window->dpi_data->square_size,
                    window->dpi_data->square_size, g_hovered_blue);
            }
            draw_icon(window, icon_min_x, 0, !g_engine_player_index, g_promotion_options[i]);
            icon_min_x += window->dpi_data->square_size;
        }
        draw_rectangle(window, icon_min_x, 0, window->dpi_data->control_border_thickness,
            window->dpi_data->square_size, g_black);
    }
    draw_player(board->board.min_y + board_width + window->dpi_data->control_border_thickness,
        board->board.min_y + window->dpi_data->text_line_height, g_engine_player_index, true);
    draw_player(0,
        board->board.min_y + 7 * window->dpi_data->square_size + window->dpi_data->text_line_height,
        !g_engine_player_index, !g_is_promoting);
}

void save_value(void**file_memory, void*value, uint32_t*file_size, size_t value_byte_count)
{
    *file_size += value_byte_count;
    for (size_t i = 0; i < value_byte_count; ++i)
    {
        **(uint8_t**)file_memory = ((uint8_t*)value)[i];
        *file_memory = (void*)((uintptr_t)*file_memory + 1);
    }
}

bool load_value(void**file_memory, void*out, uint32_t*file_size, size_t value_byte_count)
{
    if (*file_size < value_byte_count)
    {
        return false;
    }
    else
    {
        *file_size -= value_byte_count;
        for (size_t i = 0; i < value_byte_count; ++i)
        {
            ((uint8_t*)out)[i] = **(uint8_t**)file_memory;
            *file_memory = (void*)((uintptr_t)*file_memory + 1);
        }
        return true;
    }
}

#define SAVE_FILE_STATIC_PART_SIZE 119

uint32_t save_game(void*file_memory)
{
    uint64_t time_since_last_move = get_time() - g_last_move_time;
    uint32_t file_size = 0;
    save_value(&file_memory, &time_since_last_move, &file_size, sizeof(time_since_last_move));
    save_value(&file_memory, &g_time_increment, &file_size, sizeof(g_time_increment));
    Position*current_position = GET_POSITION(g_current_position_index);
    save_value(&file_memory, &current_position->en_passant_file, &file_size,
        sizeof(current_position->en_passant_file));
    uint8_t active_player_index = current_position->active_player_index;
    save_value(&file_memory, &active_player_index, &file_size, sizeof(active_player_index));
    for (size_t player_index = 0; player_index < 2; ++player_index)
    {
        save_value(&file_memory, g_times_left_as_of_last_move + player_index, &file_size,
            sizeof(g_times_left_as_of_last_move[player_index]));
        uint8_t player_pieces_index = PLAYER_PIECES_INDEX(player_index);
        Piece*player_pieces = current_position->pieces + player_pieces_index;
        for (size_t piece_index = 0; piece_index < 16; ++piece_index)
        {
            save_value(&file_memory, player_pieces + piece_index, &file_size, sizeof(Piece));
        }
    }
    save_value(&file_memory, &g_draw_by_50_count, &file_size, sizeof(g_draw_by_50_count));
    save_value(&file_memory, &g_engine_player_index, &file_size, sizeof(g_engine_player_index));
    save_value(&file_memory, &g_unique_state_count, &file_size, sizeof(g_unique_state_count));
    ASSERT(file_size <= SAVE_FILE_STATIC_PART_SIZE);
    BoardStateNode*external_nodes = EXTERNAL_STATE_NODES();
    for (size_t bucket_index = 0; bucket_index < g_state_bucket_count; ++bucket_index)
    {
        BoardStateNode*node = g_state_buckets + bucket_index;
        if (node->count && node->generation == g_state_generation)
        {
            while (true)
            {
                save_value(&file_memory, &node->state.square_mask, &file_size,
                    sizeof(node->state.square_mask));
                save_value(&file_memory, &node->state.occupied_square_hashes, &file_size,
                    sizeof(node->state.occupied_square_hashes));
                if (node->index_of_next_node != NULL_STATE)
                {
                    node = external_nodes + node->index_of_next_node;
                }
                else
                {
                    break;
                }
            }
        }
    }
    return file_size;
}

bool load_file(void*file_memory, uint32_t file_size)
{
    uint64_t time_since_last_move;
    if (!load_value(&file_memory, &time_since_last_move, &file_size, sizeof(time_since_last_move)))
    {
        return false;
    }
    if (!load_value(&file_memory, &g_time_increment, &file_size, sizeof(g_time_increment)))
    {
        return false;
    }
    Position*current_position = GET_POSITION(g_first_leaf_index);
    if (!load_value(&file_memory, &current_position->en_passant_file, &file_size,
        sizeof(current_position->en_passant_file)) ||
        current_position->en_passant_file > FILE_COUNT)
    {
        return false;
    }
    uint8_t active_player_index;
    if (!load_value(&file_memory, &active_player_index, &file_size, sizeof(active_player_index)) ||
        active_player_index > 1)
    {
        return false;
    }
    current_position->active_player_index = active_player_index;
    for (size_t player_index = 0; player_index < 2; ++player_index)
    {
        uint64_t*time_left_as_of_last_move = g_times_left_as_of_last_move + player_index;
        if (!load_value(&file_memory, time_left_as_of_last_move, &file_size,
            sizeof(*time_left_as_of_last_move)))
        {
            return false;
        }
        g_seconds_left[player_index] = *time_left_as_of_last_move / g_counts_per_second;
        Piece*player_pieces = current_position->pieces + PLAYER_PIECES_INDEX(player_index);
        for (size_t piece_index = 0; piece_index < 16; ++piece_index)
        {
            Piece*piece = player_pieces + piece_index;
            if (!load_value(&file_memory, piece, &file_size, sizeof(Piece)))
            {
                return false;
            }
            if (piece->square_index > NULL_SQUARE || piece->piece_type >= PIECE_TYPE_COUNT)
            {
                return false;
            }
        }
        player_pieces[A_ROOK_INDEX].piece_type = PIECE_ROOK;
        player_pieces[B_KNIGHT_INDEX].piece_type = PIECE_KNIGHT;
        player_pieces[C_BISHOP_INDEX].piece_type = PIECE_BISHOP;
        player_pieces[QUEEN_INDEX].piece_type = PIECE_QUEEN;
        player_pieces[KING_INDEX].piece_type = PIECE_KING;
        player_pieces[F_BISHOP_INDEX].piece_type = PIECE_BISHOP;
        player_pieces[G_KNIGHT_INDEX].piece_type = PIECE_KNIGHT;
        player_pieces[H_ROOK_INDEX].piece_type = PIECE_ROOK;
    }
    if (g_times_left_as_of_last_move[active_player_index] < time_since_last_move)
    {
        return false;
    }
    if (!load_value(&file_memory, &g_draw_by_50_count, &file_size, sizeof(g_draw_by_50_count)))
    {
        return false;
    }
    if (!load_value(&file_memory, &g_engine_player_index, &file_size,
        sizeof(g_engine_player_index)) || g_engine_player_index > PLAYER_INDEX_BLACK)
    {
        return false;
    }
    if (!load_value(&file_memory, &g_unique_state_count, &file_size, sizeof(g_unique_state_count)))
    {
        return false;
    }
    uint32_t bucket_count_bit_count;
    BIT_SCAN_REVERSE(&bucket_count_bit_count, g_unique_state_count);
    uint16_t state_bucket_count = 1 << bucket_count_bit_count;
    if (state_bucket_count < g_unique_state_count)
    {
        state_bucket_count = state_bucket_count << 1;
    }
    init_board_state_archive(state_bucket_count);
    for (uint16_t i = 0; i < g_unique_state_count; ++i)
    {
        BoardState state;
        if (!load_value(&file_memory, &state.square_mask, &file_size, sizeof(state.square_mask)))
        {
            return false;
        }
        for (size_t square_hash_index = 0;
            square_hash_index < ARRAY_COUNT(state.occupied_square_hashes); ++square_hash_index)
        {
            if (!load_value(&file_memory, state.occupied_square_hashes + square_hash_index,
                &file_size, sizeof(*state.occupied_square_hashes)))
            {
                return false;
            }
        }
        archive_board_state(&state);
    }
    for (size_t square_index = 0; square_index < ARRAY_COUNT(current_position->squares);
        ++square_index)
    {
        current_position->squares[square_index] = NULL_PIECE;
    }
    for (size_t piece_index = 0; piece_index < ARRAY_COUNT(current_position->pieces); ++piece_index)
    {
        Piece piece = current_position->pieces[piece_index];
        if (piece.square_index != NULL_SQUARE)
        {
            current_position->squares[piece.square_index] = piece_index;
        }
    }
    current_position->parent_index = NULL_POSITION;
    current_position->next_move_index = NULL_POSITION;
    SET_PREVIOUS_LEAF_INDEX(current_position, NULL_POSITION);
    SET_NEXT_LEAF_INDEX(current_position, NULL_POSITION);
    make_move_current(g_first_leaf_index);
    g_last_move_time = get_time() - time_since_last_move;
    g_selected_piece_index = NULL_PIECE;
    return true;
}

bool load_game(void*file_memory, uint32_t file_size)
{
    g_first_leaf_index = allocate_position();
    bool out = load_file(file_memory, file_size);
    if (out)
    {
        g_run_engine = true;
    }
    else
    {
        free_position(g_first_leaf_index);
    }
    return out;
}

void free_game(void)
{
    FREE_MEMORY(g_state_buckets);
    g_pool_cursor = 0;
    g_index_of_first_free_pool_position = NULL_POSITION;
}

void init_piece(Position*position, PieceType piece_type, uint8_t piece_index, uint8_t square_index,
    uint8_t player_index)
{
    piece_index += PLAYER_PIECES_INDEX(player_index);
    Piece*piece = position->pieces + piece_index;
    piece->square_index = square_index;
    piece->has_moved = false;
    piece->piece_type = piece_type;
    position->squares[square_index] = piece_index;
}

void init_game(void)
{
    g_selected_piece_index = NULL_PIECE;
    g_first_leaf_index = allocate_position();
    g_next_leaf_to_evaluate_index = g_first_leaf_index;
    Position*position = GET_POSITION(g_first_leaf_index);
    position->parent_index = NULL_POSITION;
    position->next_move_index = NULL_POSITION;
    SET_PREVIOUS_LEAF_INDEX(position, NULL_POSITION);
    SET_NEXT_LEAF_INDEX(position, NULL_POSITION);
    position->active_player_index = PLAYER_INDEX_WHITE;
    for (uint8_t player_index = 0; player_index < 2; ++player_index)
    {
        uint8_t player_pieces_index = PLAYER_PIECES_INDEX(player_index);
        uint8_t rank = KING_RANK(player_index);
        init_piece(position, PIECE_ROOK, A_ROOK_INDEX, SQUARE_INDEX(rank, 0), player_index);
        init_piece(position, PIECE_KNIGHT, B_KNIGHT_INDEX, SQUARE_INDEX(rank, 1), player_index);
        init_piece(position, PIECE_BISHOP, C_BISHOP_INDEX, SQUARE_INDEX(rank, 2), player_index);
        init_piece(position, PIECE_QUEEN, QUEEN_INDEX, SQUARE_INDEX(rank, 3), player_index);
        init_piece(position, PIECE_KING, KING_INDEX, SQUARE_INDEX(rank, 4), player_index);
        init_piece(position, PIECE_BISHOP, F_BISHOP_INDEX, SQUARE_INDEX(rank, 5), player_index);
        init_piece(position, PIECE_KNIGHT, G_KNIGHT_INDEX, SQUARE_INDEX(rank, 6), player_index);
        init_piece(position, PIECE_ROOK, H_ROOK_INDEX, SQUARE_INDEX(rank, 7), player_index);
        rank += FORWARD_DELTA(player_index);
        init_piece(position, PIECE_PAWN, 8, SQUARE_INDEX(rank, 0), player_index);
        init_piece(position, PIECE_PAWN, 9, SQUARE_INDEX(rank, 1), player_index);
        init_piece(position, PIECE_PAWN, 10, SQUARE_INDEX(rank, 2), player_index);
        init_piece(position, PIECE_PAWN, 11, SQUARE_INDEX(rank, 3), player_index);
        init_piece(position, PIECE_PAWN, 12, SQUARE_INDEX(rank, 4), player_index);
        init_piece(position, PIECE_PAWN, 13, SQUARE_INDEX(rank, 5), player_index);
        init_piece(position, PIECE_PAWN, 14, SQUARE_INDEX(rank, 6), player_index);
        init_piece(position, PIECE_PAWN, 15, SQUARE_INDEX(rank, 7), player_index);
        g_seconds_left[player_index] = g_time_control[4].digit + 10 * g_time_control[3].digit +
            60 * g_time_control[2].digit + 600 * g_time_control[1].digit +
            3600 * g_time_control[0].digit;
    }
    for (size_t square_index = 16; square_index < 48; ++square_index)
    {
        position->squares[square_index] = NULL_PIECE;
    }
    init_board_state_archive(32);
    g_selected_move_index = &g_first_leaf_index;
    make_move_current(g_first_leaf_index);
    Window*window = g_windows + WINDOW_MAIN;
    window->hovered_control_id = NULL_CONTROL;
    window->clicked_control_id = NULL_CONTROL;
    g_draw_by_50_count = 0;
    g_run_engine = true;
    g_is_promoting = false;
    g_times_left_as_of_last_move[0] = g_counts_per_second * g_seconds_left[0];
    g_times_left_as_of_last_move[1] = g_times_left_as_of_last_move[0];
    g_time_increment = g_counts_per_second * (g_increment[2].digit + 10 * g_increment[1].digit +
        60 * g_increment[0].digit);
    g_last_move_time = get_time();
}

void init(void*font_data, size_t text_font_data_size, size_t icon_font_data_size)
{
    FT_Init_FreeType(&g_freetype_library);
    FT_New_Memory_Face(g_freetype_library, font_data, text_font_data_size, 0, &g_text_face);
    FT_New_Memory_Face(g_freetype_library, (void*)((uintptr_t)font_data + text_font_data_size),
        text_font_data_size, 0, &g_icon_face);
    g_windows[WINDOW_START].controls = g_dialog_controls;
    g_position_pool = RESERVE_MEMORY(NULL_POSITION * sizeof(Position));
    g_index_of_first_free_pool_position = NULL_POSITION;
    g_pool_cursor = 0;
    for (size_t i = 0; i < ARRAY_COUNT(g_dpi_datas); ++i)
    {
        g_dpi_datas[i].reference_count = 0;
    }
    g_max_time = g_counts_per_second * 359999;
    DigitInput*digit = g_time_control;
    digit->digit = 0;
    digit->is_before_colon = true;
    ++digit;
    digit->digit = 1;
    digit->is_before_colon = false;
    ++digit;
    digit->digit = 0;
    digit->is_before_colon = true;
    ++digit;
    digit->digit = 0;
    digit->is_before_colon = false;
    ++digit;
    digit->digit = 0;
    digit->is_before_colon = false;
    digit = g_increment;
    digit->digit = 0;
    digit->is_before_colon = true;
    ++digit;
    digit->digit = 0;
    digit->is_before_colon = false;
    ++digit;
    digit->digit = 5;
    digit->is_before_colon = false;
    g_run_engine = false;
}