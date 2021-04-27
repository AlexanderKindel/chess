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
    uint8_t piece_type;
} Piece;

#define RANK_COUNT 8
#define FILE_COUNT 8

typedef struct Position
{
    uint8_t squares[RANK_COUNT * FILE_COUNT];
    Piece pieces[32];
    uint16_t node_index;
    uint8_t en_passant_file;
    uint8_t castling_rights_lost;
    uint8_t active_player_index;
    bool reset_draw_by_50_count;
} Position;

typedef struct PositionTreeNode
{
    int16_t evaluation;
    uint16_t parent_index;
    union
    {
        struct
        {
            uint16_t previous_leaf_index;
            uint16_t next_leaf_index;
        };
        uint16_t first_move_node_index;
    };
    uint16_t next_move_node_index;
    union
    {
        uint16_t position_record_index;
        uint16_t previous_transposition_index;
    };
    uint16_t next_transposion_index;
    bool is_leaf;
    bool reset_draw_by_50_count : 1;
    bool moves_have_been_found : 1;
    bool is_canonical : 1;
    bool evaluation_has_been_propagated_to_parents : 1;
} PositionTreeNode;

typedef struct CompressedPosition
{
    uint8_t square_mask[8];
    uint8_t piece_hashes[16];
    uint8_t en_passant_file;
    uint8_t castling_rights_lost : 4;
    uint8_t active_player_index : 1;
} CompressedPosition;

typedef struct TreePositionRecord
{
    CompressedPosition position;
    uint16_t index_of_next_record;
    uint16_t position_node_index;
} TreePositionRecord;

typedef struct PlayedPositionRecord
{
    CompressedPosition position;
    uint16_t index_of_next_record;
    uint8_t count;
    uint8_t generation;
} PlayedPositionRecord;

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

#define NULL_CONTROL UINT8_MAX
#define NULL_PIECE 32
#define NULL_SQUARE 64
#define NULL_POSITION_TREE_NODE UINT16_MAX
#define NULL_PLAYED_POSITION_RECORD UINT16_MAX
#define PLAYER_INDEX_WHITE 0
#define PLAYER_INDEX_BLACK 1

PositionTreeNode g_position_tree_node_pool[NULL_POSITION_TREE_NODE];
uint16_t*g_selected_move_node_index;
uint16_t g_first_leaf_index;
uint16_t g_index_of_first_free_position_tree_node;
uint16_t g_next_leaf_to_evaluate_index;
uint16_t g_position_tree_node_pool_cursor;

uint16_t g_tree_position_buckets[NULL_POSITION_TREE_NODE];
TreePositionRecord g_tree_position_records[NULL_POSITION_TREE_NODE];
uint16_t g_index_of_first_free_tree_position_record;
uint16_t g_tree_position_record_cursor;

PlayedPositionRecord*g_played_position_records;
uint16_t g_played_position_external_record_count;
uint16_t g_played_position_bucket_count;
uint16_t g_unique_played_position_count;
uint8_t g_played_position_generation;

Control g_dialog_controls[sizeof(DialogControlCount)];
Control g_main_window_controls[MAIN_WINDOW_CONTROL_COUNT];
Window g_windows[WINDOW_COUNT];
DPIData g_dpi_datas[WINDOW_COUNT];
uint8_t g_captured_piece_counts[2][PIECE_TYPE_COUNT];
Position g_current_position;
uint64_t g_times_left_as_of_last_move[2];
uint64_t g_last_move_time;
uint64_t g_time_increment;
uint32_t g_font_size;
uint16_t g_seconds_left[2];
uint16_t g_draw_by_50_count;
DigitInput g_time_control[5];
DigitInput g_increment[3];
uint8_t g_selected_piece_index;
uint8_t g_selected_digit_id;
uint8_t g_engine_player_index;
bool g_run_engine;
bool g_is_promoting;

#ifdef DEBUG
#define ASSERT(condition) if (!(condition)) *((int*)0) = 0

PositionTreeNode*get_position_tree_node(uint16_t node_index)
{
    ASSERT(node_index != NULL_POSITION_TREE_NODE);
    return g_position_tree_node_pool + node_index;
}

uint16_t get_previous_leaf_index(PositionTreeNode*node)
{
    ASSERT(node->is_leaf);
    return node->previous_leaf_index;
}

uint16_t get_next_leaf_index(PositionTreeNode*node)
{
    ASSERT(node->is_leaf);
    return node->next_leaf_index;
}

uint16_t get_first_move_node_index(PositionTreeNode*node)
{
    ASSERT(!node->is_leaf);
    return node->first_move_node_index;
}

uint16_t get_record_index(PositionTreeNode*node)
{
    ASSERT(node->is_canonical);
    return node->position_record_index;
}

uint16_t get_previous_transposition_index(PositionTreeNode*node)
{
    ASSERT(!node->is_canonical);
    return node->previous_transposition_index;
}

void set_previous_leaf_index(PositionTreeNode*node, uint16_t value)
{
    ASSERT(node->is_leaf);
    node->previous_leaf_index = value;
}

void set_next_leaf_index(PositionTreeNode*node, uint16_t value)
{
    ASSERT(node->is_leaf);
    node->next_leaf_index = value;
}

void set_record_index(PositionTreeNode*node, uint16_t value)
{
    ASSERT(node->is_canonical);
    node->position_record_index = value;
}

void set_first_move_node_index(PositionTreeNode*node, uint16_t value)
{
    ASSERT(!node->is_leaf);
    node->first_move_node_index = value;
}

void set_previous_transposition_index(PositionTreeNode*node, uint16_t value)
{
    ASSERT(!node->is_canonical);
    node->previous_transposition_index = value;
}

#define GET_POSITION_TREE_NODE(node_index) get_position_tree_node(node_index)
#define GET_PREVIOUS_LEAF_INDEX(node) get_previous_leaf_index(node)
#define GET_NEXT_LEAF_INDEX(position_tree_node) get_next_leaf_index(position_tree_node)
#define GET_FIRST_MOVE_NODE_INDEX(node) get_first_move_node_index(node)
#define GET_RECORD_INDEX(node) get_record_index(node)
#define GET_PREVIOUS_TRANSPOSITION_INDEX(node) get_previous_transposition_index(node)
#define SET_PREVIOUS_LEAF_INDEX(position_tree_node, value) set_previous_leaf_index(position_tree_node, value)
#define SET_NEXT_LEAF_INDEX(position_tree_node, value) set_next_leaf_index(position_tree_node, value)
#define SET_FIRST_MOVE_NODE_INDEX(node, value) set_first_move_node_index(node, value)
#define SET_RECORD_INDEX(node, value) set_record_index(node, value)
#define SET_PREVIOUS_TRANSPOSITION_INDEX(node, value) set_previous_transposition_index(node, value)
#else
#define ASSERT(condition)
#define GET_POSITION_TREE_NODE(node_index) (g_position_tree_node_pool + (node_index))
#define GET_PREVIOUS_LEAF_INDEX(position_tree_node) (position_tree_node)->previous_leaf_index
#define GET_NEXT_LEAF_INDEX(position_tree_node) (position_tree_node)->next_leaf_index
#define GET_FIRST_MOVE_NODE_INDEX(node) (node)->first_move_node_index
#define GET_RECORD_INDEX(node) (node)->position_record_index
#define GET_PREVIOUS_TRANSPOSITION_INDEX(node) (node)->previous_transposition_index
#define SET_PREVIOUS_LEAF_INDEX(position_tree_node, value) ((position_tree_node)->previous_leaf_index = (value))
#define SET_NEXT_LEAF_INDEX(position_tree_node, value) ((position_tree_node)->next_leaf_index = (value))
#define SET_FIRST_MOVE_NODE_INDEX(node, value) ((node)->first_move_node_index = (value))
#define SET_RECORD_INDEX(node, value) ((node)->position_record_index = (value))
#define SET_PREVIOUS_TRANSPOSITION_INDEX(node, value) ((node)->previous_transposition_index = (value))
#endif

