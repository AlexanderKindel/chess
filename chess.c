#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
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

typedef struct DigitInput
{
    uint8_t digit : 4;
    bool is_before_colon;
} DigitInput;

typedef struct TimeInput
{
    char*label;
    DigitInput*digits;
    int32_t min_x;
    int32_t min_y;
    uint8_t digit_count;
} TimeInput;

typedef struct Grid
{
    int32_t min_x;
    int32_t min_y;
    uint8_t dimension;
} Grid;

typedef enum ControlType
{
    CONTROL_BUTTON,
    CONTROL_GRID,
    CONTROL_TIME_INPUT
} ControlType;

typedef struct Control
{
    union
    {
        Button button;
        Grid grid;
        TimeInput time_input;
    };
    uint8_t base_id;
    uint8_t control_type;
} Control;

typedef enum TimeControlWindowControlIndex
{
    TIME_CONTROL_WINDOW_TIME,
    TIME_CONTROL_WINDOW_INCREMENT,
    TIME_CONTROL_WINDOW_START_GAME,
    TIME_CONTROL_WINDOW_CONTROL_COUNT
} TimeControlWindowControlIndex;

typedef enum MainWindowControlIndex
{
    MAIN_WINDOW_BOARD,
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
    char time_control[TIME_CONTROL_WINDOW_CONTROL_COUNT];
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

typedef struct Piece
{
    uint8_t square_index;
    uint8_t times_moved;
    bool can_be_captured_en_passant : 1;
    uint8_t piece_type : 3;
    bool is_captured;
} Piece;

#define RANK_COUNT 8
#define FILE_COUNT 8

typedef struct Position
{
    uint16_t next_sibling_index;
    uint16_t first_child_index;
    uint8_t squares[RANK_COUNT * FILE_COUNT];
    Piece pieces[32];
    uint8_t captured_piece_counts;
    bool reset_draw_by_50_count;
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

typedef struct DPIData
{
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
    WINDOW_TIME_CONTROL = 1,
    WINDOW_COUNT = 2
} WindowIndex;

#define COLUMNS_PER_ICON 60
#define PIXELS_PER_ICON COLUMNS_PER_ICON * COLUMNS_PER_ICON

typedef struct Game
{
    Position*position_pool;
    BoardStateNode*state_buckets;
    Color icon_bitmaps[2][PIECE_TYPE_COUNT][PIXELS_PER_ICON];
    Control dialog_controls[sizeof(DialogControlCount)];
    Control main_window_controls[MAIN_WINDOW_CONTROL_COUNT];
    Window windows[WINDOW_COUNT];
    DPIData dpi_datas[WINDOW_COUNT];
    uint16_t*selected_move_index;
    uint64_t times_left_as_of_last_move[2];
    uint64_t last_move_time;
    uint64_t time_increment;
    int32_t cursor_offset;
    uint32_t square_size;
    uint32_t font_size;
    uint16_t seconds_left[2];
    uint16_t current_position_index;
    uint16_t index_of_first_free_pool_position;
    uint16_t pool_cursor;
    uint16_t state_bucket_count;
    uint16_t external_state_node_count;
    uint16_t unique_state_count;
    uint16_t draw_by_50_count;
    DigitInput time_control[5];
    DigitInput increment[3];
    uint8_t cursor_index;
    uint8_t state_generation;
    uint8_t active_player_index;
    uint8_t selected_piece_index;
    uint8_t selected_digit_id;
} Game;

#define NULL_CONTROL UINT8_MAX
#define NULL_PIECE 32
#define NULL_POSITION UINT16_MAX
#define NULL_STATE UINT16_MAX
#define WHITE_PLAYER_INDEX 0
#define BLACK_PLAYER_INDEX 1

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

#define PLAYER_INDEX(piece_index) ((piece_index) >> 4)
#define KING_RANK(player_index) (7 * !(player_index))
#define FORWARD_DELTA(player_index) (((player_index) << 1) - 1)
#define GRID_RANK(square_index, dimension) ((square_index) / (dimension))
#define GRID_FILE(square_index, dimension) ((square_index) % (dimension))
#define RANK(square_index) GRID_RANK((square_index), 8)
#define FILE(square_index) GRID_FILE((square_index), 8)
#define GRID_SQUARE_INDEX(rank, file, dimension) ((dimension) * (rank) + (file))
#define SQUARE_INDEX(rank, file) (FILE_COUNT * (rank) + (file))
#define EXTERNAL_STATE_NODES(game) ((game)->state_buckets + (game)->state_bucket_count)
#define PLAYER_PIECES_INDEX(player_index) ((player_index) << 4)

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
    STATUS_CONTINUE,
    STATUS_REPETITION,
    STATUS_STALEMATE,
    STATUS_TIME_OUT
} GameStatus;

bool archive_board_state(Game*game, BoardState*state)
{
    //FNV hash the state into an index less than buckets.size().
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

bool piece_attacks_square_along_diagonal(Position*position, uint8_t piece_index,
    uint8_t square_index)
{
    Piece attacking_piece = position->pieces[piece_index];
    int8_t rank_delta = RANK(square_index) - RANK(attacking_piece.square_index);
    int8_t file_delta = FILE(square_index) - FILE(attacking_piece.square_index);
    if (rank_delta != file_delta && rank_delta != -file_delta)
    {
        return false;
    }
    uint8_t attacked_square = position->squares[square_index];
    if (attacked_square != NULL_PIECE && PLAYER_INDEX(attacked_square) == PLAYER_INDEX(piece_index))
    {
        return false;
    }
    int8_t square_index_increment;
    if (RANK(attacking_piece.square_index) < RANK(square_index))
    {
        square_index_increment = FILE_COUNT;
    }
    else
    {
        square_index_increment = -FILE_COUNT;
    }
    if (FILE(attacking_piece.square_index) < FILE(square_index))
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
        if (RANK(attacking_piece.square_index) == RANK(square_index))
        {
            return true;
        }
        if (position->squares[attacking_piece.square_index] != NULL_PIECE)
        {
            return false;
        }
    }
}

bool piece_attacks_square_along_axis(Position*position, uint8_t piece_index, uint8_t square_index)
{
    uint8_t attacked_square = position->squares[square_index];
    if (attacked_square != NULL_PIECE && PLAYER_INDEX(attacked_square) == PLAYER_INDEX(piece_index))
    {
        return false;
    }
    Piece attacking_piece = position->pieces[piece_index];
    int8_t square_index_increment;
    if (RANK(square_index) == RANK(attacking_piece.square_index))
    {
        if (FILE(attacking_piece.square_index) < FILE(square_index))
        {
            square_index_increment = 1;
        }
        else
        {
            square_index_increment = -1;
        }
    }
    else if (FILE(square_index) == FILE(attacking_piece.square_index))
    {
        if (RANK(attacking_piece.square_index) < RANK(square_index))
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
        if (attacking_piece.square_index == square_index)
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

bool piece_attacks_square(Position*position, uint8_t piece_index, uint8_t square_index)
{
    Piece attacking_piece = position->pieces[piece_index];
    if (attacking_piece.is_captured)
    {
        return false;
    }
    switch (attacking_piece.piece_type)
    {
    case PIECE_PAWN:
    {
        if (abs_delta(FILE(square_index), FILE(attacking_piece.square_index)) == 1)
        {
            uint8_t attacked_piece_index = position->squares[square_index];
            if (attacked_piece_index != NULL_PIECE)
            {
                uint8_t attacking_player_index = PLAYER_INDEX(piece_index);
                if (PLAYER_INDEX(attacked_piece_index) != attacking_player_index)
                {
                    int8_t attacking_forward_delta = FORWARD_DELTA(attacking_player_index);
                    return (RANK(square_index) == RANK(attacking_piece.square_index) +
                        attacking_forward_delta ||
                        (RANK(square_index) == RANK(attacking_piece.square_index) &&
                            position->squares[square_index +
                                FILE_COUNT * attacking_forward_delta] == NULL_PIECE &&
                            position->pieces[attacked_piece_index].can_be_captured_en_passant));
                }
            }
        }
        return false;
    }
    case PIECE_BISHOP:
    {
        return piece_attacks_square_along_diagonal(position, piece_index, square_index);
    }
    case PIECE_KNIGHT:
    {
        uint8_t attacked_piece_index = position->squares[square_index];
        if (attacked_piece_index != NULL_PIECE ||
            PLAYER_INDEX(attacked_piece_index) != PLAYER_INDEX(piece_index))
        {
            uint8_t rank_delta = abs_delta(RANK(square_index), RANK(attacking_piece.square_index));
            uint8_t file_delta = abs_delta(FILE(square_index), FILE(attacking_piece.square_index));
            if ((rank_delta == 2 && file_delta == 1) || (rank_delta == 1 && file_delta == 2))
            {
                return true;
            }
        }
        return false;
    }
    case PIECE_ROOK:
    {
        return piece_attacks_square_along_axis(position, piece_index, square_index);
    }
    case PIECE_QUEEN:
    {
        return piece_attacks_square_along_diagonal(position, piece_index, square_index) ||
            piece_attacks_square_along_axis(position, piece_index, square_index);
    }
    }
    uint8_t attacked_piece_index = position->squares[square_index];
    return (attacked_piece_index != NULL_PIECE ||
        PLAYER_INDEX(attacked_piece_index) != PLAYER_INDEX(piece_index)) &&
        abs_delta(RANK(square_index), RANK(attacking_piece.square_index)) <= 1 &&
        abs_delta(FILE(square_index), FILE(attacking_piece.square_index)) <= 1;
}

bool player_attacks_square(Position*position, uint8_t player_index, uint8_t square_index)
{
    uint8_t army_base_piece_index = 16 * player_index;
    for (size_t i = 0; i < 16; ++i)
    {
        if (piece_attacks_square(position, army_base_piece_index + i, square_index))
        {
            return true;
        }
    }
    return false;
}

bool player_is_checked(Position*position, uint8_t player_index)
{
    uint8_t king_square_index = position->pieces[KING_INDEX + 16 * player_index].square_index;
    uint8_t opponent_base_piece_index = 16 * !player_index;
    for (size_t i = 0; i < 16; ++i)
    {
        if (piece_attacks_square(position, opponent_base_piece_index + i, king_square_index))
        {
            return true;
        }
    }
    return false;
}

void capture_piece(Position*position, uint8_t piece_index)
{
    position->pieces[piece_index].is_captured = true;
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

uint16_t allocate_position(Game*game)
{
    Position*new_position;
    uint16_t new_position_index;
    if (game->index_of_first_free_pool_position == NULL_POSITION)
    {
        new_position_index = game->pool_cursor;
        new_position = game->position_pool + new_position_index;
        COMMIT_MEMORY(new_position, g_page_size);
        ++game->pool_cursor;
    }
    else
    {
        new_position_index = game->index_of_first_free_pool_position;
        new_position = game->position_pool + new_position_index;
        game->index_of_first_free_pool_position = new_position->first_child_index;
    }
    return new_position_index;
}

Position*add_position_duplicate_as_child(Game*game, Position*position, Position*parent)
{
    uint16_t new_position_index = allocate_position(game);
    Position*new_position = game->position_pool + new_position_index;
    memcpy(&new_position->squares, &position->squares, sizeof(position->squares));
    memcpy(&new_position->pieces, &position->pieces, sizeof(position->pieces));
    new_position->next_sibling_index = parent->first_child_index;
    new_position->first_child_index = NULL_POSITION;
    new_position->reset_draw_by_50_count = false;
    parent->first_child_index = new_position_index;
    return new_position;
}

Position*add_move(Game*game, Position*position, uint8_t piece_index,
    uint8_t destination_square_index)
{
    uint8_t destination_square = position->squares[destination_square_index];
    if (destination_square == NULL_PIECE)
    {
        Position*move = add_position_duplicate_as_child(game, position, position);
        move_piece_to_square(move, piece_index, destination_square_index);
        return move;
    }
    else if (PLAYER_INDEX(destination_square) != PLAYER_INDEX(piece_index))
    {
        Position*move = add_position_duplicate_as_child(game, position, position);
        capture_piece(move, destination_square);
        move_piece_to_square(move, piece_index, destination_square_index);
    }
    return 0;
}

void add_promotion_options(Game*game, Position*position, Position*promotion, uint8_t piece_index,
    uint8_t back_rank)
{
    if (RANK(promotion->pieces[piece_index].square_index) == back_rank)
    {
        promotion->pieces[piece_index].piece_type = PIECE_ROOK;
        promotion = add_position_duplicate_as_child(game, promotion, position);
        promotion->pieces[piece_index].piece_type = PIECE_KNIGHT;
        promotion->reset_draw_by_50_count = true;
        promotion = add_position_duplicate_as_child(game, promotion, position);
        promotion->pieces[piece_index].piece_type = PIECE_BISHOP;
        promotion->reset_draw_by_50_count = true;
        promotion = add_position_duplicate_as_child(game, promotion, position);
        promotion->pieces[piece_index].piece_type = PIECE_QUEEN;
        promotion->reset_draw_by_50_count = true;
    }
}

void add_diagonal_pawn_move(Game*game, Position*position, uint8_t piece_index,
    uint8_t destination_square_index, uint8_t back_rank)
{
    uint8_t destination_square = position->squares[destination_square_index];
    if (destination_square == NULL_PIECE)
    {
        uint8_t en_passant_capturable_square_index = position->pieces[piece_index].square_index - 1;
        uint8_t en_passant_capturable_square =
            position->squares[en_passant_capturable_square_index];
        if (en_passant_capturable_square != NULL_PIECE &&
            position->pieces[en_passant_capturable_square].can_be_captured_en_passant)
        {
            Position*move = add_position_duplicate_as_child(game, position, position);
            capture_piece(move, en_passant_capturable_square);
            move_piece_to_square(move, piece_index, destination_square_index);
        }
    }
    else if (PLAYER_INDEX(destination_square) != PLAYER_INDEX(piece_index))
    {
        Position*move = add_position_duplicate_as_child(game, position, position);
        capture_piece(move, destination_square);
        move_piece_to_square(move, piece_index, destination_square_index);
        add_promotion_options(game, position, move, piece_index, back_rank);
    }
}

void add_half_diagonal_moves(Game*game, Position*position, uint8_t piece_index, int8_t rank_delta,
    int8_t file_delta)
{
    uint8_t player_index = PLAYER_INDEX(piece_index);
    uint8_t destination_square_index = position->pieces[piece_index].square_index;
    uint8_t destination_rank = RANK(destination_square_index);
    uint8_t destination_file = FILE(destination_square_index);
    do
    {
        destination_rank += rank_delta;
        destination_file += file_delta;
    } while (destination_rank < 8 && destination_file < 8 && add_move(game, position, piece_index,
        SQUARE_INDEX(destination_rank, destination_file)));
}

void add_diagonal_moves(Game*game, Position*position, uint8_t piece_index)
{
    add_half_diagonal_moves(game, position, piece_index, 1, 1);
    add_half_diagonal_moves(game, position, piece_index, 1, -1);
    add_half_diagonal_moves(game, position, piece_index, -1, 1);
    add_half_diagonal_moves(game, position, piece_index, -1, -1);
}

void add_half_horizontal_moves(Game*game, Position*position, uint8_t piece_index, int8_t delta,
    bool loses_castling_rights)
{
    uint8_t destination_square_index = position->pieces[piece_index].square_index;
    uint8_t destination_rank = RANK(destination_square_index);
    uint8_t destination_file = FILE(destination_square_index);
    while (true)
    {
        destination_file += delta;
        if (destination_file > 7)
        {
            break;
        }
        Position*move =
            add_move(game, position, piece_index, SQUARE_INDEX(destination_rank, destination_file));
        if (!move)
        {
            break;
        }
        move->reset_draw_by_50_count |= loses_castling_rights;
    }
}

void add_half_vertical_moves(Game*game, Position*position, uint8_t piece_index, int8_t delta,
    bool loses_castling_rights)
{
    uint8_t destination_square_index = position->pieces[piece_index].square_index;
    uint8_t destination_rank = RANK(destination_square_index);
    uint8_t destination_file = FILE(destination_square_index);
    while (true)
    {
        destination_rank += delta;
        if (destination_rank > 7)
        {
            break;
        }
        Position*move =
            add_move(game, position, piece_index, SQUARE_INDEX(destination_rank, destination_file));
        if (!move)
        {
            break;
        }
        move->reset_draw_by_50_count |= loses_castling_rights;
    }
}

void add_moves_along_axes(Game*game, Position*position, uint8_t piece_index,
    bool loses_castling_rights)
{
    add_half_horizontal_moves(game, position, piece_index, 1, loses_castling_rights);
    add_half_horizontal_moves(game, position, piece_index, -1, loses_castling_rights);
    add_half_vertical_moves(game, position, piece_index, 1, loses_castling_rights);
    add_half_vertical_moves(game, position, piece_index, -1, loses_castling_rights);
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

void free_position(Game*game, uint16_t position_index)
{
    Position*position = game->position_pool + position_index;
    if (position->next_sibling_index != NULL_POSITION)
    {
        free_position(game, position->next_sibling_index);
    }
    if (position->first_child_index != NULL_POSITION)
    {
        free_position(game, position->first_child_index);
    }
    position->first_child_index = game->index_of_first_free_pool_position;
    game->index_of_first_free_pool_position = position_index;
}

void get_moves(Game*game, Position*position, uint8_t player_index)
{
    uint8_t player_pieces_index = PLAYER_PIECES_INDEX(player_index);
    uint8_t max_piece_index = player_pieces_index + 16;
    for (size_t piece_index = player_pieces_index + 8; piece_index < max_piece_index; ++piece_index)
    {
        position->pieces[piece_index].can_be_captured_en_passant = false;
    }
    for (uint8_t piece_index = player_pieces_index; piece_index < max_piece_index; ++piece_index)
    {
        Piece piece = position->pieces[piece_index];
        if (piece.is_captured)
        {
            continue;
        }
        switch (piece.piece_type)
        {
        case PIECE_PAWN:
        {
            int8_t forward_delta = FORWARD_DELTA(player_index);
            uint8_t back_rank = KING_RANK(!player_index);
            if (FILE(piece.square_index))
            {
                add_diagonal_pawn_move(game, position, piece_index,
                    piece.square_index + forward_delta * FILE_COUNT - 1, back_rank);
            }
            if (FILE(piece.square_index) < 7)
            {
                add_diagonal_pawn_move(game, position, piece_index,
                    piece.square_index + forward_delta * FILE_COUNT + 1, back_rank);
            }
            uint8_t destination_square_index = piece.square_index + forward_delta * FILE_COUNT;
            if (position->squares[destination_square_index] == NULL_PIECE)
            {
                Position*move = add_position_duplicate_as_child(game, position, position);
                move_piece_to_square(move, piece_index, destination_square_index);
                move->reset_draw_by_50_count = true;
                destination_square_index += forward_delta * FILE_COUNT;
                add_promotion_options(game, position, move, piece_index, back_rank);
                if (!piece.times_moved && position->squares[destination_square_index] == NULL_PIECE)
                {
                    move = add_position_duplicate_as_child(game, position, position);
                    move_piece_to_square(move, piece_index, destination_square_index);
                    move->reset_draw_by_50_count = true;
                    move->pieces[piece_index].can_be_captured_en_passant = true;
                }
            }
            break;
        }
        case PIECE_BISHOP:
        {
            add_diagonal_moves(game, position, piece_index);
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
                    add_move(game, position, piece_index, SQUARE_INDEX(rank - 2, file - 1));
                }
                if (rank < 6)
                {
                    add_move(game, position, piece_index, SQUARE_INDEX(rank + 2, file - 1));
                }
                if (file > 1)
                {
                    if (rank > 0)
                    {
                        add_move(game, position, piece_index, SQUARE_INDEX(rank - 1, file - 2));
                    }
                    if (rank < 7)
                    {
                        add_move(game, position, piece_index, SQUARE_INDEX(rank + 1, file - 2));
                    }
                }
            }
            if (file < 7)
            {
                if (rank > 1)
                {
                    add_move(game, position, piece_index, SQUARE_INDEX(rank - 2, file + 1));
                }
                if (rank < 6)
                {
                    add_move(game, position, piece_index, SQUARE_INDEX(rank + 2, file + 1));
                }
                if (file < 6)
                {
                    if (rank > 0)
                    {
                        add_move(game, position, piece_index, SQUARE_INDEX(rank - 1, file + 2));
                    }
                    if (rank < 7)
                    {
                        add_move(game, position, piece_index, SQUARE_INDEX(rank + 1, file + 2));
                    }
                }
            }
            break;
        }
        case PIECE_ROOK:
        {
            add_moves_along_axes(game, position, piece_index, !piece.times_moved &&
                !position->pieces[(player_index << 4) + KING_INDEX].times_moved);
            break;
        }
        case PIECE_QUEEN:
        {
            add_diagonal_moves(game, position, piece_index);
            add_moves_along_axes(game, position, piece_index, false);
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
                    Position*move = add_move(game, position, piece_index, 
                        SQUARE_INDEX(move_ranks[rank_index], move_files[file_index]));
                    if (move)
                    {
                        move->reset_draw_by_50_count |= loses_castling_rights;
                    }
                }
            }
            if (!piece.times_moved &&
                !player_attacks_square(position, !player_index, piece.square_index))
            {
                if (!position->pieces[a_rook_index].times_moved &&
                    position->squares[piece.square_index - 1] == NULL_PIECE &&
                    position->squares[piece.square_index - 2] == NULL_PIECE &&
                    position->squares[piece.square_index - 3] == NULL_PIECE &&
                    !player_attacks_square(position, !player_index, piece.square_index - 1) &&
                    !player_attacks_square(position, !player_index, piece.square_index - 2))
                {
                    Position*move = add_move(game, position, piece_index, piece.square_index - 2);
                    move->reset_draw_by_50_count = true;
                    move_piece_to_square(move, a_rook_index, piece.square_index - 1);
                }
                if (!position->pieces[h_rook_index].times_moved &&
                    position->squares[piece.square_index + 1] == NULL_PIECE &&
                    position->squares[piece.square_index + 2] == NULL_PIECE &&
                    !player_attacks_square(position, !player_index, piece.square_index + 1) &&
                    !player_attacks_square(position, !player_index, piece.square_index + 2))
                {
                    Position*move = add_move(game, position, piece_index, piece.square_index + 2);
                    move->reset_draw_by_50_count = true;
                    move_piece_to_square(move, h_rook_index, piece.square_index + 1);
                }
            }
        }
        }
    }
    uint16_t*move_index = &position->first_child_index;
    while (*move_index != NULL_POSITION)
    {
        Position*move = game->position_pool + *move_index;
        if (player_is_checked(move, player_index))
        {
            uint16_t next_move_index = move->next_sibling_index;
            move->next_sibling_index = NULL_POSITION;
            free_position(game, *move_index);
            *move_index = next_move_index;
        }
        else
        {
            move_index = &move->next_sibling_index;
        }
    }
}

bool make_position_current(Game*game, uint16_t position_index)
{
    Position*position = game->position_pool + position_index;
    if (position->reset_draw_by_50_count)
    {
        game->draw_by_50_count = 0;
        ++game->state_generation;
    }
    ++game->draw_by_50_count;
    get_moves(game, position, game->active_player_index);
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
                    PIECE_TYPE_COUNT * PLAYER_INDEX(square) + game->active_player_index;
                if (use_high_bits)
                {
                    state.occupied_square_hashes[square_hash_index] |= square_hash << 4;
                    use_high_bits = false;
                    ++square_hash_index;
                }
                else
                {
                    state.occupied_square_hashes[square_hash_index] |=
                        square_hash;
                    use_high_bits = true;
                }
                state.square_mask |= square_mask_digit;
            }
            square_mask_digit = square_mask_digit << 1;
        }
    }
    game->current_position_index = position_index;
    return archive_board_state(game, &state);
}

