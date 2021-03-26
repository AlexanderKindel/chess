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

typedef struct Rasterization
{
    FT_Bitmap bitmap;
    FT_Int left_bearing;
    FT_Int top_bearing;
    FT_Pos advance;
} Rasterization;

FT_Library g_freetype_library;
FT_Face g_face;

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
    PIECE_ROOK,
    PIECE_KNIGHT,
    PIECE_BISHOP,
    PIECE_QUEEN,
    PIECE_PAWN,
    PIECE_KING,
    PIECE_TYPE_COUNT
} PieceType; 

PieceType g_promotion_options[] = { PIECE_QUEEN, PIECE_ROOK, PIECE_BISHOP, PIECE_KNIGHT };

typedef struct Piece
{
    uint8_t square_index;
    uint8_t times_moved;
    uint8_t piece_type;
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

typedef struct CircleRasterization
{
    uint8_t*alpha;
    uint32_t radius;
} CircleRasterization;

typedef struct DPIData
{
    CircleRasterization circle_rasterizations[3];
    Rasterization rasterizations['y' - ' ' + 1];
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
    uint8_t*circle_rasterizations[3];
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

#define COLUMNS_PER_ICON 60
#define PIXELS_PER_ICON COLUMNS_PER_ICON * COLUMNS_PER_ICON

typedef struct Game
{
    Position*position_pool;
    BoardStateNode*state_buckets;
    uint16_t*selected_move_index;
    Color icon_bitmaps[2][PIECE_TYPE_COUNT][PIXELS_PER_ICON];
    Control dialog_controls[sizeof(DialogControlCount)];
    Control main_window_controls[MAIN_WINDOW_CONTROL_COUNT];
    Window windows[WINDOW_COUNT];
    DPIData dpi_datas[WINDOW_COUNT];
    uint64_t times_left_as_of_last_move[2];
    uint64_t last_move_time;
    uint64_t time_increment;
    uint32_t square_size;
    uint32_t font_size;
    uint16_t seconds_left[2];
    uint16_t current_position_index;
    uint16_t first_leaf_index;
    uint16_t next_leaf_to_evaluate_index;
    uint16_t index_of_first_free_pool_position;
    uint16_t pool_cursor;
    uint16_t state_bucket_count;
    uint16_t external_state_node_count;
    uint16_t unique_state_count;
    uint16_t draw_by_50_count;
    DigitInput time_control[5];
    DigitInput increment[3];
    uint8_t state_generation;
    uint8_t selected_piece_index;
    uint8_t selected_digit_id;
    uint8_t engine_player_index;
    bool run_engine;
    bool is_promoting;
} Game;

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

void export_position_tree(Game*game)
{
    if (game->engine_player_index ==
        game->position_pool[game->current_position_index].active_player_index)
    {
        HANDLE file_handle = CreateFileA("move_tree", GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
        if (file_handle != INVALID_HANDLE_VALUE)
        {
            DWORD bytes_written;
            WriteFile(file_handle, &game->current_position_index,
                sizeof(game->current_position_index), &bytes_written, 0);
            OVERLAPPED overlapped = { 0 };
            overlapped.Offset = 0xffffffff;
            overlapped.OffsetHigh = 0xffffffff;
            WriteFile(file_handle, game->position_pool, NULL_POSITION * sizeof(Position),
                &bytes_written, &overlapped);
            CloseHandle(file_handle);
        }
    }
}

#define EXPORT_POSITION_TREE(game) export_position_tree(game)
#else
#define ASSERT(condition)
#define EXPORT_POSITION_TREE(game)
#endif

#define PLAYER_INDEX(piece_index) ((piece_index) >> 4)
#define KING_RANK(player_index) (7 * !(player_index))
#define FORWARD_DELTA(player_index) (((player_index) << 1) - 1)
#define RANK(square_index) ((square_index) >> 3)
#define FILE(square_index) ((square_index) & 0b111)
#define SCREEN_SQUARE_INDEX(square_index, engine_player_index) ((engine_player_index) == PLAYER_INDEX_WHITE ? (63 - (square_index)) : (square_index))
#define SQUARE_INDEX(rank, file) (FILE_COUNT * (rank) + (file))
#define EXTERNAL_STATE_NODES(game) ((game)->state_buckets + (game)->state_bucket_count)
#define PLAYER_PIECES_INDEX(player_index) ((player_index) << 4)
#define EN_PASSANT_RANK(player_index, forward_delta) KING_RANK(player_index) + ((forward_delta) << 2)
#define PLAYER_WIN(player_index) (((int16_t[]){INT16_MAX, -INT16_MAX})[player_index])

size_t g_page_size;
uint64_t g_counts_per_second;
uint64_t g_max_time;

void init_board_state_archive(Game*game, uint8_t bucket_count)
{
    game->state_generation = 0;
    game->state_bucket_count = bucket_count;
    game->state_buckets = RESERVE_AND_COMMIT_MEMORY(2 * sizeof(BoardStateNode) * bucket_count);
    for (size_t i = 0; i < bucket_count; ++i)
    {
        game->state_buckets[i] = (BoardStateNode) { (BoardState) { 0 }, NULL_POSITION, 0, 0 };
    }
    game->unique_state_count = 0;
    game->external_state_node_count = 0;
}

bool archive_board_state(Game*game, BoardState*state);

void increment_unique_state_count(Game*game)
{
    ++game->unique_state_count;
    if (game->unique_state_count == game->state_bucket_count)
    {
        BoardStateNode*old_buckets = game->state_buckets;
        BoardStateNode*old_external_nodes = EXTERNAL_STATE_NODES(game);
        init_board_state_archive(game, game->state_bucket_count << 1);
        for (size_t i = 0; i < game->state_bucket_count; ++i)
        {
            BoardStateNode*node = old_buckets + i;
            if (node->count && node->generation == game->state_generation)
            { 
                while (true)
                {
                    archive_board_state(game, &node->state);
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

typedef enum GameStatus
{
    STATUS_CHECKMATE,
    STATUS_END_TURN,
    STATUS_REPETITION,
    STATUS_STALEMATE,
    STATUS_TIME_OUT,
    STATUS_CONTINUE
} GameStatus;

bool archive_board_state(Game*game, BoardState*state)
{
    //FNV hash the state into an index less than game->state_bucket_count.
    uint32_t bucket_index = 2166136261;
    for (size_t i = 0; i < sizeof(BoardState); ++i)
    {
        bucket_index ^= ((uint8_t*)state)[i];
        bucket_index *= 16777619;
    }
    uint32_t bucket_count_bit_count;
    BIT_SCAN_REVERSE(&bucket_count_bit_count, game->state_bucket_count);
    bucket_index = ((bucket_index >> bucket_count_bit_count) ^ bucket_index) &
        ((1 << bucket_count_bit_count) - 1);

    BoardStateNode*node = game->state_buckets + bucket_index;
    if (node->count && node->generation == game->state_generation)
    {
        while (memcmp(state, &node->state, sizeof(BoardState)))
        {
            if (node->index_of_next_node == NULL_STATE)
            {
                node->index_of_next_node = game->external_state_node_count;
                EXTERNAL_STATE_NODES(game)[game->external_state_node_count] =
                (BoardStateNode) { *state, NULL_STATE, 1, game->state_generation };
                ++game->external_state_node_count;
                increment_unique_state_count(game);
                return true;
            }
            else
            {
                node = EXTERNAL_STATE_NODES(game) + node->index_of_next_node;
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
        node->generation = game->state_generation;
        increment_unique_state_count(game);
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
    ++piece->times_moved;
}

jmp_buf out_of_memory_jump_buffer;

uint16_t allocate_position(Game*game)
{
    uint16_t new_position_index;
    Position*new_position;
    if (game->index_of_first_free_pool_position == NULL_POSITION)
    {
        if (game->pool_cursor == NULL_POSITION)
        {
            game->run_engine = false;
            longjmp(out_of_memory_jump_buffer, 1);
        }
        new_position_index = game->pool_cursor;
        new_position = game->position_pool + new_position_index;
        COMMIT_MEMORY(new_position, g_page_size);
        ++game->pool_cursor;
    }
    else
    {
        new_position_index = game->index_of_first_free_pool_position;
        new_position = game->position_pool + new_position_index;
        if (new_position->parent_index == NULL_POSITION)
        {
            game->index_of_first_free_pool_position = new_position->next_leaf_index;
        }
        else if (new_position->next_move_index == NULL_POSITION)
        {
            game->position_pool[new_position->parent_index].next_leaf_index =
                new_position->next_leaf_index;
            game->index_of_first_free_pool_position = new_position->parent_index;
        }
        else
        {
            game->index_of_first_free_pool_position = new_position->next_leaf_index;
        }
    }
    new_position->en_passant_file = FILE_COUNT;
    new_position->reset_draw_by_50_count = false;
    new_position->moves_have_been_found = false;
    new_position->is_leaf = true;
    return new_position_index;
}

uint16_t copy_position(Game*game, uint16_t position_index)
{
    Position*position = game->position_pool + position_index;
    uint16_t copy_index = allocate_position(game);
    Position*copy = game->position_pool + copy_index;
    memcpy(&copy->squares, &position->squares, sizeof(position->squares));
    memcpy(&copy->pieces, &position->pieces, sizeof(position->pieces));
    return copy_index;
}

void add_move(Game*game, uint16_t position_index, uint16_t move_index)
{
    Position*position = game->position_pool + position_index;
    Position*move = game->position_pool + move_index;
    move->parent_index = position_index;
    if (position->is_leaf)
    {
        move->previous_leaf_index = position->previous_leaf_index;
        move->next_leaf_index = position->next_leaf_index;
        if (move->next_leaf_index != NULL_POSITION)
        {
            game->position_pool[move->next_leaf_index].previous_leaf_index = move_index;
        }
        position->is_leaf = false;
        move->next_move_index = NULL_POSITION;
    }
    else
    {
        Position*next_move = game->position_pool + position->first_move_index;
        move->previous_leaf_index = next_move->previous_leaf_index;
        move->next_leaf_index = position->first_move_index;
        next_move->previous_leaf_index = move_index;
        move->next_move_index = position->first_move_index;
    }
    if (move->previous_leaf_index == NULL_POSITION)
    {
        game->first_leaf_index = move_index;
    }
    else
    {
        game->position_pool[move->previous_leaf_index].next_leaf_index = move_index;
    }
    position->first_move_index = move_index;
    move->active_player_index = !position->active_player_index;
}

void free_position(Game*game, uint16_t position_index)
{
    Position*position = game->position_pool + position_index;
    position->parent_index = NULL_POSITION;
    position->next_leaf_index = game->index_of_first_free_pool_position;
    game->index_of_first_free_pool_position = position_index;
}

bool add_move_if_not_king_hang(Game*game, uint16_t position_index, uint16_t move_index)
{
    Position*move = game->position_pool + move_index;
    if (player_is_checked(move, game->position_pool[position_index].active_player_index))
    {
        free_position(game, move_index);
        return false;
    }
    else
    {
        add_move(game, position_index, move_index);
        return true;
    }
}

Position*move_piece_and_add_as_move(Game*game, uint16_t position_index, uint8_t piece_index,
    uint8_t destination_square_index)
{
    Position*position = game->position_pool + position_index;
    uint8_t destination_square = position->squares[destination_square_index];
    if (destination_square == NULL_PIECE)
    {
        uint16_t move_index = copy_position(game, position_index);
        Position*move = game->position_pool + move_index;
        move_piece_to_square(move, piece_index, destination_square_index);
        add_move_if_not_king_hang(game, position_index, move_index);
        return move;
    }
    else if (PLAYER_INDEX(destination_square) != position->active_player_index)
    {
        uint16_t move_index = copy_position(game, position_index);
        Position*move = game->position_pool + move_index;
        capture_piece(move, destination_square);
        move_piece_to_square(move, piece_index, destination_square_index);
        add_move_if_not_king_hang(game, position_index, move_index);
    }
    return 0;
}

uint16_t add_position_copy_as_move(Game*game, uint16_t position_index,
    uint16_t position_to_copy_index)
{
    uint16_t copy_index = copy_position(game, position_to_copy_index);
    add_move(game, position_index, copy_index);
    return copy_index;
}

void add_forward_pawn_move_with_promotions(Game*game, uint16_t position_index,
    uint16_t promotion_index, uint8_t piece_index)
{
    if (add_move_if_not_king_hang(game, position_index, promotion_index))
    {
        Position*promotion = game->position_pool + promotion_index;
        if (RANK(promotion->pieces[piece_index].square_index) ==
            KING_RANK(!PLAYER_INDEX(piece_index)))
        {
            promotion->pieces[piece_index].piece_type = PIECE_ROOK;
            promotion = game->position_pool +
                add_position_copy_as_move(game, position_index, promotion_index);
            promotion->pieces[piece_index].piece_type = PIECE_KNIGHT;
            promotion->reset_draw_by_50_count = true;
            promotion = game->position_pool +
                add_position_copy_as_move(game, position_index, promotion_index);
            promotion->pieces[piece_index].piece_type = PIECE_BISHOP;
            promotion->reset_draw_by_50_count = true;
            promotion = game->position_pool +
                add_position_copy_as_move(game, position_index, promotion_index);
            promotion->pieces[piece_index].piece_type = PIECE_QUEEN;
            promotion->reset_draw_by_50_count = true;
        }
    }
}

void add_diagonal_pawn_move(Game*game, uint16_t position_index, uint8_t piece_index, uint8_t file)
{
    Position*position = game->position_pool + position_index;
    uint8_t rank = RANK(position->pieces[piece_index].square_index);
    int8_t forward_delta = FORWARD_DELTA(position->active_player_index);
    uint8_t destination_square_index = SQUARE_INDEX(rank + forward_delta, file);
    if (file == position->en_passant_file && rank ==
        EN_PASSANT_RANK(position->active_player_index, forward_delta))
    {
        uint16_t move_index = copy_position(game, position_index);
        Position*move = game->position_pool + move_index;
        uint8_t en_passant_square_index = SQUARE_INDEX(rank, file);
        capture_piece(move, position->squares[en_passant_square_index]);
        move_piece_to_square(move, piece_index, destination_square_index);
        move->squares[en_passant_square_index] = NULL_PIECE;
        add_move_if_not_king_hang(game, position_index, move_index);
    }
    else
    {
        uint8_t destination_square = position->squares[destination_square_index];
        if (PLAYER_INDEX(destination_square) == !position->active_player_index)
        {
            uint16_t move_index = copy_position(game, position_index);
            Position*move = game->position_pool + move_index;
            capture_piece(move, destination_square);
            move_piece_to_square(move, piece_index, destination_square_index);
            add_forward_pawn_move_with_promotions(game, position_index, move_index, piece_index);
        }
    }
}

void add_half_diagonal_moves(Game*game, uint16_t position_index, uint8_t piece_index,
    int8_t rank_delta, int8_t file_delta)
{
    uint8_t destination_square_index =
        game->position_pool[position_index].pieces[piece_index].square_index;
    uint8_t destination_rank = RANK(destination_square_index);
    uint8_t destination_file = FILE(destination_square_index);
    do
    {
        destination_rank += rank_delta;
        destination_file += file_delta;
    } while (destination_rank < 8 && destination_file < 8 && move_piece_and_add_as_move(game,
        position_index, piece_index, SQUARE_INDEX(destination_rank, destination_file)));
}

void add_diagonal_moves(Game*game, uint16_t position_index, uint8_t piece_index)
{
    add_half_diagonal_moves(game, position_index, piece_index, 1, 1);
    add_half_diagonal_moves(game, position_index, piece_index, 1, -1);
    add_half_diagonal_moves(game, position_index, piece_index, -1, 1);
    add_half_diagonal_moves(game, position_index, piece_index, -1, -1);
}

void add_half_horizontal_moves(Game*game, uint16_t position_index, uint8_t piece_index,
    int8_t delta, bool loses_castling_rights)
{
    uint8_t destination_square_index =
        game->position_pool[position_index].pieces[piece_index].square_index;
    uint8_t destination_rank = RANK(destination_square_index);
    uint8_t destination_file = FILE(destination_square_index);
    while (true)
    {
        destination_file += delta;
        if (destination_file > 7)
        {
            break;
        }
        Position*move = move_piece_and_add_as_move(game, position_index, piece_index,
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

void add_half_vertical_moves(Game*game, uint16_t position_index, uint8_t piece_index, int8_t delta,
    bool loses_castling_rights)
{
    uint8_t destination_square_index =
        game->position_pool[position_index].pieces[piece_index].square_index;
    uint8_t destination_rank = RANK(destination_square_index);
    uint8_t destination_file = FILE(destination_square_index);
    while (true)
    {
        destination_rank += delta;
        if (destination_rank > 7)
        {
            break;
        }
        Position*move = move_piece_and_add_as_move(game, position_index, piece_index,
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

void add_moves_along_axes(Game*game, uint16_t position_index, uint8_t piece_index,
    bool loses_castling_rights)
{
    add_half_horizontal_moves(game, position_index, piece_index, 1, loses_castling_rights);
    add_half_horizontal_moves(game, position_index, piece_index, -1, loses_castling_rights);
    add_half_vertical_moves(game, position_index, piece_index, 1, loses_castling_rights);
    add_half_vertical_moves(game, position_index, piece_index, -1, loses_castling_rights);
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

void get_moves(Game*game, uint16_t position_index)
{
    Position*position = game->position_pool + position_index;
    if (setjmp(out_of_memory_jump_buffer))
    {
        uint16_t move_index = position->first_move_index;
        Position*move = game->position_pool + move_index;
        if (!position->is_leaf)
        {
            position->is_leaf = true;
            position->previous_leaf_index = move->previous_leaf_index;
            while (true)
            {
                if (move->next_move_index == NULL_POSITION)
                {
                    position->next_leaf_index = move->next_leaf_index;
                    free_position(game, move_index);
                    break;
                }
                else
                {
                    free_position(game, move_index);
                }
                move_index = move->next_move_index;
                move = game->position_pool + move_index;
            }
            if (position->previous_leaf_index != NULL_POSITION)
            {
                game->position_pool[position->previous_leaf_index].next_leaf_index = position_index;
            }
            if (position->next_leaf_index != NULL_POSITION)
            {
                game->position_pool[position->next_leaf_index].previous_leaf_index = position_index;
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
        case PIECE_PAWN:
        {
            int8_t forward_delta = FORWARD_DELTA(position->active_player_index);
            uint8_t file = FILE(piece.square_index);
            if (file)
            {
                add_diagonal_pawn_move(game, position_index, piece_index, file - 1);
            }
            if (file < 7)
            {
                add_diagonal_pawn_move(game, position_index, piece_index, file + 1);
            }
            uint8_t destination_square_index = piece.square_index + forward_delta * FILE_COUNT;
            if (position->squares[destination_square_index] == NULL_PIECE)
            {
                uint16_t move_index = copy_position(game, position_index);
                Position*move = game->position_pool + move_index;
                move_piece_to_square(move, piece_index, destination_square_index);
                move->reset_draw_by_50_count = true;
                destination_square_index += forward_delta * FILE_COUNT;
                add_forward_pawn_move_with_promotions(game, position_index, move_index,
                    piece_index);
                if (!piece.times_moved && position->squares[destination_square_index] == NULL_PIECE)
                {
                    move_index = copy_position(game, position_index);
                    move = game->position_pool + move_index;
                    move_piece_to_square(move, piece_index, destination_square_index);
                    move->reset_draw_by_50_count = true;
                    move->en_passant_file = file;
                    add_move_if_not_king_hang(game, position_index, move_index);
                }
            }
            break;
        }
        case PIECE_BISHOP:
        {
            add_diagonal_moves(game, position_index, piece_index);
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
                    move_piece_and_add_as_move(game, position_index, piece_index,
                        SQUARE_INDEX(rank - 2, file - 1));
                }
                if (rank < 6)
                {
                    move_piece_and_add_as_move(game, position_index, piece_index,
                        SQUARE_INDEX(rank + 2, file - 1));
                }
                if (file > 1)
                {
                    if (rank > 0)
                    {
                        move_piece_and_add_as_move(game, position_index, piece_index,
                            SQUARE_INDEX(rank - 1, file - 2));
                    }
                    if (rank < 7)
                    {
                        move_piece_and_add_as_move(game, position_index, piece_index,
                            SQUARE_INDEX(rank + 1, file - 2));
                    }
                }
            }
            if (file < 7)
            {
                if (rank > 1)
                {
                    move_piece_and_add_as_move(game, position_index, piece_index,
                        SQUARE_INDEX(rank - 2, file + 1));
                }
                if (rank < 6)
                {
                    move_piece_and_add_as_move(game, position_index, piece_index,
                        SQUARE_INDEX(rank + 2, file + 1));
                }
                if (file < 6)
                {
                    if (rank > 0)
                    {
                        move_piece_and_add_as_move(game, position_index, piece_index,
                            SQUARE_INDEX(rank - 1, file + 2));
                    }
                    if (rank < 7)
                    {
                        move_piece_and_add_as_move(game, position_index, piece_index, 
                            SQUARE_INDEX(rank + 1, file + 2));
                    }
                }
            }
            break;
        }
        case PIECE_ROOK:
        {
            add_moves_along_axes(game, position_index, piece_index, !piece.times_moved &&
                !position->pieces[(position->active_player_index << 4) + KING_INDEX].times_moved);
            break;
        }
        case PIECE_QUEEN:
        {
            add_diagonal_moves(game, position_index, piece_index);
            add_moves_along_axes(game, position_index, piece_index, false);
            break;
        }
        case PIECE_KING:
        {
            uint8_t a_rook_index = player_pieces_index + A_ROOK_INDEX;
            uint8_t h_rook_index = player_pieces_index + H_ROOK_INDEX;
            bool loses_castling_rights = !piece.times_moved && 
                (!position->pieces[a_rook_index].times_moved ||
                    !position->pieces[h_rook_index].times_moved);
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
                    Position*move = move_piece_and_add_as_move(game, position_index, piece_index,
                        SQUARE_INDEX(move_ranks[rank_index], move_files[file_index]));
                    if (move)
                    {
                        move->reset_draw_by_50_count |= loses_castling_rights;
                    }
                }
            }
            if (!piece.times_moved &&
                !player_attacks_king_square(position, !position->active_player_index,
                    piece.square_index))
            {
                if (!position->pieces[a_rook_index].times_moved &&
                    position->squares[piece.square_index - 1] == NULL_PIECE &&
                    position->squares[piece.square_index - 2] == NULL_PIECE &&
                    position->squares[piece.square_index - 3] == NULL_PIECE &&
                    !player_attacks_king_square(position, !position->active_player_index,
                        piece.square_index - 1) &&
                    !player_attacks_king_square(position, !position->active_player_index,
                        piece.square_index - 2))
                {
                    Position*move = move_piece_and_add_as_move(game, position_index, piece_index,
                        piece.square_index - 2);
                    move->reset_draw_by_50_count = true;
                    move_piece_to_square(move, a_rook_index, piece.square_index - 1);
                }
                if (!position->pieces[h_rook_index].times_moved &&
                    position->squares[piece.square_index + 1] == NULL_PIECE &&
                    position->squares[piece.square_index + 2] == NULL_PIECE &&
                    !player_attacks_king_square(position, !position->active_player_index,
                        piece.square_index + 1) &&
                    !player_attacks_king_square(position, !position->active_player_index,
                        piece.square_index + 2))
                {
                    Position*move = move_piece_and_add_as_move(game, position_index, piece_index,
                        piece.square_index + 2);
                    move->reset_draw_by_50_count = true;
                    move_piece_to_square(move, h_rook_index, piece.square_index + 1);
                }
            }
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
        else
        {
            position->evaluation = new_evaluation;
            if (position->parent_index == NULL_POSITION)
            {
                return;
            }
        }
    }
    else
    {
        Position*move = game->position_pool + position->first_move_index;
        while (true)
        {
            for (size_t player_index = 0; player_index < 2; ++player_index)
            {
                int16_t point = FORWARD_DELTA(!player_index);
                Piece*player_pieces = position->pieces + PLAYER_PIECES_INDEX(player_index);
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
            move = game->position_pool + move->next_move_index;
        }
    }
    while (true)
    {
        int16_t new_evaluation = PLAYER_WIN(!position->active_player_index);
        uint16_t move_index = position->first_move_index;
        while (move_index != NULL_POSITION)
        {
            Position*move = game->position_pool + move_index;
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
        position = game->position_pool + position->parent_index;
    }
}

bool make_move_current(Game*game, uint16_t move_index)
{
    game->current_position_index = move_index;
    Position*position = game->position_pool + game->current_position_index;
    if (position->reset_draw_by_50_count)
    {
        game->draw_by_50_count = 0;
        ++game->state_generation;
    }
    ++game->draw_by_50_count;
    if (!position->moves_have_been_found)
    {
        get_moves(game, game->current_position_index);
    }
    game->next_leaf_to_evaluate_index = game->first_leaf_index;
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
    return archive_board_state(game, &state);
}

bool point_is_in_rect(int32_t x, int32_t y, int32_t min_x, int32_t min_y, uint32_t width,
    uint32_t height)
{
    return x >= min_x && y >= min_y && x < min_x + width && y < min_y + height;
}

uint8_t id_of_square_cursor_is_over(Control*board, int32_t cursor_x, int32_t cursor_y)
{
    if (cursor_x >= board->board.min_x && cursor_y >= board->board.min_y)
    {
        uint8_t rank = (cursor_y - board->board.min_y) / COLUMNS_PER_ICON;
        uint8_t file = (cursor_x - board->board.min_x) / COLUMNS_PER_ICON;
        if (rank < RANK_COUNT && file < FILE_COUNT)
        {
            return board->base_id + SQUARE_INDEX(rank, file);
        }
    }
    return NULL_CONTROL;
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
            uint8_t id = id_of_square_cursor_is_over(control, cursor_x, cursor_y);
            if (id != NULL_CONTROL)
            {
                return id;
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
                4 * COLUMNS_PER_ICON, COLUMNS_PER_ICON))
            {
                return control->base_id +
                    (cursor_x - control->promotion_selector.min_x) / COLUMNS_PER_ICON;
            }
            break;
        }
        case CONTROL_RADIO:
        {
            int32_t cursor_x_in_circle = cursor_x +
                window->dpi_data->circle_rasterizations[1].radius -
                (control->radio.min_x + window->dpi_data->circle_rasterizations[2].radius);
            int32_t diameter = 2 * window->dpi_data->circle_rasterizations[1].radius;
            if (cursor_x_in_circle >= 0 && cursor_x_in_circle < diameter)
            {
                int32_t cursor_y_in_circle = cursor_y +
                    window->dpi_data->circle_rasterizations[2].radius +
                    window->dpi_data->circle_rasterizations[1].radius -
                    (control->radio.label_baseline_y + window->dpi_data->text_line_height);
                for (size_t option_index = 0;
                    option_index < ARRAY_COUNT(g_radio_labels); ++option_index)
                {
                    if (cursor_y_in_circle < 0)
                    {
                        break;
                    }
                    if (cursor_y_in_circle < diameter &&
                        window->dpi_data->circle_rasterizations[option_index].
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
                        2 * ROUND26_6_TO_PIXEL(window->dpi_data->rasterizations
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

UpdateTimerStatus update_timer(Game*game)
{
    UpdateTimerStatus out = UPDATE_TIMER_CONTINUE;
    uint64_t time_since_last_move = get_time() - game->last_move_time;
    uint16_t new_seconds_left;
    uint8_t active_player_index =
        game->position_pool[game->current_position_index].active_player_index;
    if (time_since_last_move >= game->times_left_as_of_last_move[active_player_index])
    {
        new_seconds_left = 0;
        out |= UPDATE_TIMER_TIME_OUT;
    }
    else
    {
        new_seconds_left = (game->times_left_as_of_last_move[active_player_index] -
            time_since_last_move) / g_counts_per_second;
    }
    if (new_seconds_left != game->seconds_left[active_player_index])
    {
        game->seconds_left[active_player_index] = new_seconds_left;
        out |= UPDATE_TIMER_REDRAW;
    }
    return out;
}

uint16_t get_first_tree_leaf_index(Game*game, uint16_t root_position_index)
{
    while (true)
    {
        Position*position = game->position_pool + root_position_index;
        if (position->is_leaf)
        {
            return root_position_index;
        }
        root_position_index = position->first_move_index;
    }
}

uint16_t get_last_tree_leaf_index(Game*game, uint16_t root_position_index)
{
    Position*position = game->position_pool + root_position_index;
    if (position->is_leaf)
    {
        return root_position_index;
    }
    uint16_t out = position->first_move_index;
    position = game->position_pool + out;
    while (true)
    {
        if (position->next_move_index == NULL_POSITION)
        {
            if (position->is_leaf)
            {
                return out;
            }
            else
            {
                out = position->first_move_index;
                position = game->position_pool + out;
            }
        }
        else
        {
            out = position->next_move_index;
            position = game->position_pool + out;
        }
    }
}

GameStatus end_turn(Game*game)
{
    uint64_t move_time = get_time();
    uint64_t time_since_last_move = move_time - game->last_move_time;
    uint8_t active_player_index =
        game->position_pool[game->current_position_index].active_player_index;
    uint64_t*time_left_as_of_last_move = game->times_left_as_of_last_move + active_player_index;
    if (time_since_last_move >= *time_left_as_of_last_move)
    {
        game->seconds_left[active_player_index] = 0;
        return STATUS_TIME_OUT;
    }
    *time_left_as_of_last_move -= time_since_last_move;
    if (g_max_time - game->time_increment >= *time_left_as_of_last_move)
    {
        *time_left_as_of_last_move += game->time_increment;
    }
    else
    {
        *time_left_as_of_last_move = g_max_time;
    }
    game->seconds_left[active_player_index] = *time_left_as_of_last_move / g_counts_per_second;
    game->last_move_time = move_time;
    game->selected_piece_index = NULL_PIECE;
    uint16_t selected_move_index = *game->selected_move_index;
    Position*move = game->position_pool + selected_move_index;
    *game->selected_move_index = move->next_move_index;
    uint16_t first_leaf_index = get_first_tree_leaf_index(game, selected_move_index);
    Position*move_tree_first_leaf = game->position_pool + first_leaf_index;
    Position*move_tree_last_leaf =
        game->position_pool + get_last_tree_leaf_index(game, selected_move_index);
    if (move_tree_first_leaf->previous_leaf_index != NULL_POSITION)
    {
        game->position_pool[move_tree_first_leaf->previous_leaf_index].next_leaf_index =
            move_tree_last_leaf->next_leaf_index;
    }
    if (move_tree_last_leaf->next_leaf_index != NULL_POSITION)
    {
        game->position_pool[move_tree_last_leaf->next_leaf_index].previous_leaf_index =
            move_tree_first_leaf->previous_leaf_index;
        move_tree_last_leaf->next_leaf_index = NULL_POSITION;
    }
    move_tree_first_leaf->previous_leaf_index = NULL_POSITION;
    game->first_leaf_index = first_leaf_index;
    move->parent_index = NULL_POSITION;
    first_leaf_index = get_first_tree_leaf_index(game, game->current_position_index);
    game->position_pool[first_leaf_index].previous_leaf_index = NULL_POSITION;
    game->position_pool[get_last_tree_leaf_index(game, game->current_position_index)].
        next_leaf_index = game->index_of_first_free_pool_position;
    game->index_of_first_free_pool_position = first_leaf_index;
    if (make_move_current(game, selected_move_index))
    {
        if (move->is_leaf)
        {
            if (move->evaluation == PLAYER_WIN(move->active_player_index))
            {
                return STATUS_CHECKMATE;
            }
            else
            {
                return STATUS_STALEMATE;
            }
        }
        else
        {
            game->run_engine = true;
            return STATUS_END_TURN;
        }
    }
    else
    {
        return STATUS_REPETITION;
    }
}

GameStatus do_engine_iteration(Game*game)
{
    if (game->run_engine)
    {
        uint16_t first_leaf_checked_index = game->next_leaf_to_evaluate_index;
        uint16_t position_to_evaluate_index = game->next_leaf_to_evaluate_index;
        Position*position;
        while (true)
        {
            if (position_to_evaluate_index == NULL_POSITION)
            {
                position_to_evaluate_index = game->first_leaf_index;
            }
            position = game->position_pool + position_to_evaluate_index;
            if (!position->moves_have_been_found)
            {
                game->next_leaf_to_evaluate_index = position->next_leaf_index;
                get_moves(game, position_to_evaluate_index);
                break;
            }
            if (first_leaf_checked_index == position->next_leaf_index)
            {
                game->run_engine = false;
                break;
            }
            position_to_evaluate_index = position->next_leaf_index;
        }
    }
    if (!game->run_engine)
    {
        Position*current_position = game->position_pool + game->current_position_index;
        if (current_position->active_player_index == game->engine_player_index)
        {
            int64_t best_evaluation = PLAYER_WIN(!game->engine_player_index);
            uint16_t*move_index = &current_position->first_move_index;
            game->selected_move_index = move_index;
            while (*move_index != NULL_POSITION)
            {
                Position*move = game->position_pool + *move_index;
                if (move->moves_have_been_found)
                {
                    if (game->engine_player_index == PLAYER_INDEX_WHITE)
                    {
                        if (move->evaluation > best_evaluation)
                        {
                            best_evaluation = move->evaluation;
                            game->selected_move_index = move_index;
                        }
                    }
                    else if (move->evaluation < best_evaluation)
                    {
                        best_evaluation = move->evaluation;
                        game->selected_move_index = move_index;
                    }
                }
                move_index = &move->next_move_index;
            }
            EXPORT_POSITION_TREE(game);
            return end_turn(game);
        }
    }
    return STATUS_CONTINUE;
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

void draw_icon(Color*icon_bitmap, Window*window, int32_t min_x, int32_t min_y)
{
    int32_t max_x_in_bitmap = min32(COLUMNS_PER_ICON, window->width - min_x);
    int32_t max_y_in_bitmap = min32(COLUMNS_PER_ICON, window->height - min_y);
    Color*bitmap_row_min_x_in_window = window->pixels + min_y * window->width + min_x;
    icon_bitmap += PIXELS_PER_ICON;
    for (int32_t y_in_bitmap = 0; y_in_bitmap < max_y_in_bitmap; ++y_in_bitmap)
    {
        icon_bitmap -= COLUMNS_PER_ICON;
        for (int32_t x_in_bitmap = 0; x_in_bitmap < max_x_in_bitmap; ++x_in_bitmap)
        {
            Color*window_pixel = bitmap_row_min_x_in_window + x_in_bitmap;
            Color bitmap_pixel = icon_bitmap[x_in_bitmap];
            uint32_t lerp_term = (255 - bitmap_pixel.alpha) * window_pixel->alpha;
            window_pixel->red = (bitmap_pixel.red * bitmap_pixel.alpha) / 255 +
                (lerp_term * window_pixel->red) / 65025;
            window_pixel->green = (bitmap_pixel.green * bitmap_pixel.alpha) / 255 +
                (lerp_term * window_pixel->green) / 65025;
            window_pixel->blue = (bitmap_pixel.blue * bitmap_pixel.alpha) / 255 +
                (lerp_term * window_pixel->blue) / 65025;
            window_pixel->alpha = bitmap_pixel.alpha + lerp_term / 255;
        }
        bitmap_row_min_x_in_window += window->width;
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

void set_dpi_data(Game*game, Window*window, uint32_t font_size, uint16_t dpi)
{
    for (size_t i = 0; i < ARRAY_COUNT(game->dpi_datas); ++i)
    {
        DPIData*dpi_data = game->dpi_datas + i;
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
    FT_Set_Char_Size(g_face, 0, font_size, 0, dpi);
    FT_Load_Glyph(g_face, FT_Get_Char_Index(g_face, '|'), FT_LOAD_DEFAULT);
    window->dpi_data->control_border_thickness = g_face->glyph->bitmap.pitch;
    size_t rasterization_memory_size = 0;
    for (size_t i = 0; i < ARRAY_COUNT(window->dpi_data->circle_rasterizations); ++i)
    {
        CircleRasterization*circle_rasterization = window->dpi_data->circle_rasterizations + i;
        circle_rasterization->radius = (i + 1) * window->dpi_data->control_border_thickness;
        int32_t diameter = 2 * circle_rasterization->radius;
        rasterization_memory_size += diameter * diameter;
    }
    for (size_t i = 0; i < ARRAY_COUNT(g_codepoints); ++i)
    {
        FT_Load_Glyph(g_face, FT_Get_Char_Index(g_face, g_codepoints[i]), FT_LOAD_DEFAULT);
        rasterization_memory_size += g_face->glyph->bitmap.rows * g_face->glyph->bitmap.pitch;
    }
    void*rasterization_memory = RESERVE_AND_COMMIT_MEMORY(rasterization_memory_size);
    for (size_t i = 0; i < ARRAY_COUNT(window->dpi_data->circle_rasterizations); ++i)
    {
        CircleRasterization*circle_rasterization = window->dpi_data->circle_rasterizations + i;
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
    for (size_t i = 0; i < ARRAY_COUNT(g_codepoints); ++i)
    {
        char codepoint = g_codepoints[i];
        FT_Load_Glyph(g_face, FT_Get_Char_Index(g_face, codepoint), FT_LOAD_RENDER);
        Rasterization*rasterization =
            window->dpi_data->rasterizations + codepoint - g_codepoints[0];
        rasterization->bitmap = g_face->glyph->bitmap;
        rasterization->bitmap.buffer = rasterization_memory;
        size_t rasterization_size = g_face->glyph->bitmap.rows * g_face->glyph->bitmap.pitch;
        memcpy(rasterization->bitmap.buffer, g_face->glyph->bitmap.buffer, rasterization_size);
        rasterization->left_bearing = g_face->glyph->bitmap_left;
        rasterization->top_bearing = g_face->glyph->bitmap_top;
        rasterization->advance = g_face->glyph->metrics.horiAdvance;
        rasterization_memory = (void*)((uintptr_t)rasterization_memory + rasterization_size);
    }
    window->dpi_data->text_line_height = ROUND26_6_TO_PIXEL(g_face->size->metrics.height);
    window->dpi_data->text_control_height  =
        ROUND26_6_TO_PIXEL(g_face->size->metrics.ascender - 2 * g_face->size->metrics.descender);
    FT_Load_Glyph(g_face, FT_Get_Char_Index(g_face, 'M'), FT_LOAD_DEFAULT);
    window->dpi_data->text_control_padding =
        window->dpi_data->text_control_height - g_face->glyph->bitmap.rows / 2;
    window->dpi_data->digit_input_width = 0;
    for (char digit = '0'; digit <= '9'; ++digit)
    {
        window->dpi_data->digit_input_width = max32(window->dpi_data->digit_input_width,
            window->dpi_data->rasterizations[digit - g_codepoints[0]].bitmap.width);
    }
    window->dpi_data->digit_input_width *= 2;
}

void free_dpi_data(Window*window)
{
    --window->dpi_data->reference_count;
    if (!window->dpi_data->reference_count)
    {
        FREE_MEMORY(window->dpi_data->rasterizations);
    }
}

uint32_t get_string_length(Rasterization*rasterizations, char*string)
{
    FT_Pos out = 0;
    FT_UInt previous_glyph_index = 0;
    while (*string)
    {
        FT_UInt glyph_index = FT_Get_Char_Index(g_face, *string);
        FT_Vector kerning_with_previous_glyph;
        FT_Get_Kerning(g_face, previous_glyph_index, glyph_index, FT_KERNING_DEFAULT,
            &kerning_with_previous_glyph);
        out += kerning_with_previous_glyph.x + rasterizations[*string - g_codepoints[0]].advance;
        previous_glyph_index = glyph_index;
        ++string;
    }
    return ROUND26_6_TO_PIXEL(out);
}

void draw_rasterization(Window*window, Rasterization*rasterization, int32_t origin_x,
    int32_t origin_y, Color color)
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
        FT_UInt glyph_index = FT_Get_Char_Index(g_face, *string);
        FT_Vector kerning_with_previous_glyph;
        FT_Get_Kerning(g_face, previous_glyph_index, glyph_index, FT_KERNING_DEFAULT,
            &kerning_with_previous_glyph);
        advance += kerning_with_previous_glyph.x;
        Rasterization*rasterization = window->dpi_data->rasterizations + *string - g_codepoints[0];
        draw_rasterization(window, rasterization, ROUND26_6_TO_PIXEL(advance), origin_y, color);
        advance += rasterization->advance;
        previous_glyph_index = glyph_index;
        ++string;
    }
}

void draw_circle(Window*window, CircleRasterization*rasterization, int32_t min_x,
    int32_t center_y, Color color)
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

void color_square_with_index(Window*window, Board*board, Color tint, uint32_t square_size,
    uint8_t square_index)
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
        draw_rectangle(window, board->min_x + square_size * file, board->min_y + square_size * rank,
            square_size, square_size, color);
    }
}

void draw_button(Window*window, Button*button, Color cell_color, Color label_color)
{
    draw_rectangle(window, button->min_x, button->min_y, button->width,
        window->dpi_data->text_control_height, cell_color);
    draw_rectangle_border(window, button->min_x, button->min_y, button->width,
        window->dpi_data->text_control_height);
    draw_string(window, button->label,
        button->min_x + (button->width -
            get_string_length(window->dpi_data->rasterizations, button->label)) / 2,
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

void draw_controls(Game*game, Window*window)
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
                2 * window->dpi_data->circle_rasterizations[2].radius;
            int32_t white_circle_center_x = control->radio.min_x +
                window->dpi_data->circle_rasterizations[2].radius -
                window->dpi_data->circle_rasterizations[1].radius;
            int32_t option_baseline_y = control->radio.label_baseline_y;
            for (size_t option_index = 0; option_index < ARRAY_COUNT(g_radio_labels);
                ++option_index)
            {
                option_baseline_y += window->dpi_data->text_line_height;
                int32_t circle_center_y =
                    option_baseline_y - window->dpi_data->circle_rasterizations[2].radius;
                draw_circle(window, window->dpi_data->circle_rasterizations + 2,
                    control->radio.min_x, circle_center_y, g_black);
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
                draw_circle(window, window->dpi_data->circle_rasterizations + 1,
                    white_circle_center_x, circle_center_y, color);
                if (option_index != game->engine_player_index)
                {
                    draw_circle(window, window->dpi_data->circle_rasterizations,
                        control->radio.min_x + window->dpi_data->circle_rasterizations[2].radius -
                            window->dpi_data->circle_rasterizations[0].radius,
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
            if (game->selected_digit_id >= control->base_id)
            {
                selected_digit_index = game->selected_digit_id - control->base_id;
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
                Rasterization*rasterization =
                    window->dpi_data->rasterizations + digit_input->digit + '0' - g_codepoints[0];
                draw_rasterization(window, rasterization, digit_input_min_x +
                    (window->dpi_data->digit_input_width - rasterization->bitmap.width) / 2,
                    text_min_y, digit_color);
                digit_input_min_x += window->dpi_data->digit_input_width +
                    window->dpi_data->control_border_thickness;
                if (digit_input->is_before_colon)
                {
                    Rasterization*colon = window->dpi_data->rasterizations + ':' - g_codepoints[0];
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

bool setup_window_handle_left_mouse_button_up(Game*game, Window*window, int32_t cursor_x,
    int32_t cursor_y)
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
                game->engine_player_index =
                    !(id - window->controls[SETUP_WINDOW_PLAYER_COLOR].base_id);
            }
            else
            {
                game->selected_digit_id = id;
            }
            return false;
        }
        else
        {
            window->clicked_control_id = NULL_CONTROL;
        }
    }
    game->selected_digit_id = NULL_CONTROL;
    return false;
}

bool handle_time_input_key_down(Game*game, Control*time_input, KeyId key)
{
    if (game->selected_digit_id >= time_input->base_id &&
        game->selected_digit_id < time_input->base_id + time_input->time_input.digit_count)
    {
        size_t digit_index = game->selected_digit_id - time_input->base_id;
        if (key >= KEY_0 && key < KEY_0 + 10)
        {
            time_input->time_input.digits[digit_index].digit = key - KEY_0;
            if (digit_index + 1 < time_input->time_input.digit_count)
            {
                ++game->selected_digit_id;
            }
            else
            {
                game->selected_digit_id = NULL_CONTROL;
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
                --game->selected_digit_id;
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
                --game->selected_digit_id;
            }
            return true;
        }
        case KEY_RIGHT:
        {
            if (digit_index + 1 < time_input->time_input.digit_count)
            {
                ++game->selected_digit_id;
            }
            return true;
        }
        }
    }
    return false;
}

bool setup_window_handle_key_down(Game*game, Window*window, uint8_t digit)
{
    if (handle_time_input_key_down(game, window->controls + SETUP_WINDOW_TIME, digit))
    {
        return true;
    }
    else if (handle_time_input_key_down(game, window->controls + SETUP_WINDOW_INCREMENT, digit))
    {
        return true;
    }
    return false;
}

void set_setup_window_dpi(Game*game, uint16_t dpi)
{
    Window*setup_window = game->windows + WINDOW_SETUP;
    set_dpi_data(game, setup_window, game->font_size, dpi);
    RadioButtons*player_color = &setup_window->controls[SETUP_WINDOW_PLAYER_COLOR].radio;
    player_color->min_x = setup_window->dpi_data->text_control_height;
    TimeInput*time = &setup_window->controls[SETUP_WINDOW_TIME].time_input;
    time->min_x = setup_window->dpi_data->control_border_thickness +
        setup_window->dpi_data->text_control_height;
    TimeInput*increment = &setup_window->controls[SETUP_WINDOW_INCREMENT].time_input;
    increment->min_x = time->min_x;
    Button*start_game_button = &setup_window->controls[SETUP_WINDOW_START_GAME].button;
    start_game_button->min_x = time->min_x;
    start_game_button->width = max32(max32(5 * setup_window->dpi_data->digit_input_width +
        4 * ROUND26_6_TO_PIXEL(setup_window->dpi_data->
            rasterizations[':' - g_codepoints[0]].advance) +
        6 * setup_window->dpi_data->control_border_thickness,
        get_string_length(setup_window->dpi_data->rasterizations, increment->label)),
        get_string_length(setup_window->dpi_data->rasterizations, start_game_button->label) +
            2 * setup_window->dpi_data->text_control_padding);
    setup_window->width = 2 *(setup_window->dpi_data->text_control_height +
        setup_window->dpi_data->control_border_thickness) + start_game_button->width;
    player_color->label_baseline_y = 2 * setup_window->dpi_data->text_line_height;
    time->min_y = player_color->label_baseline_y + 3 * setup_window->dpi_data->text_line_height +
        setup_window->dpi_data->text_control_height +
        setup_window->dpi_data->control_border_thickness;
    increment->min_y = time->min_y + setup_window->dpi_data->text_line_height +
        2 * (setup_window->dpi_data->control_border_thickness +
            setup_window->dpi_data->text_control_height);
    start_game_button->min_y = increment->min_y + 
        2 * (setup_window->dpi_data->control_border_thickness +
            setup_window->dpi_data->text_control_height);
    setup_window->height = start_game_button->min_y +
        setup_window->dpi_data->control_border_thickness +
        2 * setup_window->dpi_data->text_control_height;
}

void init_setup_window(Game*game, uint16_t dpi)
{
    Window*setup_window = game->windows + WINDOW_SETUP;
    setup_window->control_count = SETUP_WINDOW_CONTROL_COUNT;
    Control*control = setup_window->controls + SETUP_WINDOW_PLAYER_COLOR;
    control->control_type = CONTROL_RADIO;
    game->engine_player_index = PLAYER_INDEX_BLACK;
    control = setup_window->controls + SETUP_WINDOW_TIME;
    control->control_type = CONTROL_TIME_INPUT;
    control->time_input.label = "Time:";
    control->time_input.digits = game->time_control;
    control->time_input.digit_count = ARRAY_COUNT(game->time_control);
    control = setup_window->controls + SETUP_WINDOW_INCREMENT;
    control->control_type = CONTROL_TIME_INPUT;
    control->time_input.label = "Increment:";
    control->time_input.digits = game->increment;
    control->time_input.digit_count = ARRAY_COUNT(game->increment);
    control = setup_window->controls + SETUP_WINDOW_START_GAME;
    control->control_type = CONTROL_BUTTON;
    control->button.label = "Start";
    set_control_ids(setup_window);
    set_setup_window_dpi(game, dpi);
    setup_window->pixels = 0;
    setup_window->pixel_buffer_capacity = 0;
    setup_window->hovered_control_id = NULL_CONTROL;
    setup_window->clicked_control_id = NULL_CONTROL;
    game->selected_digit_id = NULL_CONTROL;
}

void set_start_window_dpi(Game*game, char*text, uint16_t dpi)
{
    Window*start_window = game->windows + WINDOW_START;
    set_dpi_data(game, start_window, game->font_size, dpi);
    uint32_t button_padding = 2 * start_window->dpi_data->text_control_padding;
    int32_t button_min_x = start_window->dpi_data->text_control_height +
        start_window->dpi_data->control_border_thickness;
    int32_t button_min_y = start_window->dpi_data->control_border_thickness;
    if (text[0])
    {
        button_min_y += 3 * start_window->dpi_data->text_line_height;
        start_window->width = get_string_length(start_window->dpi_data->rasterizations, text);
    }
    else
    {
        button_min_y += start_window->dpi_data->text_control_height;
        start_window->width = 0;
    }
    for (size_t i = 0; i < START_CONTROL_COUNT; ++i)
    {
        Button*button = &start_window->controls[i].button;
        button->min_x = button_min_x;
        button->width = button_padding +
            get_string_length(start_window->dpi_data->rasterizations, button->label);
        button_min_x += button->width + start_window->dpi_data->control_border_thickness;
        button->min_y = button_min_y;
    }
    start_window->width = start_window->dpi_data->text_control_height +
        max32(start_window->width, button_min_x + start_window->dpi_data->control_border_thickness);
    start_window->height = button_min_y + start_window->dpi_data->text_control_height +
        start_window->dpi_data->control_border_thickness + start_window->dpi_data->text_control_height;
}

void init_start_window(Game*game, char*text, uint16_t dpi)
{
    Window*start_window = game->windows + WINDOW_START;
    start_window->control_count = START_CONTROL_COUNT;
    Control*button = start_window->controls + START_NEW_GAME;
    button->control_type = CONTROL_BUTTON;
    button->button.label = "New game";
    button = start_window->controls + START_LOAD_GAME;
    button->control_type = CONTROL_BUTTON;
    button->button.label = "Load game";
    button = start_window->controls + START_QUIT;
    button->control_type = CONTROL_BUTTON;
    button->button.label = "Quit";
    set_control_ids(start_window);
    set_start_window_dpi(game, text, dpi);
    start_window->hovered_control_id = NULL_CONTROL;
    start_window->clicked_control_id = NULL_CONTROL;
    start_window->pixels = 0;
    start_window->pixel_buffer_capacity = 0;
}

void draw_start_window(Game*game, char*text)
{
    Window*start_window = game->windows + WINDOW_START;
    draw_controls(game, start_window);
    draw_string(start_window, text, start_window->dpi_data->text_control_height,
        2 * start_window->dpi_data->text_line_height, g_black);
}

#define DRAW_BY_50_CAN_BE_CLAIMED(game) (game->draw_by_50_count > 100)

bool main_window_handle_left_mouse_button_down(Game*game, int32_t cursor_x, int32_t cursor_y)
{
    Window*main_window = game->windows + WINDOW_MAIN;
    main_window->clicked_control_id = main_window->hovered_control_id;
    if (main_window->hovered_control_id == main_window->controls[MAIN_WINDOW_CLAIM_DRAW].base_id)
    {
        if (!DRAW_BY_50_CAN_BE_CLAIMED(game))
        {
            main_window->clicked_control_id = NULL_CONTROL;
        }
    }
    else
    {
        uint8_t board_base_id = main_window->controls[MAIN_WINDOW_BOARD].base_id;
        if (main_window->hovered_control_id >= board_base_id &&
            main_window->hovered_control_id < board_base_id + 64 &&
            (game->position_pool[game->current_position_index].active_player_index ==
                game->engine_player_index || game->is_promoting))
        {
            main_window->clicked_control_id = NULL_CONTROL;
        }
        else
        {
            uint8_t promotion_selector_base_id =
                main_window->controls[MAIN_WINDOW_PROMOTION_SELECTOR].base_id;
            if (main_window->hovered_control_id >= promotion_selector_base_id &&
                main_window->hovered_control_id < promotion_selector_base_id +
                ARRAY_COUNT(g_promotion_options) && !game->is_promoting)
            {
                main_window->clicked_control_id = NULL_CONTROL;
            }
        }
    }
    return main_window->clicked_control_id != NULL_CONTROL;
}

typedef enum MainWindowLeftButtonUpAction
{
    ACTION_REDRAW,
    ACTION_SAVE_GAME,
    ACTION_CLAIM_DRAW,
    ACTION_NONE
} MainWindowLeftButtonUpAction;

MainWindowLeftButtonUpAction main_window_handle_left_mouse_button_up(Game*game, int32_t cursor_x,
    int32_t cursor_y)
{
    Window*main_window = game->windows + WINDOW_MAIN;
    if (main_window->clicked_control_id == NULL_CONTROL)
    {
        return ACTION_NONE;
    }
    uint8_t id = id_of_control_cursor_is_over(main_window, cursor_x, cursor_y);
    if (id == main_window->clicked_control_id)
    {
        main_window->clicked_control_id = NULL_CONTROL;
        if (id == main_window->controls[MAIN_WINDOW_SAVE_GAME].base_id)
        {
            return ACTION_SAVE_GAME;
        }
        if (id == main_window->controls[MAIN_WINDOW_CLAIM_DRAW].base_id)
        {
            return ACTION_CLAIM_DRAW;
        }
        Position*current_position = game->position_pool + game->current_position_index;
        if (game->selected_piece_index == NULL_PIECE)
        {
            uint8_t square_index =
                SCREEN_SQUARE_INDEX(id - main_window->controls[MAIN_WINDOW_BOARD].base_id,
                    game->engine_player_index);
            uint8_t selected_piece_index = current_position->squares[square_index];
            if (PLAYER_INDEX(selected_piece_index) == current_position->active_player_index)
            {
                game->selected_move_index = &current_position->first_move_index;
                while (*game->selected_move_index != NULL_POSITION)
                {
                    Position*move = game->position_pool + *game->selected_move_index;
                    if (move->squares[square_index] != selected_piece_index)
                    {
                        game->selected_piece_index = selected_piece_index;
                        return ACTION_REDRAW;
                    }
                    game->selected_move_index = &move->next_move_index;
                }
            }
        }
        else
        {
            uint8_t promotion_selector_base_id =
                main_window->controls[MAIN_WINDOW_PROMOTION_SELECTOR].base_id;
            if (id >= promotion_selector_base_id &&
                id < promotion_selector_base_id + ARRAY_COUNT(g_promotion_options))
            {
                PieceType selected_piece_type =
                    g_promotion_options[id - promotion_selector_base_id];
                Position*move = game->position_pool + *game->selected_move_index;
                while (move->pieces[game->selected_piece_index].piece_type != selected_piece_type)
                {
                    game->selected_move_index = &move->next_move_index;
                    move = game->position_pool + move->next_move_index;
                }
                end_turn(game);
                game->is_promoting = false;
            }
            else
            {
                uint8_t square_index =
                    SCREEN_SQUARE_INDEX(id - main_window->controls[MAIN_WINDOW_BOARD].base_id,
                        game->engine_player_index);
                uint16_t*piece_first_move_index = game->selected_move_index;
                do
                {
                    Position*move = game->position_pool + *game->selected_move_index;
                    if (move->squares[square_index] == game->selected_piece_index)
                    {
                        if (current_position->pieces[game->selected_piece_index].piece_type !=
                            move->pieces[game->selected_piece_index].piece_type)
                        {
                            game->is_promoting = true;
                        }
                        else
                        {
                            end_turn(game);
                        }
                        main_window->clicked_control_id = NULL_CONTROL;
                        return ACTION_REDRAW;
                    }
                    game->selected_move_index = &move->next_move_index;
                } while (*game->selected_move_index != NULL_POSITION);
                game->selected_move_index = piece_first_move_index;
            }
        }
    }
    main_window->clicked_control_id = NULL_CONTROL;
    return ACTION_REDRAW;
}

void set_main_window_dpi(Game*game, uint16_t dpi)
{
    Window*main_window = game->windows + WINDOW_MAIN;
    set_dpi_data(game, main_window, game->font_size, dpi);
    Board*board = &main_window->controls[MAIN_WINDOW_BOARD].board;
    board->min_x = game->square_size + main_window->dpi_data->control_border_thickness;
    board->min_y = board->min_x;
    main_window->controls[MAIN_WINDOW_PROMOTION_SELECTOR].promotion_selector.min_x =
        board->min_x + 2 * COLUMNS_PER_ICON;
    Button*save_button = &main_window->controls[MAIN_WINDOW_SAVE_GAME].button;
    save_button->min_x = board->min_x + 8 * game->square_size +
        main_window->dpi_data->text_control_height +
        2 * main_window->dpi_data->control_border_thickness;
    save_button->min_y = board->min_y + 4 * game->square_size -
        (main_window->dpi_data->text_control_height +
            main_window->dpi_data->control_border_thickness / 2);
    Button*claim_draw_button = &main_window->controls[MAIN_WINDOW_CLAIM_DRAW].button;
    claim_draw_button->min_x = save_button->min_x;
    claim_draw_button->min_y = save_button->min_y + main_window->dpi_data->text_control_height +
        main_window->dpi_data->control_border_thickness;
    save_button->width = 2 * main_window->dpi_data->text_control_padding +
        max32(get_string_length(main_window->dpi_data->rasterizations, save_button->label),
            get_string_length(main_window->dpi_data->rasterizations, claim_draw_button->label));
    claim_draw_button->width = save_button->width;
    main_window->width = save_button->min_x + save_button->width +
        main_window->dpi_data->text_control_height +
        main_window->dpi_data->control_border_thickness;
    main_window->height =
        board->min_y + 9 * game->square_size + main_window->dpi_data->control_border_thickness;
}

void init_main_window(Game*game, uint16_t dpi)
{
    Window*main_window = game->windows + WINDOW_MAIN;
    game->square_size = COLUMNS_PER_ICON;
    main_window->control_count = MAIN_WINDOW_CONTROL_COUNT;
    main_window->controls = game->main_window_controls;
    Control*control = main_window->controls + MAIN_WINDOW_BOARD;
    control->control_type = CONTROL_BOARD;
    control = main_window->controls + MAIN_WINDOW_PROMOTION_SELECTOR;
    control->control_type = CONTROL_PROMOTION_SELECTOR;
    control = main_window->controls + MAIN_WINDOW_SAVE_GAME;
    control->control_type = CONTROL_BUTTON;
    control->button.label = "Save game";
    control = main_window->controls + MAIN_WINDOW_CLAIM_DRAW;
    control->control_type = CONTROL_BUTTON;
    control->button.label = "Claim draw";
    set_control_ids(main_window);
    set_main_window_dpi(game, dpi);
    main_window->pixels = 0;
    main_window->pixel_buffer_capacity = 0;
    main_window->hovered_control_id = NULL_CONTROL;
    main_window->clicked_control_id = NULL_CONTROL;
}

void draw_player(Game*game, int32_t captured_pieces_min_y, int32_t timer_text_origin_y,
    uint8_t player_index, bool draw_captured_pieces)
{
    Position*current_position = game->position_pool + game->current_position_index;
    Window*main_window = game->windows + WINDOW_MAIN;
    Control*board = main_window->controls + MAIN_WINDOW_BOARD;
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
                draw_icon(game->icon_bitmaps[player_index][piece.piece_type], main_window,
                    captured_piece_min_x, captured_pieces_min_y);
                captured_piece_min_x += game->square_size / 2;
            }
        }
        else
        {
            uint8_t screen_square_index =
                SCREEN_SQUARE_INDEX(piece.square_index, game->engine_player_index);
            int32_t bitmap_min_x =
                board->board.min_x + game->square_size * FILE(screen_square_index);
            int32_t bitmap_min_y =
                board->board.min_y + game->square_size * RANK(screen_square_index);
            if (game->selected_piece_index == piece_index)
            {
                Color*icon_bitmap = game->icon_bitmaps[current_position->active_player_index]
                    [current_position->pieces[game->selected_piece_index].piece_type];
                Color tinted_icon_bitmap[3600];
                for (size_t i = 0; i < 3600; ++i)
                {
                    Color icon_pixel = icon_bitmap[i];
                    Color*tinted_icon_pixel = tinted_icon_bitmap + i;
                    tinted_icon_pixel->red = (icon_pixel.red + 255) / 2;
                    tinted_icon_pixel->green = icon_pixel.green / 2;
                    tinted_icon_pixel->blue = icon_pixel.blue / 2;
                    tinted_icon_pixel->alpha = icon_pixel.alpha;
                }
                draw_icon(tinted_icon_bitmap, main_window, bitmap_min_x, bitmap_min_y);
            }
            else
            {
                draw_icon(game->icon_bitmaps[player_index][piece.piece_type], main_window,
                    bitmap_min_x, bitmap_min_y);
            }
        }
    }
    uint64_t time = game->seconds_left[player_index];
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
    draw_string(main_window, time_text, main_window->controls[MAIN_WINDOW_SAVE_GAME].button.min_x,
        timer_text_origin_y, g_black);
}

void draw_main_window(Game*game)
{
    Window*main_window = game->windows + WINDOW_MAIN;
    clear_window(main_window);
    Control*board = main_window->controls + MAIN_WINDOW_BOARD;
    uint32_t board_width = FILE_COUNT * game->square_size;
    draw_rectangle_border(main_window, board->board.min_x, board->board.min_y, board_width,
        board_width);
    for (size_t rank = 0; rank < RANK_COUNT; ++rank)
    {
        int32_t square_min_y = board->board.min_y + game->square_size * rank;
        for (size_t file = 0; file < FILE_COUNT; ++file)
        {
            if ((rank + file) % 2)
            {
                draw_rectangle(main_window, board->board.min_x + game->square_size * file,
                    square_min_y, game->square_size, game->square_size, g_dark_square_gray);
            }
        }
    }
    color_square_with_index(main_window, &board->board, g_hovered_blue, game->square_size,
        main_window->hovered_control_id - board->base_id);
    color_square_with_index(main_window, &board->board, g_clicked_red, game->square_size,
        main_window->clicked_control_id - board->base_id);
    Control*save_game_button = main_window->controls + MAIN_WINDOW_SAVE_GAME;
    draw_active_button(main_window, save_game_button);
    if (DRAW_BY_50_CAN_BE_CLAIMED(game))
    {
        draw_active_button(main_window, main_window->controls + MAIN_WINDOW_CLAIM_DRAW);
    }
    else
    {
        draw_button(main_window, &main_window->controls[MAIN_WINDOW_CLAIM_DRAW].button, g_white,
            g_dark_square_gray);
    }
    if (game->is_promoting)
    {
        Control*promotion_selector = main_window->controls + MAIN_WINDOW_PROMOTION_SELECTOR;
        int32_t icon_min_x = promotion_selector->promotion_selector.min_x;
        draw_rectangle(main_window, icon_min_x - main_window->dpi_data->control_border_thickness, 0,
            main_window->dpi_data->control_border_thickness, COLUMNS_PER_ICON, g_black);
        for (size_t i = 0; i < ARRAY_COUNT(g_promotion_options); ++i)
        {
            uint8_t selection_id = promotion_selector->base_id + i;
            if (main_window->clicked_control_id == selection_id)
            {
                draw_rectangle(main_window, icon_min_x, 0, COLUMNS_PER_ICON, COLUMNS_PER_ICON,
                    g_clicked_red);
            }
            else if (main_window->hovered_control_id == selection_id)
            {
                draw_rectangle(main_window, icon_min_x, 0, COLUMNS_PER_ICON, COLUMNS_PER_ICON,
                    g_hovered_blue);
            }
            draw_icon(game->icon_bitmaps[!game->engine_player_index][g_promotion_options[i]],
                main_window, icon_min_x, 0);
            icon_min_x += COLUMNS_PER_ICON;
        }
        draw_rectangle(main_window, icon_min_x, 0,
            main_window->dpi_data->control_border_thickness, COLUMNS_PER_ICON, g_black);
    }
    draw_player(game,
        board->board.min_y + board_width + main_window->dpi_data->control_border_thickness,
        board->board.min_y + main_window->dpi_data->text_line_height, game->engine_player_index,
        true);
    draw_player(game, 0,
        board->board.min_y + 7 * game->square_size + main_window->dpi_data->text_line_height,
        !game->engine_player_index, !game->is_promoting);
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

void save_piece(void**file_memory, uint32_t*file_size, Piece piece)
{
    save_value(file_memory, &piece.square_index, file_size, sizeof(piece.square_index));
    if (piece.square_index != NULL_SQUARE)
    {
        save_value(file_memory, &piece.times_moved, file_size, sizeof(piece.times_moved));
    }
}

bool load_piece(Piece*piece, Position*position, void**file_memory, uint32_t*file_size)
{
    if (!load_value(file_memory, &piece->square_index, file_size, sizeof(piece->square_index)))
    {
        return false;
    }
    if (piece->square_index != NULL_SQUARE)
    {
        if (piece->square_index > NULL_SQUARE)
        {
            return false;
        }
        if (!load_value(file_memory, &piece->times_moved, file_size, sizeof(piece->times_moved)))
        {
            return false;
        }
    }
    return true;
}

#define SAVE_FILE_STATIC_PART_SIZE 119

uint32_t save_game(void*file_memory, Game*game)
{
    uint64_t time_since_last_move = get_time() - game->last_move_time;
    uint32_t file_size = 0;
    save_value(&file_memory, &time_since_last_move, &file_size, sizeof(time_since_last_move));
    save_value(&file_memory, &game->time_increment, &file_size, sizeof(game->time_increment));
    Position*current_position = game->position_pool + game->current_position_index;
    save_value(&file_memory, &current_position->en_passant_file, &file_size,
        sizeof(current_position->en_passant_file));
    uint8_t active_player_index = current_position->active_player_index;
    save_value(&file_memory, &active_player_index, &file_size, sizeof(active_player_index));
    for (size_t player_index = 0; player_index < 2; ++player_index)
    {
        save_value(&file_memory, game->times_left_as_of_last_move + player_index, &file_size,
            sizeof(game->times_left_as_of_last_move[player_index]));
        uint8_t player_pieces_index = PLAYER_PIECES_INDEX(player_index);
        Piece*player_pieces = current_position->pieces + player_pieces_index;
        for (size_t piece_index = 0; piece_index < A_PAWN_INDEX; ++piece_index)
        {
            save_piece(&file_memory, &file_size, player_pieces[piece_index]);
        }
        for (size_t piece_index = A_PAWN_INDEX; piece_index < A_PAWN_INDEX + 8; ++piece_index)
        {
            Piece piece = player_pieces[piece_index];
            uint8_t piece_type = piece.piece_type;
            save_value(&file_memory, &piece_type, &file_size, sizeof(piece_type));
            save_piece(&file_memory, &file_size, piece);
        }
    }
    save_value(&file_memory, &game->draw_by_50_count, &file_size, sizeof(game->draw_by_50_count));
    save_value(&file_memory, &game->engine_player_index, &file_size,
        sizeof(game->engine_player_index));
    save_value(&file_memory, &game->unique_state_count, &file_size,
        sizeof(game->unique_state_count));
    ASSERT(file_size <= SAVE_FILE_STATIC_PART_SIZE);
    BoardStateNode*external_nodes = EXTERNAL_STATE_NODES(game);
    for (size_t bucket_index = 0; bucket_index < game->state_bucket_count; ++bucket_index)
    {
        BoardStateNode*node = game->state_buckets + bucket_index;
        if (node->count && node->generation == game->state_generation)
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

bool load_file(Game*game, void*file_memory, uint32_t file_size)
{
    uint64_t time_since_last_move;
    if (!load_value(&file_memory, &time_since_last_move, &file_size, sizeof(time_since_last_move)))
    {
        return false;
    }
    if (!load_value(&file_memory, &game->time_increment, &file_size, sizeof(game->time_increment)))
    {
        return false;
    }
    Position*current_position = game->position_pool + game->first_leaf_index;
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
        uint64_t*time_left_as_of_last_move = game->times_left_as_of_last_move + player_index;
        if (!load_value(&file_memory, time_left_as_of_last_move, &file_size,
            sizeof(*time_left_as_of_last_move)))
        {
            return false;
        }
        game->seconds_left[player_index] = *time_left_as_of_last_move / g_counts_per_second;
        Piece*player_pieces = current_position->pieces + PLAYER_PIECES_INDEX(player_index);
        for (size_t piece_index = 0; piece_index < A_PAWN_INDEX; ++piece_index)
        {
            if (!load_piece(player_pieces + piece_index, current_position, &file_memory,
                &file_size))
            {
                return false;
            }
        }
        for (size_t piece_index = A_PAWN_INDEX; piece_index < A_PAWN_INDEX + 8; ++piece_index)
        {
            Piece*piece = player_pieces + piece_index;
            uint8_t piece_type;
            if (!load_value(&file_memory, &piece_type, &file_size, sizeof(piece_type)) ||
                piece_type >= PIECE_TYPE_COUNT)
            {
                return false;
            }
            piece->piece_type = piece_type;
            if (!load_piece(piece, current_position, &file_memory, &file_size))
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
    if (game->times_left_as_of_last_move[active_player_index] < time_since_last_move)
    {
        return false;
    }
    if (!load_value(&file_memory, &game->draw_by_50_count, &file_size,
        sizeof(game->draw_by_50_count)))
    {
        return false;
    }
    if (!load_value(&file_memory, &game->engine_player_index, &file_size,
        sizeof(game->engine_player_index)) || game->engine_player_index > PLAYER_INDEX_BLACK)
    {
        return false;
    }
    if (!load_value(&file_memory, &game->unique_state_count, &file_size,
        sizeof(game->unique_state_count)))
    {
        return false;
    }
    uint32_t bucket_count_bit_count;
    BIT_SCAN_REVERSE(&bucket_count_bit_count, game->unique_state_count);
    uint16_t state_bucket_count = 1 << bucket_count_bit_count;
    if (state_bucket_count < game->unique_state_count)
    {
        state_bucket_count = state_bucket_count << 1;
    }
    init_board_state_archive(game, state_bucket_count);
    for (uint16_t i = 0; i < game->unique_state_count; ++i)
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
        archive_board_state(game, &state);
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
    current_position->previous_leaf_index = NULL_POSITION;
    current_position->next_leaf_index = NULL_POSITION;
    make_move_current(game, game->first_leaf_index);
    game->last_move_time = get_time() - time_since_last_move;
    game->selected_piece_index = NULL_PIECE;
    return true;
}

bool load_game(Game*game, void*file_memory, uint32_t file_size)
{
    game->first_leaf_index = allocate_position(game);
    bool out = load_file(game, file_memory, file_size);
    if (out)
    {
        game->run_engine = true;
    }
    else
    {
        free_position(game, game->first_leaf_index);
    }
    return out;
}

void free_game(Game*game)
{
    FREE_MEMORY(game->state_buckets);
    game->pool_cursor = 0;
    game->index_of_first_free_pool_position = NULL_POSITION;
}

void init_piece(Position*position, PieceType piece_type, uint8_t piece_index, uint8_t square_index,
    uint8_t player_index)
{
    piece_index += PLAYER_PIECES_INDEX(player_index);
    Piece*piece = position->pieces + piece_index;
    piece->square_index = square_index;
    piece->times_moved = 0;
    piece->piece_type = piece_type;
    position->squares[square_index] = piece_index;
}

void init_game(Game*game)
{
    game->selected_piece_index = NULL_PIECE;
    game->first_leaf_index = allocate_position(game);
    game->next_leaf_to_evaluate_index = game->first_leaf_index;
    Position*position = game->position_pool + game->first_leaf_index;
    position->parent_index = NULL_POSITION;
    position->next_move_index = NULL_POSITION;
    position->previous_leaf_index = NULL_POSITION;
    position->next_leaf_index = NULL_POSITION;
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
        game->seconds_left[player_index] = game->time_control[4].digit +
            10 * game->time_control[3].digit + 60 * game->time_control[2].digit +
            600 * game->time_control[1].digit + 3600 * game->time_control[0].digit;
    }
    for (size_t square_index = 16; square_index < 48; ++square_index)
    {
        position->squares[square_index] = NULL_PIECE;
    }
    init_board_state_archive(game, 32);
    game->selected_move_index = &game->first_leaf_index;
    make_move_current(game, game->first_leaf_index);
    Window*main_window = game->windows + WINDOW_MAIN;
    main_window->hovered_control_id = NULL_CONTROL;
    main_window->clicked_control_id = NULL_CONTROL;
    game->draw_by_50_count = 0;
    game->run_engine = true;
    game->is_promoting = false;
    game->times_left_as_of_last_move[0] = g_counts_per_second * game->seconds_left[0];
    game->times_left_as_of_last_move[1] = game->times_left_as_of_last_move[0];
    game->time_increment = g_counts_per_second * (game->increment[2].digit +
        10 * game->increment[1].digit + 60 * game->increment[0].digit);
    game->last_move_time = get_time();
}

void init(Game*game, void*font_data, size_t font_data_size)
{
    FT_Init_FreeType(&g_freetype_library);
    FT_New_Memory_Face(g_freetype_library, font_data, font_data_size, 0, &g_face);
    game->windows[WINDOW_START].controls = game->dialog_controls;
    game->position_pool = RESERVE_MEMORY(NULL_POSITION * sizeof(Position));
    game->index_of_first_free_pool_position = NULL_POSITION;
    game->pool_cursor = 0;
    for (size_t i = 0; i < ARRAY_COUNT(game->dpi_datas); ++i)
    {
        game->dpi_datas[i].reference_count = 0;
    }
    g_max_time = g_counts_per_second * 359999;
    DigitInput*digit = game->time_control;
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
    digit = game->increment;
    digit->digit = 0;
    digit->is_before_colon = true;
    ++digit;
    digit->digit = 0;
    digit->is_before_colon = false;
    ++digit;
    digit->digit = 5;
    digit->is_before_colon = false;
    game->run_engine = false;
}