#define PLAYER_INDEX(piece_index) ((piece_index) >> 4)
#define KING_RANK(player_index) (7 * !(player_index))
#define FORWARD_DELTA(player_index) (((player_index) << 1) - 1)
#define RANK(square_index) ((square_index) >> 3)
#define FILE(square_index) ((square_index) & 0b111)
#define SCREEN_SQUARE_INDEX(square_index) (g_engine_player_index == PLAYER_INDEX_WHITE ? (63 - (square_index)) : (square_index))
#define SQUARE_INDEX(rank, file) (FILE_COUNT * (rank) + (file))
#define EXTERNAL_PLAYED_POSITION_RECORDS() (g_played_position_records + g_played_position_bucket_count)
#define PLAYER_PIECES_INDEX(player_index) ((player_index) << 4)
#define EN_PASSANT_RANK(player_index, forward_delta) KING_RANK(player_index) + ((forward_delta) << 2)
#define PLAYER_WIN(player_index) (((int16_t[]){INT16_MAX, -INT16_MAX})[player_index])

uint64_t g_counts_per_second;
uint64_t g_max_time;

void init_played_position_archive(uint8_t bucket_count)
{
    g_played_position_generation = 0;
    g_played_position_bucket_count = bucket_count;
    g_played_position_records = ALLOCATE(2 * sizeof(PlayedPositionRecord) * bucket_count);
    for (size_t i = 0; i < bucket_count; ++i)
    {
        g_played_position_records[i] =
            (PlayedPositionRecord) { (CompressedPosition) { 0 }, NULL_POSITION_TREE_NODE, 0, 0 };
    }
    g_unique_played_position_count = 0;
    g_played_position_external_record_count = 0;
}

bool archive_played_position(CompressedPosition*position);