bool point_is_in_rect(int32_t x, int32_t y, int32_t min_x, int32_t min_y, uint32_t width,
    uint32_t height)
{
    return x >= min_x && y >= min_y && x < min_x + width && y < min_y + height;
}

uint8_t id_of_grid_square_cursor_is_over(Control*grid, int32_t cursor_x, int32_t cursor_y)
{
    if (cursor_x >= grid->grid.min_x && cursor_y >= grid->grid.min_y)
    {
        uint8_t rank = (cursor_y - grid->grid.min_y) / COLUMNS_PER_ICON;
        uint8_t file = (cursor_x - grid->grid.min_x) / COLUMNS_PER_ICON;
        if (rank < grid->grid.dimension && file < grid->grid.dimension)
        {
            return grid->base_id + GRID_SQUARE_INDEX(rank, file, grid->grid.dimension);
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
        case CONTROL_BUTTON:
        {
            if (point_is_in_rect(cursor_x, cursor_y, control->button.min_x, control->button.min_y,
                control->button.width, window->dpi_data->text_control_height))
            {
                return control->base_id;
            }
            break;
        }
        case CONTROL_GRID:
        {
            uint8_t id = id_of_grid_square_cursor_is_over(control, cursor_x, cursor_y);
            if (id != NULL_CONTROL)
            {
                return id;
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
    if (time_since_last_move >= game->times_left_as_of_last_move[game->active_player_index])
    {
        new_seconds_left = 0;
        out |= UPDATE_TIMER_TIME_OUT;
    }
    else
    {
        new_seconds_left = (game->times_left_as_of_last_move[game->active_player_index] -
            time_since_last_move) / g_counts_per_second;
    }
    if (new_seconds_left != game->seconds_left[game->active_player_index])
    {
        game->seconds_left[game->active_player_index] = new_seconds_left;
        out |= UPDATE_TIMER_REDRAW;
    }
    return out;
}

GameStatus make_selected_move_current(Game*game)
{
    uint64_t move_time = get_time();
    uint64_t time_since_last_move = move_time - game->last_move_time;
    uint64_t*time_left_as_of_last_move =
        game->times_left_as_of_last_move + game->active_player_index;
    if (time_since_last_move >= *time_left_as_of_last_move)
    {
        game->seconds_left[game->active_player_index] = 0;
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
    game->seconds_left[game->active_player_index] =
        *time_left_as_of_last_move / g_counts_per_second;
    game->last_move_time = move_time;
    uint32_t old_current_position_index = game->current_position_index;
    game->active_player_index = !game->active_player_index;
    game->selected_piece_index = NULL_PIECE;
    if (make_position_current(game, *game->selected_move_index))
    {
        Position*move = game->position_pool + game->current_position_index;
        if (move->first_child_index == NULL_POSITION)
        {
            if (player_is_checked(move, game->active_player_index))
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
            *game->selected_move_index = move->next_sibling_index;
            move->next_sibling_index = NULL_POSITION;
            free_position(game, old_current_position_index);
            return STATUS_CONTINUE;
        }
    }
    else
    {
        return STATUS_REPETITION;
    }
}

void select_promotion(Game*game, PieceType promotion_type)
{
    Position*move = game->position_pool + *game->selected_move_index;
    while (move->pieces[game->selected_piece_index].piece_type != promotion_type)
    {
        game->selected_move_index = &move->next_sibling_index;
        move = game->position_pool + move->next_sibling_index;
    }
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
    size_t rasterization_memory_size = 0;
    for (size_t i = 0; i < ARRAY_COUNT(g_codepoints); ++i)
    {
        FT_Load_Glyph(g_face, FT_Get_Char_Index(g_face, g_codepoints[i]), FT_LOAD_DEFAULT);
        rasterization_memory_size += g_face->glyph->bitmap.rows * g_face->glyph->bitmap.pitch;
    }
    void*rasterization_memory = RESERVE_AND_COMMIT_MEMORY(rasterization_memory_size);
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
    FT_Load_Glyph(g_face, FT_Get_Char_Index(g_face, '|'), FT_LOAD_DEFAULT);
    window->dpi_data->control_border_thickness = g_face->glyph->bitmap.pitch;
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

Color tint_color(Color color, Color tint)
{
    return (Color) { .red = (color.red * tint.red) / 255, .green = (color.green * tint.green) / 255,
        .blue = (color.blue * tint.blue) / 255, .alpha = 255 };
}

void color_square_with_index(Window*window, Grid*grid, Color tint, uint32_t square_size,
    uint8_t square_index)
{
    if (square_index < grid->dimension * grid->dimension)
    {
        uint8_t rank = GRID_RANK(square_index, grid->dimension);
        uint8_t file = GRID_FILE(square_index, grid->dimension);
        Color color;
        if ((rank + file) % 2)
        {
            color = tint_color(g_dark_square_gray, tint);
        }
        else
        {
            color = tint;
        }
        draw_rectangle(window, grid->min_x + square_size * file, grid->min_y + square_size * rank,
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

void draw_grid(Window*window, Control*grid, uint32_t square_size)
{
    uint32_t grid_width = grid->grid.dimension * square_size;
    draw_rectangle_border(window, grid->grid.min_x, grid->grid.min_y, grid_width, grid_width);
    for (size_t rank = 0; rank < grid->grid.dimension; ++rank)
    {
        int32_t square_min_y = grid->grid.min_y + square_size * rank;
        for (size_t file = 0; file < grid->grid.dimension; ++file)
        {
            if ((rank + file) % 2)
            {
                draw_rectangle(window, grid->grid.min_x + square_size * file, square_min_y,
                    square_size, square_size, g_dark_square_gray);
            }
        }
    }
    color_square_with_index(window, &grid->grid, g_hovered_blue, square_size,
        window->hovered_control_id - grid->base_id);
    color_square_with_index(window, &grid->grid, g_clicked_red, square_size,
        window->clicked_control_id - grid->base_id);
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
        case CONTROL_GRID:
        {
            draw_grid(window, control, game->square_size);
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
        case CONTROL_BUTTON:
        {
            ++id;
            break;
        }
        case CONTROL_GRID:
        {
            id += control->grid.dimension * control->grid.dimension;
            break;
        }
        case CONTROL_TIME_INPUT:
        {
            id += control->time_input.digit_count;
        }
        }
    }
}

bool time_control_window_handle_left_mouse_button_up(Game*game, Window*window, int32_t cursor_x,
    int32_t cursor_y)
{
    if (window->clicked_control_id != NULL_CONTROL)
    {
        uint8_t id = id_of_control_cursor_is_over(window, cursor_x, cursor_y);
        if (id == window->clicked_control_id)
        {
            window->clicked_control_id = NULL_CONTROL;
            if (id == window->controls[TIME_CONTROL_WINDOW_START_GAME].base_id)
            {
                return true;
            }
            game->selected_digit_id = id;
            return false;
        }
        window->clicked_control_id = NULL_CONTROL;
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

bool time_control_window_handle_key_down(Game*game, Window*window, uint8_t digit)
{
    if (handle_time_input_key_down(game, window->controls + TIME_CONTROL_WINDOW_TIME, digit))
    {
        return true;
    }
    else if (handle_time_input_key_down(game, window->controls + TIME_CONTROL_WINDOW_INCREMENT,
        digit))
    {
        return true;
    }
    return false;
}

void set_time_control_window_dpi(Game*game, uint16_t dpi)
{
    Window*time_control_window = game->windows + WINDOW_TIME_CONTROL;
    set_dpi_data(game, time_control_window, game->font_size, dpi);
    TimeInput*time = &time_control_window->controls[TIME_CONTROL_WINDOW_TIME].time_input;
    time->min_x = time_control_window->dpi_data->control_border_thickness +
        time_control_window->dpi_data->text_control_height;
    TimeInput*increment = &time_control_window->controls[TIME_CONTROL_WINDOW_INCREMENT].time_input;
    increment->min_x = time->min_x;
    Button*start_game_button =
        &time_control_window->controls[TIME_CONTROL_WINDOW_START_GAME].button;
    start_game_button->min_x = time->min_x;
    start_game_button->width = max32(max32(5 * time_control_window->dpi_data->digit_input_width +
        4 * ROUND26_6_TO_PIXEL(time_control_window->dpi_data->
            rasterizations[':' - g_codepoints[0]].advance) +
        6 * time_control_window->dpi_data->control_border_thickness,
        get_string_length(time_control_window->dpi_data->rasterizations, increment->label)),
        get_string_length(time_control_window->dpi_data->rasterizations, start_game_button->label) +
            2 * time_control_window->dpi_data->text_control_padding);
    time_control_window->width = 2 *(time_control_window->dpi_data->text_control_height +
        time_control_window->dpi_data->control_border_thickness) + start_game_button->width;
    time->min_y = time_control_window->dpi_data->text_line_height +
        time_control_window->dpi_data->text_control_height +
        time_control_window->dpi_data->control_border_thickness;
    increment->min_y = time->min_y +
        time_control_window->dpi_data->text_line_height +
        2 * (time_control_window->dpi_data->control_border_thickness +
            time_control_window->dpi_data->text_control_height);
    start_game_button->min_y = increment->min_y + 
        2 * (time_control_window->dpi_data->control_border_thickness +
            time_control_window->dpi_data->text_control_height);
    time_control_window->height = start_game_button->min_y +
        time_control_window->dpi_data->control_border_thickness + 
        2 * time_control_window->dpi_data->text_control_height;
}

void init_time_control_window(Game*game, uint16_t dpi)
{
    Window*time_control_window = game->windows + WINDOW_TIME_CONTROL;
    time_control_window->control_count = TIME_CONTROL_WINDOW_CONTROL_COUNT;
    Control*control = time_control_window->controls + TIME_CONTROL_WINDOW_TIME;
    control->control_type = CONTROL_TIME_INPUT;
    control->time_input.label = "Time:";
    control->time_input.digits = game->time_control;
    control->time_input.digit_count = ARRAY_COUNT(game->time_control);
    control = time_control_window->controls + TIME_CONTROL_WINDOW_INCREMENT;
    control->control_type = CONTROL_TIME_INPUT;
    control->time_input.label = "Increment:";
    control->time_input.digits = game->increment;
    control->time_input.digit_count = ARRAY_COUNT(game->increment);
    control = time_control_window->controls + TIME_CONTROL_WINDOW_START_GAME;
    control->control_type = CONTROL_BUTTON;
    control->button.label = "Start";
    set_control_ids(time_control_window);
    set_time_control_window_dpi(game, dpi);
    time_control_window->pixels = 0;
    time_control_window->pixel_buffer_capacity = 0;
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
    start_window->pixels = 0;
    start_window->pixel_buffer_capacity = 0;
}

void init_promotion_window(Game*game)
{
    Window*promotion_window = game->windows + WINDOW_PROMOTION;
    promotion_window->control_count = 1;
    promotion_window->controls->control_type = CONTROL_GRID;
    promotion_window->controls->grid.min_x = game->square_size;
    promotion_window->controls->grid.min_y = game->square_size;
    promotion_window->controls->grid.dimension = 2;
    promotion_window->controls->base_id = 0;
    promotion_window->width = 4 * game->square_size;
    promotion_window->height = promotion_window->width;
    promotion_window->pixels = 0;
    promotion_window->pixel_buffer_capacity = 0;
}

void draw_start_window(Game*game, char*text)
{
    Window*start_window = game->windows + WINDOW_START;
    draw_controls(game, start_window);
    draw_string(start_window, text, start_window->dpi_data->text_control_height,
        2 * start_window->dpi_data->text_line_height, g_black);
}

PieceType g_promotion_options[] = { PIECE_QUEEN, PIECE_ROOK, PIECE_BISHOP, PIECE_KNIGHT };

void draw_promotion_window(Game*game)
{
    Window*promotion_window = game->windows + WINDOW_PROMOTION;
    draw_controls(game, promotion_window);
    for (size_t i = 0; i < 4; ++i)
    {
        draw_icon(game->icon_bitmaps[game->active_player_index][g_promotion_options[i]],
            promotion_window, COLUMNS_PER_ICON * ((i % 2) + 1), COLUMNS_PER_ICON * ((i / 2) + 1));
    }
}

#define DRAW_BY_50_CAN_BE_CLAIMED(game) (game->draw_by_50_count > 100)

bool main_window_handle_left_mouse_button_down(Game*game, int32_t cursor_x, int32_t cursor_y)
{
    Window*main_window = game->windows + WINDOW_MAIN;
    main_window->clicked_control_id = main_window->hovered_control_id;
    if (main_window->hovered_control_id == main_window->controls[MAIN_WINDOW_CLAIM_DRAW].base_id &&
        !DRAW_BY_50_CAN_BE_CLAIMED(game))
    {
        main_window->clicked_control_id = NULL_CONTROL;
    }
    return main_window->clicked_control_id != NULL_CONTROL;
}

typedef enum MainWindowLeftButtonUpAction
{
    ACTION_MOVE,
    ACTION_PROMOTE,
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
        uint8_t square_index = id - main_window->controls[MAIN_WINDOW_BOARD].base_id;
        Position*current_position = game->position_pool + game->current_position_index;
        if (game->selected_piece_index == NULL_PIECE)
        {
            uint8_t selected_piece_index = current_position->squares[square_index];
            if (PLAYER_INDEX(selected_piece_index) == game->active_player_index)
            {
                game->selected_move_index = &current_position->first_child_index;
                while (*game->selected_move_index != NULL_POSITION)
                {
                    Position*move = game->position_pool + *game->selected_move_index;
                    if (move->squares[square_index] != selected_piece_index)
                    {
                        game->selected_piece_index = selected_piece_index;
                        return ACTION_REDRAW;
                    }
                    game->selected_move_index = &move->next_sibling_index;
                }
            }
        }
        else
        {
            uint16_t*move_index = game->selected_move_index;
            do
            {
                Position*move = game->position_pool + *move_index;
                if (move->squares[square_index] == game->selected_piece_index)
                {
                    game->selected_move_index = move_index;
                    if (current_position->pieces[game->selected_piece_index].piece_type !=
                        move->pieces[game->selected_piece_index].piece_type)
                    {
                        return ACTION_PROMOTE;
                    }
                    return ACTION_MOVE;
                }
                move_index = &move->next_sibling_index;
            } while (*move_index != NULL_POSITION);
        }
    }
    main_window->clicked_control_id = NULL_CONTROL;
    return ACTION_REDRAW;
}

void set_main_window_dpi(Game*game, uint16_t dpi)
{
    Window*main_window = game->windows + WINDOW_MAIN;
    set_dpi_data(game, main_window, game->font_size, dpi);
    Grid*board = &main_window->controls[MAIN_WINDOW_BOARD].grid;
    board->min_x = game->square_size + main_window->dpi_data->control_border_thickness;
    board->min_y = board->min_x;
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
    control->control_type = CONTROL_GRID;
    control->grid.dimension = RANK_COUNT;
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
}

void draw_main_window(Game*game)
{
    Window*main_window = game->windows + WINDOW_MAIN;
    clear_window(main_window);
    draw_grid(main_window, main_window->controls + MAIN_WINDOW_BOARD, game->square_size);
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
    Control*board = main_window->controls + MAIN_WINDOW_BOARD;
    Position*current_position = game->position_pool + game->current_position_index;
    for (uint8_t player_index = 0; player_index < 2; ++player_index)
    {
        uint8_t player_pieces_index = PLAYER_PIECES_INDEX(player_index);
        uint8_t captured_piece_count = 0;
        for (size_t i = 0; i < 16; ++i)
        {
            uint8_t piece_index = player_pieces_index + i;
            Piece*piece = current_position->pieces + piece_index;
            int32_t bitmap_min_x = board->grid.min_x;
            int32_t bitmap_min_y = board->grid.min_y;
            if (piece->is_captured)
            {
                bitmap_min_x += ((int32_t)game->square_size * captured_piece_count) / 2;
                int8_t forward_delta = FORWARD_DELTA(player_index);
                bitmap_min_y += forward_delta * main_window->dpi_data->control_border_thickness +
                    ((int32_t)game->square_size * (7 + 9 * forward_delta)) / 2;
                captured_piece_count += 1;
            }
            else
            {
                bitmap_min_x += game->square_size * FILE(piece->square_index);
                bitmap_min_y += game->square_size * RANK(piece->square_index);
            }
            if (game->selected_piece_index == piece_index)
            {
                Color*icon_bitmap = game->icon_bitmaps[PLAYER_INDEX(game->selected_piece_index)]
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
                draw_icon(game->icon_bitmaps[player_index][piece->piece_type], main_window,
                    bitmap_min_x, bitmap_min_y);
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
        draw_string(main_window, time_text, save_game_button->button.min_x, board->grid.min_y +
            KING_RANK(player_index) * game->square_size + main_window->dpi_data->text_line_height,
            g_black);
    }
}

void save_multi_byte_value(FILE*file, uint64_t value, size_t byte_count)
{
    for (size_t i = 0; i < byte_count; ++i)
    {
        fwrite((uint8_t*)&value + i, 1, 1, file);
    }
}

uint64_t load_multi_byte_value(FILE*file, size_t byte_count)
{
    uint64_t out = 0;
    for (size_t i = 0; i < byte_count; ++i)
    {
        fread((uint8_t*)&out + i, 1, 1, file);
    }
    return out;
}

void save_piece(FILE*file, Piece piece)
{
    fwrite(&piece.is_captured, 1, 1, file);
    if (!piece.is_captured)
    {
        fwrite(&piece.square_index, 1, 1, file);
        fwrite(&piece.times_moved, 1, 1, file);
    }
}

bool load_piece(FILE*file, Position*position, Piece*piece)
{
    fread(&piece->is_captured, 1, 1, file);
    if (!piece->is_captured)
    {
        fread(&piece->square_index, 1, 1, file);
        if (piece->square_index >= 64)
        {
            return false;
        }
        fread(&piece->times_moved, 1, 1, file);
    }
    return true;
}

void save_game(char*file_path, Game*game)
{
    FILE*file = fopen(file_path, "wb");
    uint64_t time_since_last_move = get_time() - game->last_move_time;
    save_multi_byte_value(file, time_since_last_move, sizeof(time_since_last_move));
    save_multi_byte_value(file, game->time_increment, sizeof(game->time_increment));
    fwrite(&game->active_player_index, 1, 1, file);
    Position*current_position = game->position_pool + game->current_position_index;
    uint8_t index_of_en_passant_capturable_pawn = NULL_PIECE;
    for (size_t player_index = 0; player_index < 2; ++player_index)
    {
        save_multi_byte_value(file, game->times_left_as_of_last_move[player_index],
            sizeof(game->times_left_as_of_last_move[player_index]));
        uint8_t player_pieces_index = PLAYER_PIECES_INDEX(player_index);
        Piece*player_pieces = current_position->pieces + player_pieces_index;
        for (size_t piece_index = 0; piece_index < A_PAWN_INDEX; ++piece_index)
        {
            save_piece(file, player_pieces[piece_index]);
        }
        for (size_t piece_index = A_PAWN_INDEX; piece_index < A_PAWN_INDEX + 8; ++piece_index)
        {
            Piece piece = player_pieces[piece_index];
            uint8_t piece_type = piece.piece_type;
            fwrite(&piece_type, 1, 1, file);
            save_piece(file, piece);
            if (piece.can_be_captured_en_passant)
            {
                index_of_en_passant_capturable_pawn = player_pieces_index + piece_index;
            }
        }
    }
    fwrite(&index_of_en_passant_capturable_pawn, 1, 1, file);
    fwrite(&game->selected_piece_index, 1, 1, file);
    save_multi_byte_value(file, game->draw_by_50_count, sizeof(game->draw_by_50_count));
    save_multi_byte_value(file, game->unique_state_count, sizeof(game->unique_state_count));
    BoardStateNode*external_nodes = EXTERNAL_STATE_NODES(game);
    for (size_t bucket_index = 0; bucket_index < game->state_bucket_count; ++bucket_index)
    {
        BoardStateNode*node = game->state_buckets + bucket_index;
        if (node->count && node->generation == game->state_generation)
        {
            while (true)
            {
                save_multi_byte_value(file, node->state.square_mask,
                    sizeof(node->state.square_mask));
                fwrite(node->state.occupied_square_hashes, 1,
                    ARRAY_COUNT(node->state.occupied_square_hashes), file);
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
    fclose(file);
}

bool load_file(FILE*file, Game*game)
{
    uint64_t time_since_last_move = load_multi_byte_value(file, sizeof(time_since_last_move));
    game->time_increment = load_multi_byte_value(file, sizeof(game->time_increment));
    fread(&game->active_player_index, 1, 1, file);
    if (game->active_player_index > 1)
    {
        return false;
    }
    uint16_t position_index = allocate_position(game);
    Position*current_position = game->position_pool + position_index;
    for (size_t player_index = 0; player_index < 2; ++player_index)
    {
        uint64_t*time_left_as_of_last_move = game->times_left_as_of_last_move + player_index;
        *time_left_as_of_last_move =
            load_multi_byte_value(file, sizeof(*time_left_as_of_last_move));
        game->seconds_left[player_index] = *time_left_as_of_last_move / g_counts_per_second;
        Piece*player_pieces = current_position->pieces + PLAYER_PIECES_INDEX(player_index);
        for (size_t piece_index = 0; piece_index < A_PAWN_INDEX; ++piece_index)
        {
            if (!load_piece(file, current_position, player_pieces + piece_index))
            {
                return false;
            }
        }
        for (size_t piece_index = A_PAWN_INDEX; piece_index < A_PAWN_INDEX + 8; ++piece_index)
        {
            Piece*piece = player_pieces + piece_index;
            uint8_t piece_type;
            fread(&piece_type, 1, 1, file);
            if (piece_type >= PIECE_TYPE_COUNT)
            {
                return false;
            }
            piece->piece_type = piece_type;
            if (!load_piece(file, current_position, piece))
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
    if (game->times_left_as_of_last_move[game->active_player_index] < time_since_last_move)
    {
        return false;
    }
    uint8_t index_of_en_passant_capturable_pawn;
    fread(&index_of_en_passant_capturable_pawn, 1, 1, file);
    if (index_of_en_passant_capturable_pawn > NULL_PIECE)
    {
        return false;
    }
    else if (index_of_en_passant_capturable_pawn < NULL_PIECE)
    {
        current_position->pieces[index_of_en_passant_capturable_pawn].can_be_captured_en_passant =
            true;
    }
    fread(&game->selected_piece_index, 1, 1, file);
    if (game->selected_piece_index > NULL_PIECE)
    {
        return false;
    }
    game->draw_by_50_count = load_multi_byte_value(file, sizeof(game->draw_by_50_count));
    game->unique_state_count = load_multi_byte_value(file, sizeof(game->unique_state_count));
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
        state.square_mask = load_multi_byte_value(file, sizeof(state.square_mask));
        fread(&state.occupied_square_hashes, 1, ARRAY_COUNT(state.occupied_square_hashes), file);
        archive_board_state(game, &state);
    }
    for (size_t square_index = 0; square_index < ARRAY_COUNT(current_position->squares);
        ++square_index)
    {
        current_position->squares[square_index] = NULL_PIECE;
    }
    for (size_t piece_index = 0; piece_index < ARRAY_COUNT(current_position->pieces); ++piece_index)
    {
        current_position->squares[current_position->pieces[piece_index].square_index] = piece_index;
    }
    current_position->next_sibling_index = NULL_POSITION;
    current_position->first_child_index = NULL_POSITION;
    make_position_current(game, position_index);
    game->last_move_time = get_time() - time_since_last_move;
    return true;
}

bool load_game(char*file_path, Game*game)
{
    FILE*file = fopen(file_path, "rb");
    bool out = load_file(file, game);
    fclose(file);
    return out;
}

void free_game(Game*game)
{
    FREE_MEMORY(game->state_buckets);
    free_position(game, game->current_position_index);
}

void init_piece(Position*position, PieceType piece_type, uint8_t piece_index, uint8_t square_index,
    uint8_t player_index)
{
    piece_index += PLAYER_PIECES_INDEX(player_index);
    Piece*piece = position->pieces + piece_index;
    piece->square_index = square_index;
    piece->times_moved = 0;
    piece->can_be_captured_en_passant = false;
    piece->piece_type = piece_type;
    piece->is_captured = false;
    position->squares[square_index] = piece_index;
}

void init_game(Game*game)
{
    game->active_player_index = WHITE_PLAYER_INDEX;
    game->selected_piece_index = NULL_PIECE;
    uint16_t position_index = allocate_position(game);
    Position*position = game->position_pool + position_index;
    position->next_sibling_index = NULL_POSITION;
    position->first_child_index = NULL_POSITION;
    position->reset_draw_by_50_count = false;
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
    make_position_current(game, position_index);
    Window*main_window = game->windows + WINDOW_MAIN;
    main_window->hovered_control_id = NULL_CONTROL;
    main_window->clicked_control_id = NULL_CONTROL;
    game->draw_by_50_count = 0;
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
    game->position_pool = RESERVE_MEMORY(512 * sizeof(Position));
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
}