void increment_unique_position_count(void)
{
    ++g_unique_played_position_count;
    if (g_unique_played_position_count == g_played_position_bucket_count)
    {
        PlayedPositionRecord*old_buckets = g_played_position_records;
        PlayedPositionRecord*old_external_records = EXTERNAL_PLAYED_POSITION_RECORDS();
        init_played_position_archive(g_played_position_bucket_count << 1);
        for (size_t i = 0; i < g_played_position_bucket_count; ++i)
        {
            PlayedPositionRecord*record = old_buckets + i;
            if (record->count && record->generation == g_played_position_generation)
            { 
                while (true)
                {
                    archive_played_position(&record->position);
                    if (record->index_of_next_record < NULL_PLAYED_POSITION_RECORD)
                    {
                        record = old_external_records + record->index_of_next_record;
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

uint32_t FNVHashCompressedPosition(CompressedPosition*position, uint16_t bit_count)
{
    uint32_t hash = 2166136261;
    for (size_t i = 0; i < sizeof(CompressedPosition); ++i)
    {
        hash ^= ((uint8_t*)position)[i];
        hash *= 16777619;
    }
    return ((hash >> bit_count) ^ hash) & ((1 << bit_count) - 1);
}

bool archive_played_position(CompressedPosition*position)
{
    uint32_t bucket_count_bit_count;
    BIT_SCAN_REVERSE(&bucket_count_bit_count, g_played_position_bucket_count);
    uint32_t bucket_index = FNVHashCompressedPosition(position, bucket_count_bit_count);
    PlayedPositionRecord*record = g_played_position_records + bucket_index;
    if (record->count && record->generation == g_played_position_generation)
    {
        while (memcmp(position, &record->position, sizeof(CompressedPosition)))
        {
            if (record->index_of_next_record == NULL_PLAYED_POSITION_RECORD)
            {
                record->index_of_next_record = g_played_position_external_record_count;
                EXTERNAL_PLAYED_POSITION_RECORDS()[g_played_position_external_record_count] =
                    (PlayedPositionRecord) { *position, NULL_PLAYED_POSITION_RECORD, 1,
                        g_played_position_generation };
                ++g_played_position_external_record_count;
                increment_unique_position_count();
                return true;
            }
            else
            {
                record = EXTERNAL_PLAYED_POSITION_RECORDS() + record->index_of_next_record;
            }
        }
        ++record->count;
        if (record->count == 3)
        {
            return false;
        }
    }
    else
    {
        record->position = *position;
        record->index_of_next_record = NULL_PLAYED_POSITION_RECORD;
        record->count = 1;
        record->generation = g_played_position_generation;
        increment_unique_position_count();
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
        position->pieces[PLAYER_PIECES_INDEX(player_index)].square_index);
}

void capture_piece(Position*position, uint8_t piece_index)
{
    position->pieces[piece_index].square_index = NULL_SQUARE;
    position->reset_draw_by_50_count = true;
}

void move_piece(Position*position, uint8_t piece_index, uint8_t destination_square_index)
{
    position->squares[destination_square_index] = piece_index;
    Piece*piece = position->pieces + piece_index;
    position->squares[piece->square_index] = NULL_PIECE;
    piece->square_index = destination_square_index;
}

uint16_t get_position_record_bucket_index(CompressedPosition*position)
{
    uint16_t out = FNVHashCompressedPosition(position, 16);
    if (out == NULL_POSITION_TREE_NODE)
    {
        return 0;
    }
    return out;
}

CompressedPosition*get_compressed_position(PositionTreeNode*node)
{
    while (!node->is_canonical)
    {
        node = GET_POSITION_TREE_NODE(GET_PREVIOUS_TRANSPOSITION_INDEX(node));
    }
    return &g_tree_position_records[GET_RECORD_INDEX(node)].position;
}

jmp_buf out_of_memory_jump_buffer;

uint16_t allocate_position_tree_node(void)
{
    uint16_t new_node_index;
    PositionTreeNode*new_node;
    if (g_index_of_first_free_position_tree_node == NULL_POSITION_TREE_NODE)
    {
        if (g_position_tree_node_pool_cursor == NULL_POSITION_TREE_NODE)
        {
            g_run_engine = false;
            longjmp(out_of_memory_jump_buffer, 1);
        }
        new_node_index = g_position_tree_node_pool_cursor;
        new_node = GET_POSITION_TREE_NODE(new_node_index);
        ++g_position_tree_node_pool_cursor;
        new_node->is_leaf = true;
    }
    else
    {
        new_node_index = g_index_of_first_free_position_tree_node;
        new_node = GET_POSITION_TREE_NODE(new_node_index);
        if (new_node->is_canonical)
        {
            if (new_node->next_transposion_index == NULL_POSITION_TREE_NODE)
            {
                TreePositionRecord*record = g_tree_position_records + GET_RECORD_INDEX(new_node);
                uint16_t*bucket =
                    g_tree_position_buckets + get_position_record_bucket_index(&record->position);
                while (*bucket != GET_RECORD_INDEX(new_node))
                {
                    bucket = &g_tree_position_records[*bucket].index_of_next_record;
                }
                *bucket = record->index_of_next_record;
                record->index_of_next_record = g_index_of_first_free_tree_position_record;
                g_index_of_first_free_tree_position_record = GET_RECORD_INDEX(new_node);
            }
            else
            {
                PositionTreeNode*transposition =
                    GET_POSITION_TREE_NODE(new_node->next_transposion_index);
                transposition->is_canonical = true;
                SET_RECORD_INDEX(transposition, GET_RECORD_INDEX(new_node));
            }
        }
        else
        {
            GET_POSITION_TREE_NODE(GET_PREVIOUS_TRANSPOSITION_INDEX(new_node))->
                next_transposion_index = new_node->next_transposion_index;
            if (new_node->next_transposion_index != NULL_POSITION_TREE_NODE)
            {
                SET_PREVIOUS_TRANSPOSITION_INDEX(
                    GET_POSITION_TREE_NODE(new_node->next_transposion_index),
                    GET_PREVIOUS_TRANSPOSITION_INDEX(new_node));
            }
        }
        new_node->is_leaf = true;
        if (new_node->parent_index == NULL_POSITION_TREE_NODE)
        {
            g_index_of_first_free_position_tree_node = GET_NEXT_LEAF_INDEX(new_node);
        }
        else if (new_node->next_move_node_index == NULL_POSITION_TREE_NODE)
        {
            GET_POSITION_TREE_NODE(new_node->parent_index)->next_leaf_index =
                GET_NEXT_LEAF_INDEX(new_node);
            g_index_of_first_free_position_tree_node = new_node->parent_index;
        }
        else
        {
            g_index_of_first_free_position_tree_node = GET_NEXT_LEAF_INDEX(new_node);
        }
    }
    new_node->moves_have_been_found = false;
    return new_node_index;
}

void compress_position(CompressedPosition*out, Position*position)
{
    memset(out->square_mask, 0, sizeof(out->square_mask));
    memset(out->piece_hashes, 0, sizeof(out->piece_hashes));
    size_t piece_hash_index = 0;
    uint8_t shift = 0;
    for (size_t square_index = 0; square_index < 64; ++square_index)
    {
        uint8_t square = position->squares[square_index];
        if (square != NULL_PIECE)
        {
            out->piece_hashes[piece_hash_index] |= (position->pieces[square].piece_type |
                (PLAYER_INDEX(square) << 3)) << shift;
            piece_hash_index += shift >> 2;
            shift ^= 4;
            out->square_mask[square_index >> 3] |= 1 << (square_index & 0b111);
        }
    }
    out->en_passant_file = position->en_passant_file;
    out->castling_rights_lost = position->castling_rights_lost;
    out->active_player_index = position->active_player_index;
}

void add_tree_position_record(CompressedPosition*compressed_position, uint16_t node_index)
{
    PositionTreeNode*node = GET_POSITION_TREE_NODE(node_index);
    TreePositionRecord*record;
    node->is_canonical = true;
    if (g_tree_position_record_cursor == NULL_POSITION_TREE_NODE)
    {
        ASSERT(g_index_of_first_free_tree_position_record != NULL_POSITION_TREE_NODE);
        SET_RECORD_INDEX(node, g_index_of_first_free_tree_position_record);
        record = g_tree_position_records + GET_RECORD_INDEX(node);
        g_index_of_first_free_tree_position_record = record->index_of_next_record;
    }
    else
    {
        SET_RECORD_INDEX(node, g_tree_position_record_cursor);
        record = g_tree_position_records + GET_RECORD_INDEX(node);
        ++g_tree_position_record_cursor;
    }
    record->position = *compressed_position;
    record->index_of_next_record = NULL_POSITION_TREE_NODE;
    record->position_node_index = node_index;
    node->next_transposion_index = NULL_POSITION_TREE_NODE;
}

void compress_position_to_node(Position*position, PositionTreeNode*out)
{
    out->reset_draw_by_50_count = position->reset_draw_by_50_count;
    CompressedPosition compressed_position;
    compress_position(&compressed_position, position);
    uint16_t*bucket =
        g_tree_position_buckets + get_position_record_bucket_index(&compressed_position);
    TreePositionRecord*record;
    if (*bucket == NULL_POSITION_TREE_NODE)
    {
        add_tree_position_record(&compressed_position, position->node_index);
        *bucket = GET_RECORD_INDEX(out);
    }
    else
    {
        record = g_tree_position_records + *bucket;
        while (memcmp(&compressed_position, &record->position, sizeof(compressed_position)))
        {
            if (record->index_of_next_record == NULL_POSITION_TREE_NODE)
            {
                add_tree_position_record(&compressed_position, position->node_index);
                record->index_of_next_record = GET_RECORD_INDEX(out);
                return;
            }
            else
            {
                record = g_tree_position_records + record->index_of_next_record;
            }
        }
        out->is_canonical = false;
        SET_PREVIOUS_TRANSPOSITION_INDEX(out, record->position_node_index);
        PositionTreeNode*canonical_node = GET_POSITION_TREE_NODE(record->position_node_index);
        out->next_transposion_index = canonical_node->next_transposion_index;
        if (out->next_transposion_index != NULL_POSITION_TREE_NODE)
        {
            SET_PREVIOUS_TRANSPOSITION_INDEX(GET_POSITION_TREE_NODE(out->next_transposion_index),
                position->node_index);
        }
        canonical_node->next_transposion_index = position->node_index;
        out->evaluation = canonical_node->evaluation;
        out->evaluation_has_been_propagated_to_parents = false;
    }
}

void decompress_position(Position*out, uint16_t position_tree_node_index)
{
    CompressedPosition*position =
        get_compressed_position(GET_POSITION_TREE_NODE(position_tree_node_index));
    out->node_index = position_tree_node_index;
    uint8_t player_next_piece_index[] = { 1, 17 };
    size_t piece_hash_index = 0;
    uint8_t shift = 0;
    for (size_t square_index = 0; square_index < 64; ++square_index)
    {
        if (position->square_mask[square_index >> 3] & (1 << (square_index & 0b111)))
        {
            uint8_t piece_hash = (position->piece_hashes[piece_hash_index] >> shift) & 0b1111;
            uint8_t player_index = piece_hash >> 3;
            PieceType piece_type = piece_hash & 0b111;
            uint8_t piece_index;
            if (piece_type == PIECE_KING)
            {
                piece_index = PLAYER_PIECES_INDEX(player_index);
            }
            else
            {
                piece_index = player_next_piece_index[player_index];
                ++player_next_piece_index[player_index];
            }
            out->squares[square_index] = piece_index;
            Piece*piece = out->pieces + piece_index;
            piece->square_index = square_index;
            piece->piece_type = piece_type;
            piece_hash_index += shift >> 2;
            shift ^= 4;
        }
        else
        {
            out->squares[square_index] = NULL_PIECE;
        }
    }
    for (size_t piece_index = player_next_piece_index[0]; piece_index < 16; ++piece_index)
    {
        out->pieces[piece_index].square_index = NULL_SQUARE;
    }
    for (size_t piece_index = player_next_piece_index[1]; piece_index < 32; ++piece_index)
    {
        out->pieces[piece_index].square_index = NULL_SQUARE;
    }
    out->en_passant_file = position->en_passant_file;
    out->castling_rights_lost = position->castling_rights_lost;
    out->active_player_index = position->active_player_index;
}

void add_move(Position*position, Position*move)
{
    move->node_index = allocate_position_tree_node();
    PositionTreeNode*move_node = GET_POSITION_TREE_NODE(move->node_index);
    move_node->parent_index = position->node_index;
    PositionTreeNode*node = GET_POSITION_TREE_NODE(position->node_index);
    if (node->is_leaf)
    {
        SET_PREVIOUS_LEAF_INDEX(move_node, GET_PREVIOUS_LEAF_INDEX(node));
        SET_NEXT_LEAF_INDEX(move_node, GET_NEXT_LEAF_INDEX(node));
        if (GET_NEXT_LEAF_INDEX(move_node) != NULL_POSITION_TREE_NODE)
        {
            SET_PREVIOUS_LEAF_INDEX(GET_POSITION_TREE_NODE(GET_NEXT_LEAF_INDEX(move_node)),
                move->node_index);
        }
        node->is_leaf = false;
        move_node->next_move_node_index = NULL_POSITION_TREE_NODE;
    }
    else
    {
        PositionTreeNode*next_move_node = GET_POSITION_TREE_NODE(GET_FIRST_MOVE_NODE_INDEX(node));
        SET_PREVIOUS_LEAF_INDEX(move_node, GET_PREVIOUS_LEAF_INDEX(next_move_node));
        SET_NEXT_LEAF_INDEX(move_node, GET_FIRST_MOVE_NODE_INDEX(node));
        SET_PREVIOUS_LEAF_INDEX(next_move_node, move->node_index);
        move_node->next_move_node_index = GET_FIRST_MOVE_NODE_INDEX(node);
    }
    if (GET_PREVIOUS_LEAF_INDEX(move_node) == NULL_POSITION_TREE_NODE)
    {
        g_first_leaf_index = move->node_index;
    }
    else
    {
        SET_NEXT_LEAF_INDEX(GET_POSITION_TREE_NODE(GET_PREVIOUS_LEAF_INDEX(move_node)),
            move->node_index);
    }
    SET_FIRST_MOVE_NODE_INDEX(node, move->node_index);
    move->active_player_index = !position->active_player_index;
    compress_position_to_node(move, move_node);
}

void free_position_tree_node(uint16_t node_index)
{
    PositionTreeNode*node = GET_POSITION_TREE_NODE(node_index);
    node->parent_index = NULL_POSITION_TREE_NODE;
    node->next_leaf_index = g_index_of_first_free_position_tree_node;
    g_index_of_first_free_position_tree_node = node_index;
}

bool add_move_if_not_king_hang(Position*position, Position*move)
{
    if (player_is_checked(move, position->active_player_index))
    {
        return false;
    }
    else
    {
        add_move(position, move);
        return true;
    }
}

void copy_position(Position*position, Position*out)
{
    memcpy(&out->squares, &position->squares, sizeof(position->squares));
    memcpy(&out->pieces, &position->pieces, sizeof(position->pieces));
    out->en_passant_file = FILE_COUNT;
    out->castling_rights_lost = position->castling_rights_lost;
    out->reset_draw_by_50_count = false;
}

typedef enum MoveAttemptStatus
{
    MOVE_ATTEMPT_FAILURE,
    MOVE_ATTEMPT_MOVE,
    MOVE_ATTEMPT_CAPTURE
} MoveAttemptStatus;

MoveAttemptStatus move_piece_if_legal(Position*position, Position*move, uint8_t piece_index,
    uint8_t destination_square_index)
{
    uint8_t destination_square = position->squares[destination_square_index];
    MoveAttemptStatus out;
    if (destination_square == NULL_PIECE)
    {
        copy_position(position, move);
        out = MOVE_ATTEMPT_MOVE;
    }
    else if (PLAYER_INDEX(destination_square) != position->active_player_index)
    {
        copy_position(position, move);
        capture_piece(move, destination_square);
        out = MOVE_ATTEMPT_CAPTURE;
    }
    else
    {
        return MOVE_ATTEMPT_FAILURE;
    }
    move_piece(move, piece_index, destination_square_index);
    if (player_is_checked(move, position->active_player_index))
    {
        return MOVE_ATTEMPT_FAILURE;
    }
    return out;
}

MoveAttemptStatus move_piece_and_add_as_move(Position*position, uint8_t piece_index,
    uint8_t destination_square_index)
{
    Position move;
    MoveAttemptStatus out =
        move_piece_if_legal(position, &move, piece_index, destination_square_index);
    if (out)
    {
        add_move(position, &move);
    }
    return out;
}

void add_pawn_move_with_promotions(Position*position, Position*move, uint8_t piece_index)
{
    if (!player_is_checked(move, position->active_player_index))
    {
        if (RANK(move->pieces[piece_index].square_index) == KING_RANK(!PLAYER_INDEX(piece_index)))
        {
            move->reset_draw_by_50_count = true;
            for (size_t i = 0; i < ARRAY_COUNT(g_promotion_options); ++i)
            {
                move->pieces[piece_index].piece_type = g_promotion_options[i];
                add_move(position, move);
            }
        }
        else
        {
            add_move(position, move);
        }
    }
}

void add_diagonal_pawn_move(Position*position, uint8_t piece_index, uint8_t file)
{
    uint8_t rank = RANK(position->pieces[piece_index].square_index);
    int8_t forward_delta = FORWARD_DELTA(position->active_player_index);
    uint8_t destination_square_index = SQUARE_INDEX(rank + forward_delta, file);
    if (file == position->en_passant_file && rank ==
        EN_PASSANT_RANK(position->active_player_index, forward_delta))
    {
        Position move;
        copy_position(position, &move);
        uint8_t en_passant_square_index = SQUARE_INDEX(rank, file);
        capture_piece(&move, position->squares[en_passant_square_index]);
        move_piece(&move, piece_index, destination_square_index);
        move.squares[en_passant_square_index] = NULL_PIECE;
        add_move_if_not_king_hang(position, &move);
    }
    else
    {
        uint8_t destination_square = position->squares[destination_square_index];
        if (PLAYER_INDEX(destination_square) == !position->active_player_index)
        {
            Position move;
            copy_position(position, &move);
            capture_piece(&move, destination_square);
            move_piece(&move, piece_index, destination_square_index);
            add_pawn_move_with_promotions(position, &move, piece_index);
        }
    }
}

void add_half_diagonal_moves(Position*position, uint8_t piece_index, int8_t rank_delta,
    int8_t file_delta)
{
    uint8_t destination_square_index = position->pieces[piece_index].square_index;
    uint8_t destination_rank = RANK(destination_square_index);
    uint8_t destination_file = FILE(destination_square_index);
    do
    {
        destination_rank += rank_delta;
        destination_file += file_delta;
    } while (destination_rank < 8 && destination_file < 8 && move_piece_and_add_as_move(position,
        piece_index, SQUARE_INDEX(destination_rank, destination_file)) == MOVE_ATTEMPT_MOVE);
}

void add_diagonal_moves(Position*position, uint8_t piece_index)
{
    add_half_diagonal_moves(position, piece_index, 1, 1);
    add_half_diagonal_moves(position, piece_index, 1, -1);
    add_half_diagonal_moves(position, piece_index, -1, 1);
    add_half_diagonal_moves(position, piece_index, -1, -1);
}

void add_half_horizontal_moves(Position*position, uint8_t piece_index, int8_t delta,
    uint8_t castling_right_lost)
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
        Position move;
        MoveAttemptStatus status = move_piece_if_legal(position, &move, piece_index,
            SQUARE_INDEX(destination_rank, destination_file));
        if (!(move.castling_rights_lost & castling_right_lost))
        {
            move.castling_rights_lost |= castling_right_lost;
            move.reset_draw_by_50_count |= castling_right_lost;
        }
        switch (status)
        {
        case MOVE_ATTEMPT_MOVE:
        {
            add_move(position, &move);
            break;
        }
        case MOVE_ATTEMPT_CAPTURE:
        {
            add_move(position, &move);
        }
        case MOVE_ATTEMPT_FAILURE:
        {
            return;
        }
        }
    }
}

void add_half_vertical_moves(Position*position, uint8_t piece_index, int8_t delta,
    uint8_t castling_right_lost)
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
        Position move;
        MoveAttemptStatus status = move_piece_if_legal(position, &move, piece_index,
            SQUARE_INDEX(destination_rank, destination_file));
        if (!(move.castling_rights_lost & castling_right_lost))
        {
            move.castling_rights_lost |= castling_right_lost;
            move.reset_draw_by_50_count |= castling_right_lost;
        }
        switch (status)
        {
        case MOVE_ATTEMPT_MOVE:
        {
            add_move(position, &move);
            break;
        }
        case MOVE_ATTEMPT_CAPTURE:
        {
            add_move(position, &move);
        }
        case MOVE_ATTEMPT_FAILURE:
        {
            return;
        }
        }
    }
}

void add_moves_along_axes(Position*position, uint8_t piece_index, uint8_t castling_right_lost)
{
    add_half_horizontal_moves(position, piece_index, 1, castling_right_lost);
    add_half_horizontal_moves(position, piece_index, -1, castling_right_lost);
    add_half_vertical_moves(position, piece_index, 1, castling_right_lost);
    add_half_vertical_moves(position, piece_index, -1, castling_right_lost);
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

void propagate_evaluation_to_transpositions(PositionTreeNode*node)
{
    ASSERT(node->is_canonical);
    int16_t evaluation = node->evaluation;
    while (node->next_transposion_index != NULL_POSITION_TREE_NODE)
    {
        node = GET_POSITION_TREE_NODE(node->next_transposion_index);
        node->evaluation = evaluation;
        node->evaluation_has_been_propagated_to_parents = false;
    }
}

void propagate_evaluation_to_parents(PositionTreeNode*node, uint8_t player_index)
{
    node->evaluation_has_been_propagated_to_parents = true;
    while (node->parent_index != NULL_POSITION_TREE_NODE)
    {
        node = GET_POSITION_TREE_NODE(node->parent_index);
        int16_t new_evaluation = PLAYER_WIN(player_index);
        player_index = !player_index;
        uint16_t move_node_index = GET_FIRST_MOVE_NODE_INDEX(node);
        while (move_node_index != NULL_POSITION_TREE_NODE)
        {
            PositionTreeNode*move_node = GET_POSITION_TREE_NODE(move_node_index);
            if (player_index == PLAYER_INDEX_WHITE)
            {
                if (move_node->evaluation > new_evaluation)
                {
                    new_evaluation = move_node->evaluation;
                }
            }
            else if (move_node->evaluation < new_evaluation)
            {
                new_evaluation = move_node->evaluation;
            }
            move_node_index = move_node->next_move_node_index;
        }
        if (new_evaluation == node->evaluation)
        {
            return;
        }
        node->evaluation = new_evaluation;
        propagate_evaluation_to_transpositions(node);
    }
}

void get_moves(Position*position)
{
    PositionTreeNode*node = GET_POSITION_TREE_NODE(position->node_index);
    if (setjmp(out_of_memory_jump_buffer))
    {
        if (!node->is_leaf)
        {
            uint16_t move_node_index = GET_FIRST_MOVE_NODE_INDEX(node);
            PositionTreeNode*move_node = GET_POSITION_TREE_NODE(move_node_index);
            node->is_leaf = true;
            SET_PREVIOUS_LEAF_INDEX(node, GET_PREVIOUS_LEAF_INDEX(move_node));
            while (true)
            {
                if (move_node->next_move_node_index == NULL_POSITION_TREE_NODE)
                {
                    SET_NEXT_LEAF_INDEX(node, GET_NEXT_LEAF_INDEX(move_node));
                    free_position_tree_node(move_node_index);
                    break;
                }
                free_position_tree_node(move_node_index);
                move_node_index = move_node->next_move_node_index;
                move_node = GET_POSITION_TREE_NODE(move_node_index);
            }
            if (GET_PREVIOUS_LEAF_INDEX(node) != NULL_POSITION_TREE_NODE)
            {
                SET_NEXT_LEAF_INDEX(GET_POSITION_TREE_NODE(GET_PREVIOUS_LEAF_INDEX(node)),
                    position->node_index);
            }
            if (GET_NEXT_LEAF_INDEX(node) != NULL_POSITION_TREE_NODE)
            {
                SET_PREVIOUS_LEAF_INDEX(GET_POSITION_TREE_NODE(GET_NEXT_LEAF_INDEX(node)),
                    position->node_index);
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
            add_diagonal_moves(position, piece_index);
            break;
        }
        case PIECE_KING:
        {
            uint8_t castling_rights_lost = 0b11 << (position->active_player_index << 1);
            bool reset_draw_by_50_count = castling_rights_lost != position->castling_rights_lost;
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
                    Position move;
                    if (move_piece_if_legal(position, &move, piece_index,
                        SQUARE_INDEX(move_ranks[rank_index], move_files[file_index])))
                    {
                        move.castling_rights_lost |= castling_rights_lost;
                        move.reset_draw_by_50_count |= reset_draw_by_50_count;
                        add_move(position, &move);
                    }
                }
            }
            if (!player_attacks_king_square(position, !position->active_player_index,
                piece.square_index))
            {
                if (!(position->castling_rights_lost &
                        (1 << (position->active_player_index << 1))) &&
                    position->squares[piece.square_index - 1] == NULL_PIECE &&
                    position->squares[piece.square_index - 2] == NULL_PIECE &&
                    position->squares[piece.square_index - 3] == NULL_PIECE &&
                    !player_attacks_king_square(position, !position->active_player_index,
                        piece.square_index - 1) &&
                    !player_attacks_king_square(position, !position->active_player_index,
                        piece.square_index - 2))
                {
                    Position move;
                    copy_position(position, &move);
                    move_piece(&move, piece_index, piece.square_index - 2);
                    move_piece(&move, position->squares[piece.square_index - 4],
                        piece.square_index - 1);
                    move.castling_rights_lost |= castling_rights_lost;
                    move.reset_draw_by_50_count = true;
                    add_move(position, &move);
                }
                if (!(position->castling_rights_lost &
                        (0b10 << (position->active_player_index << 1))) &&
                    position->squares[piece.square_index + 1] == NULL_PIECE &&
                    position->squares[piece.square_index + 2] == NULL_PIECE &&
                    !player_attacks_king_square(position, !position->active_player_index,
                        piece.square_index + 1) &&
                    !player_attacks_king_square(position, !position->active_player_index,
                        piece.square_index + 2))
                {
                    Position move;
                    copy_position(position, &move);
                    move_piece(&move, piece_index, piece.square_index + 2);
                    move_piece(&move, position->squares[piece.square_index + 3],
                        piece.square_index + 1);
                    move.castling_rights_lost |= castling_rights_lost;
                    move.reset_draw_by_50_count = true;
                    add_move(position, &move);
                }
            }
            break;
        }
        case PIECE_KNIGHT:
        {
            uint8_t rank = RANK(piece.square_index);
            uint8_t file = FILE(piece.square_index);
            if (file > 0)
            {
                if (rank > 1)
                {
                    move_piece_and_add_as_move(position, piece_index,
                        SQUARE_INDEX(rank - 2, file - 1));
                }
                if (rank < 6)
                {
                    move_piece_and_add_as_move(position, piece_index,
                        SQUARE_INDEX(rank + 2, file - 1));
                }
                if (file > 1)
                {
                    if (rank > 0)
                    {
                        move_piece_and_add_as_move(position, piece_index,
                            SQUARE_INDEX(rank - 1, file - 2));
                    }
                    if (rank < 7)
                    {
                        move_piece_and_add_as_move(position, piece_index,
                            SQUARE_INDEX(rank + 1, file - 2));
                    }
                }
            }
            if (file < 7)
            {
                if (rank > 1)
                {
                    move_piece_and_add_as_move(position, piece_index,
                        SQUARE_INDEX(rank - 2, file + 1));
                }
                if (rank < 6)
                {
                    move_piece_and_add_as_move(position, piece_index,
                        SQUARE_INDEX(rank + 2, file + 1));
                }
                if (file < 6)
                {
                    if (rank > 0)
                    {
                        move_piece_and_add_as_move(position, piece_index,
                            SQUARE_INDEX(rank - 1, file + 2));
                    }
                    if (rank < 7)
                    {
                        move_piece_and_add_as_move(position, piece_index,
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
                add_diagonal_pawn_move(position, piece_index, file - 1);
            }
            if (file < 7)
            {
                add_diagonal_pawn_move(position, piece_index, file + 1);
            }
            uint8_t destination_square_index = piece.square_index + forward_delta * FILE_COUNT;
            if (position->squares[destination_square_index] == NULL_PIECE)
            {
                Position move;
                copy_position(position, &move);
                move_piece(&move, piece_index, destination_square_index);
                move.reset_draw_by_50_count = true;
                destination_square_index += forward_delta * FILE_COUNT;
                add_pawn_move_with_promotions(position, &move, piece_index);
                if (position->squares[destination_square_index] == NULL_PIECE &&
                    RANK(piece.square_index) == KING_RANK(position->active_player_index) +
                        forward_delta)
                {
                    copy_position(position, &move);
                    move_piece(&move, piece_index, destination_square_index);
                    move.reset_draw_by_50_count = true;
                    move.en_passant_file = file;
                    add_move_if_not_king_hang(position, &move);
                }
            }
            break;
        }
        case PIECE_QUEEN:
        {
            add_diagonal_moves(position, piece_index);
            add_moves_along_axes(position, piece_index, 0);
            break;
        }
        case PIECE_ROOK:
        {
            uint8_t castling_right_lost;
            uint8_t file = FILE(piece.square_index);
            switch (file)
            {
            case 0:
            {
                castling_right_lost = 1 << (position->active_player_index << 1);
                break;
            }
            case 7:
            {
                castling_right_lost = 1 << ((position->active_player_index << 1) + 1);
                break;
            }
            default:
            {
                castling_right_lost = 0;
            }
            }
            add_moves_along_axes(position, piece_index, castling_right_lost);
        }
        }
    }
    node->moves_have_been_found = true;
    int16_t new_evaluation;
    if (node->is_leaf)
    {
        if (player_is_checked(position, position->active_player_index))
        {
            new_evaluation = PLAYER_WIN(!position->active_player_index);
        }
        else
        {
            new_evaluation = 0;
        }
    }
    else
    {
        new_evaluation = PLAYER_WIN(!position->active_player_index);
        uint16_t move_node_index = GET_FIRST_MOVE_NODE_INDEX(node);
        PositionTreeNode*move_node = GET_POSITION_TREE_NODE(move_node_index);
        while (true)
        {
            if (move_node->is_canonical)
            {
                move_node->evaluation = 0;
                Position move;
                decompress_position(&move, move_node_index);
                for (size_t player_index = 0; player_index < 2; ++player_index)
                {
                    int16_t point = FORWARD_DELTA(!player_index);
                    Piece*player_pieces = move.pieces + PLAYER_PIECES_INDEX(player_index);
                    for (size_t piece_index = 0; piece_index < 16; ++piece_index)
                    {
                        Piece piece = player_pieces[piece_index];
                        if (piece.square_index < NULL_SQUARE)
                        {
                            switch (piece.piece_type)
                            {
                            case PIECE_PAWN:
                            {
                                move_node->evaluation += point;
                                break;
                            }
                            case PIECE_BISHOP:
                            case PIECE_KNIGHT:
                            {
                                move_node->evaluation += 3 * point;
                                break;
                            }
                            case PIECE_ROOK:
                            {
                                move_node->evaluation += 5 * point;
                                break;
                            }
                            case PIECE_QUEEN:
                            {
                                move_node->evaluation += 9 * point;
                            }
                            }
                        }
                    }
                }
            }
            if (position->active_player_index == PLAYER_INDEX_WHITE)
            {
                if (move_node->evaluation > new_evaluation)
                {
                    new_evaluation = move_node->evaluation;
                }
            }
            else if (move_node->evaluation < new_evaluation)
            {
                new_evaluation = move_node->evaluation;
            }
            if (move_node->next_move_node_index == NULL_POSITION_TREE_NODE)
            {
                break;
            }
            move_node_index = move_node->next_move_node_index;
            move_node = GET_POSITION_TREE_NODE(move_node_index);
        }
    }
    if (new_evaluation == node->evaluation)
    {
        return;
    }
    node->evaluation = new_evaluation;
    propagate_evaluation_to_transpositions(node);
    propagate_evaluation_to_parents(node, position->active_player_index);
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
    if (time_since_last_move >=
        g_times_left_as_of_last_move[g_current_position.active_player_index])
    {
        new_seconds_left = 0;
        out |= UPDATE_TIMER_TIME_OUT;
    }
    else
    {
        new_seconds_left = (g_times_left_as_of_last_move[g_current_position.active_player_index] -
            time_since_last_move) / g_counts_per_second;
    }
    if (new_seconds_left != g_seconds_left[g_current_position.active_player_index])
    {
        g_seconds_left[g_current_position.active_player_index] = new_seconds_left;
        out |= UPDATE_TIMER_REDRAW;
    }
    return out;
}

uint16_t get_first_tree_leaf_index(uint16_t root_node_index)
{
    while (true)
    {
        PositionTreeNode*node = GET_POSITION_TREE_NODE(root_node_index);
        if (node->is_leaf)
        {
            return root_node_index;
        }
        root_node_index = GET_FIRST_MOVE_NODE_INDEX(node);
    }
}

uint16_t get_last_tree_leaf_index(uint16_t root_node_index)
{
    PositionTreeNode*node = GET_POSITION_TREE_NODE(root_node_index);
    if (node->is_leaf)
    {
        return root_node_index;
    }
    uint16_t out = GET_FIRST_MOVE_NODE_INDEX(node);
    while (true)
    {
        node = GET_POSITION_TREE_NODE(out);
        if (node->next_move_node_index == NULL_POSITION_TREE_NODE)
        {
            if (node->is_leaf)
            {
                return out;
            }
            else
            {
                out = GET_FIRST_MOVE_NODE_INDEX(node);
            }
        }
        else
        {
            out = node->next_move_node_index;
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
    uint64_t*time_left_as_of_last_move =
        g_times_left_as_of_last_move + g_current_position.active_player_index;
    if (time_since_last_move >= *time_left_as_of_last_move)
    {
        g_seconds_left[g_current_position.active_player_index] = 0;
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
    g_seconds_left[g_current_position.active_player_index] =
        *time_left_as_of_last_move / g_counts_per_second;
    g_last_move_time = move_time;
    g_selected_piece_index = NULL_PIECE;
    uint16_t selected_move_node_index = *g_selected_move_node_index;
    PositionTreeNode*move_node = GET_POSITION_TREE_NODE(selected_move_node_index);
    *g_selected_move_node_index = move_node->next_move_node_index;
    move_node->parent_index = NULL_POSITION_TREE_NODE;
    if (GET_FIRST_MOVE_NODE_INDEX(GET_POSITION_TREE_NODE(g_current_position.node_index)) ==
        NULL_POSITION_TREE_NODE)
    {
        free_position_tree_node(g_current_position.node_index);
    }
    else
    {
        uint16_t first_leaf_index = get_first_tree_leaf_index(selected_move_node_index);
        PositionTreeNode*move_tree_first_leaf = GET_POSITION_TREE_NODE(first_leaf_index);
        PositionTreeNode*move_tree_last_leaf =
            GET_POSITION_TREE_NODE(get_last_tree_leaf_index(selected_move_node_index));
        if (GET_PREVIOUS_LEAF_INDEX(move_tree_first_leaf) != NULL_POSITION_TREE_NODE)
        {
            SET_NEXT_LEAF_INDEX(
                GET_POSITION_TREE_NODE(GET_PREVIOUS_LEAF_INDEX(move_tree_first_leaf)),
                GET_NEXT_LEAF_INDEX(move_tree_last_leaf));
        }
        if (GET_NEXT_LEAF_INDEX(move_tree_last_leaf) != NULL_POSITION_TREE_NODE)
        {
            SET_PREVIOUS_LEAF_INDEX(GET_POSITION_TREE_NODE(
                GET_NEXT_LEAF_INDEX(move_tree_last_leaf)),
                GET_PREVIOUS_LEAF_INDEX(move_tree_first_leaf));
            SET_NEXT_LEAF_INDEX(move_tree_last_leaf, NULL_POSITION_TREE_NODE);
        }
        SET_PREVIOUS_LEAF_INDEX(move_tree_first_leaf, NULL_POSITION_TREE_NODE);
        g_first_leaf_index = first_leaf_index;
        first_leaf_index = get_first_tree_leaf_index(g_current_position.node_index);
        SET_PREVIOUS_LEAF_INDEX(GET_POSITION_TREE_NODE(first_leaf_index), NULL_POSITION_TREE_NODE);
        SET_NEXT_LEAF_INDEX(
            GET_POSITION_TREE_NODE(get_last_tree_leaf_index(g_current_position.node_index)),
            g_index_of_first_free_position_tree_node);
        g_index_of_first_free_position_tree_node = first_leaf_index;
    }
    uint8_t*player_captured_piece_counts =
        g_captured_piece_counts[!g_current_position.active_player_index];
    uint8_t player_pieces_index = PLAYER_PIECES_INDEX(!g_current_position.active_player_index);
    uint8_t max_piece_index = player_pieces_index + 16;
    for (uint8_t piece_index = player_pieces_index; piece_index < max_piece_index; ++piece_index)
    {
        Piece piece = g_current_position.pieces[piece_index];
        if (piece.square_index != NULL_SQUARE)
        {
            ++player_captured_piece_counts[g_current_position.pieces[piece_index].piece_type];
        }
    }
    decompress_position(&g_current_position, selected_move_node_index);
    for (uint8_t piece_index = player_pieces_index; piece_index < max_piece_index; ++piece_index)
    {
        Piece piece = g_current_position.pieces[piece_index];
        if (piece.square_index != NULL_SQUARE)
        {
            --player_captured_piece_counts[g_current_position.pieces[piece_index].piece_type];
        }
    }
    if (g_current_position.reset_draw_by_50_count)
    {
        g_draw_by_50_count = 0;
        ++g_played_position_generation;
    }
    ++g_draw_by_50_count;
    if (!move_node->moves_have_been_found)
    {
        get_moves(&g_current_position);
    }
    if (move_node->is_leaf)
    {
        if (move_node->evaluation == PLAYER_WIN(!g_current_position.active_player_index))
        {
            return ACTION_CHECKMATE;
        }
        else
        {
            return ACTION_STALEMATE;
        }
    }
    g_next_leaf_to_evaluate_index = g_first_leaf_index;
    if (archive_played_position(get_compressed_position(move_node)))
    {
        g_run_engine = true;
        return ACTION_REDRAW;
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
        uint16_t node_to_evaluate_index = g_next_leaf_to_evaluate_index;
        PositionTreeNode*node;
        while (true)
        {
            if (node_to_evaluate_index == NULL_POSITION_TREE_NODE)
            {
                node_to_evaluate_index = g_first_leaf_index;
            }
            node = GET_POSITION_TREE_NODE(node_to_evaluate_index);
            if (node->is_canonical)
            {
                if (!node->moves_have_been_found)
                {
                    g_next_leaf_to_evaluate_index = GET_NEXT_LEAF_INDEX(node);
                    Position position;
                    decompress_position(&position, node_to_evaluate_index);
                    get_moves(&position);
                    break;
                }
            }
            else if (!node->evaluation_has_been_propagated_to_parents)
            {
                propagate_evaluation_to_parents(node,
                    get_compressed_position(node)->active_player_index);
            }
            if (first_leaf_checked_index == GET_NEXT_LEAF_INDEX(node))
            {
                g_run_engine = false;
                break;
            }
            node_to_evaluate_index = GET_NEXT_LEAF_INDEX(node);
        }
    }
    if (!g_run_engine)
    {
        if (g_current_position.active_player_index == g_engine_player_index)
        {
            int64_t best_evaluation = PLAYER_WIN(!g_engine_player_index);
            PositionTreeNode*current_node = GET_POSITION_TREE_NODE(g_current_position.node_index);
            ASSERT(!current_node->is_leaf);
            uint16_t*move_node_index = &current_node->first_move_node_index;
            g_selected_move_node_index = move_node_index;
            while (*move_node_index != NULL_POSITION_TREE_NODE)
            {
                PositionTreeNode*move_node = GET_POSITION_TREE_NODE(*move_node_index);
                if (g_engine_player_index == PLAYER_INDEX_WHITE)
                {
                    if (move_node->evaluation > best_evaluation)
                    {
                        best_evaluation = move_node->evaluation;
                        g_selected_move_node_index = move_node_index;
                    }
                }
                else if (move_node->evaluation < best_evaluation)
                {
                    best_evaluation = move_node->evaluation;
                    g_selected_move_node_index = move_node_index;
                }
                move_node_index = &move_node->next_move_node_index;
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
    PieceType piece_type)
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
    void*rasterization_memory = ALLOCATE(rasterization_memory_size);
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
            (g_current_position.active_player_index == g_engine_player_index || g_is_promoting))
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
        if (g_selected_piece_index == NULL_PIECE)
        {
            uint8_t square_index =
                SCREEN_SQUARE_INDEX(id - window->controls[MAIN_WINDOW_BOARD].base_id);
            uint8_t selected_piece_index = g_current_position.squares[square_index];
            if (PLAYER_INDEX(selected_piece_index) == g_current_position.active_player_index)
            {
                g_selected_move_node_index =
                    &GET_POSITION_TREE_NODE(g_current_position.node_index)->first_move_node_index;
                while (*g_selected_move_node_index != NULL_POSITION_TREE_NODE)
                {
                    Position move;
                    decompress_position(&move, *g_selected_move_node_index);
                    uint8_t source_square = move.squares[square_index];
                    if (source_square == NULL_PIECE || move.pieces[source_square].piece_type !=
                        g_current_position.pieces[selected_piece_index].piece_type)
                    {
                        g_selected_piece_index = selected_piece_index;
                        return ACTION_REDRAW;
                    }
                    g_selected_move_node_index =
                        &GET_POSITION_TREE_NODE(*g_selected_move_node_index)->next_move_node_index;
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
                Position move;
                decompress_position(&move, *g_selected_move_node_index);
                PositionTreeNode*move_node = GET_POSITION_TREE_NODE(*g_selected_move_node_index);
                while (move.pieces[g_selected_piece_index].piece_type != selected_piece_type)
                {
                    g_selected_move_node_index = &move_node->next_move_node_index;
                    move_node = GET_POSITION_TREE_NODE(move_node->next_move_node_index);
                }
                end_turn();
                g_is_promoting = false;
            }
            else
            {
                Piece current_position_selected_piece =
                    g_current_position.pieces[g_selected_piece_index];
                uint8_t square_index =
                    SCREEN_SQUARE_INDEX(id - window->controls[MAIN_WINDOW_BOARD].base_id);
                if (current_position_selected_piece.square_index != square_index)
                {
                    uint16_t*piece_first_move_node_index = g_selected_move_node_index;
                    do
                    {
                        Position move;
                        decompress_position(&move, *g_selected_move_node_index);
                        uint8_t destination_square = move.squares[square_index];
                        if (PLAYER_INDEX(destination_square) ==
                            g_current_position.active_player_index)
                        {
                            PieceType moved_piece_type = move.pieces[destination_square].piece_type;
                            if (moved_piece_type == current_position_selected_piece.piece_type)
                            {
                                if (current_position_selected_piece.piece_type != moved_piece_type)
                                {
                                    g_is_promoting = true;
                                    return ACTION_REDRAW;
                                }
                                return end_turn();
                            }
                        }
                        g_selected_move_node_index = &GET_POSITION_TREE_NODE(
                            *g_selected_move_node_index)->next_move_node_index;
                    } while (*g_selected_move_node_index != NULL_POSITION_TREE_NODE);
                    g_selected_move_node_index = piece_first_move_node_index;
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
    Window*window = g_windows + WINDOW_MAIN;
    Control*board = window->controls + MAIN_WINDOW_BOARD;
    uint8_t player_pieces_index = PLAYER_PIECES_INDEX(player_index);
    uint8_t max_piece_index = player_pieces_index + 16;
    for (size_t piece_index = player_pieces_index; piece_index < max_piece_index; ++piece_index)
    {
        Piece piece = g_current_position.pieces[piece_index];
        if (piece.square_index != NULL_SQUARE)
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
    int32_t captured_piece_min_x = board->board.min_x;
    uint8_t*player_captured_piece_counts = g_captured_piece_counts[player_index];
    PieceType captured_piece_type_order[] =
    { PIECE_QUEEN, PIECE_ROOK, PIECE_BISHOP, PIECE_KNIGHT, PIECE_PAWN };
    for (size_t piece_type_index = 0; piece_type_index < ARRAY_COUNT(captured_piece_type_order);
        ++piece_type_index)
    {
        PieceType piece_type = captured_piece_type_order[piece_type_index];
        size_t count = player_captured_piece_counts[piece_type];
        for (size_t i = 0; i < count; ++i)
        {
            draw_icon(window, captured_piece_min_x, captured_pieces_min_y, player_index, piece_type);
            captured_piece_min_x += window->dpi_data->square_size / 2;
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

bool load_compressed_position(void**file_memory, CompressedPosition*out, uint32_t*file_size)
{
    if (*file_size < sizeof(CompressedPosition))
    {
        return false;
    }
    memcpy(out, *file_memory, sizeof(CompressedPosition));
    *file_memory = (void*)((uintptr_t)*file_memory + sizeof(CompressedPosition));
    return true;
}

#define SAVE_FILE_STATIC_PART_SIZE 63

uint32_t save_game(void*file_memory)
{
    uint64_t time_since_last_move = get_time() - g_last_move_time;
    uint32_t file_size = 0;
    save_value(&file_memory, &time_since_last_move, &file_size, sizeof(time_since_last_move));
    save_value(&file_memory, &g_time_increment, &file_size, sizeof(g_time_increment));
    CompressedPosition current_position;
    compress_position(&current_position, &g_current_position);
    file_size += sizeof(CompressedPosition);
    memcpy(file_memory, &current_position, sizeof(CompressedPosition));
    file_memory = (void*)((uintptr_t)file_memory + sizeof(CompressedPosition));
    save_value(&file_memory, &g_times_left_as_of_last_move[0], &file_size,
        sizeof(g_times_left_as_of_last_move[0]));
    save_value(&file_memory, &g_times_left_as_of_last_move[1], &file_size,
        sizeof(g_times_left_as_of_last_move[1]));
    save_value(&file_memory, &g_draw_by_50_count, &file_size, sizeof(g_draw_by_50_count));
    save_value(&file_memory, &g_engine_player_index, &file_size, sizeof(g_engine_player_index));
    save_value(&file_memory, &g_unique_played_position_count, &file_size,
        sizeof(g_unique_played_position_count));
    ASSERT(file_size == SAVE_FILE_STATIC_PART_SIZE);
    PlayedPositionRecord*external_records = EXTERNAL_PLAYED_POSITION_RECORDS();
    for (size_t bucket_index = 0; bucket_index < g_played_position_bucket_count; ++bucket_index)
    {
        PlayedPositionRecord*record = g_played_position_records + bucket_index;
        if (record->count && record->generation == g_played_position_generation)
        {
            while (true)
            {
                file_size += sizeof(CompressedPosition);
                memcpy(&record->position, file_memory, sizeof(CompressedPosition));
                file_memory = (void*)((uintptr_t)file_memory + sizeof(CompressedPosition));
                if (record->index_of_next_record != NULL_PLAYED_POSITION_RECORD)
                {
                    record = external_records + record->index_of_next_record;
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

void init_game(void)
{
    PositionTreeNode*current_node = GET_POSITION_TREE_NODE(g_current_position.node_index);
    current_node->parent_index = NULL_POSITION_TREE_NODE;
    current_node->next_move_node_index = NULL_POSITION_TREE_NODE;
    SET_PREVIOUS_LEAF_INDEX(current_node, NULL_POSITION_TREE_NODE);
    SET_NEXT_LEAF_INDEX(current_node, NULL_POSITION_TREE_NODE);
    get_moves(&g_current_position);
    g_next_leaf_to_evaluate_index = g_first_leaf_index;
    archive_played_position(&g_tree_position_records[GET_RECORD_INDEX(current_node)].position);
    g_selected_piece_index = NULL_PIECE;
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
    PositionTreeNode*current_node = GET_POSITION_TREE_NODE(g_first_leaf_index);
    load_compressed_position(&file_memory,
        &g_tree_position_records[GET_RECORD_INDEX(current_node)].position, &file_size);
    decompress_position(&g_current_position, g_first_leaf_index);
    if (!load_value(&file_memory, &g_times_left_as_of_last_move[0], &file_size,
        sizeof(g_times_left_as_of_last_move[0])))
    {
        return false;
    }
    if (!load_value(&file_memory, &g_times_left_as_of_last_move[1], &file_size,
        sizeof(g_times_left_as_of_last_move[1])))
    {
        return false;
    }
    if (g_times_left_as_of_last_move[g_current_position.active_player_index] < time_since_last_move)
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
    if (!load_value(&file_memory, &g_unique_played_position_count, &file_size,
        sizeof(g_unique_played_position_count)))
    {
        return false;
    }
    uint32_t bucket_count_bit_count;
    BIT_SCAN_REVERSE(&bucket_count_bit_count, g_unique_played_position_count);
    uint16_t position_bucket_count = 1 << bucket_count_bit_count;
    if (position_bucket_count < g_unique_played_position_count)
    {
        position_bucket_count = position_bucket_count << 1;
    }
    init_played_position_archive(position_bucket_count);
    for (uint16_t i = 0; i < g_unique_played_position_count; ++i)
    {
        CompressedPosition position;
        load_compressed_position(&file_memory, &position, &file_size);
        archive_played_position(&position);
    }
    init_game();
    g_last_move_time = get_time() - time_since_last_move;
    return true;
}

bool load_game(void*file_memory, uint32_t file_size)
{
    for (size_t i = 0; i < ARRAY_COUNT(g_tree_position_buckets); ++i)
    {
        g_tree_position_buckets[i] = NULL_POSITION_TREE_NODE;
    }
    g_first_leaf_index = allocate_position_tree_node();
    bool out = load_file(file_memory, file_size);
    if (out)
    {
        g_run_engine = true;
    }
    else
    {
        free_position_tree_node(g_first_leaf_index);
    }
    return out;
}

void init_pools()
{
    g_position_tree_node_pool_cursor = 0;
    g_index_of_first_free_position_tree_node = NULL_POSITION_TREE_NODE;
    g_tree_position_record_cursor = 0;
    g_index_of_first_free_tree_position_record = NULL_POSITION_TREE_NODE;
}

void free_game(void)
{
    FREE_MEMORY(g_played_position_records);
    init_pools();
}

void init_piece(PieceType piece_type, uint8_t piece_index, uint8_t square_index,
    uint8_t player_index)
{
    piece_index += PLAYER_PIECES_INDEX(player_index);
    Piece*piece = g_current_position.pieces + piece_index;
    piece->square_index = square_index;
    piece->piece_type = piece_type;
    g_current_position.squares[square_index] = piece_index;
}

void init_new_game(void)
{
    for (uint8_t player_index = 0; player_index < 2; ++player_index)
    {
        uint8_t player_pieces_index = PLAYER_PIECES_INDEX(player_index);
        uint8_t rank = KING_RANK(player_index);
        init_piece(PIECE_KING, 0, SQUARE_INDEX(rank, 4), player_index);
        init_piece(PIECE_ROOK, 1, SQUARE_INDEX(rank, 0), player_index);
        init_piece(PIECE_KNIGHT, 2, SQUARE_INDEX(rank, 1), player_index);
        init_piece(PIECE_BISHOP, 3, SQUARE_INDEX(rank, 2), player_index);
        init_piece(PIECE_QUEEN, 4, SQUARE_INDEX(rank, 3), player_index);
        init_piece(PIECE_BISHOP, 5, SQUARE_INDEX(rank, 5), player_index);
        init_piece(PIECE_KNIGHT, 6, SQUARE_INDEX(rank, 6), player_index);
        init_piece(PIECE_ROOK, 7, SQUARE_INDEX(rank, 7), player_index);
        rank += FORWARD_DELTA(player_index);
        for (uint8_t file = 0; file < 8; ++file)
        {
            init_piece(PIECE_PAWN, 8 + file, SQUARE_INDEX(rank, file), player_index);
        }
        g_seconds_left[player_index] = g_time_control[4].digit + 10 * g_time_control[3].digit +
            60 * g_time_control[2].digit + 600 * g_time_control[1].digit +
            3600 * g_time_control[0].digit;
    }
    for (size_t square_index = 16; square_index < 48; ++square_index)
    {
        g_current_position.squares[square_index] = NULL_PIECE;
    }
    g_current_position.en_passant_file = FILE_COUNT;
    g_current_position.active_player_index = PLAYER_INDEX_WHITE;
    for (size_t i = 0; i < ARRAY_COUNT(g_tree_position_buckets); ++i)
    {
        g_tree_position_buckets[i] = NULL_POSITION_TREE_NODE;
    }
    g_first_leaf_index = allocate_position_tree_node();
    compress_position_to_node(&g_current_position, GET_POSITION_TREE_NODE(g_first_leaf_index));
    init_played_position_archive(32);
    init_game();
    Window*window = g_windows + WINDOW_MAIN;
    window->hovered_control_id = NULL_CONTROL;
    window->clicked_control_id = NULL_CONTROL;
    g_draw_by_50_count = 0;
    g_run_engine = true;
    g_is_promoting = false;
    memset(g_captured_piece_counts, 0, sizeof(g_captured_piece_counts));
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
    init_pools();
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